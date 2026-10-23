/* server.c - central server.
 * Thread-per-connection design: one listener thread, one thread per client,
 * one sweeper thread for heartbeat-timeout detection, admin console runs on
 * the main thread. A slow client therefore never blocks the others.
 * Winsock2 on Windows (link ws2_32), POSIX sockets on Linux for dev/testing. */
#include "server.h"
#include "protocol.h"
#include "database.h"
#include "monitor.h"
#include "alerts.h"
#include "client.h"
#include "sqlite3.h"

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef SOCKET sock_t;
  #define BAD_SOCK INVALID_SOCKET
  #define close_sock closesocket
  typedef HANDLE thread_t;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <pthread.h>
  typedef int sock_t;
  #define BAD_SOCK (-1)
  #define close_sock close
  typedef pthread_t thread_t;
#endif

static AppConfig      g_cfg;
static HealthThresholds g_th;
static volatile int   g_running = 0;
static volatile int   g_sim_stop = 0;
static sock_t         g_listen = BAD_SOCK;

static int start_thread(void *(*fn)(void *), void *arg) {
#ifdef _WIN32
    HANDLE h = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)fn, arg, 0, NULL);
    if (!h) return -1; CloseHandle(h); return 0;
#else
    pthread_t t; int r = pthread_create(&t, NULL, fn, arg); if (r == 0) pthread_detach(t); return r ? -1 : 0;
#endif
}
static void sleep_s(int s) {
#ifdef _WIN32
    Sleep((DWORD)s * 1000);
#else
    sleep((unsigned)s);
#endif
}

static void handle_message(sock_t c, const Message *msg, int *authed) {
    char tok[64] = "";
    proto_kv_get(msg, "token", tok, sizeof(tok));
    if (strcmp(tok, g_cfg.lab_token) != 0) {
        char out[SEL_MAX_PAYLOAD]; int n = proto_build(out, sizeof(out), MSG_ERROR, msg->pc_id, sel_now(), "reason=bad_token");
        if (n > 0) proto_send((long long)c, out, n);
        return;
    }
    if (msg->type == MSG_REGISTER || msg->type == MSG_HEALTH_UPDATE || msg->type == MSG_HEARTBEAT) {
        *authed = 1;
        HealthMetrics m; memset(&m, 0, sizeof(m));
        sel_safe_copy(m.pc_id, sizeof(m.pc_id), msg->pc_id);
        char v[128];
        if (proto_kv_get(msg, "host", v, sizeof(v))) sel_safe_copy(m.hostname, sizeof(m.hostname), v);
        if (proto_kv_get(msg, "cpu", v, sizeof(v))) m.cpu_pct = atof(v);
        if (proto_kv_get(msg, "ram", v, sizeof(v))) m.ram_pct = atof(v);
        if (proto_kv_get(msg, "net", v, sizeof(v))) m.net_ok = atoi(v);
        if (proto_kv_get(msg, "sw", v, sizeof(v))) m.software_ok = atoi(v);
        if (proto_kv_get(msg, "procs", v, sizeof(v))) m.process_count = atoi(v);
        m.timestamp = msg->timestamp ? msg->timestamp : sel_now();
        if (!sel_clamp_pct(m.cpu_pct) || !sel_clamp_pct(m.ram_pct)) {
            char out[SEL_MAX_PAYLOAD]; int n = proto_build(out, sizeof(out), MSG_ERROR, msg->pc_id, sel_now(), "reason=bad_metrics");
            if (n > 0) proto_send((long long)c, out, n);
            return;
        }
        db_lock();
        PcStatus prev = ST_UNKNOWN; db_get_computer_status(m.pc_id, &prev);
        int assigned = db_is_assigned(m.pc_id);
        sqlite3_stmt *q = NULL; int maint = 0;
        sqlite3_prepare_v2(g_db, "SELECT maintenance FROM computers WHERE pc_id=?1", -1, &q, NULL);
        sqlite3_bind_text(q,1,m.pc_id,-1,SQLITE_TRANSIENT);
        if (q && sqlite3_step(q) == SQLITE_ROW) maint = sqlite3_column_int(q,0);
        sqlite3_finalize(q);
        PcStatus st = health_evaluate(&m, &g_th, assigned, maint, 0);
        db_upsert_computer(&m, st);
        db_insert_health(&m, st);
        alerts_for_health(m.pc_id, &m, st);
        if (prev == ST_DISCONNECTED) alerts_auto_resolve_pc(m.pc_id, "DISCONNECTED");
        db_unlock();
        char out[SEL_MAX_PAYLOAD]; int n = proto_build(out, sizeof(out), MSG_ACK, msg->pc_id, sel_now(), "ok=1");
        if (n > 0) proto_send((long long)c, out, n);
        return;
    }
    if (msg->type == MSG_SHUTDOWN) { *authed = 1; return; }
    char out[SEL_MAX_PAYLOAD]; int n = proto_build(out, sizeof(out), MSG_ERROR, msg->pc_id, sel_now(), "reason=unsupported_type");
    if (n > 0) proto_send((long long)c, out, n);
}

