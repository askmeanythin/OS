/* server.h - TCP server: Winsock2 on Windows, POSIX sockets elsewhere */
#ifndef SEL_SERVER_H
#define SEL_SERVER_H

#include "config.h"

int  server_start(const AppConfig *cfg);  /* non-blocking: spawns listener + sweeper threads */
void server_stop(void);
int  server_is_running(void);
/* Run N simulated clients in-process, each on its own thread, talking real TCP
 * over loopback to this server (simulation mode). */
int  server_start_simulation(int n_pcs);

#endif
