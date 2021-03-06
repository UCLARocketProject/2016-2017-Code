#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
//#include <fcntl.h>   /* File control definitions */
//#include <errno.h>   /* Error number definitions */
//#include <termios.h> /* POSIX terminal control definitions */
#include <signal.h>
//#include <sys/time.h>
//#include <time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
//#include <mysql/mysql.h>
#include <stdint.h>
#include <pthread.h>


#define PORT_NAME "/dev/my_pipe"
#define PORT_SPEED -1


void sighandler(int signum);
void print_help_message();
soc_node* add_soc_node(soc_node* soc, char* ip, int port);
void free_soc_nodes(soc_node* soc);
void change_port_permissions(char* serial_path);

#endif