static void *client_thread(void *arg) {
    sock_t c = (sock_t)(uintptr_t)arg;
    int authed = 0;
    char buf[SEL_MAX_PAYLOAD + 1];
    for (;;) {
        int n = proto_recv((long long)c, buf, sizeof(buf));
        if (n <= 0) break;                       /* disconnect / malformed / oversized */
        Message msg;
        if (proto_parse(buf, n, &msg) != 0) break; /* malformed message: drop connection safely */
        handle_message(c, &msg, &authed);
        if (msg.type == MSG_SHUTDOWN) break;
    }
    close_sock(c);
    (void)authed;
    return NULL;
}

static void *sweeper_thread(void *arg) {
    (void)arg;
    while (g_running) {
        sleep_s(2);
        if (!g_running) break;
        db_lock();
        long cutoff = sel_now() - g_th.heartbeat_timeout_s;
        sqlite3_stmt *q = NULL;
        sqlite3_prepare_v2(g_db, "SELECT pc_id FROM computers WHERE status<>'DISCONNECTED' AND last_seen>0 AND last_seen<?1", -1, &q, NULL);
        sqlite3_bind_int64(q,1,cutoff);
        char ids[64][SEL_MAX_ID]; int nids = 0;
        while (q && sqlite3_step(q) == SQLITE_ROW && nids < 64)
            sel_safe_copy(ids[nids++], SEL_MAX_ID, (const char *)sqlite3_column_text(q,0));
        sqlite3_finalize(q);
        for (int i = 0; i < nids; i++) {
            db_set_computer_status(ids[i], ST_DISCONNECTED);
            char msg[256]; snprintf(msg, sizeof(msg), "%.31s heartbeat timeout - client disconnected or network down", ids[i]);
            alerts_raise(ids[i], NULL, "DISCONNECTED", SEV_CRITICAL, msg);
        }
        db_unlock();
    }
    return NULL;
}

static void *listener_thread(void *arg) {
    (void)arg;
    while (g_running) {
        struct sockaddr_in peer; 
#ifdef _WIN32
        int plen = sizeof(peer);
#else
        socklen_t plen = sizeof(peer);
#endif
        sock_t c = accept(g_listen, (struct sockaddr *)&peer, &plen);
        if (c == BAD_SOCK) { if (g_running) continue; break; }
        if (start_thread(client_thread, (void *)(uintptr_t)c) != 0) close_sock(c);
    }
    return NULL;
}

int server_start(const AppConfig *cfg) {
    g_cfg = *cfg;
    g_th.cpu_warn_pct = cfg->cpu_warn_pct;
    g_th.ram_warn_pct = cfg->ram_warn_pct;
    g_th.heartbeat_timeout_s = cfg->heartbeat_timeout_s;
    g_th.freshness_s = cfg->heartbeat_timeout_s;
    g_th.require_software = 1;
#ifdef _WIN32
    WSADATA w; if (WSAStartup(MAKEWORD(2,2), &w) != 0) { fprintf(stderr, "WSAStartup failed\n"); return -1; }
#endif
    g_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listen == BAD_SOCK) { fprintf(stderr, "socket() failed\n"); return -1; }
    int one = 1; setsockopt(g_listen, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_port = htons((unsigned short)cfg->port);
    if (inet_pton(AF_INET, cfg->bind_addr, &addr.sin_addr) != 1) addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(g_listen, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "bind %s:%d failed (port in use or blocked?)\n", cfg->bind_addr, cfg->port);
        close_sock(g_listen); g_listen = BAD_SOCK; return -1;
    }
    if (listen(g_listen, 32) != 0) { fprintf(stderr, "listen failed\n"); close_sock(g_listen); return -1; }
    g_running = 1;
    if (start_thread(listener_thread, NULL) != 0) return -1;
    if (start_thread(sweeper_thread, NULL) != 0) return -1;
    printf("Server listening on %s:%d (db: %s)\n", cfg->bind_addr, cfg->port, cfg->db_path);
    return 0;
}

void server_stop(void) {
    g_running = 0; g_sim_stop = 1;
    if (g_listen != BAD_SOCK) { close_sock(g_listen); g_listen = BAD_SOCK; }
#ifdef _WIN32
    WSACleanup();
#endif
}
int server_is_running(void) { return g_running; }

/* ---- simulation: real client agents in threads, over real loopback TCP ---- */
typedef struct { AppConfig cfg; } SimArg;
static void *sim_thread(void *arg) {
    SimArg *a = (SimArg *)arg;
    client_run(&a->cfg, 1, &g_sim_stop);
    free(a);
    return NULL;
}
int server_start_simulation(int n_pcs) {
    if (n_pcs < 1 || n_pcs > 60) return -1;
    g_sim_stop = 0;
    for (int i = 0; i < n_pcs; i++) {
        SimArg *a = (SimArg *)malloc(sizeof(SimArg)); if (!a) return -1;
        a->cfg = g_cfg;
        snprintf(a->cfg.pc_id, sizeof(a->cfg.pc_id), "SIM-PC-%02d", i + 1);
        sel_safe_copy(a->cfg.server_host, sizeof(a->cfg.server_host), "127.0.0.1");
        if (start_thread(sim_thread, a) != 0) { free(a); return -1; }
    }
    printf("Simulation: %d simulated PC agents started (loopback TCP, simulated metrics).\n", n_pcs);
    return 0;
}
