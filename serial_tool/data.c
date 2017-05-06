#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <signal.h> 
#include <sys/time.h>
#include <sys/types.h>
#include <mysql/mysql.h>

#include "serial_handler.h"

soc_node* add_soc_node(soc_node* soc, char* ip, int port) {
  soc_node* soc_curr = (soc_node*)malloc(sizeof(soc_node)); 
  soc_curr->next = NULL;
  if (soc != NULL)
    soc->next = soc_curr;

  if (verbose && soc != NULL)
    printf("Sending UDP packets will occur to %s on port %i\n", ip, port);
  struct hostent* hp; 

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

str_node* add_str_node(str_node* str, char* line) {
  str_node* str_curr;
  str_curr = (str_node*)malloc(sizeof(str_node));
  str_curr->next = NULL;
  if (str != NULL)
    str->next = str_curr;
  strncpy(str_curr->text, line, sizeof(str_curr->text));
  (str_curr->text)[sizeof(str_curr->text)-1] = 0;

  return str_curr;
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

void free_str_nodes(str_node* str) {
  str_node* last_node;
  str_node* curr_node;
  curr_node = str;
  while (curr_node != NULL) {
    last_node = curr_node; 
    curr_node = curr_node->next;
    free(last_node);
  }
}

str_node* handle_str_list(str_node* head) {
  uint64_t tw1 = millis();
  if (head == NULL) 
    return NULL;

  char** model_keys;
  char** model_values;
  char** model_data_keys;
  char** model_data_values;
  int model_n = parseJSON(&model_keys, &model_values, model);
  int model_data_n;
  int i;
  for (i = 0; i < model_n; i++) {
    if (strcmp(model_keys[i], "data") == 0) {
      model_data_n = parseJSON(&model_data_keys, &model_data_values, model_values[i]);
      //printf("model_values[i] is %s\n", model_values[i]);
    }
  }

#define SQL_N (1<<20)
  char* sql = (char*)malloc(sizeof(char) * SQL_N);
  char temp[256];
  sprintf(sql, "INSERT INTO %s (%s,", mysql_table, "abs_t");
  int k = 0;
  for (k = 0; k < model_data_n; k++) {
    sprintf(temp, "%s,", model_data_keys[k]);
    strcat(sql, temp);
  }
  sql[strlen(sql)-1] = 0; // delete last comma
  strcat(sql, ") VALUES ");
  size_t sql_len = strlen(sql);

  str_node* curr_node = head;
  str_node* last_node = NULL;
  size_t analyzed_nodes = 0;
  while (curr_node->next != NULL && (sql_len + 256) < SQL_N) {
    last_node = curr_node;
    curr_node = curr_node->next;
    free(last_node);
    str_list_size--;
    //if (analyzed_nodes > 1000)
    //  continue;
    //printf("-------------\n%s------------------\n", curr_node->text);

    char** keys;
    char** values;
    char** data_keys;
    char** data_values;

    char time_str[256]; 
    time_str[0] = 0;
    char values_line[256];
    values_line[0] = 0;

    int n = parseJSON(&keys, &values, curr_node->text);
    int dn = 0;
    int i;
    for (i = 0; i < n; i++) {
      if (strcmp(keys[i], "t") == 0) {
        strncpy(time_str, values[i], 256);   
        sprintf(values_line, "(%s,", time_str);
        time_str[256-1] = 0;
      } else if (strcmp(keys[i], "data") == 0) {
        dn = parseJSON(&data_keys, &data_values, values[i]);
      }
    }
    //printf("dn = %i and model_data_n = %i\n", dn, model_data_n);
    if (dn == model_data_n) {
      for (i = 0; i < dn; i++) {
        //printf("%s\n", data_values[i]);
        sprintf(temp, "%s,", data_values[i]);
        strcat(values_line, temp);
      }
      int values_len = strlen(values_line);
      values_line[values_len-1] = ')'; // replace comma with closing )
      values_line[values_len] = ','; // add trailing comma
      values_line[values_len+1] = ' '; // add trailing comma
      values_line[values_len+2] = 0;
      //printf("values_line is %s\n", values_line);
      sql_len += values_len + 2;
      //printf("sql_len is %i\n", sql_len);
      strcat(sql, values_line);
      analyzed_nodes++;
    } else {
      printf("##Incoming data misformated\n");
      printf("dn is %d and model_data_n is %d\n", dn, model_data_n);
    }

    free_JSON_array(keys, n);
    free_JSON_array(values, n);
    free_JSON_array(data_keys, dn);
    free_JSON_array(data_values, dn);
  }
  sql[sql_len-2] = 0; // delete last comma
  sql_len--;
  if (log_fd != NULL) {
    fwrite(sql, sizeof(char), sql_len, log_fd);
    fwrite("\n", sizeof(char), 1, log_fd);
  }

#define TOO_MANY_DATA_NODES (1<<18) // 100,000 - roughly
  if (str_list_size > TOO_MANY_DATA_NODES) {
    printf("TOO MANY DATA NODES: %d\n", str_list_size);
  }
  if (analyzed_nodes > 0 && str_list_size < TOO_MANY_DATA_NODES) {
    /* DO SQL INSERTION */
    connect_to_mysql(0);
    uint64_t t1 = millis(); 
    printf("strlen(sql) == %zu\n", strlen(sql));
    if (con != NULL && mysql_query(con, sql)) {
      if (verbose)
        printf("%s\n", mysql_error(con));
      mysql_close(con);
      con = NULL;
    }
    uint64_t t2 = millis(); 
    if (con != NULL) {
      printf("MYSQL insertion took %llu ms\n", t2 - t1);
      mysql_close(con);
      con = NULL;
    }
    printf("%s\n", sql);
  }
  sql[sql_len-1] = '\n';
  sql[sql_len] = 0;
  soc_node* soc_list = head_soc_list;
  if (soc_list != NULL) {
    //skip the dummy, dummy necessary for safe linked list access of concurrent
    //threads, I think, unless something can come up with something better
    soc_list = soc_list->next; 
  }
  while (soc_list != NULL) {
    sendto(soc_list->fd, sql, sql_len, 0, (struct sockaddr*)&(soc_list->servaddr), sizeof(soc_list->servaddr));
    soc_list = soc_list->next;
  }

  free_JSON_array(model_keys, model_n);
  free_JSON_array(model_values, model_n);
  free_JSON_array(model_data_keys, model_data_n);
  free_JSON_array(model_data_values, model_data_n);
  free(sql);
  uint64_t tw2 = millis();
  printf("##Whole loop-run took %llu ms\n", tw2 - tw1);
  return curr_node;
}

void connect_to_mysql(int init) {
  if (init) {
    printf("##Connecting to mysql database at %s with the login: %s and "\
        "password: %s; using database: %s and table: %s\n", 
        mysql_addr, mysql_login, mysql_pswd, mysql_db, mysql_table);
  }
  char** model_keys = NULL;
  char** model_values = NULL;
  char** model_data_keys = NULL;
  char** model_data_values = NULL;
  int model_n = parseJSON(&model_keys, &model_values, model);
  int model_data_n = 0;
  int i;
  for (i = 0; i < model_n; i++) {
    if (strcmp(model_keys[i], "data") == 0)
      model_data_n = parseJSON(&model_data_keys, &model_data_values, model_values[i]);
  }

  char temp[1024];
  char sql_temp[512]; 
  unsigned int mysql_arg = 2; // 2 seconds

  con = mysql_init(NULL);
  if (con == NULL) {
    if (verbose)
      printf("##%s\n", mysql_error(con));
    mysql_close(con);
    con = NULL;
    return;
  }
  if (mysql_options(con, MYSQL_OPT_READ_TIMEOUT, &mysql_arg)) {
    if (verbose)
      printf("##%s\n", mysql_error(con));
    mysql_close(con);
    con = NULL;
    return;
  }
  if (mysql_options(con, MYSQL_OPT_CONNECT_TIMEOUT, &mysql_arg)) {
    if (verbose)
      printf("##%s\n", mysql_error(con));
    mysql_close(con);
    con = NULL;
    return;
  }

  if (mysql_real_connect(con, mysql_addr, mysql_login, mysql_pswd, NULL, 0, NULL, 0) == NULL) {
    if (verbose)
      printf("##%s\n", mysql_error(con));
    mysql_close(con);
    con = NULL;
    return;
  }

  if (init) {
    sprintf(sql_temp, "CREATE DATABASE IF NOT EXISTS %s", mysql_db);
    if (verbose)
      printf("##%s\n", sql_temp);
    if (mysql_query(con, sql_temp)) {
      if (verbose)
        printf("##%s\n", mysql_error(con));
      mysql_close(con);
    }
  }

  sprintf(sql_temp, "USE %s", mysql_db);
  if (verbose)
    printf("##%s\n", sql_temp);
  if (mysql_query(con, sql_temp)) {
    if (verbose)
      printf("##%s\n", mysql_error(con));
    mysql_close(con);
  }

  if (init) {
    sprintf(sql_temp, "CREATE TABLE IF NOT EXISTS %s (id int primary key not null auto_increment,", mysql_table);
    strcat(sql_temp, "abs_t double,");
    int k = 0;
    for (k = 0; k < model_data_n; k++) {
      sprintf(temp, "%s int,", model_data_keys[k]);
      strcat(sql_temp, temp);
    }
    sql_temp[strlen(sql_temp)-1] = 0; // delete last comma
    strcat(sql_temp, ")");
    if (verbose)
      printf("##%s\n", sql_temp);
    if (mysql_query(con, sql_temp)) {
      if (verbose)
        printf("##%s\n", mysql_error(con));
      mysql_close(con);
    }
  }

  free_JSON_array(model_keys, model_n);
  free_JSON_array(model_values, model_n);
  free_JSON_array(model_data_keys, model_data_n);
  free_JSON_array(model_data_values, model_data_n);
}

void* handle_data(void* arg) {
  str_node* head_str_list = (str_node*)arg;
  uint64_t last_written = millis();

  connect_to_mysql(1);
  log_fd = open_new_log(path_to_log_dir);

  if (head_str_list == NULL) {
    printf("!!Error: head_str_list is NULL, exiting mysql thread\n");
    return NULL;
  }

  while (running) {
    if (millis() - last_written > 100) {
      head_str_list = handle_str_list(head_str_list);
      last_written = millis();
      fflush(log_fd);
    }
    delay(10);
  }
  printf("##Exiting mysql thread\n");
  if (con != NULL) {
    mysql_close(con);
  }
  return NULL;
}

