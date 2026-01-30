#ifndef STATEMENT_H
#define STATEMENT_H

#include "common.h"
#include "table.h"

typedef enum {
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

typedef enum {
    PREPARE_SUCCESS,
    PREPARE_SYNTAX_ERROR,
    PREPARE_UNRECOGNIZED_STATEMENT
} PrepareResult;

typedef enum {
    STATEMENT_INSERT,
    STATEMENT_SELECT,
    STATEMENT_DELETE,
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
} Statement;

typedef enum {
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL,
    EXECUTE_DUPLICATE_KEY,
    EXECUTE_UNKNOWN_ERROR
} ExecuteResult;

PrepareResult prepare_statement(char *line, Statement *statement, Schema *schema);
ExecuteResult execute_statement(Statement *statement, Table *t);

#endif
