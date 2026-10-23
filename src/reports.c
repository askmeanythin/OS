/* reports.c - reports + CSV exports, always from stored data */
#include "reports.h"
#include "database.h"
#include "sqlite3.h"

void csv_escape(FILE *f, const char *s) {
    if (!s) s = "";
    int need = strchr(s, ',') || strchr(s, '"') || strchr(s, '\n');
    if (!need) { fputs(s, f); return; }
    fputc('"', f);
    for (const char *p = s; *p; p++) { if (*p == '"') fputc('"', f); fputc(*p, f); }
    fputc('"', f);
}
static void field_sep(FILE *f, int first) { if (!first) fputc(',', f); }

int reports_export_computers_csv(const char *out_path) {
    FILE *f = fopen(out_path, "w"); if (!f) return -1;
    fputs("pc_id,hostname,status,cpu_pct,ram_pct,net_ok,software_ok,last_seen\n", f);
    sqlite3_stmt *s = NULL;
    sqlite3_prepare_v2(g_db, "SELECT pc_id,hostname,status,cpu_pct,ram_pct,net_ok,software_ok,last_seen FROM computers ORDER BY pc_id", -1, &s, NULL);
    char tb[40];
    while (s && sqlite3_step(s) == SQLITE_ROW) {
        csv_escape(f, (const char *)sqlite3_column_text(s,0)); field_sep(f,0);
        csv_escape(f, (const char *)sqlite3_column_text(s,1)); field_sep(f,0);
        csv_escape(f, (const char *)sqlite3_column_text(s,2)); field_sep(f,0);
        fprintf(f, "%.1f,%.1f,%d,%d,", sqlite3_column_double(s,3), sqlite3_column_double(s,4),
                sqlite3_column_int(s,5), sqlite3_column_int(s,6));
        sel_format_time((long)sqlite3_column_int64(s,7), tb, sizeof(tb)); csv_escape(f, tb);
        fputc('\n', f);
    }
    sqlite3_finalize(s); fclose(f); return 0;
}
int reports_export_assignments_csv(const char *exam_id, const char *out_path) {
    FILE *f = fopen(out_path, "w"); if (!f) return -1;
    fputs("assignment_id,exam_id,student_id,pc_id,assigned_at,released_at\n", f);
    sqlite3_stmt *s = NULL;
    sqlite3_prepare_v2(g_db, "SELECT id,exam_id,student_id,pc_id,assigned_at,released_at FROM assignments WHERE exam_id=?1 ORDER BY id", -1, &s, NULL);
    sqlite3_bind_text(s,1,exam_id,-1,SQLITE_TRANSIENT);
    char tb[40];
    while (s && sqlite3_step(s) == SQLITE_ROW) {
        fprintf(f, "%d,", sqlite3_column_int(s,0));
        csv_escape(f, (const char *)sqlite3_column_text(s,1)); field_sep(f,0);
        csv_escape(f, (const char *)sqlite3_column_text(s,2)); field_sep(f,0);
        csv_escape(f, (const char *)sqlite3_column_text(s,3)); field_sep(f,0);
        sel_format_time((long)sqlite3_column_int64(s,4), tb, sizeof(tb)); csv_escape(f, tb); field_sep(f,0);
        if (sqlite3_column_type(s,5) == SQLITE_NULL) fputs("", f);
        else { sel_format_time((long)sqlite3_column_int64(s,5), tb, sizeof(tb)); csv_escape(f, tb); }
        fputc('\n', f);
    }
    sqlite3_finalize(s); fclose(f); return 0;
}
int reports_export_alerts_csv(const char *out_path) {
    FILE *f = fopen(out_path, "w"); if (!f) return -1;
    fputs("alert_id,pc_id,exam_id,category,severity,message,status,created_at,resolved_at\n", f);
    sqlite3_stmt *s = NULL;
    sqlite3_prepare_v2(g_db, "SELECT id,pc_id,exam_id,category,severity,message,status,created_at,resolved_at FROM alerts ORDER BY id", -1, &s, NULL);
    char tb[40];
    while (s && sqlite3_step(s) == SQLITE_ROW) {
        fprintf(f, "%d,", sqlite3_column_int(s,0));
        csv_escape(f, (const char *)sqlite3_column_text(s,1)); field_sep(f,0);
        csv_escape(f, (const char *)sqlite3_column_text(s,2)); field_sep(f,0);
        csv_escape(f, (const char *)sqlite3_column_text(s,3)); field_sep(f,0);
        csv_escape(f, (const char *)sqlite3_column_text(s,4)); field_sep(f,0);
        csv_escape(f, (const char *)sqlite3_column_text(s,5)); field_sep(f,0);
        csv_escape(f, (const char *)sqlite3_column_text(s,6)); field_sep(f,0);
        sel_format_time((long)sqlite3_column_int64(s,7), tb, sizeof(tb)); csv_escape(f, tb); field_sep(f,0);
        if (sqlite3_column_type(s,8) == SQLITE_NULL) fputs("", f);
        else { sel_format_time((long)sqlite3_column_int64(s,8), tb, sizeof(tb)); csv_escape(f, tb); }
        fputc('\n', f);
    }
    sqlite3_finalize(s); fclose(f); return 0;
}

