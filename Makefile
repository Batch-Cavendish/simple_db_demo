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

# standard clean: only delete intermediate files
clean:
	rm -f $(OBJ)

# perform a full clean, deleting all generated files
distclean: clean
	rm -f $(TARGET)

# make clean      # quick clean, keep executable (for recompilation)
# make distclean  # full clean (for publishing or testing full build)
