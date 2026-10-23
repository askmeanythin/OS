/* main_client.c - client agent entry point.
 * Usage: exam_client.exe [--config config/client.conf] [--simulate]
 *                        [--pc-id PC-07] [--server 192.168.1.10] [--port 5050] */
#include "common.h"
#include "config.h"
#include "client.h"
#include <signal.h>

static volatile int g_stop = 0;
#ifdef _WIN32
  #include <windows.h>
  static BOOL WINAPI ctrl_handler(DWORD t) { if (t == CTRL_C_EVENT || t == CTRL_CLOSE_EVENT) { g_stop = 1; return TRUE; } return FALSE; }
#else
  static void sig_handler(int s) { (void)s; g_stop = 1; }
#endif

int main(int argc, char **argv) {
    AppConfig cfg; config_defaults(&cfg);
    const char *cfg_path = "config/client.conf";
    int simulate = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--config") && i + 1 < argc) cfg_path = argv[++i];
        else if (!strcmp(argv[i], "--simulate")) simulate = 1;
        else if (!strcmp(argv[i], "--pc-id") && i + 1 < argc) { config_load(&cfg, cfg_path); sel_safe_copy(cfg.pc_id, sizeof(cfg.pc_id), argv[++i]); cfg_path = NULL; }
        else if (!strcmp(argv[i], "--server") && i + 1 < argc) { config_load(&cfg, cfg_path); sel_safe_copy(cfg.server_host, sizeof(cfg.server_host), argv[++i]); cfg_path = NULL; }
        else if (!strcmp(argv[i], "--port") && i + 1 < argc) { config_load(&cfg, cfg_path); cfg.port = atoi(argv[++i]); cfg_path = NULL; }
        else if (!strcmp(argv[i], "--token") && i + 1 < argc) { config_load(&cfg, cfg_path); sel_safe_copy(cfg.lab_token, sizeof(cfg.lab_token), argv[++i]); cfg_path = NULL; }
    }
    if (cfg_path && config_load(&cfg, cfg_path) != 0)
        printf("Note: %s not found, using defaults.\n", cfg_path);
#ifdef _WIN32
    SetConsoleCtrlHandler(ctrl_handler, TRUE);
#else
    signal(SIGINT, sig_handler); signal(SIGTERM, sig_handler);
#endif
    printf("Exam Lab client agent: pc_id=%s server=%s:%d mode=%s\n",
           cfg.pc_id, cfg.server_host, cfg.port, simulate ? "SIMULATION metrics" : "real OS metrics");
    printf("Press Ctrl+C to stop.\n");
    return client_run(&cfg, simulate, &g_stop);
}
