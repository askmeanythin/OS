/* protocol.c - length-prefixed, field-based protocol.
 * Never sends raw structs: explicit text serialization avoids padding /
 * endianness problems. Framing: 4-byte big-endian length + payload. */
#include "protocol.h"

#ifdef _WIN32
  #include <winsock2.h>
  #define SOCK_T SOCKET
#else
  #include <sys/socket.h>
  #include <unistd.h>
  #define SOCK_T int
#endif

const char *proto_type_name(MsgType t) {
    switch (t) {
        case MSG_REGISTER: return "REGISTER";
        case MSG_HEARTBEAT: return "HEARTBEAT";
        case MSG_HEALTH_UPDATE: return "HEALTH_UPDATE";
        case MSG_ACK: return "ACK";
        case MSG_ERROR: return "ERROR";
        case MSG_SHUTDOWN: return "SHUTDOWN";
        default: return "UNKNOWN";
    }
}
MsgType proto_type_from_name(const char *s) {
    if (!s) return MSG_UNKNOWN;
    if (!strcmp(s,"REGISTER")) return MSG_REGISTER;
    if (!strcmp(s,"HEARTBEAT")) return MSG_HEARTBEAT;
    if (!strcmp(s,"HEALTH_UPDATE")) return MSG_HEALTH_UPDATE;
    if (!strcmp(s,"ACK")) return MSG_ACK;
    if (!strcmp(s,"ERROR")) return MSG_ERROR;
    if (!strcmp(s,"SHUTDOWN")) return MSG_SHUTDOWN;
    return MSG_UNKNOWN;
}

static int valid_id(const char *s) { /* pc ids: letters, digits, dash, underscore, 1..31 */
    if (!s || !*s) return 0;
    size_t n = strlen(s); if (n >= SEL_MAX_ID) return 0;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_')) return 0;
    }
    return 1;
}

int proto_build(char *out, size_t outsz, MsgType t, const char *pc_id, long ts, const char *kv_body) {
    if (!out || !valid_id(pc_id)) return -1;
    int n = snprintf(out, outsz, "%d|%s|%s|%ld|%s", SEL_PROTO_VERSION,
                     proto_type_name(t), pc_id, ts, kv_body ? kv_body : "");
    if (n < 0 || (size_t)n >= outsz || n > SEL_MAX_PAYLOAD) return -1;
    return n;
}

int proto_parse(const char *payload, int len, Message *msg) {
    if (!payload || len <= 0 || len > SEL_MAX_PAYLOAD || !msg) return -1;
    char buf[SEL_MAX_PAYLOAD + 1];
    memcpy(buf, payload, (size_t)len); buf[len] = '\0';
    if (strchr(buf, '\n') || strchr(buf, '\r')) return -1; /* single-line frames only */
    char *save = NULL;
#ifdef _WIN32
    #define SEL_STRTOK strtok_s
#else
    #define SEL_STRTOK strtok_r
#endif
    char *v = SEL_STRTOK(buf, "|", &save);
    char *ty = SEL_STRTOK(NULL, "|", &save);
    char *id = SEL_STRTOK(NULL, "|", &save);
    char *ts = SEL_STRTOK(NULL, "|", &save);
    char *rest = SEL_STRTOK(NULL, "", &save); /* remainder may contain no '|' by design */
    if (!v || !ty || !id || !ts) return -1;
    msg->version = atoi(v);
    if (msg->version != SEL_PROTO_VERSION) return -1;
    msg->type = proto_type_from_name(ty);
    if (msg->type == MSG_UNKNOWN) return -1;
    if (!valid_id(id)) return -1;
    sel_safe_copy(msg->pc_id, sizeof(msg->pc_id), id);
    msg->timestamp = atol(ts);
    sel_safe_copy(msg->body, sizeof(msg->body), rest ? rest : "");
    return 0;
}

const char *proto_kv_get(const Message *msg, const char *key, char *buf, size_t n) {
    if (!msg || !key || !buf || n == 0) return NULL;
    char tmp[SEL_MAX_MSG]; sel_safe_copy(tmp, sizeof(tmp), msg->body);
    char kbuf[64]; snprintf(kbuf, sizeof(kbuf), "%s=", key);
    char *p = tmp;
    while (p && *p) {
        char *semi = strchr(p, ';');
        if (strncmp(p, kbuf, strlen(kbuf)) == 0) {
            const char *val = p + strlen(kbuf);
            size_t len = semi ? (size_t)(semi - val) : strlen(val);
            if (len >= n) len = n - 1;
            memcpy(buf, val, len); buf[len] = '\0';
            return buf;
        }
        p = semi ? semi + 1 : NULL;
    }
    return NULL;
}

/* ---- stream framing: loop until all bytes moved (partial read/write safe) ---- */
static int send_all(SOCK_T s, const char *buf, int len) {
    int sent = 0;
    while (sent < len) {
        int r = send(s, buf + sent, len - sent, 0);
        if (r <= 0) return -1;
        sent += r;
    }
    return 0;
}
static int recv_all(SOCK_T s, char *buf, int len) {
    int got = 0;
    while (got < len) {
        int r = recv(s, buf + got, len - got, 0);
        if (r <= 0) return -1;
        got += r;
    }
    return 0;
}
int proto_send(long long sock, const char *payload, int len) {
    if (len <= 0 || len > SEL_MAX_PAYLOAD) return -1;
    unsigned char hdr[4];
    hdr[0] = (unsigned char)((len >> 24) & 0xFF);
    hdr[1] = (unsigned char)((len >> 16) & 0xFF);
    hdr[2] = (unsigned char)((len >> 8) & 0xFF);
    hdr[3] = (unsigned char)(len & 0xFF);
    if (send_all((SOCK_T)sock, (const char *)hdr, 4) != 0) return -1;
    return send_all((SOCK_T)sock, payload, len);
}
int proto_recv(long long sock, char *buf, int bufsz) {
    unsigned char hdr[4];
    if (recv_all((SOCK_T)sock, (char *)hdr, 4) != 0) return -1;
    int len = ((int)hdr[0] << 24) | ((int)hdr[1] << 16) | ((int)hdr[2] << 8) | (int)hdr[3];
    if (len <= 0 || len > SEL_MAX_PAYLOAD || len >= bufsz) return -1; /* oversized/malformed */
    if (recv_all((SOCK_T)sock, buf, len) != 0) return -1;
    buf[len] = '\0';
    return len;
}
