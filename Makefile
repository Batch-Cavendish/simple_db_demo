CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Iinclude
SRC = src/main.c
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
