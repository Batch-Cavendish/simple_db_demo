#include "statement.h"
#include "btree.h"
#include "schema.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static PrepareResult prepare_insert(char *line, Statement *statement, Schema *schema) {
    statement->type = STATEMENT_INSERT;
    char *token = strtok(line, " "); // Skip "insert" keyword
    token = strtok(nullptr, " ");

    for (uint32_t i = 0; i < schema->num_fields; i++) {
        if (token == nullptr) return PREPARE_SYNTAX_ERROR;
        if (schema->fields[i].type == FIELD_INT) {
            statement->insert_values[i] = atoi(token);
        } else {
            statement->insert_strings[i] = strdup(token);
            // If the primary key (first field) is TEXT, hash it to use as the B-Tree key
            if (i == 0) {
                statement->insert_values[i] = hash_string(token);
            }
        }
        token = strtok(nullptr, " ");
    }
    return PREPARE_SUCCESS;
}

static PrepareResult prepare_create(char *line, Statement *statement) {
    statement->type = STATEMENT_CREATE_TABLE;
    statement->new_schema.num_fields = 0;
    statement->new_schema.row_size = 0;

    char *token = strtok(line, " "); // Skip "create"
    token = strtok(nullptr, " ");

    while (token) {
        Field *f = &statement->new_schema.fields[statement->new_schema.num_fields++];
        strncpy(f->name, token, FIELD_NAME_MAX - 1);
        token = strtok(nullptr, " ");
        if (token == nullptr) return PREPARE_SYNTAX_ERROR;

        if (strcmp(token, "int") == 0) {
            f->type = FIELD_INT;
            f->size = 4;
        } else {
            f->type = FIELD_TEXT;
            f->size = 32;
        }
        f->offset = statement->new_schema.row_size;
        statement->new_schema.row_size += f->size;
        token = strtok(nullptr, " ");
    }
    return PREPARE_SUCCESS;
}

PrepareResult prepare_statement(char *line, Statement *statement, Schema *schema) {
    if (strncmp(line, "insert", 6) == 0) {
        return prepare_insert(line, statement, schema);
    }
    if (strncmp(line, "select", 6) == 0) {
        statement->type = STATEMENT_SELECT;
        char *token = strtok(line, " "); // skip "select"
        token = strtok(nullptr, " ");
        if (token == nullptr) {
            statement->select_whole_table = true;
            return PREPARE_SUCCESS;
        }

        statement->select_whole_table = false;
        if (schema->num_fields > 0 && schema->fields[0].type == FIELD_TEXT) {
            statement->select_key = hash_string(token);
        } else {
            statement->select_key = atoi(token);
        }
        return PREPARE_SUCCESS;
    }
    if (strncmp(line, "delete", 6) == 0) {
        statement->type = STATEMENT_DELETE;
        char *token = strtok(line, " "); // skip "delete"
        token = strtok(nullptr, " ");
        if (token == nullptr) return PREPARE_SYNTAX_ERROR;

        if (schema->num_fields > 0 && schema->fields[0].type == FIELD_TEXT) {
            statement->delete_id = hash_string(token);
        } else {
            statement->delete_id = atoi(token);
        }
        return PREPARE_SUCCESS;
    }
    if (strncmp(line, "create", 6) == 0) {
        return prepare_create(line, statement);
    }
    return PREPARE_UNRECOGNIZED_STATEMENT;
}

static ExecuteResult execute_insert(Statement *statement, Table *t) {
    Cursor *c = find_node(t, t->root_page_num, statement->insert_values[0]);
    leaf_node_insert(c, statement->insert_values[0], statement);

    // Free strings after insertion
    for (uint32_t i = 0; i < t->schema.num_fields; i++) {
        if (t->schema.fields[i].type == FIELD_TEXT)
            free(statement->insert_strings[i]);
    }
    free(c);
    unpin_page_all(t->pager);
    return EXECUTE_SUCCESS;
}

static ExecuteResult execute_select(Statement *statement, Table *t) {
    Cursor *c;
    if (statement->select_whole_table) {
        c = table_start(t);
    } else {
        c = find_node(t, t->root_page_num, statement->select_key);
    }

    while (true) {
        void *node = get_page(t->pager, c->page_num);
        if (c->cell_num >= *leaf_node_num_cells(node)) {
            uint32_t next = *leaf_node_next_leaf(node);
            if (next == 0) break;
            c->page_num = next;
            c->cell_num = 0;
            continue;
        }

        // If selecting by key, verify we match.
        if (!statement->select_whole_table) {
            uint32_t key = *leaf_node_key(node, c->cell_num, &t->schema);
            if (key != statement->select_key) break;
        }

        void *val = leaf_node_value(node, c->cell_num, &t->schema);
        printf("(");
        for (uint32_t i = 0; i < t->schema.num_fields; i++) {
            if (t->schema.fields[i].type == FIELD_INT) {
                uint32_t v;
                deserialize_field(&t->schema, i, val, &v);
                printf("%d", v);
            } else {
                char v[33] = {0};
                deserialize_field(&t->schema, i, val, v);
                printf("%s", v);
            }
            if (i < t->schema.num_fields - 1)
                printf(", ");
        }
        printf(")\n");
        c->cell_num++;
    }
    free(c);
    return EXECUTE_SUCCESS;
}

static ExecuteResult execute_delete(Statement *statement, Table *t) {
    uint32_t id = statement->delete_id;
    Cursor *c = find_node(t, t->root_page_num, id);
    void *node = get_page(t->pager, c->page_num);
    if (c->cell_num < *leaf_node_num_cells(node) &&
        *leaf_node_key(node, c->cell_num, &t->schema) == id) {
        leaf_node_delete(c);
        printf("Deleted.\n");
    } else {
        printf("Key not found.\n");
    }
    free(c);
    unpin_page_all(t->pager);
    return EXECUTE_SUCCESS;
}

static ExecuteResult execute_create(Statement *statement, Table *t) {
    t->schema = statement->new_schema;
    t->has_schema = true;
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(Statement *statement, Table *t) {
    switch (statement->type) {
        case STATEMENT_INSERT:
            return execute_insert(statement, t);
        case STATEMENT_SELECT:
            return execute_select(statement, t);
        case STATEMENT_DELETE:
            return execute_delete(statement, t);
        case STATEMENT_CREATE_TABLE:
            return execute_create(statement, t);
    }
    return EXECUTE_UNKNOWN_ERROR;
}
