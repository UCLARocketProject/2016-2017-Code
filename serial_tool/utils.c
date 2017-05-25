#include "serial_handler.h"
#include "utils.h"

int fd = -1;

int open_port(char* serial_path, int speed) {
  fd = open(serial_path, O_RDWR | O_NOCTTY | O_NDELAY);
  if (fd == -1) {
    if (verbose) printf("Could not have opened the port: %s!\n", serial_path);
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

size_t parse(char* buffer, size_t buff_len, str_node** str_list, str_node** str_list2) {
  size_t i, j;
  char line[M];
  for (i = 0, j = 0; i < buff_len; i++) {
    if (buffer[i] == '\n') {
      memcpy(line, buffer+j, i-j);
      line[i-j] = 0;
      if (i-j > 0) {
        double curr_time = ((double)micros())/1000000.0;
        *str_list = add_str_node(*str_list, line, curr_time);
        //str_list_size++;
        atomicAdd32(&str_list_size, 1);
        *str_list2 = add_str_node(*str_list2, line, curr_time);
        //str_list_size2++;
        atomicAdd32(&str_list_size2, 1);
      }
      j = i + 1;
    }
  }
  return j;
}

void* obtain_data(void* arg) {
  signal(SIGUSR1, ignore_sigusr1);
  //str_node* tail_str_list = (str_node*)arg;

  /* start serial reception */
  fd = open_port(serial_path, serial_speed);

  char line[M];
  char buffer[N<<2];
  size_t buff_len = 0;
  line[0] = 0;
  uint64_t bytes_written = 0;
  //unsigned writing_occurred = 0;
  while (running) {
    /* attempt to open socket when closed */
    //char socket_neg = ((fd == -1) ? 1 : 0);
    //char write_wrong = ((write(fd, "", 0) < 0) ? 1 : 0);
    char socket_neg = 0;
    char write_wrong = 0;
    if (fd == -1) socket_neg = 1;
    else socket_neg = 0;
    //if (write(fd, "", 0) < 0) write_wrong = 1;
    //else write_wrong = 0;

    if (socket_neg) { //socket is closed 
      close(fd);
      fd = open_port(serial_path, serial_speed);
      if (verbose) {
        if (socket_neg) printf("Socket positive test failed\n");
        if (write_wrong) printf("Zero write test failed\n");
        printf("##Re-opening port\n");
      }   
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
        int j = parse(buffer, buff_len, &tail_str_list, &tail_str_list2);
        memmove(buffer, buffer+j, buff_len - j);
        buff_len = buff_len - j;
        buffer[buff_len] = 0;
        bytes_written += j;

        line[read_bytes] = 0;
        //if (verbose) printf("%s", line);
      } else if (read_bytes == 0) {
        // WEIRD CASE
      } else {
        // NO DATA CASE 
      }
    }
  }
  printf("EXITING SERIAL OBTAINING THREAD\n");
  return NULL;
}

void ignore_sigusr1(int signum) {
  return;
}
