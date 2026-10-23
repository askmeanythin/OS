/* test_health.c - health evaluation rules incl. all allocation-relevant cases */
#include "monitor.h"
#include <assert.h>
#include <stdio.h>
static HealthThresholds TH = {85.0, 90.0, 30, 30, 1};
static HealthMetrics base(void){ HealthMetrics m; memset(&m,0,sizeof m); sel_safe_copy(m.pc_id,sizeof m.pc_id,"PC-01"); m.cpu_pct=20; m.ram_pct=40; m.net_ok=1; m.software_ok=1; return m; }
int main(void){
    HealthMetrics m = base();
    assert(health_evaluate(&m,&TH,0,0,0)==ST_READY); printf("ok healthy -> READY\n");
    m = base(); m.cpu_pct=95; assert(health_evaluate(&m,&TH,0,0,0)==ST_WARNING); printf("ok high CPU -> WARNING\n");
    m = base(); m.ram_pct=97; assert(health_evaluate(&m,&TH,0,0,0)==ST_WARNING); printf("ok high RAM -> WARNING\n");
    m = base(); assert(health_evaluate(&m,&TH,0,0,31)==ST_DISCONNECTED); printf("ok stale heartbeat -> DISCONNECTED\n");
    m = base(); m.software_ok=0; assert(health_evaluate(&m,&TH,0,0,0)==ST_NOT_READY); printf("ok missing software -> NOT_READY\n");
    m = base(); m.net_ok=0; assert(health_evaluate(&m,&TH,0,0,0)==ST_NOT_READY); printf("ok net down -> NOT_READY\n");
    m = base(); assert(health_evaluate(&m,&TH,1,0,0)==ST_ASSIGNED); printf("ok assigned healthy -> ASSIGNED\n");
    m = base(); assert(health_evaluate(&m,&TH,0,1,0)==ST_MAINTENANCE); printf("ok maintenance flag -> MAINTENANCE\n");
    /* precedence: stale beats everything */
    m = base(); m.software_ok=0; assert(health_evaluate(&m,&TH,1,1,99)==ST_DISCONNECTED); printf("ok precedence stale > maintenance > not_ready > assigned\n");
    printf("test_health: ALL PASS\n"); return 0;
}
