CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Iinclude
SRC = src/main.c src/pager.c src/table.c src/btree.c src/statement.c src/schema.c
OBJ = $(SRC:.c=.o)
TARGET = db

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f src/*.o $(TARGET)
