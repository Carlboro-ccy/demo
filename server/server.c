#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/select.h>
#include "sqlite3.h"

int db_init()
{
    sqlite3 *db;
    char *err_msg = 0;
    int rc = sqlite3_open("temp.db", &db);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }
    const char *sql = "CREATE TABLE IF NOT EXISTS temp (id INTEGER PRIMARY KEY AUTOINCREMENT,message TEXT);";
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return -1;
    }
    return db;
}
int db_save(sqlite3 *db, char *message)
{
    char *err_msg = 0;
    char sql[256];
    snprintf(sql, sizeof(sql), "INSERT INTO temp (message) VALUES ('%s');", message);
    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }
    return 0;
}

int getoption(int argc, char **argv, struct sockaddr_in *addr)
{
    int opt;
    
    while ((opt = getopt(argc, argv, "p:a:h")) != -1)
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

int main(int argc, char **argv)
{
    int skfd = -1;
    int fds[1024];
    int maxfd = -1;
    int opt = 1;
    sqlite3 *db = db_init();
    if (db == NULL)
    {
        fprintf(stderr, "Failed to initialize database\n");
        return -1;
    }
    for (int i = 0; i < 1024; i++)
    {
        fds[i] = -1;
    }
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(0);
    serveraddr.sin_addr.s_addr = INADDR_ANY;
    if (getoption(argc, argv, &serveraddr) == -1)
    {
        perror("getoption failed");
        return -1;
    }
    if (skfd = socket(AF_INET, SOCK_STREAM, 0) == -1)
    {
        perror("socket failed");
        return -1;
    }
    if (setsockopt(skfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        perror("setsockopt failed");
        close(skfd);
        return -1;
    }
    if (bind(skfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1)
    {
        perror("bind failed");
        close(skfd);
        return -1;
    }
    if (listen(skfd, 5) == -1)
    {
        perror("listen failed");
        close(skfd);
        return -1;
    }
    printf("socket created and listening on port %d\n", ntohs(serveraddr.sin_port));
    fds[0] = skfd;
    fd_set readfds;
    while (1)
    {
        printf("开始select\n");
        FD_ZERO(&readfds);
        for (int i = 0; i < 1024; i++)
        {
            if (fds[i] == -1)
                continue;
            else
            {
                FD_SET(fds[i], &readfds);
                maxfd = maxfd > fds[i] ? maxfd : fds[i];
            }
        }
        printf("maxfd:%d\n", maxfd);
        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) == -1)
        {
            perror("select failed");
            close(skfd);
            return -1;
        }
        for (int i = 0; i < 1024; i++)
        {
            if (FD_ISSET(fds[i], &readfds))
            {
                if (i == 0)
                {
                    int clientfd = accept(skfd, NULL, NULL);
                    if (clientfd == -1)
                    {
                        perror("accept failed");
                        continue;
                    }
                    printf("Accepted new connection: %d\n", clientfd);
                    for (int j = 1; j < 1024; j++)
                    {
                        if (fds[j] == -1)
                        {
                            fds[j] = clientfd;
                            break;
                        }
                    }
                }
                else
                {
                    char buf[1024];
                    ssize_t bytes_read = read(fds[i], buf, sizeof(buf));
                    if (bytes_read <= 0)
                    {
                        close(fds[i]);
                        fds[i] = -1;
                        printf("Connection closed: %d\n", fds[i]);
                    }
                    else
                    {
                        printf("Received from fd: %d: %s\n", fds[i], buf);
                        if (db_save(db, buf) == -1)
                        {
                            printf("Failed to save data to database\n");
                            sqlite3_close(db);
                            return -1;
                        }
                        printf("Echoed back to %d: %s\n", fds[i], buf);
                    }
                }
            }
        }
    }
}