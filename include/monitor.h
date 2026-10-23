/* monitor.h - system metrics collection + health evaluation */
#ifndef SEL_MONITOR_H
#define SEL_MONITOR_H

#include "common.h"

/* Collect metrics for this machine.
 * Real backends: Windows API (GetSystemTimes / GlobalMemoryStatusEx) on _WIN32,
 * /proc on Linux. CPU% is interval based: two samples 'interval_ms' apart. */
int monitor_collect(HealthMetrics *m, const char *pc_id, int interval_ms,
                    const char *required_software_csv);

/* Deterministic simulated metrics for one simulated PC (simulation mode only).
 * seed varies per PC; 'tick' advances time. Occasionally produces a warning /
 * not-ready PC so dashboards and alerts can be demonstrated honestly. */
void monitor_simulate(HealthMetrics *m, const char *pc_id, unsigned int tick);

/* Pure, unit-testable health evaluation.
 * assigned: PC already has an active assignment
 * maintenance: admin flagged the PC
 * age_s: seconds since last heartbeat (server-side freshness) */
PcStatus health_evaluate(const HealthMetrics *m, const HealthThresholds *th,
                         int assigned, int maintenance, long age_s);

int monitor_software_present(const char *exe_name); /* 1 present, 0 missing */
int monitor_net_ok(void);                            /* TCP connect check to gateway-ish target */

#endif
