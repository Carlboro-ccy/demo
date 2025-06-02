#include <sqlite3.h>        // SQLite 数据库操作
#include <stdio.h>           // fprintf(), printf(), perror()
#include <stdlib.h>          // 可能用于 exit() 函数 (error_exit 实现中)
#include <string.h>          // strlen()
#include <unistd.h>          // write()
#include <errno.h>           // perror() 可能使用的 errno

int init_db(sqlite3 **db);

void store_data_in_db(const char *message, sqlite3 *db);

int is_dbdata(sqlite3 *db);

void send_dbdata(int skfd, sqlite3 *db);