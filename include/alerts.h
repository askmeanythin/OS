/* alerts.h - alert lifecycle: raise (deduplicated), acknowledge, resolve */
#ifndef SEL_ALERTS_H
#define SEL_ALERTS_H

#include "common.h"

/* Raises an alert unless an OPEN/ACKED alert for same pc+category exists.
 * Returns alert id, 0 if deduplicated, -1 on error. */
int alerts_raise(const char *pc_id, const char *exam_id, const char *category,
                 Severity sev, const char *message);
int alerts_acknowledge(int alert_id, const char *by);
int alerts_resolve(int alert_id);
int alerts_auto_resolve_pc(const char *pc_id, const char *category);
/* Evaluate a fresh health record and raise/resolve alerts accordingly. */
void alerts_for_health(const char *pc_id, const HealthMetrics *m, PcStatus st);

#endif
