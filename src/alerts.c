/* alerts.c - deduplicated alert lifecycle */
#include "alerts.h"
#include "database.h"
#include "sqlite3.h"

int alerts_raise(const char *pc_id, const char *exam_id, const char *category,
                 Severity sev, const char *message) {
    sqlite3_stmt *s = NULL;
    /* partial unique index ux_open_alert also enforces dedup at DB level */
    sqlite3_prepare_v2(g_db,
        "INSERT OR IGNORE INTO alerts(pc_id,exam_id,category,severity,message,created_at) VALUES(?1,?2,?3,?4,?5,?6)", -1, &s, NULL);
    if (!s) return -1;
    if (pc_id) sqlite3_bind_text(s,1,pc_id,-1,SQLITE_TRANSIENT); else sqlite3_bind_null(s,1);
    if (exam_id) sqlite3_bind_text(s,2,exam_id,-1,SQLITE_TRANSIENT); else sqlite3_bind_null(s,2);
    sqlite3_bind_text(s,3,category,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,4,sel_severity_name(sev),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,5,message,-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(s,6,sel_now());
    int rc = sqlite3_step(s); sqlite3_finalize(s);
    if (rc != SQLITE_DONE) return -1;
    return sqlite3_changes(g_db) > 0 ? (int)sqlite3_last_insert_rowid(g_db) : 0;
}
int alerts_acknowledge(int alert_id, const char *by) {
    sqlite3_stmt *s = NULL;
    sqlite3_prepare_v2(g_db, "UPDATE alerts SET status='ACKED', ack_by=?2, ack_at=?3 WHERE id=?1 AND status='OPEN'", -1, &s, NULL);
    if (!s) return -1;
    sqlite3_bind_int(s,1,alert_id);
    sqlite3_bind_text(s,2,by?by:"admin",-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(s,3,sel_now());
    int rc = sqlite3_step(s); int ch = sqlite3_changes(g_db); sqlite3_finalize(s);
    return (rc == SQLITE_DONE && ch > 0) ? 0 : -1;
}
int alerts_resolve(int alert_id) {
    sqlite3_stmt *s = NULL;
    sqlite3_prepare_v2(g_db, "UPDATE alerts SET status='RESOLVED', resolved_at=?2 WHERE id=?1 AND status IN ('OPEN','ACKED')", -1, &s, NULL);
    if (!s) return -1;
    sqlite3_bind_int(s,1,alert_id); sqlite3_bind_int64(s,2,sel_now());
    int rc = sqlite3_step(s); int ch = sqlite3_changes(g_db); sqlite3_finalize(s);
    return (rc == SQLITE_DONE && ch > 0) ? 0 : -1;
}
int alerts_auto_resolve_pc(const char *pc_id, const char *category) {
    sqlite3_stmt *s = NULL;
    sqlite3_prepare_v2(g_db, "UPDATE alerts SET status='RESOLVED', resolved_at=?3 WHERE pc_id=?1 AND category=?2 AND status IN ('OPEN','ACKED')", -1, &s, NULL);
    if (!s) return -1;
    sqlite3_bind_text(s,1,pc_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,category,-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(s,3,sel_now());
    int rc = sqlite3_step(s); sqlite3_finalize(s);
    return rc == SQLITE_DONE ? 0 : -1;
}
void alerts_for_health(const char *pc_id, const HealthMetrics *m, PcStatus st) {
    char msg[256];
    if (st == ST_WARNING && m->cpu_pct >= 85.0) { snprintf(msg, sizeof(msg), "%s CPU at %.1f%%", pc_id, m->cpu_pct); alerts_raise(pc_id, NULL, "HIGH_CPU", SEV_WARNING, msg); }
    else alerts_auto_resolve_pc(pc_id, "HIGH_CPU");
    if (st == ST_WARNING && m->ram_pct >= 90.0) { snprintf(msg, sizeof(msg), "%s RAM at %.1f%%", pc_id, m->ram_pct); alerts_raise(pc_id, NULL, "HIGH_RAM", SEV_WARNING, msg); }
    else alerts_auto_resolve_pc(pc_id, "HIGH_RAM");
    if (!m->software_ok) { snprintf(msg, sizeof(msg), "%s missing required software", pc_id); alerts_raise(pc_id, NULL, "MISSING_SOFTWARE", SEV_CRITICAL, msg); }
    else alerts_auto_resolve_pc(pc_id, "MISSING_SOFTWARE");
    if (!m->net_ok) { snprintf(msg, sizeof(msg), "%s reports network failure", pc_id); alerts_raise(pc_id, NULL, "NETWORK", SEV_CRITICAL, msg); }
    else alerts_auto_resolve_pc(pc_id, "NETWORK");
}
