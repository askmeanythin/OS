/* client.h - lab client agent */
#ifndef SEL_CLIENT_H
#define SEL_CLIENT_H

#include "config.h"

/* Runs the agent loop (register, heartbeat, reconnect with backoff).
 * If simulate!=0, metrics come from monitor_simulate instead of the OS.
 * Blocks until *stop_flag becomes non-zero (or forever if NULL). */
int client_run(const AppConfig *cfg, int simulate, volatile int *stop_flag);

#endif
