CC = gcc
CFLAGS = -Wall
# 在src目录下查找源文件
SRCS = $(wildcard src/*.c)
# 打印SRCS变量的值
$(info Source files: $(SRCS))
# 生成的目标文件
OBJS = $(SRCS:.c=.o)
HEADERS = $(wildcard include/*.h)
# 打印OBJS变量的值
$(info Object files: $(OBJS))

# 生成的可执行文件依赖于目标文件
all: client
client: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
clean:
	rm -f $(OBJS) client
.PHONY: all clean
# 生成的目标文件依赖于源文件
$(OBJS): %.o: %.c $(HEADERS)
	$(info Compiling: $^)
	$(CC) $(CFLAGS) -c $< -o $@ -I include