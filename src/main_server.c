/* main_server.c - server entry point + terminal administrator console */
#include "common.h"
#include "config.h"
#include "database.h"
#include "server.h"
#include "monitor.h"
#include "allocation.h"
#include "alerts.h"
#include "reports.h"
#include "sqlite3.h"

static AppConfig g_cfg;
static HealthThresholds g_th;

static void read_line(const char *prompt, char *buf, int n) {
    printf("%s", prompt); fflush(stdout);
    if (!fgets(buf, n, stdin)) { buf[0] = '\0'; return; }
    buf[strcspn(buf, "\r\n")] = '\0';
}

static void list_pcs(void) {
    db_lock();
    sqlite3_stmt *q = NULL;
    sqlite3_prepare_v2(g_db, "SELECT pc_id,hostname,status,cpu_pct,ram_pct,net_ok,software_ok,last_seen,maintenance FROM computers ORDER BY pc_id", -1, &q, NULL);
    printf("\n%-12s %-18s %-13s %6s %6s %-4s %-4s %-6s %s\n", "PC", "HOSTNAME", "STATUS", "CPU%", "RAM%", "NET", "SW", "MAINT", "LAST SEEN");
    int any = 0; char tb[40];
    while (q && sqlite3_step(q) == SQLITE_ROW) {
        any = 1;
        sel_format_time((long)sqlite3_column_int64(q,7), tb, sizeof(tb));
        printf("%-12s %-18s %-13s %6.1f %6.1f %-4d %-4d %-6d %s\n",
            (const char *)sqlite3_column_text(q,0), (const char *)sqlite3_column_text(q,1),
            (const char *)sqlite3_column_text(q,2), sqlite3_column_double(q,3), sqlite3_column_double(q,4),
            sqlite3_column_int(q,5), sqlite3_column_int(q,6), sqlite3_column_int(q,8), tb);
    }
    sqlite3_finalize(q);
    if (!any) printf("(no computers registered yet - start a client or use simulation mode)\n");
    db_unlock();
}
static void list_students(void) {
    db_lock();
    sqlite3_stmt *q = NULL;
    sqlite3_prepare_v2(g_db, "SELECT student_id,name FROM students ORDER BY student_id", -1, &q, NULL);
    printf("\nStudents:\n"); int any = 0;
    while (q && sqlite3_step(q) == SQLITE_ROW) { any = 1; printf("  %s - %s\n", (const char *)sqlite3_column_text(q,0), (const char *)sqlite3_column_text(q,1)); }
    sqlite3_finalize(q); if (!any) printf("  (none)\n");
    db_unlock();
}
static void list_assignments(void) {
    db_lock();
    sqlite3_stmt *q = NULL;
    sqlite3_prepare_v2(g_db, "SELECT id,exam_id,student_id,pc_id,assigned_at,released_at FROM assignments ORDER BY id DESC LIMIT 50", -1, &q, NULL);
    printf("\nAssignments (latest 50):\n"); int any = 0; char tb[40];
    while (q && sqlite3_step(q) == SQLITE_ROW) {
        any = 1; sel_format_time((long)sqlite3_column_int64(q,4), tb, sizeof(tb));
        printf("  #%d exam=%s student=%s pc=%s at %s %s\n", sqlite3_column_int(q,0),
            (const char *)sqlite3_column_text(q,1), (const char *)sqlite3_column_text(q,2),
            (const char *)sqlite3_column_text(q,3), tb,
            sqlite3_column_type(q,5) == SQLITE_NULL ? "(active)" : "(released)");
    }
    sqlite3_finalize(q); if (!any) printf("  (none)\n");
    db_unlock();
}
static void list_alerts(void) {
    db_lock();
    sqlite3_stmt *q = NULL;
    sqlite3_prepare_v2(g_db, "SELECT id,pc_id,category,severity,message,status,created_at FROM alerts ORDER BY id DESC LIMIT 50", -1, &q, NULL);
    printf("\nAlerts (latest 50):\n"); int any = 0; char tb[40];
    while (q && sqlite3_step(q) == SQLITE_ROW) {
        any = 1; sel_format_time((long)sqlite3_column_int64(q,6), tb, sizeof(tb));
        printf("  #%d [%s] %s %s: %s (%s, %s)\n", sqlite3_column_int(q,0),
            (const char *)sqlite3_column_text(q,5), (const char *)sqlite3_column_text(q,1),
            (const char *)sqlite3_column_text(q,2), (const char *)sqlite3_column_text(q,4),
            (const char *)sqlite3_column_text(q,3), tb);
    }
    sqlite3_finalize(q); if (!any) printf("  (none)\n");
    db_unlock();
}
static void resweep(void) { /* recompute cached statuses from last metrics + freshness */
    db_lock();
    sqlite3_stmt *q = NULL;
    sqlite3_prepare_v2(g_db, "SELECT pc_id,cpu_pct,ram_pct,net_ok,software_ok,last_seen,maintenance FROM computers", -1, &q, NULL);
    typedef struct { char id[SEL_MAX_ID]; HealthMetrics m; long last; int maint; } Row;
    Row rows[256]; int n = 0;
    while (q && sqlite3_step(q) == SQLITE_ROW && n < 256) {
        memset(&rows[n].m, 0, sizeof(HealthMetrics));
        sel_safe_copy(rows[n].id, SEL_MAX_ID, (const char *)sqlite3_column_text(q,0));
        rows[n].m.cpu_pct = sqlite3_column_double(q,1); rows[n].m.ram_pct = sqlite3_column_double(q,2);
        rows[n].m.net_ok = sqlite3_column_int(q,3); rows[n].m.software_ok = sqlite3_column_int(q,4);
        rows[n].last = (long)sqlite3_column_int64(q,5); rows[n].maint = sqlite3_column_int(q,6); n++;
    }
    sqlite3_finalize(q);
    for (int i = 0; i < n; i++) {
        int assigned = db_is_assigned(rows[i].id);
        PcStatus st = health_evaluate(&rows[i].m, &g_th, assigned, rows[i].maint, sel_now() - rows[i].last);
        db_set_computer_status(rows[i].id, st);
    }
    db_unlock();
    printf("Health sweep done for %d PCs.\n", n);
}
static void seed_demo(void) {
    db_lock();
    const char *students[][2] = {
        {"STU-001","Aarav Sharma"},{"STU-002","Diya Patel"},{"STU-003","Kabir Singh"},
        {"STU-004","Meera Nair"},{"STU-005","Arjun Reddy"},{"STU-006","Sara Khan"}};
    for (int i = 0; i < 6; i++) db_add_student(students[i][0], students[i][1]);
    db_create_exam("OS-EXAM-1", "Operating Systems Lab Exam");
    db_unlock();
    printf("Demo data: 6 sample students + exam OS-EXAM-1 (SCHEDULED). Start it from the menu, then allocate.\n");
    printf("Note: sample students are demonstration data only, not real student records.\n");
}

