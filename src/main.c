#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btree.h"
#include "database.h"
#include "statement.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Must supply a database filename.\n");
    exit(EXIT_FAILURE);
  }

  Database *db = db_open(argv[1]);
  char line[1024];

  while (printf("db > "), fgets(line, 1024, stdin)) {
    line[strcspn(line, "\n")] = 0;
    line[strcspn(line, "\r")] = 0; // Handle Windows-style line endings

    // Meta-commands
    if (line[0] == '.') {
      if (strcmp(line, ".exit") == 0) {
        break;
      }
      if (strcmp(line, ".tables") == 0) {
        for (uint32_t i = 0; i < db->catalog.num_tables; i++) {
          printf("%s (%u columns)\n", db->catalog.tables[i].name,
                 db->catalog.tables[i].schema.num_fields);
        }
        continue;
      }
      if (strncmp(line, ".check ", 7) == 0) {
        char *table_name = line + 7;
        int idx = find_table(db, table_name);
        if (idx == -1) {
          printf("Error: Table not found.\n");
        } else {
          if (verify_btree(db, idx)) {
            printf("B-Tree integrity: OK\n");
          } else {
            printf("B-Tree integrity: CORRUPT\n");
          }
        }
        continue;
      }
      printf("Unrecognized meta-command '%s'\n", line);
      continue;
    }

    Statement statement = {};
    PrepareResult prepare_result = prepare_statement(line, &statement, db);

    switch (prepare_result) {
    case PREPARE_SUCCESS: {
      ExecuteResult execute_result = execute_statement(&statement, db);
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
      break;
    }
    case PREPARE_SYNTAX_ERROR:
      printf("Syntax error. Could not parse statement.\n");
      break;
    case PREPARE_UNRECOGNIZED_STATEMENT:
      printf("Unrecognized keyword at start of '%s'.\n", line);
      break;
    case PREPARE_NO_SCHEMA:
      printf("Error: No table created. Use CREATE TABLE first.\n");
      break;
    case PREPARE_NO_TABLE:
      printf("Error: Table not found.\n");
      break;
    case PREPARE_TABLE_ALREADY_EXISTS:
      printf("Error: Table already exists.\n");
      break;
    case PREPARE_CATALOG_FULL:
      printf("Error: Catalog full. Cannot create more tables.\n");
      break;
    case PREPARE_STRING_TOO_LONG:
      printf("Error: String value too long.\n");
      break;
    }

    free_statement(&statement);
  }

  db_close(db);
  return 0;
}
