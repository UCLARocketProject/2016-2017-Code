#ifndef SERIAL_HANDLER_H
#define SERIAL_HANDLER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <mysql/mysql.h>
#include <stdint.h>
#include <pthread.h>

#include "JSON.h"

#define PORT_NAME "/dev/my_pipe"
#define PORT_SPEED -1
#define N (1<<10)
#define M N-256
#define MAX_LOG_N 256
#define MAX_FILE_SIZE (1<<29) // half a Gigabyte

extern volatile int running;
extern char model[1024];
extern char mysql_addr[256];
extern char mysql_login[256];
extern char mysql_pswd[256];
extern char mysql_table[256];
extern char mysql_db[256];

extern MYSQL* con;

extern int verbose;

extern FILE* log_fd;
extern char path_to_log_dir[4096];

extern int str_list_size;

struct str_node {
  char text[256];
  struct str_node* next;
};
struct soc_node {
  int fd;
  struct sockaddr_in servaddr;
  struct soc_node* next;
};
typedef struct str_node str_node;
typedef struct soc_node soc_node;

extern soc_node* head_soc_list;
extern soc_node* tail_soc_list;

void sighandler(int signum);
int open_port(char* serial_path, int speed);
uint64_t millis();
uint64_t micros();
void delay(uint64_t millisec);
void hard_delay(uint64_t millisec);
void delayMicroseconds(uint64_t microsec);
void hard_delayMicroseconds(uint64_t microsec);
int wait_on_data(int fd, uint64_t millisec);
void imprint_current_time(char* s);
size_t parse_newline_and_log(FILE* log_fd, char* buffer, size_t buff_len, soc_node* soc_list, str_node** str_list);
FILE* open_new_log(char* path_to_log_dir);
void print_help_message();
soc_node* add_soc_node(soc_node* soc, char* ip, int port);
str_node* add_str_node(str_node* str, char* line);
void free_soc_nodes(soc_node* soc);
void free_str_nodes(str_node* str);
str_node* handle_str_list(str_node* head);
void* handle_data(void* arg);
void connect_to_mysql(int init);

#endif
