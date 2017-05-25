#include "server.h"

void* server(void* arg) {
  signal(SIGUSR1, ignore_sigusr1);
#define BODY_SIZE (1<<20)
  char* body = (char*)malloc(sizeof(char)*BODY_SIZE);

  int servSock;
  int clntSock;
  struct sockaddr_in echoServAddr;
  struct sockaddr_in echoClntAddr;
  unsigned short echoServPort;
  unsigned int clntLen;

  echoServPort = 8888;
  servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (servSock < 0) die("Could not create the socket");

  memset(&echoServAddr, 0, sizeof(echoServAddr));
  echoServAddr.sin_family = AF_INET;
  echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  echoServAddr.sin_port = htons(echoServPort);

  int True = 1;
  setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, &True, sizeof(True));
  int b = bind(servSock, (struct sockaddr*)&echoServAddr, sizeof(echoServAddr));
  if (b < 0) die("Could not bind to the socket");

#define MAXPENDING 10
  int l = listen(servSock, MAXPENDING);
  if (l < 0) die("listen failed");

  struct pollfd pollServSock;
  memset(&pollServSock, 0, sizeof(pollServSock));
  pollServSock.fd = servSock;
  pollServSock.events = POLLIN | POLLPRI;
  while (running == 1) {
    clntLen = sizeof(echoClntAddr);
    
    int p = poll(&pollServSock, 1, 1000);

    if (p > 0) {
      int body_size = build_a_recent_list(body, BODY_SIZE-1, &head_str_list2, 1000);
      clntSock = accept(servSock, (struct sockaddr*)&echoClntAddr, &clntLen);
      if (clntSock < 0) die("accept failed");

      handleClntSock(clntSock, body, body_size);
    }
    //printf("Looping\n");
  }

  printf("Exiting the SERVER PTHREAD\n");
  free_str_nodes(head_str_list2);
  free(body);
  return NULL;
}

void die(char* s) {
  printf("%s\n", s);
  pthread_exit(0);
}

void handleClntSock(int clntSock, char* body, size_t body_size) {
#define BUFSIZE (1<<20)
  char* recvBuffer = (char*)malloc(sizeof(char)*BUFSIZE);
  memset(recvBuffer, 0, BUFSIZE);
  int recvMsgSize;

  recvMsgSize = recv(clntSock, recvBuffer, BUFSIZE-1, 0);
  recvBuffer[recvMsgSize] = 0;
  if (recvMsgSize == 0) die("message has zero size"); 

  while (recvMsgSize > 0) {
    //int s = send(clntSock, recvBuffer, recvMsgSize, 0);
    //if (s != recvMsgSize) die("sent not the same nb of bytes");

    recvMsgSize = recv(clntSock, recvBuffer, BUFSIZE-1, 0);
    recvBuffer[recvMsgSize] = 0;
    //if (recvMsgSize < 0) die("recvMsgSize is less than zero");
  }

  send(clntSock, body, body_size+1, 0);
  //if (s != recvMsgSize) die("sent not the same nb of bytes");
  //printf("Closing socket\n");
  close(clntSock);
  free(recvBuffer);
}

size_t build_a_recent_list(char* body, size_t body_n, str_node** str_list, uint64_t milliseconds) {
  if (*str_list == NULL) return 0;
  if (body_n < 256) return 0;

  memset(body, 0, body_n*sizeof(char));
  size_t body_size = 0;

  sprintf(body, "%s,%s\n", "abs_t", model);
  body_size = strlen(body);

  str_node* last_node;
  str_node* curr_node;
  curr_node = *str_list;

  uint64_t now = millis();
  double recent_limit = ((double)(now - milliseconds))/1000.0;
  while (curr_node->next != NULL) {
    last_node = curr_node; 
    curr_node = curr_node->next;
    *str_list = curr_node;
    free(last_node);
    atomicAdd32(&str_list_size2, -1);

    if (curr_node->time > recent_limit) break;
  }

  char** model_fields;
  int model_data_n = parse_CSV(model, &model_fields);

  char temp[512];
  while (curr_node->next != NULL && (body_size + 256) < body_n) {
    curr_node = curr_node->next;
    char** line_fields;
    int line_fields_n = parse_CSV(curr_node->text, &line_fields);
    if (line_fields_n == model_data_n) {
      sprintf(temp, "%f,%s\n", curr_node->time, curr_node->text);
      int temp_len = strlen(temp);
      memcpy(body+body_size, temp, temp_len+1);
      body_size += temp_len;
    }
    free_CSV_fields(line_fields, line_fields_n);
  }
  //printf("%s", body);

  free_CSV_fields(model_fields, model_data_n);
  return body_size;
}
