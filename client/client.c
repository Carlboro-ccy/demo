#include <sys/socket.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <bits/getopt_core.h>
#include <linux/time.h>

void sockaddr_init(struct sockaddr_in *addr)
{
  memset(addr, 0, sizeof(addr));
  addr->sin_family = AF_INET;
  addr->sin_port = htons(0);
  addr->sin_addr.s_addr = inet_addr("127.0.0.1");
}

int is_connected(int fd)
{
  char buf[1] = {0};
  ssize_t rv = -1;
  if ((rv = recv(fd, buf, 1, MSG_PEEK | MSG_DONTWAIT)) == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
  {
    printf("socket is closed\n");
    return 0;
  }
  if (send(fd, NULL, 0, MSG_NOSIGNAL | MSG_DONTWAIT) == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
  {
    printf("socket is closed\n");
    return 0;
  }
  return 1;
}

void error_exit()
{
  exit(1);
}

int getoption(int argc, char **argv, struct sockaddr_in *addr, int *time)
{
  int opt;
  while ((opt = getopt(argc, argv, "p:a:ht:")) != -1)
  {
    printf("opt: %c\n", opt);
    switch (opt)
    {
    case 'p':
      addr->sin_port = htons(atoi(optarg));
      break;
    case 'a':
      addr->sin_addr.s_addr = inet_addr(optarg);
      break;
    case 't':
      *time = atoi(optarg);
      break;
    case '?':
    case ':':
      printf("client -p [port] -a [address]\n");
      return -1;
    case 'h':
      printf("client -p [port] -a [address]\n");
      return -1;
    }
  }
  return 0;
}

int socket_connect(struct sockaddr_in serveraddr)
{
  int fd = -1;
  int flag = -1;
  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror("socket failure");
    return -1;
  }
  flag = fcntl(fd, F_GETFL, 0);
  if (flag == -1)
  {
    perror("fcntl failed");
    close(fd);
    return -1;
  }
/*  if (connect(fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)))
  {
    perror("connect failed");
    close(fd);
    return -1;
  }
  return fd;
*/
  if (fcntl(fd, F_SETFL, flag | O_NONBLOCK) == -1)
  {
    perror("fcntl failed");
    close(fd);
    return -1;
  }
  if (connect(fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1 && errno != EISCONN)
  {
    if (errno != EINPROGRESS)
    {
      perror("connect failed");
      close(fd);
      return -1;
    }
    else
    {
      perror("connect failed");
      return -1;
    }
  }
  
}

void get_data(char *buffer, size_t size)
{
  char *ptr;
  int id = 1;
  int fd;
  char buf[128] = {0};
  float temp;
  fd = open("/sys/bus/w1/devices/28-2402000318f9/w1_slave", O_RDONLY);
  read(fd, buf, sizeof(buf));
  ptr = strstr(buf, "t=");
  temp = (atof(ptr + 2) / 1000);
  time_t now = time(NULL);
  struct tm tm_info;
  localtime_r(&now, &tm_info);
  char time_string[128];
  strftime(time_string, sizeof(time_string), "%c", &tm_info);
  snprintf(buffer, size, "[id: %02d | %s] tempreture: %.3f", id, time_string, temp);
}

int main(int argc, char **argv)
{
  int skfd = -1;
  int rv = -1;
  int time = 30;
  struct timespec last, now;
  struct sockaddr_in serveraddr;
  sockaddr_init(&serveraddr);
  getoption(argc, argv, &serveraddr, &time);
  printf("完成参数解析\n");
  printf("time: %d\n", time);
  if ((skfd = socket_connect(serveraddr)) == -1)
  {
    perror("socket connect failed");
    error_exit();
  }
  printf("skfd: %d\n", skfd);
  if ((clock_gettime(CLOCK_MONOTONIC_RAW, &last)) == -1)
  {
    perror("clock failed");
    error_exit();
  }
  while (1)
  {
    int rv;
    char buf[128] = {0};
    if ((clock_gettime(CLOCK_MONOTONIC_RAW, &now)) == -1)
    {
      perror("clock failed");
      error_exit();
    }

    if (((now.tv_sec - last.tv_sec) * 1000000000L + (now.tv_nsec - last.tv_nsec)) >= time * 1000000000L)
    {
      get_data(buf, sizeof(buf));
      printf("getdata:%s\n", buf);
      last = now;
    }
    rv = is_connected(skfd);
    if (rv == 1)
    {
      if (write(skfd, buf, sizeof(buf)) == -1)
      {
        perror("write failed");
        close(skfd);
        error_exit();
      }
      if (is_dbdata())
      {
        send_dbdata();
      }
    }
    else
    {
      printf("connection closed\n");
      close(skfd);
      skfd = -1;
      printf("reconnect\n");
      if ((skfd = socket_connect(serveraddr)) == -1)
      {
        continue;
      }
    }
    return 0;
  }
}