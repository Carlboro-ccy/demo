#include <sys/types.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#define PORT 11011
int main(int argc, char **argv)
{
	int opt;
	int clientfd,skfd = -1;
	struct sockaddr_in clientaddr;
	socklen_t clientaddr_len;
	struct sockaddr_in serveraddr;
	skfd = socket(AF_INET, SOCK_STREAM, 0);
	if (skfd == -1)
	{
		perror("socket");
		return -1;
	}
	printf("skfd= %d\n", skfd);
	memset( &serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
//	serveraddr.sin_port = htons(PORT);
//	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	while((opt = getopt(argc,argv,"ha:p:")) != -1)
	{
		printf("开始参数解析,opt:%c, optarg:%s\n", opt,optarg);
		switch(opt)
		{
			case 'h':
				printf("输入样例：\n%s -p [port] -a [address]\n", argv[0]);
				return -3;
			case 'p':
				serveraddr.sin_port = htons(atoi(optarg));
				printf("%d\n", atoi(optarg));
				break;
			case 'a':
				serveraddr.sin_addr.s_addr = inet_addr(optarg);
				break;
			case '?':
				printf("输入样例：\n%s -p [port] -a [address]\n", argv[0]);
				return -1;
			case ':':
				printf("输入样例：\n%s -p [port] -a [address]\n", argv[0]);
				return -2;
		}
	}
	if (bind(skfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1)
	{
		perror("bind");
		printf("failure:%s\n",strerror(errno));
		return -2;
	}
	printf("socket %d bind on port %d \n", skfd, ntohs(serveraddr.sin_port));
	if (listen(skfd, 10) == -1)
	{
		perror("listen failure:");
		return -3;
	}
	printf("开启监听状态\n");
	while(1)
	{
		int rv = -1;
		char buf[128];
		memset(buf, 0, sizeof(buf));
		clientaddr_len = sizeof(clientaddr);
		printf("进入循环\n");
		if ((clientfd = accept(skfd, (struct sockaddr *)&clientaddr, &clientaddr_len)) == -1)
		{
			perror("accept failure");
			return -4;
		}
		printf("new fd:%d\n", clientfd);
		if ((rv = read(clientfd, buf, sizeof(buf))) == -1)
		{
			perror("read failure");
			close(clientfd);
			continue;
		}
		if (rv == 0)
		{
			printf("socket disconnect");
			close(clientfd);
			continue;
		}
		if( write(clientfd, buf, rv) == -1)
		{
			perror("write");
			close(clientfd);
			continue;
		}
		sleep(1);
		printf("服务器收到信息：%s\n", buf);
		close(clientfd);
	}
	close(skfd);
	return 0;
}