static int count_q(const char *sql, const char *bind) {
    sqlite3_stmt *s = NULL; sqlite3_prepare_v2(g_db, sql, -1, &s, NULL);
    if (!s) return -1;
    if (bind) sqlite3_bind_text(s,1,bind,-1,SQLITE_TRANSIENT);
    int v = -1; if (sqlite3_step(s) == SQLITE_ROW) v = sqlite3_column_int(s,0);
    sqlite3_finalize(s); return v;
}

int reports_write_exam_report(const char *exam_id, const char *out_path) {
    char state[32], title[128] = "";
    if (db_exam_state(exam_id, state, sizeof(state)) != 0) return -1;
    sqlite3_stmt *s = NULL;
    sqlite3_prepare_v2(g_db, "SELECT title FROM exams WHERE exam_id=?1", -1, &s, NULL);
    sqlite3_bind_text(s,1,exam_id,-1,SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) sel_safe_copy(title, sizeof(title), (const char *)sqlite3_column_text(s,0));
    sqlite3_finalize(s);

    FILE *f = fopen(out_path, "w"); if (!f) return -1;
    char tb[40]; sel_format_time(sel_now(), tb, sizeof(tb));
    fprintf(f, "Smart Exam Lab Management System - Exam Report\n");
    fprintf(f, "Generated: %s\n\n", tb);
    fprintf(f, "Exam: %s (%s)\nState: %s\n\n", title, exam_id, state);
    fprintf(f, "Registered students (all): %d\n", db_count("SELECT COUNT(*) FROM students"));
    fprintf(f, "Total registered PCs: %d\n", db_count("SELECT COUNT(*) FROM computers"));
    fprintf(f, "PCs READY: %d | WARNING: %d | NOT_READY: %d | DISCONNECTED: %d | ASSIGNED: %d | MAINTENANCE: %d\n",
        db_count("SELECT COUNT(*) FROM computers WHERE status='READY'"),
        db_count("SELECT COUNT(*) FROM computers WHERE status='WARNING'"),
        db_count("SELECT COUNT(*) FROM computers WHERE status='NOT_READY'"),
        db_count("SELECT COUNT(*) FROM computers WHERE status='DISCONNECTED'"),
        db_count("SELECT COUNT(*) FROM computers WHERE status='ASSIGNED'"),
        db_count("SELECT COUNT(*) FROM computers WHERE status='MAINTENANCE'"));
    fprintf(f, "Allocations in this exam: %d (active: %d)\n",
        count_q("SELECT COUNT(*) FROM assignments WHERE exam_id=?1", exam_id),
        count_q("SELECT COUNT(*) FROM assignments WHERE exam_id=?1 AND released_at IS NULL", exam_id));
    fprintf(f, "Disconnection incidents (health records marked DISCONNECTED): %d\n",
        db_count("SELECT COUNT(*) FROM health_checks WHERE status='DISCONNECTED'"));
    fprintf(f, "Health warning records: %d\n", db_count("SELECT COUNT(*) FROM health_checks WHERE status='WARNING'"));
    fprintf(f, "Unresolved alerts: %d\n\n", db_count("SELECT COUNT(*) FROM alerts WHERE status IN ('OPEN','ACKED')"));

    fprintf(f, "-- Assignment history --\n");
    s = NULL;
    sqlite3_prepare_v2(g_db, "SELECT student_id,pc_id,assigned_at,released_at FROM assignments WHERE exam_id=?1 ORDER BY id", -1, &s, NULL);
    sqlite3_bind_text(s,1,exam_id,-1,SQLITE_TRANSIENT);
    while (s && sqlite3_step(s) == SQLITE_ROW) {
        char a[40], r[40];
        sel_format_time((long)sqlite3_column_int64(s,2), a, sizeof(a));
        if (sqlite3_column_type(s,3) == SQLITE_NULL) sel_safe_copy(r, sizeof(r), "-");
        else sel_format_time((long)sqlite3_column_int64(s,3), r, sizeof(r));
        fprintf(f, "  %s -> %s  assigned %s  released %s\n",
                (const char *)sqlite3_column_text(s,0), (const char *)sqlite3_column_text(s,1), a, r);
    }
    sqlite3_finalize(s);

    fprintf(f, "\n-- Unassigned students (no assignment ever in this exam) --\n");
    s = NULL;
    sqlite3_prepare_v2(g_db, "SELECT student_id FROM students WHERE student_id NOT IN (SELECT student_id FROM assignments WHERE exam_id=?1) ORDER BY student_id", -1, &s, NULL);
    sqlite3_bind_text(s,1,exam_id,-1,SQLITE_TRANSIENT);
    int any = 0;
    while (s && sqlite3_step(s) == SQLITE_ROW) { fprintf(f, "  %s\n", (const char *)sqlite3_column_text(s,0)); any = 1; }
    sqlite3_finalize(s);
    if (!any) fprintf(f, "  (none)\n");
    fclose(f); return 0;
}
