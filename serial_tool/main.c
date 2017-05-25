#include "serial_handler.h"
#include "main.h"

volatile int running = 1; //very much global
volatile int verbose = 1; //all, must be global
//char model[1024] = "{rel_t:1,a0:0,a1:0,a2:0,a3:0,a4:0,a5:0}";
char model[1024] = "rel_t,a0,a1,a2,a3,a4,a5";
char mysql_addr[256] = "127.0.0.1";
char mysql_login[256] = "root";
char mysql_pswd[256] = "pinetree";
char mysql_table[256] = "pins";
char mysql_db[256] = "testing";
char path_to_log_dir[4096]; //used by main and data, could pass, but nah
soc_node* head_soc_list = NULL;
soc_node* tail_soc_list = NULL;
str_node* head_str_list = NULL;
str_node* tail_str_list = NULL;
str_node* head_str_list2 = NULL;
str_node* tail_str_list2 = NULL;
int32_t str_list_size = 0;
int32_t str_list_size2 = 0;
uint64_t last_report; //main and data

char serial_path[2048];
int serial_speed;

int main(int argc, char* argv[]) {
  /* ------------------------------------------------------------------------*/
  head_str_list = add_str_node(head_str_list, "dummy", 0.0);
  tail_str_list = head_str_list;
  str_list_size++;
  head_str_list2 = add_str_node(head_str_list2, "dummy", 0.0);
  tail_str_list2 = head_str_list2;
  str_list_size2++;
  /* ------------------------------------------------------------------------*/


  /* ------------------------------------------------------------------------*/
  strncpy(serial_path, PORT_NAME, sizeof(serial_path));
  serial_path[sizeof(serial_path)-1] = 0;
  serial_speed = PORT_SPEED;
  /* ------------------------------------------------------------------------*/


  /* ------------------------------------------------------------------------*/
  strcpy(path_to_log_dir, "./");
  /* ------------------------------------------------------------------------*/


  /* ------------------------------------------------------------------------*/
  int use_host = 0;
  char host[2048];
  int port = 8888;
  /* ------------------------------------------------------------------------*/


  /* ------------------------------------------------------------------------*/
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
          char** fields;
          int n = parse_CSV(argv[i+1], &fields);
          if (n > 0) {
            strncpy(model, argv[i+1], 1024);
            model[1024-1] = 0;
          }
          free_CSV_fields(fields, n);
          i++;
        }
      } else if (strcmp(argv[i], "--mysql-password") == 0) {
        if (i + 1 < argc) {
          strncpy(mysql_pswd, argv[i+1], 256);
          mysql_pswd[256-1] = 0;
          i++;
        }
      } else if (strcmp(argv[i], "--mysql-login") == 0) {
        if (i + 1 < argc) {
          strncpy(mysql_login, argv[i+1], 256);
          mysql_login[256-1] = 0;
          i++;
        }
      } else if (strcmp(argv[i], "--mysql-database") == 0) {
        if (i + 1 < argc) {
          strncpy(mysql_db, argv[i+1], 256);
          mysql_db[256-1] = 0;
          i++;
        }
      } else if (strcmp(argv[i], "--mysql-table") == 0) {
        if (i + 1 < argc) {
          strncpy(mysql_table, argv[i+1], 256);
          mysql_table[256-1] = 0;
          i++;
        }
      } else if (strcmp(argv[i], "--mysql-address") == 0) {
        if (i + 1 < argc) {
          strncpy(mysql_addr, argv[i+1], 256);
          mysql_addr[256-1] = 0;
          i++;
        }
      } else {
        printf("Unrecognized option: %s\n", argv[i]);
      }
    }
  }
  /* ------------------------------------------------------------------------*/


  /* ------------------------------------------------------------------------*/
  change_port_permissions(serial_path);
  /* ------------------------------------------------------------------------*/


  /* ------------------------------------------------------------------------*/
  /* allow for graceful quitting */
  //signal(SIGTERM, sighandler);
  //signal(SIGABRT, sighandler);
  signal(SIGINT, sighandler);
  /* ------------------------------------------------------------------------*/


  /* ------------------------------------------------------------------------*/
  /* create connection to cmd line specified host */
  // struct sockaddr_in servaddr;
  int dummy_port = 8888;
  char dummy_host[] = "dummy";
  tail_soc_list = add_soc_node(tail_soc_list, dummy_host, dummy_port);
  head_soc_list = tail_soc_list;
  if (use_host) tail_soc_list = add_soc_node(tail_soc_list, host, port);
  /* ------------------------------------------------------------------------*/


  /* ------------------------------------------------------------------------*/
  if (verbose) printf("Serial pipe \"%s\" will be used with a speed of %i\n", serial_path, serial_speed);
  if (verbose) printf("Using model: \"%s\"\n", model);
  /* ------------------------------------------------------------------------*/


  /* ------------------------------------------------------------------------*/
  pthread_t mysql_handler;
  last_report = millis();
  if (pthread_create(&mysql_handler, NULL, handle_data, NULL)) {
    pthread_detach(mysql_handler);
    if (verbose) printf("Error creating the data handling thread\n");
    exit(5);
  }
  pthread_t serial_handler;
  if (pthread_create(&serial_handler, NULL, obtain_data, NULL)) {
    pthread_detach(serial_handler);
    if (verbose) printf("Error creating the data obtaining thread\n");
    exit(6);
  }
  pthread_t server_handler;
  if (pthread_create(&server_handler, NULL, server, NULL)) {
    pthread_detach(server_handler);
    if (verbose) printf("Error creating the server thread\n");
    exit(7);
  }
  /* ------------------------------------------------------------------------*/


  /* ------------------------------------------------------------------------*/
  while (running) {
    uint64_t data_thread_frozen = ((millis() - last_report) > 5000);
    if (data_thread_frozen) {
      delay(1);
      data_thread_frozen = ((millis() - last_report) > 5000);
    }
    if (data_thread_frozen) {
      if (verbose) printf("Check not fine\n");
      pthread_kill(mysql_handler, SIGUSR1);
      last_report = millis();
      if (pthread_create(&mysql_handler, NULL, handle_data, NULL)) {
        pthread_detach(mysql_handler);
        if (verbose) printf("!!Error: creating the data handling thread failed\n");
        exit(5);
      }
    }
    delay(100);
  }
  /* ------------------------------------------------------------------------*/


  /* ------------------------------------------------------------------------*/
  if (verbose) printf("\nClosing...\n");
  delay(100);

  if (millis() > last_report) pthread_kill(mysql_handler, SIGUSR1);

  printf("Closing pipe\n");
  close(fd);

  printf("Freeing socket nodes\n");
  free_soc_nodes(head_soc_list);
  /* ------------------------------------------------------------------------*/

  exit(0);
}

