/* test_database.c - schema creation, CRUD, persistence across reopen */
#include "database.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
int main(void){
    unlink("/tmp/sel_test.db"); unlink("/tmp/sel_test.db-wal"); unlink("/tmp/sel_test.db-shm");
    assert(db_open("/tmp/sel_test.db")==0);
    HealthMetrics m; memset(&m,0,sizeof m);
    sel_safe_copy(m.pc_id,sizeof m.pc_id,"PC-01"); sel_safe_copy(m.hostname,sizeof m.hostname,"LAB1-PC01");
    m.cpu_pct=22; m.ram_pct=41; m.net_ok=1; m.software_ok=1; m.timestamp=sel_now();
    assert(db_upsert_computer(&m, ST_READY)==0);
    PcStatus st; assert(db_get_computer_status("PC-01",&st)==0 && st==ST_READY);
    assert(db_get_last_seen("PC-01")>0);
    assert(db_add_student("STU-1","Test Student")==0 && db_student_exists("STU-1"));
    assert(db_create_exam("EX-1","OS Test")==0);
    char state[32]; assert(db_exam_state("EX-1",state,sizeof state)==0 && !strcmp(state,"SCHEDULED"));
    assert(db_insert_health(&m, ST_READY)==0);
    db_close();
    /* persistence: reopen same file, data must survive */
    assert(db_open("/tmp/sel_test.db")==0);
    assert(db_student_exists("STU-1"));
    assert(db_get_computer_status("PC-01",&st)==0);
    db_close();
    printf("test_database: ALL PASS (incl. persistence across reopen)\n"); return 0;
}
