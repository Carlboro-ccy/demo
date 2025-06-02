#include "input.h"

int getoption(int argc, char **argv, struct sockaddr_in *addr, int *interval_time)
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
			*interval_time = atoi(optarg);
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