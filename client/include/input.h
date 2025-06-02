#include <stdlib.h>  // strtol()
#include <stdio.h>   // printf()/fprintf()
#include <unistd.h>  // getopt()
#include <arpa/inet.h> // inet_addr()

int getoption(int argc, char **argv, struct sockaddr_in *addr, int *interval_time);