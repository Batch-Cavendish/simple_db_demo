CC = gcc
CFLAGS = -std=c23 -Wall -Wextra -Wpedantic -Iinclude
SRC = src/main.c src/pager.c src/table.c src/btree.c src/statement.c src/schema.c
OBJ = $(SRC:.c=.o)
TARGET = db
TEST_TARGET = unit_tests

.PHONY: all clean distclean test

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

test: unit_test golden_test

unit_test: tests/unit_tests.c src/pager.c src/table.c src/btree.c src/statement.c src/schema.c
	$(CC) $(CFLAGS) -o $(TEST_TARGET) $^
	./$(TEST_TARGET)
	rm -f $(TEST_TARGET)

golden_test: $(TARGET)
	cd tests/golden_tests && ./run_golden_tests.sh

run: $(TARGET)
	./$(TARGET) mydb.db

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
