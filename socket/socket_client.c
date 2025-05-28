#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#define text "你好李思渝"
int main(int argc, char**argv)
{
	char buf[128];
	int rv = -1;
	int client_fd = -1;
	struct sockaddr_in clientaddr;
	clientaddr.sin_family = AF_INET;
	clientaddr.sin_port = htons(11011);
	inet_aton( "127.0.0.1", (struct in_addr *)&clientaddr.sin_addr);
	printf("111\n");
	if( (client_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		printf("client_fd:%d\n", client_fd);
		return -1;
	}
	printf("client_fd:%d\n", client_fd);
	int opt;
        while((opt = getopt(argc,argv,"ha:p:")) != -1)
        {
                printf("开始参数解析,opt:%c, optarg:%s\n", opt,optarg);                                                                                                                                                              
                switch(opt)                                                                                                                                                                                                          
                {                                                                                                                                                                                                                    
                        case 'h':                                                                                                                                                                                                    
                                printf("输入样例：\n%s -p [port] -a [address]\n", argv[0]);                                                                                                                                          
                                return -3;                                                                                                                                                                                           
                        case 'p':                                                                                                                                                                                                    
                                clientaddr.sin_port = htons(atoi(optarg));                                                                                                                                                           
                                printf("%d\n", atoi(optarg));                                                                                                                                                                        
                                break;                                                                                                                                                                                               
                        case 'a':                                                                                                                                                                                                    
                                clientaddr.sin_addr.s_addr = inet_addr(optarg);                                                                                                                                                      
                                break;                                                                                                                                                                                               
                        case '?':                                                                                                                                                                                                    
                                printf("输入样例：\n%s -p [port] -a [address]\n", argv[0]);                                                                                                                                          
                                return -1;                                                                                                                                                                                           
                        case ':':                                                                                                                                                                                                    
                                printf("输入样例：\n%s -p [port] -a [address]\n", argv[0]);                                                                                                                                          
                                return -2;                                                                                                                                                                                           
                }                                                                                                                                                                                                                    
        }                                                                                                                                                                                                                            

	if( (connect(client_fd, (struct sockaddr *)&clientaddr, sizeof(clientaddr))) == -1)
	{
		perror("connect");
		return -2;
	}
	printf("client_fd: %d\n", client_fd);
	memset((void *)buf, 0, sizeof(buf));
	if( (write(client_fd, text, sizeof(text))) == -1)
	{
		perror("write");
		return -3;
	}
	printf("%d,111\n",client_fd);
	if( (read(client_fd, buf, sizeof(text))) <= 0)
	{
		perror("read failure or disconnect");
		return -4;
	}
	printf("text from server: %s\n", buf);
	while (1)
	{
		// 使用getsockopt检测socket连接状态
		int optval;
		socklen_t optlen = sizeof(optval);
		if (getsockopt(client_fd, SOL_SOCKET, SO_ERROR, &optval, &optlen) == -1)
		{
			perror("getsockopt failed");
			return -5;
		}
		if (optval != 0)
		{
			printf("Socket error: %s\n", strerror(optval));
			break;
		}
		printf("Socket is still connected.\n");
		sleep(1); // 每秒检查一次
	}
	close(client_fd);
	return 0;
}
