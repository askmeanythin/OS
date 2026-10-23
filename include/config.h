/* config.h - key=value configuration file handling */
#ifndef SEL_CONFIG_H
#define SEL_CONFIG_H

typedef struct {
    char bind_addr[64];      /* server: e.g. 0.0.0.0 (LAN) or 127.0.0.1 */
    int  port;               /* default 5050 */
    char db_path[256];       /* default data/examlab.db */
    char lab_token[64];      /* shared provisioning token (see docs/setup_guide.md) */
    char pc_id[32];
    char server_host[64];
    char required_software[256]; /* comma separated executable names */
    int  heartbeat_interval_s;   /* default 5 */
    double cpu_warn_pct;         /* default 85 */
    double ram_warn_pct;         /* default 90 */
    int  heartbeat_timeout_s;    /* default 30 */
} AppConfig;

void config_defaults(AppConfig *cfg);
int  config_load(AppConfig *cfg, const char *path); /* 0 ok, -1 file missing (defaults kept) */
const char *config_get_raw(const char *path, const char *key, char *buf, int bufsz);

#endif
