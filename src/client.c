/* client.c - lab client agent.
 * Collects this machine's metrics (or simulated ones), registers with the
 * server, sends periodic health updates and reconnects with backoff after
 * failures. Collects ONLY the documented metrics - never passwords,
 * keystrokes, screenshots or personal files. */
#include "client.h"
#include "protocol.h"
#include "monitor.h"

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef SOCKET sock_t;
  #define BAD_SOCK INVALID_SOCKET
  #define close_sock closesocket
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  typedef int sock_t;
  #define BAD_SOCK (-1)
  #define close_sock close
#endif

static void sleep_slice(volatile int *stop, int ms) {
    int step = 100, done = 0;
    while (done < ms) {
        if (stop && *stop) return;
#ifdef _WIN32
        Sleep((DWORD)step);
#else
        usleep(100000);
#endif
        done += step;
    }
}

static sock_t connect_to(const AppConfig *cfg) {
    sock_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == BAD_SOCK) return BAD_SOCK;
    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET; a.sin_port = htons((unsigned short)cfg->port);
    if (inet_pton(AF_INET, cfg->server_host, &a.sin_addr) != 1) { close_sock(s); return BAD_SOCK; }
    /* 5 second send/recv timeouts so a dead server cannot hang the agent */
    int tv = 5000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));
    if (connect(s, (struct sockaddr *)&a, sizeof(a)) != 0) { close_sock(s); return BAD_SOCK; }
    return s;
}

static int send_metrics(sock_t s, const AppConfig *cfg, MsgType type, const HealthMetrics *m) {
    char kv[512];
    snprintf(kv, sizeof(kv), "token=%s;host=%s;cpu=%.1f;ram=%.1f;net=%d;sw=%d;procs=%d",
             cfg->lab_token, m->hostname, m->cpu_pct, m->ram_pct, m->net_ok, m->software_ok, m->process_count);
    char payload[SEL_MAX_PAYLOAD];
    int n = proto_build(payload, sizeof(payload), type, cfg->pc_id, sel_now(), kv);
    if (n <= 0) return -1;
    if (proto_send((long long)s, payload, n) != 0) return -1;
    char buf[SEL_MAX_PAYLOAD + 1];
    int r = proto_recv((long long)s, buf, sizeof(buf));   /* expect ACK */
    if (r <= 0) return -1;
    Message ack; if (proto_parse(buf, r, &ack) != 0 || ack.type != MSG_ACK) return -1;
    return 0;
}

int client_run(const AppConfig *cfg, int simulate, volatile int *stop_flag) {
#ifdef _WIN32
    WSADATA w; if (WSAStartup(MAKEWORD(2,2), &w) != 0) return -1;
#endif
    unsigned int tick = 0;
    int backoff_s = 2;
    while (!stop_flag || !*stop_flag) {
        sock_t s = connect_to(cfg);
        if (s == BAD_SOCK) { sleep_slice(stop_flag, backoff_s * 1000); if (backoff_s < 10) backoff_s += 2; continue; }
        backoff_s = 2;
        HealthMetrics m;
        if (simulate) monitor_simulate(&m, cfg->pc_id, tick);
        else monitor_collect(&m, cfg->pc_id, 250, cfg->required_software);
        if (send_metrics(s, cfg, MSG_REGISTER, &m) != 0) { close_sock(s); sleep_slice(stop_flag, 2000); continue; }
        int failed = 0;
        while ((!stop_flag || !*stop_flag) && !failed) {
            sleep_slice(stop_flag, cfg->heartbeat_interval_s * 1000);
            if (stop_flag && *stop_flag) break;
            tick++;
            if (simulate) monitor_simulate(&m, cfg->pc_id, tick);
            else monitor_collect(&m, cfg->pc_id, 250, cfg->required_software);
            if (send_metrics(s, cfg, MSG_HEALTH_UPDATE, &m) != 0) failed = 1; /* -> reconnect */
        }
        /* graceful shutdown note (best effort) */
        char payload[SEL_MAX_PAYLOAD];
        char kv[160]; snprintf(kv, sizeof(kv), "token=%s", cfg->lab_token);
        int n = proto_build(payload, sizeof(payload), MSG_SHUTDOWN, cfg->pc_id, sel_now(), kv);
        if (n > 0) proto_send((long long)s, payload, n);
        close_sock(s);
    }
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
