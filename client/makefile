CC = gcc
CFLAGS = -Wall -Wextra -O2
# 在src目录下查找源文件
SRCS = $(wildcard src/*.c)
# 生成的目标文件
OBJS = $(SRCS:.c=.o)
all: client
client: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
