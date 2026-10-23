#include "config.h"
#include "common.h"

void config_defaults(AppConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    sel_safe_copy(cfg->bind_addr, sizeof(cfg->bind_addr), "0.0.0.0");
    cfg->port = 5050;
    sel_safe_copy(cfg->db_path, sizeof(cfg->db_path), "data/examlab.db");
    sel_safe_copy(cfg->lab_token, sizeof(cfg->lab_token), "CHANGE-ME-LAB-TOKEN");
    sel_safe_copy(cfg->pc_id, sizeof(cfg->pc_id), "PC-01");
    sel_safe_copy(cfg->server_host, sizeof(cfg->server_host), "127.0.0.1");
    sel_safe_copy(cfg->required_software, sizeof(cfg->required_software), "");
    cfg->heartbeat_interval_s = 5;
    cfg->cpu_warn_pct = 85.0;
    cfg->ram_warn_pct = 90.0;
    cfg->heartbeat_timeout_s = 30;
}

static void trim(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\r'||s[n-1]=='\n')) s[--n]='\0';
    memmove(s, s + strspn(s, " \t"), strlen(s) - strspn(s, " \t") + 1);
}

int config_load(AppConfig *cfg, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (!line[0] || line[0]=='#' || line[0]==';') continue;
        char *eq = strchr(line, '='); if (!eq) continue;
        *eq = '\0'; char *k = line, *v = eq + 1; trim(k); trim(v);
        if (!strcmp(k,"bind_addr")) sel_safe_copy(cfg->bind_addr, sizeof(cfg->bind_addr), v);
        else if (!strcmp(k,"port")) cfg->port = atoi(v);
        else if (!strcmp(k,"db_path")) sel_safe_copy(cfg->db_path, sizeof(cfg->db_path), v);
        else if (!strcmp(k,"lab_token")) sel_safe_copy(cfg->lab_token, sizeof(cfg->lab_token), v);
        else if (!strcmp(k,"pc_id")) sel_safe_copy(cfg->pc_id, sizeof(cfg->pc_id), v);
        else if (!strcmp(k,"server_host")) sel_safe_copy(cfg->server_host, sizeof(cfg->server_host), v);
        else if (!strcmp(k,"required_software")) sel_safe_copy(cfg->required_software, sizeof(cfg->required_software), v);
        else if (!strcmp(k,"heartbeat_interval_s")) cfg->heartbeat_interval_s = atoi(v);
        else if (!strcmp(k,"cpu_warn_pct")) cfg->cpu_warn_pct = atof(v);
        else if (!strcmp(k,"ram_warn_pct")) cfg->ram_warn_pct = atof(v);
        else if (!strcmp(k,"heartbeat_timeout_s")) cfg->heartbeat_timeout_s = atoi(v);
    }
    fclose(f);
    if (cfg->port <= 0 || cfg->port > 65535) cfg->port = 5050;
    if (cfg->heartbeat_interval_s < 1) cfg->heartbeat_interval_s = 5;
    if (cfg->heartbeat_timeout_s < cfg->heartbeat_interval_s * 2) cfg->heartbeat_timeout_s = cfg->heartbeat_interval_s * 2;
    return 0;
}
