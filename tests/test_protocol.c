/* test_protocol.c - framing format, parsing, validation, malformed inputs */
#include "protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    char buf[SEL_MAX_PAYLOAD]; Message m;
    int n = proto_build(buf, sizeof buf, MSG_HEARTBEAT, "PC-01", 1700000000L, "token=T;cpu=12.5;ram=40.0;net=1;sw=1");
    assert(n > 0);
    assert(proto_parse(buf, n, &m)==0);
    assert(m.type==MSG_HEARTBEAT && !strcmp(m.pc_id,"PC-01") && m.timestamp==1700000000L);
    char v[32]; assert(proto_kv_get(&m,"cpu",v,sizeof v) && !strcmp(v,"12.5"));
    assert(proto_kv_get(&m,"missing",v,sizeof v)==NULL);
    printf("ok build/parse/kv\n");
    assert(proto_parse("2|HEARTBEAT|PC-01|1|", 22, &m)!=0); printf("ok wrong version rejected\n");
    assert(proto_parse("1|BOGUS|PC-01|1|", 16, &m)!=0); printf("ok unknown type rejected\n");
    assert(proto_parse("1|HEARTBEAT|bad id!|1|", 22, &m)!=0); printf("ok bad pc id rejected\n");
    assert(proto_parse("garbage", 7, &m)!=0); printf("ok garbage rejected\n");
    assert(proto_build(buf, sizeof buf, MSG_ACK, "bad id!", 0, "")<0); printf("ok build validates id\n");
    char big[SEL_MAX_PAYLOAD+64]; memset(big,'A',sizeof big);
    assert(proto_parse(big, SEL_MAX_PAYLOAD+10, &m)!=0); printf("ok oversized rejected\n");
    printf("test_protocol: ALL PASS\n"); return 0;
}
