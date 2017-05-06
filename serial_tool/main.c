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

volatile int running = 1;
//char model[1024] = "{t:1,data:{rel_t:0,a0:0,a1:0,a2:0,a3:0,a4:0,a5:0}}";
char model[1024] = "{rel_t:1,a0:0,a1:0,a2:0,a3:0,a4:0,a5:0}";
char mysql_addr[256] = "127.0.0.1";
char mysql_login[256] = "root";
char mysql_pswd[256] = "pinetree";
char mysql_table[256] = "pins";
char mysql_db[256] = "testing";
char path_to_log_dir[4096];

MYSQL* con = NULL;

int verbose = 1;

FILE* log_fd;

int str_list_size;

soc_node* head_soc_list = NULL;
soc_node* tail_soc_list = NULL;

int main(int argc, char* argv[]) {

  str_node* head_str_list = NULL;
  str_node* tail_str_list = NULL;
  head_str_list = add_str_node(head_str_list, "dummy");
  tail_str_list = head_str_list;
  str_list_size = 1;


  char serial_path[2048];
  strncpy(serial_path, PORT_NAME, sizeof(serial_path));
  serial_path[sizeof(serial_path)-1] = 0;

  int serial_speed = PORT_SPEED;

  strcpy(path_to_log_dir, "./");

  log_fd = NULL;

  int use_host = 0;
  char host[2048];
  int port = 8888;

  char* temp_model = (char*)malloc(sizeof(char)*512);
  strncpy(temp_model, model, 512);
  temp_model[512-1] = 0;
  sprintf(model, "{t:0,data:%s}", temp_model);
  free(temp_model);

  /* check for help message flag */
  if (argc > 1) {
    int i;
    for (i = 1; i < argc; i++)
      if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
        print_help_message();
        return 0;
      }
  }
  /* parse arguments */
  if (argc > 1) {
    int i;
    for (i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-s") == 0) {
        verbose = 0; 
      } else if (strcmp(argv[i], "-F") == 0 || strcmp(argv[i], "-l") == 0) {
        if (i + 1 < argc) {
          if (strlen(argv[i+1]) < sizeof(path_to_log_dir)) {
            strncpy(path_to_log_dir, argv[i+1], 4096);
            path_to_log_dir[4096-2] = 0;
            size_t ptl_len = strlen(path_to_log_dir);
            if (path_to_log_dir[ptl_len-1] != '/') {
              path_to_log_dir[ptl_len] = '/';
              path_to_log_dir[ptl_len+1] = 0;
            }
          }
          i++;
        }
      } else if (strcmp(argv[i], "-H") == 0) {
        if (i + 1 < argc) {
          if (strlen(argv[i+1]) < sizeof(host)) {
            strcpy(host, argv[i+1]);
            use_host = 1;
          }
          i++;
        }
      } else if (strcmp(argv[i], "-p") == 0) {
        if (i + 1 < argc) {
          port = atoi(argv[i+1]);
          if (port < 0)
            port = 8888;
          //use_host = 1;
          i++;
        }
      } else if (strcmp(argv[i], "-P") == 0) {
        if (i + 1 < argc) {
          if (strlen(argv[i+1]) < sizeof(serial_path))
            strcpy(serial_path, argv[i+1]);
          i++;
        }
      } else if (strcmp(argv[i], "-S") == 0) {
        if (i + 1 < argc) {
          serial_speed = atoi(argv[i+1]);
          if (serial_speed < 0)
            serial_speed = PORT_SPEED;
          i++;
        }
      } else if (strcmp(argv[i], "-m") == 0) {
        if (i + 1 < argc) {
          char** keys = NULL;
          char** values = NULL;
          int n = parseJSON(&keys, &values, argv[i+1]);
          if (n > 0) {
            //strncpy(model, argv[i+1], 1024);
            if (strlen(argv[i+1]) >= 512)
              argv[i+1][512] = 0;
            sprintf(model, "{t:0,data:%s}", argv[i+1]);
          }
          model[1024-1] = 0;
          free_JSON_array(keys, n);
          free_JSON_array(values, n);
          i++;
        }
      } else if (strcmp(argv[i], "--mysql-password") == 0) {
        if (i + 1 < argc) {
          strncpy(mysql_pswd, argv[i+1], 256);
          model[256-1] = 0;
          i++;
        }
      } else if (strcmp(argv[i], "--mysql-login") == 0) {
        if (i + 1 < argc) {
          strncpy(mysql_login, argv[i+1], 256);
          model[256-1] = 0;
          i++;
        }
      } else if (strcmp(argv[i], "--mysql-database") == 0) {
        if (i + 1 < argc) {
          strncpy(mysql_db, argv[i+1], 256);
          model[256-1] = 0;
          i++;
        }
      } else if (strcmp(argv[i], "--mysql-table") == 0) {
        if (i + 1 < argc) {
          strncpy(mysql_table, argv[i+1], 256);
          model[256-1] = 0;
          i++;
        }
      } else if (strcmp(argv[i], "--mysql-address") == 0) {
        if (i + 1 < argc) {
          strncpy(mysql_addr, argv[i+1], 256);
          model[256-1] = 0;
          i++;
        }
      } else {
        printf("Unrecognized option: %s\n", argv[i]);
      }
    }
  }

  int dummy_port = 8888;
  char dummy_host[] = "192.168.1.17";
  tail_soc_list = add_soc_node(tail_soc_list, dummy_host, dummy_port);
  head_soc_list = tail_soc_list;


  /* allow for graceful quitting */
  signal(SIGTERM, sighandler);
  signal(SIGABRT, sighandler);
  signal(SIGINT, sighandler);

  /* create connection to cmd line specified host */
  // struct sockaddr_in servaddr;
  if (use_host)
    tail_soc_list = add_soc_node(tail_soc_list, host, port);

  if (verbose)
    printf("##Serial pipe \"%s\" will be used with a speed of %i\n", serial_path, serial_speed);
  if (verbose)
    printf("##Using model: \"%s\"\n", model);

  /* start serial reception */
  int fd = open_port(serial_path, serial_speed);

  char line[M];
  char buffer[N<<2];
  size_t buff_len = 0;
  line[0] = 0;
  uint64_t bytes_written = 0;
  //unsigned writing_occurred = 0;
  //int r = 0;
  pthread_t mysql_handler;
  if (pthread_create(&mysql_handler, NULL, handle_data, (void*)head_str_list)) {
    if (verbose)
      printf("!!Error creating the data handling thread\n");
    return 5;
  }
  while (running) {
    /* attempt to open socket when closed */
    if (fd == -1 || write(fd, "", 0) < 0) { //socket is closed 
      close(fd);
      fd = open_port(serial_path, serial_speed);
      if (verbose)
        printf("##Re-opening port\n");
      delay(100);
      /* read data from open socket */
    } else {
      wait_on_data(fd, 1000);
      int read_bytes = read(fd, line, M-1);
      if (read_bytes > 0) {
        memcpy(buffer+buff_len, line, read_bytes);
        buff_len += read_bytes;
        if (buff_len > N<<2)
          buff_len = N<<2;
        buffer[buff_len] = 0;
        int j = parse_newline_and_log(log_fd, buffer, buff_len, head_soc_list, &tail_str_list);
        //r++;
        //if (r % 3 == 0)
        //  head_str_list = handle_str_list(head_str_list);
        memmove(buffer, buffer+j, buff_len - j);
        buff_len = buff_len - j;
        buffer[buff_len] = 0;
        bytes_written += j;

        line[read_bytes] = 0;
        if (verbose) {
          //printf("%s", line);
          fflush(stdout);
        }
        //writing_occurred = 1;
      } else if (read_bytes == 0) {
        // WEIRD CASE
      } else {
        // NO DATA CASE 
      }
    }
  }
  if (verbose)
    printf("\nClosing...\n");
  if (pthread_join(mysql_handler, NULL)) {
    if (verbose)
      printf("!!Error joining thread\n");
  }
  if (log_fd != NULL)
    fclose(log_fd);
  close(fd);
  free_soc_nodes(head_soc_list);
  free_str_nodes(head_str_list);
  return 0;
}

