/* database.c - SQLite persistence. Prepared statements everywhere,
 * foreign keys ON, schema created automatically, existing DB never wiped. */
#include "database.h"
#include "sqlite3.h"

sqlite3 *g_db = NULL;

/* One process-wide lock: SQLite handle is shared by listener client threads,
 * the sweeper thread and the admin console thread. */
#ifdef _WIN32
  #include <windows.h>
  static CRITICAL_SECTION g_db_cs; static int g_db_cs_init = 0;
  void db_lock(void) { if (!g_db_cs_init) { InitializeCriticalSection(&g_db_cs); g_db_cs_init = 1; } EnterCriticalSection(&g_db_cs); }
  void db_unlock(void) { LeaveCriticalSection(&g_db_cs); }
#else
  #include <pthread.h>
  static pthread_mutex_t g_db_mu = PTHREAD_MUTEX_INITIALIZER;
  void db_lock(void) { pthread_mutex_lock(&g_db_mu); }
  void db_unlock(void) { pthread_mutex_unlock(&g_db_mu); }
#endif

static const char *SCHEMA =
"CREATE TABLE IF NOT EXISTS computers("
" pc_id TEXT PRIMARY KEY, hostname TEXT NOT NULL DEFAULT '',"
" registered_at INTEGER NOT NULL, last_seen INTEGER NOT NULL DEFAULT 0,"
" cpu_pct REAL NOT NULL DEFAULT 0, ram_pct REAL NOT NULL DEFAULT 0,"
" net_ok INTEGER NOT NULL DEFAULT 0, software_ok INTEGER NOT NULL DEFAULT 0,"
" status TEXT NOT NULL DEFAULT 'DISCONNECTED', maintenance INTEGER NOT NULL DEFAULT 0);"
"CREATE TABLE IF NOT EXISTS students("
" student_id TEXT PRIMARY KEY, name TEXT NOT NULL, created_at INTEGER NOT NULL);"
"CREATE TABLE IF NOT EXISTS exams("
" exam_id TEXT PRIMARY KEY, title TEXT NOT NULL,"
" state TEXT NOT NULL DEFAULT 'SCHEDULED' CHECK(state IN ('SCHEDULED','ACTIVE','ENDED')),"
" created_at INTEGER NOT NULL, started_at INTEGER, ended_at INTEGER);"
"CREATE TABLE IF NOT EXISTS assignments("
" id INTEGER PRIMARY KEY AUTOINCREMENT,"
" exam_id TEXT NOT NULL REFERENCES exams(exam_id),"
" student_id TEXT NOT NULL REFERENCES students(student_id),"
" pc_id TEXT NOT NULL REFERENCES computers(pc_id),"
" assigned_at INTEGER NOT NULL, released_at INTEGER);"
"CREATE UNIQUE INDEX IF NOT EXISTS ux_active_pc ON assignments(pc_id) WHERE released_at IS NULL;"
"CREATE UNIQUE INDEX IF NOT EXISTS ux_active_student ON assignments(student_id, exam_id) WHERE released_at IS NULL;"
"CREATE TABLE IF NOT EXISTS health_checks("
" id INTEGER PRIMARY KEY AUTOINCREMENT, pc_id TEXT NOT NULL REFERENCES computers(pc_id),"
" ts INTEGER NOT NULL, cpu_pct REAL NOT NULL, ram_pct REAL NOT NULL,"
" net_ok INTEGER NOT NULL, software_ok INTEGER NOT NULL, status TEXT NOT NULL);"
"CREATE INDEX IF NOT EXISTS ix_health_pc_ts ON health_checks(pc_id, ts);"
"CREATE TABLE IF NOT EXISTS alerts("
" id INTEGER PRIMARY KEY AUTOINCREMENT, pc_id TEXT, exam_id TEXT,"
" category TEXT NOT NULL, severity TEXT NOT NULL, message TEXT NOT NULL,"
" created_at INTEGER NOT NULL, status TEXT NOT NULL DEFAULT 'OPEN'"
"   CHECK(status IN ('OPEN','ACKED','RESOLVED')), ack_by TEXT, ack_at INTEGER, resolved_at INTEGER);"
"CREATE UNIQUE INDEX IF NOT EXISTS ux_open_alert ON alerts(pc_id, category) WHERE status IN ('OPEN','ACKED');"
"CREATE TABLE IF NOT EXISTS config(key TEXT PRIMARY KEY, value TEXT);"
"CREATE TABLE IF NOT EXISTS audit(id INTEGER PRIMARY KEY AUTOINCREMENT, ts INTEGER NOT NULL,"
" actor TEXT, action TEXT, detail TEXT);";

