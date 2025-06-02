#include "main.h"




void error_exit(const char *msg, sqlite3 *db)
{
	perror(msg);
	if (db)
	{
		sqlite3_close(db);
	}
	exit(1);
}




void get_data(char *buffer, size_t size)
{
	char *ptr;
	int fd;
	fd = open( "/sys/bus/w1/devices/28-2402000318f9/w1_slave", O_RDONLY);
	read( fd, buffer, size);
	ptr=strstr(buffer,"t=");
	while(1)
	{
		close(fd);
		fd = open( "/sys/bus/w1/devices/28-2402000318f9/w1_slave", O_RDONLY);
		read( fd, buffer, size);
		sleep(1);
		printf("%s\n",buffer);
		printf("温度是：%.3f摄氏度\n", atof(ptr+2)/1000);
	}
	return 0;
}

int main(int argc, char **argv)
{
	int skfd = -1;
	sqlite3 *db = NULL;
	int interval_time = 30;
	struct timespec last, now_ts;
	struct sockaddr_in serveraddr;

	sockaddr_init(&serveraddr);
	if (getoption(argc, argv, &serveraddr, &interval_time) == -1)
	{
		fprintf(stderr, "Failed to parse options. Exiting.\\n");
		exit(1);
	}

	init_db(&db);

	printf("完成参数解析\n");
	printf("Interval time: %d seconds\n", interval_time);
	printf("Server Address: %s, Port: %d\n", inet_ntoa(serveraddr.sin_addr), ntohs(serveraddr.sin_port));

	if ((skfd = socket_connect(serveraddr)) == -1)
	{
		error_exit("initial socket_connect failed", db);
	}
	printf("Connected to server. skfd: %d\n", skfd);

	if ((clock_gettime(CLOCK_MONOTONIC_RAW, &last)) == -1)
	{
		error_exit("clock_gettime (last) failed", db);
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
			if (strstr(data_buf, "ERROR") == NULL)
			{
				store_data_in_db(data_buf);
			}
			else
			{
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
			if (skfd != -1)
			{
				close(skfd);
				skfd = -1;
			}

			printf("Attempting to reconnect...\n");

			skfd = socket_connect(serveraddr);
			if (skfd == -1)
			{
				perror("Reconnect attempt failed");
				printf("Will retry connection later.\n");
				sleep(5);
			}
			else
			{
				printf("Reconnected successfully. New skfd: %d\n", skfd);
			}
		}
		usleep(100000);
	}

	if (db)
	{
		sqlite3_close(db);
	}
	if (skfd != -1)
	{
		close(skfd);
	}
	return 0;
}