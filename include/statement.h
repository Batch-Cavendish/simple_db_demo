#ifndef STATEMENT_H
#define STATEMENT_H

#include "common.h"
#include "database.h"

typedef enum : uint8_t {
  META_COMMAND_SUCCESS,
  META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

typedef enum : uint8_t {
  PREPARE_SUCCESS,
  PREPARE_SYNTAX_ERROR,
  PREPARE_UNRECOGNIZED_STATEMENT,
  PREPARE_NO_SCHEMA,
  PREPARE_NO_TABLE,
  PREPARE_TABLE_ALREADY_EXISTS,
  PREPARE_CATALOG_FULL,
  PREPARE_STRING_TOO_LONG
} PrepareResult;

typedef enum : uint8_t {
  STATEMENT_INSERT,
  STATEMENT_SELECT,
  STATEMENT_DELETE,
  STATEMENT_UPDATE,
  STATEMENT_CREATE_TABLE,
  STATEMENT_BEGIN,
  STATEMENT_COMMIT,
  STATEMENT_ROLLBACK
} StatementType;

typedef enum : uint8_t {
  WHERE_NONE,
  WHERE_EQUALS,
  WHERE_GREATER_THAN,
  WHERE_LESS_THAN
} WhereCondition;

typedef struct Statement {
  StatementType type;
  char table_name[TABLE_NAME_MAX];
  uint32_t table_index; // Looked up during prepare
  uint32_t delete_id;
  uint32_t insert_values[MAX_FIELDS];
  char *insert_strings[MAX_FIELDS];
  Schema new_schema; // For CREATE TABLE
  WhereCondition where_condition;
  uint32_t where_key;
  uint32_t update_key;
  bool update_mask[MAX_FIELDS];
} Statement;

typedef enum : uint8_t {
  EXECUTE_SUCCESS,
  EXECUTE_TABLE_FULL,
  EXECUTE_DUPLICATE_KEY,
  EXECUTE_KEY_NOT_FOUND,
  EXECUTE_UNKNOWN_ERROR
} ExecuteResult;

[[nodiscard]] PrepareResult prepare_statement(char *line, Statement *statement,
                                              Database *db);
[[nodiscard]] ExecuteResult execute_statement(Statement *statement,
                                              Database *db);
void free_statement(Statement *statement);

#endif