int db_exec_schema(void) {
    char *err = NULL;
    if (sqlite3_exec(g_db, SCHEMA, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "schema error: %s\n", err ? err : "?"); sqlite3_free(err); return -1;
    }
    return 0;
}

int db_open(const char *path) {
    if (sqlite3_open(path, &g_db) != SQLITE_OK) {
        fprintf(stderr, "db open failed: %s\n", g_db ? sqlite3_errmsg(g_db) : "no handle");
        return -1;
    }
    sqlite3_exec(g_db, "PRAGMA foreign_keys=ON;", NULL, NULL, NULL);
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_busy_timeout(g_db, 5000);
    return db_exec_schema();
}
void db_close(void) { if (g_db) { sqlite3_close(g_db); g_db = NULL; } }

static sqlite3_stmt *prep(const char *sql) {
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &st, NULL) != SQLITE_OK) {
        fprintf(stderr, "prepare failed: %s | %s\n", sqlite3_errmsg(g_db), sql);
        return NULL;
    }
    return st;
}

int db_upsert_computer(const HealthMetrics *m, PcStatus st) {
    sqlite3_stmt *s = prep("INSERT INTO computers(pc_id,hostname,registered_at,last_seen,cpu_pct,ram_pct,net_ok,software_ok,status)"
        " VALUES(?1,?2,?3,?3,?4,?5,?6,?7,?8)"
        " ON CONFLICT(pc_id) DO UPDATE SET hostname=?2,last_seen=?3,cpu_pct=?4,ram_pct=?5,net_ok=?6,software_ok=?7,status=?8");
    if (!s) return -1;
    sqlite3_bind_text(s,1,m->pc_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,m->hostname,-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(s,3,sel_now());
    sqlite3_bind_double(s,4,m->cpu_pct);
    sqlite3_bind_double(s,5,m->ram_pct);
    sqlite3_bind_int(s,6,m->net_ok);
    sqlite3_bind_int(s,7,m->software_ok);
    sqlite3_bind_text(s,8,sel_status_name(st),-1,SQLITE_TRANSIENT);
    int rc = sqlite3_step(s); sqlite3_finalize(s);
    return rc == SQLITE_DONE ? 0 : -1;
}
int db_insert_health(const HealthMetrics *m, PcStatus st) {
    sqlite3_stmt *s = prep("INSERT INTO health_checks(pc_id,ts,cpu_pct,ram_pct,net_ok,software_ok,status) VALUES(?1,?2,?3,?4,?5,?6,?7)");
    if (!s) return -1;
    sqlite3_bind_text(s,1,m->pc_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(s,2,m->timestamp ? m->timestamp : sel_now());
    sqlite3_bind_double(s,3,m->cpu_pct); sqlite3_bind_double(s,4,m->ram_pct);
    sqlite3_bind_int(s,5,m->net_ok); sqlite3_bind_int(s,6,m->software_ok);
    sqlite3_bind_text(s,7,sel_status_name(st),-1,SQLITE_TRANSIENT);
    int rc = sqlite3_step(s); sqlite3_finalize(s);
    return rc == SQLITE_DONE ? 0 : -1;
}
int db_set_computer_status(const char *pc_id, PcStatus st) {
    sqlite3_stmt *s = prep("UPDATE computers SET status=?2 WHERE pc_id=?1");
    if (!s) return -1;
    sqlite3_bind_text(s,1,pc_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,sel_status_name(st),-1,SQLITE_TRANSIENT);
    int rc = sqlite3_step(s); sqlite3_finalize(s);
    return rc == SQLITE_DONE ? 0 : -1;
}
int db_set_maintenance(const char *pc_id, int on) {
    sqlite3_stmt *s = prep("UPDATE computers SET maintenance=?2 WHERE pc_id=?1");
    if (!s) return -1;
    sqlite3_bind_text(s,1,pc_id,-1,SQLITE_TRANSIENT); sqlite3_bind_int(s,2,on?1:0);
    int rc = sqlite3_step(s); int ch = sqlite3_changes(g_db); sqlite3_finalize(s);
    return (rc == SQLITE_DONE && ch > 0) ? 0 : -1;
}
int db_get_computer_status(const char *pc_id, PcStatus *out) {
    sqlite3_stmt *s = prep("SELECT status FROM computers WHERE pc_id=?1");
    if (!s) return -1;
    sqlite3_bind_text(s,1,pc_id,-1,SQLITE_TRANSIENT);
    int rc = -1;
    if (sqlite3_step(s) == SQLITE_ROW) {
        const char *t = (const char *)sqlite3_column_text(s,0);
        if (t) { for (int i = 0; i <= ST_MAINTENANCE; i++) if (!strcmp(t, sel_status_name((PcStatus)i))) { *out = (PcStatus)i; rc = 0; } }
    }
    sqlite3_finalize(s); return rc;
}
long db_get_last_seen(const char *pc_id) {
    sqlite3_stmt *s = prep("SELECT last_seen FROM computers WHERE pc_id=?1");
    if (!s) return 0;
    sqlite3_bind_text(s,1,pc_id,-1,SQLITE_TRANSIENT);
    long v = 0; if (sqlite3_step(s) == SQLITE_ROW) v = (long)sqlite3_column_int64(s,0);
    sqlite3_finalize(s); return v;
}
int db_is_assigned(const char *pc_id) {
    sqlite3_stmt *s = prep("SELECT 1 FROM assignments WHERE pc_id=?1 AND released_at IS NULL LIMIT 1");
    if (!s) return 0;
    sqlite3_bind_text(s,1,pc_id,-1,SQLITE_TRANSIENT);
    int yes = sqlite3_step(s) == SQLITE_ROW; sqlite3_finalize(s); return yes;
}
int db_add_student(const char *student_id, const char *name) {
    sqlite3_stmt *s = prep("INSERT OR IGNORE INTO students(student_id,name,created_at) VALUES(?1,?2,?3)");
    if (!s) return -1;
    sqlite3_bind_text(s,1,student_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,name,-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(s,3,sel_now());
    int rc = sqlite3_step(s); sqlite3_finalize(s);
    return rc == SQLITE_DONE ? 0 : -1;
}
int db_student_exists(const char *student_id) {
    sqlite3_stmt *s = prep("SELECT 1 FROM students WHERE student_id=?1");
    if (!s) return 0;
    sqlite3_bind_text(s,1,student_id,-1,SQLITE_TRANSIENT);
    int yes = sqlite3_step(s) == SQLITE_ROW; sqlite3_finalize(s); return yes;
}
int db_create_exam(const char *exam_id, const char *title) {
    sqlite3_stmt *s = prep("INSERT OR IGNORE INTO exams(exam_id,title,created_at) VALUES(?1,?2,?3)");
    if (!s) return -1;
    sqlite3_bind_text(s,1,exam_id,-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,2,title,-1,SQLITE_TRANSIENT);
    sqlite3_bind_int64(s,3,sel_now());
    int rc = sqlite3_step(s); sqlite3_finalize(s);
    return rc == SQLITE_DONE ? 0 : -1;
}
int db_exam_state(const char *exam_id, char *buf, int n) {
    sqlite3_stmt *s = prep("SELECT state FROM exams WHERE exam_id=?1");
    if (!s) return -1;
    sqlite3_bind_text(s,1,exam_id,-1,SQLITE_TRANSIENT);
    int rc = -1;
    if (sqlite3_step(s) == SQLITE_ROW) { const char *t=(const char*)sqlite3_column_text(s,0); if (t){ sel_safe_copy(buf,(size_t)n,t); rc=0; } }
    sqlite3_finalize(s); return rc;
}
int db_set_exam_state(const char *exam_id, const char *state) {
    const char *sql = "UPDATE exams SET state=?2 WHERE exam_id=?1";
    if (!strcmp(state,"ACTIVE")) sql = "UPDATE exams SET state='ACTIVE', started_at=COALESCE(started_at,?3) WHERE exam_id=?1";
    if (!strcmp(state,"ENDED"))  sql = "UPDATE exams SET state='ENDED', ended_at=?3 WHERE exam_id=?1";
    sqlite3_stmt *s = prep(sql); if (!s) return -1;
    sqlite3_bind_text(s,1,exam_id,-1,SQLITE_TRANSIENT);
    if (strstr(sql,"?2")) sqlite3_bind_text(s,2,state,-1,SQLITE_TRANSIENT);
    if (strstr(sql,"?3")) sqlite3_bind_int64(s,3,sel_now());
    int rc = sqlite3_step(s); int ch = sqlite3_changes(g_db); sqlite3_finalize(s);
    return (rc == SQLITE_DONE && ch > 0) ? 0 : -1;
}
int db_count(const char *sql) {
    sqlite3_stmt *s = prep(sql); if (!s) return -1;
    int v = -1; if (sqlite3_step(s) == SQLITE_ROW) v = sqlite3_column_int(s,0);
    sqlite3_finalize(s); return v;
}
int db_audit(const char *actor, const char *action, const char *detail) {
    sqlite3_stmt *s = prep("INSERT INTO audit(ts,actor,action,detail) VALUES(?1,?2,?3,?4)");
    if (!s) return -1;
    sqlite3_bind_int64(s,1,sel_now());
    sqlite3_bind_text(s,2,actor?actor:"",-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,3,action? action:"",-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(s,4,detail?detail:"",-1,SQLITE_TRANSIENT);
    int rc = sqlite3_step(s); sqlite3_finalize(s);
    return rc == SQLITE_DONE ? 0 : -1;
}
