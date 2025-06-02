#include "socket.h"

void sockaddr_init(struct sockaddr_in *addr)
{
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(0);
    addr->sin_addr.s_addr = inet_addr("127.0.0.1");
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

int is_connected(int fd)
{
	// 检查文件描述符有效性
	if (fd < 0)
	{
		return -1;
	}

	// 尝试获取 TCP 信息
	struct tcp_info info;
	socklen_t len = sizeof(info);

	if (getsockopt(fd, IPPROTO_TCP, TCP_INFO, &info, &len) < 0)
	{
		// 获取失败通常意味着连接已关闭
		return 0;
	}

	// 检查 TCP 状态
	if (info.tcpi_state == TCP_ESTABLISHED)
	{
		return 1;
	}
	return 0;
}