#ifndef SERIAL_HANDLER_H
#define SERIAL_HANDLER_H 

/* BEGIN EXTERNAL LIBRARY INCLUDES */
#include <netdb.h>
#include <mysql/mysql.h>
/*END EXTERNAL LIBRARY INCLUDES */


/*BEGIN INTERNAL LIBRARY INCLUDES */
#include "JSON.h"
#include "CSV.h"
/*END INTERNAL LIBRARY INCLUDES */


/* BEGIN DEFAULT VALUES DEFINITIONS */
/* END DEFAULT VALUES DEFINITIONS */


/* BEGIN TYPES DEFINITIONS */
struct str_node {
  char text[256];
  double time;
  struct str_node* next;
};
struct soc_node {
  int fd;
  struct sockaddr_in servaddr;
  struct soc_node* next;
};
typedef struct str_node str_node;
typedef struct soc_node soc_node;
/* END TYPES DEFINITIONS */


/* BEGIN GLOBALS DEFINITIONS */
extern volatile int running;
extern char model[1024];
extern char mysql_addr[256];
extern char mysql_login[256];
extern char mysql_pswd[256];
extern char mysql_table[256];
extern char mysql_db[256];
char serial_path[2048];
extern MYSQL* con;
extern volatile int verbose;
extern FILE* log_fd;
extern int fd;
extern char path_to_log_dir[4096];
extern soc_node* head_soc_list;
extern soc_node* tail_soc_list;
extern str_node* head_str_list;
extern str_node* tail_str_list;
extern str_node* head_str_list2;
extern str_node* tail_str_list2;
extern int32_t str_list_size;
extern int32_t str_list_size2;
extern uint64_t last_report;
extern int serial_speed;
extern char* sql;
/* END GLOBALS DEFINITIONS */


/* BEGIN FUNCTIONS DEFINITIONS */
//used by main
//void sighandler(int signum);
//void print_help_message();
//soc_node* add_soc_node(soc_node* soc, char* ip, int port);
//void free_soc_nodes(soc_node* soc);

//used by utils
//int open_port(char* serial_path, int speed);
//int wait_on_data(int fd, uint64_t millisec);
//void imprint_current_time(char* s);
//size_t parse_newline_and_log(char* buffer, size_t buff_len, soc_node* soc_list, str_node** str_list);
str_node* add_str_node(str_node* str, char* line, double time);
void* obtain_data(void* arg);
void ignore_sigusr1(int signum);

//low level timing functions
uint64_t millis();
uint64_t micros();
void delay(uint64_t millisec);
void hard_delay(uint64_t millisec);
void delayMicroseconds(uint64_t microsec);
void hard_delayMicroseconds(uint64_t microsec);

void* server(void* arg);

//used by data
//FILE* open_new_log(char* path_to_log_dir);
void free_str_nodes(str_node* str);
//void handle_str_list(str_node** head_ptr);
void* handle_data(void* arg);
//void break_yourself(int signum);
//void connect_to_mysql(int init);
/* END FUNCTIONS DEFINITIONS */

#endif
