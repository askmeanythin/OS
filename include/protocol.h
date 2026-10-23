/* protocol.h - versioned, length-prefixed message protocol.
 *
 * Wire format (see docs/protocol.md):
 *   [4-byte big-endian payload length][payload]
 * Payload is ASCII, field based (never a raw C struct):
 *   1|<TYPE>|<pc_id>|<epoch>|<k=v;k=v;...>
 * Types: REGISTER, HEARTBEAT, HEALTH_UPDATE, ACK, ERROR, SHUTDOWN
 */
#ifndef SEL_PROTOCOL_H
#define SEL_PROTOCOL_H

#include "common.h"

#define SEL_PROTO_VERSION 1
#define SEL_MAX_PAYLOAD   1024

typedef enum {
    MSG_UNKNOWN = 0, MSG_REGISTER, MSG_HEARTBEAT, MSG_HEALTH_UPDATE,
    MSG_ACK, MSG_ERROR, MSG_SHUTDOWN
} MsgType;

typedef struct {
    int     version;
    MsgType type;
    char    pc_id[SEL_MAX_ID];
    long    timestamp;
    char    body[SEL_MAX_MSG]; /* raw k=v section */
} Message;

const char *proto_type_name(MsgType t);
MsgType     proto_type_from_name(const char *s);
int  proto_build(char *out, size_t outsz, MsgType t, const char *pc_id,
                 long ts, const char *kv_body);           /* returns payload len or -1 */
int  proto_parse(const char *payload, int len, Message *msg); /* 0 ok, -1 malformed */
const char *proto_kv_get(const Message *msg, const char *key, char *buf, size_t n);

/* Framing over a connected stream socket. Handles partial reads/writes.
 * sock is SOCKET on Windows, int on POSIX - passed as long long via macro layer. */
int  proto_send(long long sock, const char *payload, int len);  /* 0 ok */
int  proto_recv(long long sock, char *buf, int bufsz);          /* payload len or -1 */

#endif
