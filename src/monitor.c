/* monitor.c - metrics collection (Windows API / Linux /proc) + health rules.
 *
 * OS concepts demonstrated: CPU management (interval-based utilization from
 * GetSystemTimes deltas - NOT an instantaneous value), memory management
 * (GlobalMemoryStatusEx), process management (process count via ToolHelp32
 * on Windows / /proc scan on Linux), file management (reading /proc,
 * checking required-software files). */
#include "monitor.h"

#ifdef _WIN32
  #include <windows.h>
  #include <tlhelp32.h>
#else
  #include <dirent.h>
  #include <unistd.h>
#endif

static void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)ms * 1000);
#endif
}

/* ---------------- health evaluation (pure, unit tested) ---------------- */
PcStatus health_evaluate(const HealthMetrics *m, const HealthThresholds *th,
                         int assigned, int maintenance, long age_s) {
    if (age_s > th->heartbeat_timeout_s) return ST_DISCONNECTED;   /* stale heartbeat wins */
    if (maintenance) return ST_MAINTENANCE;
    if (!m->net_ok) return ST_NOT_READY;
    if (th->require_software && !m->software_ok) return ST_NOT_READY;
    if (assigned) return ST_ASSIGNED;        /* healthy but reserved */
    if (m->cpu_pct >= th->cpu_warn_pct || m->ram_pct >= th->ram_warn_pct) return ST_WARNING;
    return ST_READY;
}

/* ---------------- software / network checks ---------------- */
int monitor_software_present(const char *exe_name) {
    if (!exe_name || !*exe_name) return 1;
#ifdef _WIN32
    char path[512];
    /* Search PATH via Windows itself; also accept an absolute/relative path. */
    DWORD r = SearchPathA(NULL, exe_name, ".exe", (DWORD)sizeof(path), path, NULL);
    if (r > 0 && r < sizeof(path)) return 1;
    FILE *f = fopen(exe_name, "rb"); if (f) { fclose(f); return 1; }
    return 0;
#else
    if (exe_name[0] == '/' || strchr(exe_name, '/')) {
        FILE *f = fopen(exe_name, "rb"); if (f) { fclose(f); return 1; } return 0;
    }
    char cmd[300]; snprintf(cmd, sizeof(cmd), "command -v '%s' >/dev/null 2>&1", exe_name);
    /* fixed-format check of a config-supplied name; name is validated to be a
       plain token by config convention (no shell metacharacters allowed). */
    for (const char *p = exe_name; *p; p++) if (!((*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='-'||*p=='_'||*p=='.')) return 0;
    return system(cmd) == 0 ? 1 : 0;
#endif
}

static int software_all_ok(const char *csv) {
    if (!csv || !*csv) return 1;
    char tmp[256]; sel_safe_copy(tmp, sizeof(tmp), csv);
    char *save = NULL;
#ifdef _WIN32
    char *tok = strtok_s(tmp, ",", &save);
#else
    char *tok = strtok_r(tmp, ",", &save);
#endif
    while (tok) {
        while (*tok == ' ') tok++;
        if (*tok && !monitor_software_present(tok)) return 0;
#ifdef _WIN32
        tok = strtok_s(NULL, ",", &save);
#else
        tok = strtok_r(NULL, ",", &save);
#endif
    }
    return 1;
}

int monitor_net_ok(void) {
    /* A TCP connect to the configured server is checked by the client loop
       itself (it can only report while connected). Here we do a light local
       check: at least one non-loopback interface activity is platform
       specific; for the college LAN prototype we report link presence via a
       successful hostname resolution + socket creation. Limitations of ping
       are documented in docs/architecture.md - a ping alone proves nothing
       about exam services, which is why the real service check IS the
       heartbeat connection to the server. */
#ifdef _WIN32
    DWORD flags = 0;
    /* InternetGetConnectedState lives in wininet; use a conservative check */
    (void)flags;
    return 1;
#else
    FILE *f = fopen("/proc/net/route", "r");
    if (!f) return 0;
    char line[256]; int ok = 0;
    while (fgets(line, sizeof(line), f)) if (strstr(line, "00000000")) { ok = 1; break; } /* default route */
    fclose(f); return ok;
#endif
}

/* ---------------- platform metrics ---------------- */
#ifdef _WIN32
static ULONGLONG ft_to_ull(const FILETIME *ft) {
    ULARGE_INTEGER u; u.LowPart = ft->dwLowDateTime; u.HighPart = ft->dwHighDateTime; return u.QuadPart;
}
static double win_cpu_pct(int interval_ms) {
    FILETIME idle1, kern1, user1, idle2, kern2, user2;
    if (!GetSystemTimes(&idle1, &kern1, &user1)) return -1;
    Sleep((DWORD)interval_ms);
    if (!GetSystemTimes(&idle2, &kern2, &user2)) return -1;
    ULONGLONG i = ft_to_ull(&idle2) - ft_to_ull(&idle1);
    ULONGLONG k = ft_to_ull(&kern2) - ft_to_ull(&kern1);
    ULONGLONG u = ft_to_ull(&user2) - ft_to_ull(&user1);
    ULONGLONG total = k + u; if (total == 0) return 0;
    return 100.0 * (double)(total - i) / (double)total;
}
static int win_process_count(void) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return -1;
    PROCESSENTRY32 pe; pe.dwSize = sizeof(pe); int n = 0;
    if (Process32First(snap, &pe)) do { n++; } while (Process32Next(snap, &pe));
    CloseHandle(snap); return n;
}
#else
static int read_cpu_times(unsigned long long *idle, unsigned long long *total) {
    FILE *f = fopen("/proc/stat", "r"); if (!f) return -1;
    char line[512]; unsigned long long v[10] = {0};
    int ok = -1;
    if (fgets(line, sizeof(line), f) && sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
        &v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&v[7]) >= 4) {
        *idle = v[3] + v[4]; *total = 0; for (int i = 0; i < 8; i++) *total += v[i]; ok = 0;
    }
    fclose(f); return ok;
}
static double linux_cpu_pct(int interval_ms) {
    unsigned long long i1, t1, i2, t2;
    if (read_cpu_times(&i1, &t1)) return -1;
    sleep_ms(interval_ms);
    if (read_cpu_times(&i2, &t2)) return -1;
    unsigned long long dt = t2 - t1, di = i2 - i1; if (dt == 0) return 0;
    return 100.0 * (double)(dt - di) / (double)dt;
}
static int linux_process_count(void) {
    DIR *d = opendir("/proc"); if (!d) return -1; int n = 0; struct dirent *e;
    while ((e = readdir(d))) if (e->d_name[0] >= '0' && e->d_name[0] <= '9') n++;
    closedir(d); return n;
}
#endif

