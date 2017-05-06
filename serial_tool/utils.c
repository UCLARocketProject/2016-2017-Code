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
#include <poll.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <mysql/mysql.h>
#include <stdint.h>
#include <pthread.h>

#include "serial_handler.h"

void sighandler(int signum) {
  running = 0;
}

int open_port(char* serial_path, int speed) {
  int fd = open(serial_path, O_RDWR | O_NOCTTY | O_NDELAY);
  if (fd == -1) {
    //printf("Could not have opened the port!\n");
    //printf("%s\n", PORT_NAME);
    return -1;
  } 
  fcntl(fd, F_SETFL, FNDELAY);

  struct termios options;
  tcgetattr(fd, &options);
  if (speed > 0) {
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);
  }

  options.c_cflag |= (CLOCAL | CREAD);
  tcsetattr(fd, TCSANOW, &options);

  //tcgetattr(fd, &options);
  return fd;
}

uint64_t millis() {
  struct timeval tv;  
  gettimeofday(&tv, NULL);
  return ((uint64_t)tv.tv_sec * 1000LLU) + ((uint64_t)tv.tv_usec) / 1000LLU;
}

uint64_t micros() {
  struct timeval tv;  
  gettimeofday(&tv, NULL);
  return (((uint64_t)tv.tv_sec) * 1000000LLU) + ((uint64_t)tv.tv_usec);
}

void delay(uint64_t millisec) {
  struct timespec tv;
  tv.tv_sec = millisec / 1000LLU;
  tv.tv_nsec = (millisec % 1000LLU) * 1000000LLU;
  nanosleep(&tv, NULL);
}

void delayMicroseconds(uint64_t microsec) {
  struct timespec tv;
  tv.tv_sec = microsec / 1000000LLU;
  tv.tv_nsec = (microsec % 1000000LLU) * 1000LLU;
  nanosleep(&tv, NULL);
}

void hard_delay(uint64_t millisec) {
  uint64_t init_time = millis(); 
  while (millis() - init_time < millisec) {
    asm("nop");
  }
}

void hard_delayMicroseconds(uint64_t microsec) {
  uint64_t init_time = micros(); 
  while (micros() - init_time < microsec) {
    asm("nop");
  }
}

FILE* open_new_log(char* path_to_log_dir) {
  FILE* fd = NULL;
  char file_name[1024];
  char file_name_temp[1024];
  size_t path_len = strlen(path_to_log_dir);
  if (path_len > 512)
    return NULL;
  if (path_to_log_dir[path_len-1] != '/') {
    path_to_log_dir[path_len+1] = 0;
    path_to_log_dir[path_len] = '/';
  }

  int i = 0;
  do {
    if (fd != NULL)
      fclose(fd);
    sprintf(file_name_temp, "log%03i.txt", i);
    strcpy(file_name, path_to_log_dir);
    strncat(file_name, file_name_temp, 1024 - strlen(file_name) - 1);
    fd = fopen(file_name, "r");
    if (i++ >= MAX_LOG_N) 
      return NULL;
  } while (fd != NULL);
  if (verbose)
    printf("Opening log with %s name\n", file_name);
  fd = fopen(file_name, "w+"); 
  if (fd == NULL) {
    printf("INVALID file desc\n");
  }
  return fd;
}

int wait_on_data(int fd, uint64_t millisec) {
  struct pollfd fds;
  int retval;

  fds.fd = fd;
  fds.events = POLLIN | POLLRDNORM | POLLPRI;

  //uint64_t t1 = millis();
  retval = poll(&fds, 1, millisec);
  //printf("%lu millisec elapsed\n", millis() - t1);
  if (retval == -1) {
    //printf("An error occurred with select()\n");
  }
  return retval;
}

void imprint_current_time(char* s) {
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);

  sprintf(s, "%d/%d/%d UTC %02d:%02d:%02d:%03d", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(millis() % 1000));
}

size_t parse_newline_and_log(FILE* log_fd, char* buffer, size_t buff_len, soc_node* soc_list, str_node** str_list) {
  size_t i, j;
  char line[M];
  char json_formatted[N];
  for (i = 0, j = 0; i < buff_len; i++) {
    if (buffer[i] == '\n') {
      memcpy(line, buffer+j, i-j);
      line[i-j] = 0;
      if (strlen(line) > 0) {
        char UDP_line[M];
        size_t l = strlen(line);
        memcpy(UDP_line, line, l);
        UDP_line[l] = '\n';
        UDP_line[l+1] = 0;
        while (soc_list != NULL) {
          sendto(soc_list->fd, UDP_line, l+1, 0, (struct sockaddr*)&(soc_list->servaddr), sizeof(soc_list->servaddr));
          soc_list = soc_list->next;
        }
        sprintf(json_formatted, "{\"t\":%.5f, \"data\":\"%s\"}\n", ((double)micros())/1000000.0, line);
        //fwrite(json_formatted, sizeof(char), strlen(json_formatted), log_fd);
        //printf("%s", json_formatted);
        *str_list = add_str_node(*str_list, json_formatted);
      }
      j = i + 1;
    }
  }
  return j;
}

void print_help_message() {
  char message[] = "\nThe following options are supported:\n" \
"-h,--help          prints this help message\n" \
"-H                 specify host to which logged data is sent using UDP\n" \
"                   UDP transmissions only occur when host is specified\n" \
"                   host defaults to \"localhost\"\n" \
"-p                 specify port to which data should be sent using UDP\n"
"                   UDP transmissions only occur when host or port \n" \
"                   is specified port defaults to \"8888\"\n" \
"-F,-l             specifies the file to which logging should occur\n" \
"-s                 silent mode, no output is produced\n" \
"-P                 specify serial pipe to be used\n" \
"                   only one pipe per process is currently supported\n" \
"-S                 specify the speed of serial\n" \
"-m                 specify model JSON string for MySql parsing\n" \
"--mysql-login      specify mysql login for the server\n" \
"--mysql-password   specify mysql password for the server\n" \
"--mysql-database   specify mysql database\n" \
"--mysql-table      specify mysql table for data\n" \
"--mysql-address    specify mysql addr for the server\n\n" \
"Notes:\n" \
"Serial pipes of USB ports are only readable on root by default, make sure" \
"to change their permissions first or execute this program as root\n\n"
"Examples:\n" \
"sudo ./exec -P \"/dev/ttyACM0\" -l \"$HOME/Desktop\" --mysql-database" \
" \"testing\"\n\n" \
"Press Ctrl + C to terminate this program safely\n";
  puts(message);
}
