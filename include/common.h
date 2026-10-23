/* common.h - shared types for Smart Exam Lab Management System
 * Project: Smart Exam Lab Management System (Operating Systems project)
 * Owner: Rajesh Kumar Gautam (24BCE0326)
 * Team: Shourya Thakur (24BCI0171), Priyanshu Gautam (24BCE0317)
 * Submitted to: Prof. Geraldine Bessie Amali, SCOPE
 */
#ifndef SEL_COMMON_H
#define SEL_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define SEL_MAX_ID        32
#define SEL_MAX_HOST      64
#define SEL_MAX_NAME      96
#define SEL_MAX_MSG       1024   /* max protocol payload bytes */
#define SEL_MAX_LINE      512

/* PC status categories. Precedence (highest first) is implemented in
 * monitor.c: DISCONNECTED > MAINTENANCE > NOT_READY > ASSIGNED > WARNING > READY */
typedef enum {
    ST_UNKNOWN = 0,
    ST_READY,
    ST_WARNING,
    ST_NOT_READY,
    ST_DISCONNECTED,
    ST_ASSIGNED,
    ST_MAINTENANCE
} PcStatus;

typedef enum {
    SEV_INFO = 0, SEV_WARNING, SEV_CRITICAL
} Severity;

typedef struct {
    char   pc_id[SEL_MAX_ID];
    char   hostname[SEL_MAX_HOST];
    double cpu_pct;        /* 0..100, interval-based measurement */
    double ram_pct;        /* 0..100 */
    double ram_avail_mb;
    int    net_ok;         /* 1 reachable / 0 not */
    int    software_ok;    /* 1 all required software present / 0 missing */
    int    process_count;  /* observed running processes (process mgmt demo) */
    long   timestamp;      /* epoch seconds when measured */
} HealthMetrics;

typedef struct {
    double cpu_warn_pct;       /* default 85 */
    double ram_warn_pct;       /* default 90 */
    long   heartbeat_timeout_s;/* default 30 */
    long   freshness_s;        /* max age of health record for allocation, default 30 */
    int    require_software;   /* default 1 */
} HealthThresholds;

const char *sel_status_name(PcStatus s);
const char *sel_severity_name(Severity s);
long        sel_now(void);
void        sel_format_time(long epoch, char *buf, size_t n);
void        sel_safe_copy(char *dst, size_t dstsz, const char *src);
int         sel_clamp_pct(double v); /* returns 0/1 validity */

#endif
