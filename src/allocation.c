/* allocation.c - smart allocation.
 * Pseudocode and complexity: docs/architecture.md (O(P log P) candidate scan).
 * Atomicity: BEGIN IMMEDIATE takes the DB write lock before selection, so a
 * second concurrent request blocks (busy_timeout) and then re-reads state -
 * it can never observe the same PC as free. Partial unique indexes on
 * assignments are the hard guarantee even if this code path changes. */
#include "allocation.h"
#include "database.h"
#include "monitor.h"
#include "sqlite3.h"

typedef struct { char pc_id[SEL_MAX_ID]; double score; } Cand;

static int cmp_cand(const void *a, const void *b) {
    const Cand *x = (const Cand *)a, *y = (const Cand *)b;
    if (x->score < y->score) return -1;
    if (x->score > y->score) return 1;
    return strcmp(x->pc_id, y->pc_id); /* deterministic tie-break */
}

AllocResult allocation_allocate(const char *exam_id, const char *student_id,
                                const HealthThresholds *th,
                                char *out_pc_id, int out_pc_sz,
                                char *err, int err_sz) {
    char state[32];
    if (!db_student_exists(student_id)) { snprintf(err, (size_t)err_sz, "unknown student %s", student_id); return ALLOC_ERR_STUDENT_UNKNOWN; }
    if (db_exam_state(exam_id, state, sizeof(state)) != 0) { snprintf(err, (size_t)err_sz, "unknown exam %s", exam_id); return ALLOC_ERR_EXAM_UNKNOWN; }
    if (strcmp(state, "ACTIVE") != 0) { snprintf(err, (size_t)err_sz, "exam %s is %s, not ACTIVE", exam_id, state); return ALLOC_ERR_EXAM_NOT_ACTIVE; }

    if (sqlite3_exec(g_db, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) {
        snprintf(err, (size_t)err_sz, "db busy"); return ALLOC_ERR_DB;
    }
    AllocResult result = ALLOC_ERR_NO_PC_AVAILABLE;
    sqlite3_stmt *chk = NULL;
    sqlite3_prepare_v2(g_db, "SELECT 1 FROM assignments WHERE student_id=?1 AND exam_id=?2 AND released_at IS NULL", -1, &chk, NULL);
    sqlite3_bind_text(chk,1,student_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(chk,2,exam_id,-1,SQLITE_TRANSIENT);
    if (sqlite3_step(chk) == SQLITE_ROW) result = ALLOC_ERR_STUDENT_ALREADY_ASSIGNED;
    sqlite3_finalize(chk);
    if (result == ALLOC_ERR_STUDENT_ALREADY_ASSIGNED) {
        snprintf(err, (size_t)err_sz, "student %s already assigned in %s", student_id, exam_id);
        sqlite3_exec(g_db, "ROLLBACK", NULL, NULL, NULL); return result;
    }

    /* Candidate scan: stored status + freshness + live eligibility re-check */
    sqlite3_stmt *q = NULL;
    sqlite3_prepare_v2(g_db,
        "SELECT c.pc_id,c.cpu_pct,c.ram_pct,c.net_ok,c.software_ok,c.last_seen,c.maintenance "
        "FROM computers c WHERE c.maintenance=0 AND NOT EXISTS "
        "(SELECT 1 FROM assignments a WHERE a.pc_id=c.pc_id AND a.released_at IS NULL)", -1, &q, NULL);
    Cand cands[512]; int nc = 0;
    long now = sel_now();
    while (q && sqlite3_step(q) == SQLITE_ROW && nc < 512) {
        HealthMetrics m; memset(&m, 0, sizeof(m));
        sel_safe_copy(m.pc_id, sizeof(m.pc_id), (const char *)sqlite3_column_text(q,0));
        m.cpu_pct = sqlite3_column_double(q,1); m.ram_pct = sqlite3_column_double(q,2);
        m.net_ok = sqlite3_column_int(q,3); m.software_ok = sqlite3_column_int(q,4);
        long last = (long)sqlite3_column_int64(q,5);
        int maint = sqlite3_column_int(q,6);
        long age = now - last;
        if (age > th->freshness_s) continue;                    /* stale health record */
        PcStatus st = health_evaluate(&m, th, 0, maint, age);
        if (st != ST_READY) continue;  /* policy: only READY PCs are allocated (WARNING excluded, documented) */
        cands[nc].score = m.cpu_pct + m.ram_pct;
        sel_safe_copy(cands[nc].pc_id, sizeof(cands[nc].pc_id), m.pc_id);
        nc++;
    }
    sqlite3_finalize(q);
    if (nc == 0) {
        snprintf(err, (size_t)err_sz, "no eligible PC (ready, fresh, unassigned) available");
        sqlite3_exec(g_db, "ROLLBACK", NULL, NULL, NULL); return ALLOC_ERR_NO_PC_AVAILABLE;
    }
    qsort(cands, (size_t)nc, sizeof(Cand), cmp_cand);

    sqlite3_stmt *ins = NULL;
    sqlite3_prepare_v2(g_db, "INSERT INTO assignments(exam_id,student_id,pc_id,assigned_at) VALUES(?1,?2,?3,?4)", -1, &ins, NULL);
    sqlite3_bind_text(ins,1,exam_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(ins,2,student_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(ins,3,cands[0].pc_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(ins,4,now);
    int rc = sqlite3_step(ins); sqlite3_finalize(ins);
    if (rc != SQLITE_DONE) {
        snprintf(err, (size_t)err_sz, "insert failed (concurrent assignment?): %s", sqlite3_errmsg(g_db));
        sqlite3_exec(g_db, "ROLLBACK", NULL, NULL, NULL); return ALLOC_ERR_DB;
    }
    db_set_computer_status(cands[0].pc_id, ST_ASSIGNED);
    if (sqlite3_exec(g_db, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
        snprintf(err, (size_t)err_sz, "commit failed"); return ALLOC_ERR_DB;
    }
    sel_safe_copy(out_pc_id, (size_t)out_pc_sz, cands[0].pc_id);
    return ALLOC_OK;
}

int allocation_release(const char *exam_id, const char *student_id) {
    sqlite3_stmt *s = NULL;
    sqlite3_prepare_v2(g_db, "UPDATE assignments SET released_at=?3 WHERE student_id=?1 AND exam_id=?2 AND released_at IS NULL", -1, &s, NULL);
    if (!s) return -1;
    sqlite3_bind_text(s,1,student_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,exam_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(s,3,sel_now());
    int rc = sqlite3_step(s); int ch = sqlite3_changes(g_db); sqlite3_finalize(s);
    return (rc == SQLITE_DONE && ch > 0) ? 0 : -1;
}
int allocation_release_pc(const char *pc_id) {
    sqlite3_stmt *s = NULL;
    sqlite3_prepare_v2(g_db, "UPDATE assignments SET released_at=?2 WHERE pc_id=?1 AND released_at IS NULL", -1, &s, NULL);
    if (!s) return -1;
    sqlite3_bind_text(s,1,pc_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(s,2,sel_now());
    int rc = sqlite3_step(s); int ch = sqlite3_changes(g_db); sqlite3_finalize(s);
    if (rc == SQLITE_DONE && ch > 0) { db_set_computer_status(pc_id, ST_READY); return 0; }
    return -1;
}
