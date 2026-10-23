/* test_allocation.c - the spec's allocation matrix:
 * all healthy / high CPU / high RAM / stale heartbeat / missing software /
 * already assigned / none eligible / duplicate student / exam not active /
 * unknown student / release + reassignment.
 * True concurrency is enforced by BEGIN IMMEDIATE + the partial unique index
 * ux_active_pc; the threaded case is covered in the integration run. */
#include "allocation.h"
#include "database.h"
#include "monitor.h"
#include "sqlite3.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
static HealthThresholds TH = {85.0, 90.0, 30, 30, 1};
static void add_pc(const char *id, double cpu, double ram, int net, int sw){
    HealthMetrics m; memset(&m,0,sizeof m);
    sel_safe_copy(m.pc_id,sizeof m.pc_id,id); sel_safe_copy(m.hostname,sizeof m.hostname,id);
    m.cpu_pct=cpu; m.ram_pct=ram; m.net_ok=net; m.software_ok=sw; m.timestamp=sel_now();
    db_upsert_computer(&m, health_evaluate(&m,&TH,0,0,0));
}
static void make_stale(const char *id){
    char sql[256]; snprintf(sql,sizeof sql,"UPDATE computers SET last_seen=%ld WHERE pc_id='%s'", sel_now()-3600, id);
    assert(sqlite3_exec(g_db, sql, NULL, NULL, NULL)==SQLITE_OK);
}
int main(void){
    unlink("/tmp/sel_alloc.db"); unlink("/tmp/sel_alloc.db-wal"); unlink("/tmp/sel_alloc.db-shm");
    assert(db_open("/tmp/sel_alloc.db")==0);
    db_add_student("S1","One"); db_add_student("S2","Two"); db_add_student("S3","Three");
    db_create_exam("EX","OS Exam");
    char pc[64], err[200];
    add_pc("PC-A",10,20,1,1);   /* best score 30 */
    add_pc("PC-B",30,30,1,1);   /* score 60 */
    add_pc("PC-C",95,30,1,1);   /* high CPU -> WARNING, excluded */
    add_pc("PC-D",20,97,1,1);   /* high RAM -> WARNING, excluded */
    add_pc("PC-E",20,30,1,0);   /* missing software -> NOT_READY, excluded */
    add_pc("PC-F",5,5,1,1);     /* best metrics... */
    make_stale("PC-F");           /* ...but stale heartbeat -> excluded by freshness check */
    assert(allocation_allocate("EX","S1",&TH,pc,sizeof pc,err,sizeof err)==ALLOC_ERR_EXAM_NOT_ACTIVE);
    printf("ok exam not active rejected: %s\n", err);
    assert(db_set_exam_state("EX","ACTIVE")==0);
    assert(allocation_allocate("EX","NOPE",&TH,pc,sizeof pc,err,sizeof err)==ALLOC_ERR_STUDENT_UNKNOWN);
    printf("ok unknown student rejected\n");
    assert(allocation_allocate("EX","S1",&TH,pc,sizeof pc,err,sizeof err)==ALLOC_OK && !strcmp(pc,"PC-A"));
    printf("ok S1 -> PC-A (lowest load wins; stale PC-F with better metrics correctly skipped)\n");
    assert(allocation_allocate("EX","S1",&TH,pc,sizeof pc,err,sizeof err)==ALLOC_ERR_STUDENT_ALREADY_ASSIGNED);
    printf("ok duplicate student allocation rejected\n");
    assert(allocation_allocate("EX","S2",&TH,pc,sizeof pc,err,sizeof err)==ALLOC_OK && !strcmp(pc,"PC-B"));
    printf("ok S2 -> PC-B (assigned PC-A skipped)\n");
    assert(allocation_allocate("EX","S3",&TH,pc,sizeof pc,err,sizeof err)==ALLOC_ERR_NO_PC_AVAILABLE);
    printf("ok high CPU / high RAM / missing sw / stale all excluded -> NO_PC_AVAILABLE: %s\n", err);
    assert(allocation_release("EX","S1")==0);
    assert(allocation_allocate("EX","S3",&TH,pc,sizeof pc,err,sizeof err)==ALLOC_OK && !strcmp(pc,"PC-A"));
    printf("ok release + reassignment works (S3 -> PC-A)\n");
    db_close();
    printf("test_allocation: ALL PASS\n"); return 0;
}
