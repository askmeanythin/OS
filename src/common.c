#include "common.h"

const char *sel_status_name(PcStatus s) {
    switch (s) {
        case ST_READY: return "READY";
        case ST_WARNING: return "WARNING";
        case ST_NOT_READY: return "NOT_READY";
        case ST_DISCONNECTED: return "DISCONNECTED";
        case ST_ASSIGNED: return "ASSIGNED";
        case ST_MAINTENANCE: return "MAINTENANCE";
        default: return "UNKNOWN";
    }
}
const char *sel_severity_name(Severity s) {
    switch (s) { case SEV_WARNING: return "WARNING"; case SEV_CRITICAL: return "CRITICAL"; default: return "INFO"; }
}
long sel_now(void) { return (long)time(NULL); }
void sel_format_time(long epoch, char *buf, size_t n) {
    if (epoch <= 0) { sel_safe_copy(buf, n, "-"); return; }
    time_t t = (time_t)epoch; struct tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    strftime(buf, n, "%Y-%m-%d %H:%M:%S", &tmv);
}
void sel_safe_copy(char *dst, size_t dstsz, const char *src) {
    if (!dst || dstsz == 0) return;
    if (!src) src = "";
    size_t i = 0; for (; i + 1 < dstsz && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}
int sel_clamp_pct(double v) { return (v >= 0.0 && v <= 100.0) ? 1 : 0; }
