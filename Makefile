CC = gcc
CFLAGS = -std=c23 -Wall -Wextra -Wpedantic -Iinclude
SRC = src/main.c src/pager.c src/table.c src/btree.c src/statement.c src/schema.c
OBJ = $(SRC:.c=.o)
TARGET = db

.PHONY: all clean distclean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# 标准清理：只删除中间文件
clean:
	rm -f $(OBJ)

# 彻底清理：删除所有生成文件
distclean: clean
	rm -f $(TARGET)

# make clean      # 快速清理，保留可执行文件（用于重新编译）
# make distclean  # 完全清理（用于发布或测试完整构建）