soc_node* add_soc_node(soc_node* soc, char* ip, int port) {
  soc_node* soc_curr = (soc_node*)malloc(sizeof(soc_node)); 
  soc_curr->next = NULL;
  if (soc != NULL)
    soc->next = soc_curr;

  if (verbose && soc != NULL)
    printf("Sending UDP packets will occur to %s on port %i\n", ip, port);
  struct hostent* hp; 

  if (strcmp(ip, "dummy") == 0) return soc_curr;

  memset((char*)&(soc_curr->servaddr), 0, sizeof(soc_curr->servaddr));
  (soc_curr->servaddr).sin_family = AF_INET;
  (soc_curr->servaddr).sin_port = htons(port);
  hp = gethostbyname(ip);
  if (!hp) {
    if (verbose)
      printf("Could not have obtained host address of %s:%i!\n", ip, port);
    if (soc != NULL)
      soc->next = NULL;
    free(soc_curr);
    return NULL; 
  }
  memcpy((void*)&(soc_curr->servaddr).sin_addr, hp->h_addr_list[0], hp->h_length);
  soc_curr->fd = socket(AF_INET, SOCK_DGRAM, 0);

  return soc_curr;
}

void free_soc_nodes(soc_node* soc) {
  soc_node* last_node;
  soc_node* curr_node;
  curr_node = soc;
  while (curr_node != NULL) {
    last_node = curr_node; 
    curr_node = curr_node->next;
    close(last_node->fd);
    free(last_node);
  }
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

void sighandler(int signum) {
  running = 0;
}

void change_port_permissions(char* serial_path) {
  if (verbose) {
    printf("Changing permissions on port %s to 766.\n", serial_path);
    printf("Get ready to input your password\n");
  }
  char* cmd = (char*)malloc(sizeof(char)*(strlen(serial_path)+64));
  sprintf(cmd, "sudo chmod 766 %s", serial_path);
  system(cmd);
  free(cmd);
}
