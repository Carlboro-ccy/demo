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
#include "../../sqlite3.h"

sqlite3 *db = NULL;
const char *db_file = "client_local_data.db";

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
  if (recv(fd, buf, 1, MSG_PEEK | MSG_DONTWAIT) == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
  {
    perror("is_connected (recv)");
    return 0;
  }
  if (send(fd, NULL, 0, MSG_NOSIGNAL | MSG_DONTWAIT) == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
  {
    perror("is_connected (send)");
    return 0;
  }
  return 1;
}

void error_exit(const char *msg)
{
  perror(msg);
  if (db) {
    sqlite3_close(db);
  }
  exit(1);
}

void init_db() {
    int rc = sqlite3_open(db_file, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        db = NULL;
        error_exit("sqlite3_open");
    }

    const char *sql_create_table = 
        "CREATE TABLE IF NOT EXISTS pending_data ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "message TEXT NOT NULL, "
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";
    
    char *err_msg = 0;
    rc = sqlite3_exec(db, sql_create_table, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create table: %s\\n", err_msg);
        sqlite3_free(err_msg);
        error_exit("sqlite3_exec create table");
    }
    printf("Database initialized and table 'pending_data' is ready.\\n");
}

void store_data_in_db(const char *message) {
    if (!db) {
        fprintf(stderr, "Database not initialized. Cannot store data.\\n");
        return;
    }
    sqlite3_stmt *stmt;
    const char *sql_insert = "INSERT INTO pending_data (message) VALUES (?);";
    int rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\\n", sqlite3_errmsg(db));
        return;
    }
    sqlite3_bind_text(stmt, 1, message, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute statement: %s\\n", sqlite3_errmsg(db));
    } else {
        printf("Data stored in DB: %s\\n", message);
    }
    sqlite3_finalize(stmt);
}

