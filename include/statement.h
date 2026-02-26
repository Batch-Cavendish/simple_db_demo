#ifndef STATEMENT_H
#define STATEMENT_H

#include "common.h"
#include "table.h"

typedef enum : uint8_t {
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

typedef enum : uint8_t {
    PREPARE_SUCCESS,
    PREPARE_SYNTAX_ERROR,
    PREPARE_UNRECOGNIZED_STATEMENT,
    PREPARE_NO_SCHEMA,
    PREPARE_TABLE_ALREADY_EXISTS,
    PREPARE_STRING_TOO_LONG
} PrepareResult;

typedef enum : uint8_t {
    STATEMENT_INSERT,
    STATEMENT_SELECT,
    STATEMENT_DELETE,
    STATEMENT_UPDATE,
    STATEMENT_CREATE_TABLE
} StatementType;

typedef struct Statement {
    StatementType type;
    uint32_t delete_id;
    uint32_t insert_values[MAX_FIELDS];
    char *insert_strings[MAX_FIELDS];
    Schema new_schema; // For CREATE TABLE
    bool select_whole_table;
    uint32_t select_key;
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

[[nodiscard]] PrepareResult prepare_statement(char *line, Statement *statement, Table *t);
[[nodiscard]] ExecuteResult execute_statement(Statement *statement, Table *t);

#endif