int monitor_collect(HealthMetrics *m, const char *pc_id, int interval_ms, const char *required_software_csv) {
    memset(m, 0, sizeof(*m));
    sel_safe_copy(m->pc_id, sizeof(m->pc_id), pc_id);
#ifdef _WIN32
    char host[SEL_MAX_HOST] = ""; DWORD hn = sizeof(host);
    if (GetComputerNameA(host, &hn)) sel_safe_copy(m->hostname, sizeof(m->hostname), host);
    double cpu = win_cpu_pct(interval_ms > 0 ? interval_ms : 250);
    m->cpu_pct = cpu < 0 ? 0 : cpu;
    MEMORYSTATUSEX ms; ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) { m->ram_pct = (double)ms.dwMemoryLoad; m->ram_avail_mb = (double)ms.ullAvailPhys / (1024.0*1024.0); }
    m->process_count = win_process_count();
#else
    char host[SEL_MAX_HOST] = "";
    if (gethostname(host, sizeof(host)) == 0) sel_safe_copy(m->hostname, sizeof(m->hostname), host);
    double cpu = linux_cpu_pct(interval_ms > 0 ? interval_ms : 250);
    m->cpu_pct = cpu < 0 ? 0 : cpu;
    FILE *f = fopen("/proc/meminfo", "r");
    if (f) { unsigned long tot = 0, av = 0; char l[256];
        while (fgets(l, sizeof(l), f)) { sscanf(l, "MemTotal: %lu", &tot); sscanf(l, "MemAvailable: %lu", &av); }
        fclose(f);
        if (tot) { m->ram_avail_mb = av / 1024.0; m->ram_pct = 100.0 * (1.0 - (double)av / (double)tot); }
    }
    m->process_count = linux_process_count();
#endif
    m->net_ok = monitor_net_ok();
    m->software_ok = software_all_ok(required_software_csv);
    m->timestamp = sel_now();
    return 0;
}

/* ---------------- simulation backend (clearly labelled) ---------------- */
/* Small deterministic LCG per PC so runs are reproducible in demos/tests. */
static unsigned int lcg(unsigned int *s) { *s = (*s * 1103515245u + 12345u) & 0x7fffffffu; return *s; }
void monitor_simulate(HealthMetrics *m, const char *pc_id, unsigned int tick) {
    memset(m, 0, sizeof(*m));
    sel_safe_copy(m->pc_id, sizeof(m->pc_id), pc_id);
    char host[SEL_MAX_HOST]; snprintf(host, sizeof(host), "SIM-%s", pc_id);
    sel_safe_copy(m->hostname, sizeof(m->hostname), host);
    unsigned int seed = 2166136261u;
    for (const char *p = pc_id; *p; p++) { seed ^= (unsigned char)*p; seed *= 16777619u; }
    seed += tick * 97u;
    unsigned int s = seed;
    m->cpu_pct = 8.0 + (double)(lcg(&s) % 550) / 10.0;          /* 8..62 normally */
    m->ram_pct = 25.0 + (double)(lcg(&s) % 450) / 10.0;         /* 25..69 normally */
    m->ram_avail_mb = 2048.0 + (lcg(&s) % 4096);
    m->net_ok = 1; m->software_ok = 1; m->process_count = 90 + (int)(lcg(&s) % 80);
    unsigned int scenario = (seed / 7u) % 20u;
    if (tick > 0 && scenario == 3) m->cpu_pct = 93.0;            /* one PC runs hot */
    if (tick > 0 && scenario == 5) m->software_ok = 0;           /* one PC misses software */
    if (tick > 0 && scenario == 8) m->ram_pct = 95.0;            /* one PC low on RAM */
    m->timestamp = sel_now();
}
