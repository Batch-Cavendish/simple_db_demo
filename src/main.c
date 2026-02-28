#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "statement.h"
#include "table.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Must supply a database filename.\n");
    exit(EXIT_FAILURE);
  }

  Table *t = db_open(argv[1]);
  char line[1024];

  while (printf("db > "), fgets(line, 1024, stdin)) {
    line[strcspn(line, "\n")] = 0;
    line[strcspn(line, "\r")] = 0; // Handle Windows-style line endings

    // Meta-commands
    if (line[0] == '.') {
      if (strcmp(line, ".exit") == 0) {
        break;
      }
      printf("Unrecognized meta-command '%s'\n", line);
      continue;
    }

    Statement statement = {};
    PrepareResult prepare_result = prepare_statement(line, &statement, t);

    switch (prepare_result) {
    case PREPARE_SUCCESS:
      break;
    case PREPARE_SYNTAX_ERROR:
      printf("Syntax error. Could not parse statement.\n");
      continue;
    case PREPARE_UNRECOGNIZED_STATEMENT:
      printf("Unrecognized keyword at start of '%s'.\n", line);
      continue;
    case PREPARE_NO_SCHEMA:
      printf("Error: No table created. Use CREATE TABLE first.\n");
      continue;
    case PREPARE_TABLE_ALREADY_EXISTS:
      printf("Error: Table already exists.\n");
      continue;
    case PREPARE_STRING_TOO_LONG:
      printf("Error: String value too long.\n");
      continue;
    }

    ExecuteResult execute_result = execute_statement(&statement, t);
    switch (execute_result) {
    case EXECUTE_SUCCESS:
      break;
    case EXECUTE_TABLE_FULL:
      printf("Error: Table full.\n");
      break;
    case EXECUTE_DUPLICATE_KEY:
      printf("Error: Duplicate key.\n");
      break;
    case EXECUTE_KEY_NOT_FOUND:
      printf("Error: Key not found.\n");
      break;
    case EXECUTE_UNKNOWN_ERROR:
      printf("Unknown error.\n");
      break;
    }
  }

  db_close(t);
  return 0;
}
