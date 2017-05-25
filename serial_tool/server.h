#ifndef SERVER_H
#define SERVER_H

#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <pthread.h>

#include "serial_handler.h"
#include "sync_tools.h"

void die(char* s);
void handleClntSock(int clntSock, char* body, size_t body_size);
void* server(void* arg);
size_t build_a_recent_list(char* body, size_t body_n, str_node** str_list, uint64_t milliseconds);

#endif
