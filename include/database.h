/* database.h - SQLite persistence (prepared statements only) */
#ifndef SEL_DATABASE_H
#define SEL_DATABASE_H

#include "common.h"

typedef struct sqlite3 sqlite3;
extern sqlite3 *g_db;

int  db_open(const char *path);   /* creates schema if missing; never deletes existing DB */
void db_close(void);
void db_lock(void);               /* serialize all DB access across server threads */
void db_unlock(void);
int  db_exec_schema(void);

/* computers */
int  db_upsert_computer(const HealthMetrics *m, PcStatus st);
int  db_set_computer_status(const char *pc_id, PcStatus st);
int  db_set_maintenance(const char *pc_id, int on);
int  db_get_computer_status(const char *pc_id, PcStatus *out); /* -1 if unknown */
long db_get_last_seen(const char *pc_id);                      /* 0 if unknown */
int  db_is_assigned(const char *pc_id);
int  db_insert_health(const HealthMetrics *m, PcStatus st);

/* students & exams */
int  db_add_student(const char *student_id, const char *name);
int  db_student_exists(const char *student_id);
int  db_create_exam(const char *exam_id, const char *title);
int  db_exam_state(const char *exam_id, char *buf, int n); /* SCHEDULED/ACTIVE/ENDED, -1 unknown */
int  db_set_exam_state(const char *exam_id, const char *state);

/* generic helpers used by reports/admin */
int  db_count(const char *sql);
int  db_audit(const char *actor, const char *action, const char *detail);

#endif