int is_dbdata(void) {
    if (!db) {
        fprintf(stderr, "Database not initialized. Cannot check for data.\\n");
        return 0;
    }
    sqlite3_stmt *stmt;
    const char *sql_count = "SELECT COUNT(*) FROM pending_data;";
    int count = 0;
    int rc = sqlite3_prepare_v2(db, sql_count, -1, &stmt, 0);
    if (rc == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
    } else {
        fprintf(stderr, "Failed to prepare count statement: %s\\n", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
    return count > 0;
}

void send_dbdata(int skfd) {
    if (!db) {
        fprintf(stderr, "Database not initialized. Cannot send data.\\n");
        return;
    }
    sqlite3_stmt *stmt_select;
    const char *sql_select = "SELECT id, message FROM pending_data ORDER BY id ASC LIMIT 1;";
    long data_id = -1;
    const unsigned char *message = NULL;

    int rc = sqlite3_prepare_v2(db, sql_select, -1, &stmt_select, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare select statement: %s\\n", sqlite3_errmsg(db));
        return;
    }

    if (sqlite3_step(stmt_select) == SQLITE_ROW) {
        data_id = sqlite3_column_int(stmt_select, 0);
        message = sqlite3_column_text(stmt_select, 1);

        if (message) {
            printf("Sending DB data (ID: %ld): %s\\n", data_id, message);
            ssize_t written_bytes = write(skfd, message, strlen((const char*)message));
            if (written_bytes == -1) {
                perror("send_dbdata: write failed");
            } else if (written_bytes < (ssize_t)strlen((const char*)message)) {
                fprintf(stderr, "send_dbdata: partial write, sent %zd of %zu bytes\\n", written_bytes, strlen((const char*)message));
            } else {
                printf("DB data (ID: %ld) sent successfully.\\n", data_id);
                sqlite3_stmt *stmt_delete;
                const char *sql_delete = "DELETE FROM pending_data WHERE id = ?;";
                rc = sqlite3_prepare_v2(db, sql_delete, -1, &stmt_delete, 0);
                if (rc == SQLITE_OK) {
                    sqlite3_bind_int(stmt_delete, 1, data_id);
                    if (sqlite3_step(stmt_delete) != SQLITE_DONE) {
                        fprintf(stderr, "Failed to delete record (ID: %ld): %s\\n", data_id, sqlite3_errmsg(db));
                    } else {
                        printf("Record (ID: %ld) deleted from DB.\\n", data_id);
                    }
                    sqlite3_finalize(stmt_delete);
                } else {
                    fprintf(stderr, "Failed to prepare delete statement: %s\\n", sqlite3_errmsg(db));
                }
            }
        }
    } else {
         if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
            fprintf(stderr, "Failed to step select statement: %s\\n", sqlite3_errmsg(db));
        }
    }
    sqlite3_finalize(stmt_select);
}


int getoption(int argc, char **argv, struct sockaddr_in *addr, int *interval_time)
{
  int opt;
  char *endptr;
  long val;

  while ((opt = getopt(argc, argv, "p:a:ht:")) != -1)
  {
    printf("opt: %c\\n", opt);
    switch (opt)
    {
    case 'p':
      val = strtol(optarg, &endptr, 10);
      if (*endptr != '\\0' || val <= 0 || val > 65535) {
        fprintf(stderr, "Invalid port number: %s\\n", optarg);
        return -1;
      }
      addr->sin_port = htons((uint16_t)val);
      break;
    case 'a':
      if (inet_addr(optarg) == INADDR_NONE) {
          fprintf(stderr, "Invalid address: %s\\n", optarg);
          return -1;
      }
      addr->sin_addr.s_addr = inet_addr(optarg);
      break;
    case 't':
      val = strtol(optarg, &endptr, 10);
      if (*endptr != '\\0' || val <= 0) {
        fprintf(stderr, "Invalid time interval: %s\\n", optarg);
        return -1;
      }
      *interval_time = (int)val;
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
  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror("socket failure");
    return -1;
  }

  if (connect(fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1)
  {
    perror("connect failed");
    close(fd);
    return -1;
  }
  return fd;
}

int socket_connect_noblock(struct sockaddr_in serveraddr)
{

  int fd = -1;
  int flags = -1;
  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror("socket failure");
    return -1;
  }

  // 获取当前sockopt
  flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
  {
    perror("fcntl F_GETFL failed");
    close(fd);
    return -1;
  }

  // 设置非阻塞
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
  {
    perror("fcntl F_SETFL O_NONBLOCK failed");
    close(fd);
    return -1;
  }

  int ret = connect(fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
  if (ret == 0) // 立即成功（几率不大，常见于使用环回地址通信）
  {
    if (fcntl(fd, F_SETFL, flags) == -1)
    {
      perror("fcntl F_SETFL failed to restore flags");
      close(fd);
      return -1;
    }
    return fd;
  }
  else if (errno == EINPROGRESS)
  {
    return fd;
  }
  else if (errno == EISCONN)
  {
    // socket已连接
    if (fcntl(fd, F_SETFL, flags) == -1)
    {
      perror("fcntl F_SETFL failed");
      close(fd);
      return -1;
    }
    return fd;
  }
  else
  {
    // 其他连接错误
    perror("connect failed (non-blocking)");
    close(fd);
    return -1;
  }
}


void get_data(char *buffer, size_t size)
{
  char *ptr;
  int id = 1;
  int sensor_fd;
  char sensor_buf[256] = {0};
  float temp;

  sensor_fd = open("/sys/bus/w1/devices/28-2402000318f9/w1_slave", O_RDONLY);
  if (sensor_fd == -1) {
      perror("get_data: open w1_slave failed");
      snprintf(buffer, size, "[id: %02d | ERROR] failed to open sensor", id);
      return;
  }

  ssize_t bytes_read = read(sensor_fd, sensor_buf, sizeof(sensor_buf) -1);
  close(sensor_fd);

  if (bytes_read <= 0) {
      perror("get_data: read w1_slave failed or empty");
      snprintf(buffer, size, "[id: %02d | ERROR] failed to read sensor", id);
      return;
  }
  sensor_buf[bytes_read] = '\\0';

  ptr = strstr(sensor_buf, "t=");
  if (ptr == NULL) {
      fprintf(stderr, "get_data: Could not find 't=' in sensor data: %s\\n", sensor_buf);
      snprintf(buffer, size, "[id: %02d | ERROR] invalid sensor data format", id);
      return;
  }

  temp = (atof(ptr + 2) / 1000.0); 

  time_t current_time_val;
  struct tm tm_info;
  
  if (time(&current_time_val) == (time_t)-1) {
      perror("get_data: time() failed");
      snprintf(buffer, size, "[id: %02d | ERROR] failed to get current time", id);
      return;
  }
  
  if (localtime_r(&current_time_val, &tm_info) == NULL) {
      perror("get_data: localtime_r failed");
      snprintf(buffer, size, "[id: %02d | ERROR] failed to convert time", id);
      return;
  }
  
  char time_string[128];
  strftime(time_string, sizeof(time_string), "%c", &tm_info); 
  snprintf(buffer, size, "[id: %02d | %s] temperature: %.3f", id, time_string, temp);
}

int main(int argc, char **argv)
{
  int skfd = -1;
  int interval_time = 30; 
  struct timespec last, now_ts;
  struct sockaddr_in serveraddr;

  sockaddr_init(&serveraddr);
  if (getoption(argc, argv, &serveraddr, &interval_time) == -1) {
      fprintf(stderr, "Failed to parse options. Exiting.\\n");
      exit(1);
  }
  
  init_db();

  printf("完成参数解析\\n");
  printf("Interval time: %d seconds\\n", interval_time);
  printf("Server Address: %s, Port: %d\\n", inet_ntoa(serveraddr.sin_addr), ntohs(serveraddr.sin_port));


  if ((skfd = socket_connect(serveraddr)) == -1)
  {
    error_exit("initial socket_connect failed");
  }
  printf("Connected to server. skfd: %d\\n", skfd);

  if ((clock_gettime(CLOCK_MONOTONIC_RAW, &last)) == -1)
  {
    error_exit("clock_gettime (last) failed");
  }

  while (1)
  {
    int connection_status;
    char data_buf[256] = {0};

    if ((clock_gettime(CLOCK_MONOTONIC_RAW, &now_ts)) == -1)
    {
      perror("clock_gettime (now_ts) failed in loop");
      sleep(1);
      continue;
    }

    if (((now_ts.tv_sec - last.tv_sec) * 1000000000L + (now_ts.tv_nsec - last.tv_nsec)) >= (long)interval_time * 1000000000L)
    {
      get_data(data_buf, sizeof(data_buf));
      printf("getdata raw: %s\\n", data_buf);
      if (strstr(data_buf, "ERROR") == NULL) {
          store_data_in_db(data_buf);
      } else {
          printf("Skipping storage of erroneous data: %s\\n", data_buf);
      }
      last = now_ts;
    }

    connection_status = is_connected(skfd);
    if (connection_status == 1)
    {
      if (is_dbdata())
      {
        send_dbdata(skfd);
      }
    }
    else
    {
      printf("Connection lost or skfd invalid (%d).\\n", skfd);
      if (skfd != -1) {
          close(skfd);
          skfd = -1;
      }
      
      printf("Attempting to reconnect...\\n");

      skfd = socket_connect(serveraddr);
      if (skfd == -1) {
        perror("Reconnect attempt failed");
        printf("Will retry connection later.\\n");
        sleep(5); 
      } else {
        printf("Reconnected successfully. New skfd: %d\\n", skfd);
      }
    }
    usleep(100000);
  }

  if (db) {
    sqlite3_close(db);
  }
  if (skfd != -1) {
      close(skfd);
  }
  return 0;
}