int main(int argc, char **argv) {
    config_defaults(&g_cfg);
    const char *cfg_path = "config/server.conf";
    int simulate = 0, do_seed = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--config") && i + 1 < argc) cfg_path = argv[++i];
        else if (!strcmp(argv[i], "--simulate") && i + 1 < argc) simulate = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed-demo")) do_seed = 1;
        else if (!strcmp(argv[i], "--port") && i + 1 < argc) { config_load(&g_cfg, cfg_path); g_cfg.port = atoi(argv[++i]); cfg_path = NULL; }
    }
    if (cfg_path) { if (config_load(&g_cfg, cfg_path) != 0) printf("Note: %s not found, using defaults (see config/server.example.conf).\n", cfg_path); }
    g_th.cpu_warn_pct = g_cfg.cpu_warn_pct; g_th.ram_warn_pct = g_cfg.ram_warn_pct;
    g_th.heartbeat_timeout_s = g_cfg.heartbeat_timeout_s; g_th.freshness_s = g_cfg.heartbeat_timeout_s;
    g_th.require_software = 1;

    if (db_open(g_cfg.db_path) != 0) { fprintf(stderr, "Cannot open database %s (does data/ exist?)\n", g_cfg.db_path); return 1; }
    if (do_seed) seed_demo();
    if (server_start(&g_cfg) != 0) { db_close(); return 1; }
    if (simulate > 0) server_start_simulation(simulate);

    printf("\n=== Smart Exam Lab - Administrator Console ===\n");
    char line[128];
    for (;;) {
        printf("\n 1 PCs | 2 Add student | 3 Students | 4 Create exam | 5 Start exam | 6 End exam\n"
               " 7 Allocate | 8 Release | 9 Assignments | 10 Alerts | 11 Ack alert | 12 Resolve alert\n"
               " 13 Health sweep | 14 Maintenance on/off | 15 Report+CSV | 16 Seed demo data | 0 Exit\n");
        read_line("Choice: ", line, sizeof(line));
        int c = atoi(line);
        char a[96], b[96];
        switch (c) {
            case 0: goto done;
            case 1: list_pcs(); break;
            case 2: read_line("Student ID: ", a, sizeof(a)); read_line("Name: ", b, sizeof(b));
                    db_lock(); printf(db_add_student(a, b) == 0 ? "Student saved.\n" : "Failed.\n"); db_unlock(); break;
            case 3: list_students(); break;
            case 4: read_line("Exam ID: ", a, sizeof(a)); read_line("Title: ", b, sizeof(b));
                    db_lock(); printf(db_create_exam(a, b) == 0 ? "Exam created (SCHEDULED).\n" : "Failed.\n"); db_unlock(); break;
            case 5: read_line("Exam ID: ", a, sizeof(a));
                    db_lock(); printf(db_set_exam_state(a, "ACTIVE") == 0 ? "Exam ACTIVE.\n" : "Failed (unknown exam?).\n"); db_unlock(); break;
            case 6: read_line("Exam ID: ", a, sizeof(a));
                    db_lock(); printf(db_set_exam_state(a, "ENDED") == 0 ? "Exam ENDED.\n" : "Failed.\n"); db_unlock(); break;
            case 7: { read_line("Exam ID: ", a, sizeof(a)); read_line("Student ID: ", b, sizeof(b));
                    char pc[SEL_MAX_ID] = "", err[160] = "";
                    db_lock(); AllocResult r = allocation_allocate(a, b, &g_th, pc, sizeof(pc), err, sizeof(err)); db_unlock();
                    if (r == ALLOC_OK) printf("Allocated %s -> %s\n", b, pc); else printf("Allocation failed: %s\n", err);
                    break; }
            case 8: read_line("Exam ID: ", a, sizeof(a)); read_line("Student ID: ", b, sizeof(b));
                    db_lock(); printf(allocation_release(a, b) == 0 ? "Released.\n" : "No active assignment found.\n"); db_unlock(); break;
            case 9: list_assignments(); break;
            case 10: list_alerts(); break;
            case 11: read_line("Alert ID: ", a, sizeof(a));
                     db_lock(); printf(alerts_acknowledge(atoi(a), "admin") == 0 ? "Acknowledged.\n" : "Failed (not OPEN?).\n"); db_unlock(); break;
            case 12: read_line("Alert ID: ", a, sizeof(a));
                     db_lock(); printf(alerts_resolve(atoi(a)) == 0 ? "Resolved.\n" : "Failed.\n"); db_unlock(); break;
            case 13: resweep(); break;
            case 14: read_line("PC ID: ", a, sizeof(a)); read_line("Maintenance 1=on 0=off: ", b, sizeof(b));
                     db_lock(); printf(db_set_maintenance(a, atoi(b)) == 0 ? "Updated.\n" : "Failed (unknown PC?).\n"); db_unlock(); resweep(); break;
            case 15: { read_line("Exam ID for report: ", a, sizeof(a));
                     char rp[300], c1[300], c2[300], c3[300];
                     snprintf(rp, sizeof(rp), "reports/report_%s.txt", a);
                     snprintf(c1, sizeof(c1), "reports/computers.csv");
                     snprintf(c2, sizeof(c2), "reports/assignments_%s.csv", a);
                     snprintf(c3, sizeof(c3), "reports/alerts.csv");
                     db_lock();
                     int ok1 = reports_write_exam_report(a, rp);
                     int ok2 = reports_export_computers_csv(c1);
                     int ok3 = reports_export_assignments_csv(a, c2);
                     int ok4 = reports_export_alerts_csv(c3);
                     db_unlock();
                     printf("Report: %s | computers.csv: %s | assignments.csv: %s | alerts.csv: %s\n",
                            ok1==0?rp:"FAILED", ok2==0?"ok":"FAILED", ok3==0?"ok":"FAILED", ok4==0?"ok":"FAILED");
                     break; }
            case 16: seed_demo(); break;
            default: printf("Unknown choice.\n"); break;
        }
    }
done:
    printf("Shutting down server...\n");
    server_stop();
    db_close();
    return 0;
}
