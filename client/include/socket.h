#include <sys/socket.h>      // socket(), connect(), sockaddr_in, AF_INET, SOCK_STREAM
#include <netinet/in.h>      // sockaddr_in, htons(), IPPROTO_TCP
#include <netinet/tcp.h>     // TCP_INFO, tcp_info, TCP_ESTABLISHED
#include <arpa/inet.h>       // inet_addr()
#include <unistd.h>          // close()
#include <fcntl.h>           // fcntl(), O_NONBLOCK, F_GETFL, F_SETFL
#include <string.h>          // memset()
#include <stdio.h>           // perror()
#include <errno.h>           // errno, EINPROGRESS, EISCONN

void sockaddr_init(struct sockaddr_in *addr);

int socket_connect(struct sockaddr_in serveraddr);

int socket_connect_noblock(struct sockaddr_in serveraddr);

int is_connected(int fd);