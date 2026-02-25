#include "btree.h"
#include "schema.h"
#include "statement.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static char *skip_whitespace(char *str) {
  while (*str && isspace(*str))
    str++;
  return str;
}

static char *consume_token(char **str) {
  char *s = skip_whitespace(*str);
  if (*s == '\0')
    return nullptr;

  char *start = s;
  if (*s == '(' || *s == ')' || *s == ',' || *s == '=' || *s == ';') {
    *str = s + 1;
    char *token = malloc(2);
    token[0] = *s;
    token[1] = '\0';
    return token;
  }

  if (*s == '\'') {
    s++;
    start = s;
    while (*s && *s != '\'')
      s++;
    size_t len = s - start;
    char *token = malloc(len + 1);
    memcpy(token, start, len);
    token[len] = '\0';
    if (*s == '\'')
      s++;
    *str = s;
    return token;
  }

  while (*s && !isspace(*s) && *s != '(' && *s != ')' && *s != ',' && *s != '=' &&
         *s != ';')
    s++;
  size_t len = s - start;
  char *token = malloc(len + 1);
  memcpy(token, start, len);
  token[len] = '\0';
  *str = s;
  return token;
}

static bool expect_token(char **str, const char *expected) {
  char *token = consume_token(str);
  if (token == nullptr)
    return false;
  bool match = (strcasecmp(token, expected) == 0);
  free(token);
  return match;
}

static PrepareResult prepare_insert(char *line, Statement *statement,
                                    Schema *schema) {
  statement->type = STATEMENT_INSERT;
  char *curr = line;

  // INSERT INTO table_name VALUES (val1, val2, ...)
  if (!expect_token(&curr, "insert"))
    return PREPARE_UNRECOGNIZED_STATEMENT;
  if (!expect_token(&curr, "into"))
    return PREPARE_SYNTAX_ERROR;
  free(consume_token(&curr)); // Skip table name
  if (!expect_token(&curr, "values"))
    return PREPARE_SYNTAX_ERROR;
  if (!expect_token(&curr, "("))
    return PREPARE_SYNTAX_ERROR;

  for (uint32_t i = 0; i < schema->num_fields; i++) {
    char *token = consume_token(&curr);
    if (token == nullptr)
      return PREPARE_SYNTAX_ERROR;

    if (schema->fields[i].type == FIELD_INT) {
      statement->insert_values[i] = atoi(token);
      free(token);
    } else {
      statement->insert_strings[i] = token; // Already malloc'd
      if (i == 0) {
        statement->insert_values[i] = hash_string(token);
      }
    }

    if (i < schema->num_fields - 1) {
      if (!expect_token(&curr, ","))
        return PREPARE_SYNTAX_ERROR;
    }
  }

  if (!expect_token(&curr, ")"))
    return PREPARE_SYNTAX_ERROR;

  char *semi = consume_token(&curr);
  if (semi)
    free(semi);

  return PREPARE_SUCCESS;
}

static PrepareResult prepare_create(char *line, Statement *statement) {
  statement->type = STATEMENT_CREATE_TABLE;
  statement->new_schema.num_fields = 0;
  statement->new_schema.row_size = 0;

  char *curr = line;
  if (!expect_token(&curr, "create"))
    return PREPARE_UNRECOGNIZED_STATEMENT;
  if (!expect_token(&curr, "table"))
    return PREPARE_SYNTAX_ERROR;
  free(consume_token(&curr)); // Skip table name
  if (!expect_token(&curr, "("))
    return PREPARE_SYNTAX_ERROR;

  while (true) {
    char *name = consume_token(&curr);
    if (name == nullptr || strcmp(name, ")") == 0) {
      if (name)
        free(name);
      break;
    }

    Field *f =
        &statement->new_schema.fields[statement->new_schema.num_fields++];
    strncpy(f->name, name, FIELD_NAME_MAX - 1);
    free(name);

    char *type = consume_token(&curr);
    if (type == nullptr)
      return PREPARE_SYNTAX_ERROR;

    if (strcasecmp(type, "int") == 0) {
      f->type = FIELD_INT;
      f->size = 4;
    } else {
      f->type = FIELD_TEXT;
      f->size = 32;
    }
    free(type);

    f->offset = statement->new_schema.row_size;
    statement->new_schema.row_size += f->size;

    char *next = consume_token(&curr);
    if (next == nullptr)
      return PREPARE_SYNTAX_ERROR;
    if (strcmp(next, ")") == 0) {
      free(next);
      break;
    }
    if (strcmp(next, ",") != 0) {
      free(next);
      return PREPARE_SYNTAX_ERROR;
    }
    free(next);
  }

  char *semi = consume_token(&curr);
  if (semi)
    free(semi);

  return PREPARE_SUCCESS;
}

static PrepareResult prepare_select(char *line, Statement *statement,
                                    Schema *schema) {
  statement->type = STATEMENT_SELECT;
  char *curr = line;

  if (!expect_token(&curr, "select"))
    return PREPARE_UNRECOGNIZED_STATEMENT;
  if (!expect_token(&curr, "*"))
    return PREPARE_SYNTAX_ERROR;
  if (!expect_token(&curr, "from"))
    return PREPARE_SYNTAX_ERROR;
  free(consume_token(&curr)); // Skip table name

  char *where = consume_token(&curr);
  if (where == nullptr || strcmp(where, ";") == 0) {
    if (where)
      free(where);
    statement->select_whole_table = true;
    return PREPARE_SUCCESS;
  }

  if (strcasecmp(where, "where") != 0) {
    free(where);
    return PREPARE_SYNTAX_ERROR;
  }
  free(where);

  free(consume_token(&curr)); // Skip column name
  if (!expect_token(&curr, "="))
    return PREPARE_SYNTAX_ERROR;

  char *val = consume_token(&curr);
  if (val == nullptr)
    return PREPARE_SYNTAX_ERROR;

  statement->select_whole_table = false;
  if (schema->num_fields > 0 && schema->fields[0].type == FIELD_TEXT) {
    statement->select_key = hash_string(val);
  } else {
    statement->select_key = atoi(val);
  }
  free(val);

  char *semi = consume_token(&curr);
  if (semi)
    free(semi);

  return PREPARE_SUCCESS;
}

static PrepareResult prepare_delete(char *line, Statement *statement,
                                    Schema *schema) {
  statement->type = STATEMENT_DELETE;
  char *curr = line;

  if (!expect_token(&curr, "delete"))
    return PREPARE_UNRECOGNIZED_STATEMENT;
  if (!expect_token(&curr, "from"))
    return PREPARE_SYNTAX_ERROR;
  free(consume_token(&curr)); // Skip table name

  if (!expect_token(&curr, "where"))
    return PREPARE_SYNTAX_ERROR;
  free(consume_token(&curr)); // Skip column name
  if (!expect_token(&curr, "="))
    return PREPARE_SYNTAX_ERROR;

  char *val = consume_token(&curr);
  if (val == nullptr)
    return PREPARE_SYNTAX_ERROR;

  if (schema->num_fields > 0 && schema->fields[0].type == FIELD_TEXT) {
    statement->delete_id = hash_string(val);
  } else {
    statement->delete_id = atoi(val);
  }
  free(val);

  char *semi = consume_token(&curr);
  if (semi)
    free(semi);

  return PREPARE_SUCCESS;
}

PrepareResult prepare_statement(char *line, Statement *statement,
                                Schema *schema) {
  if (strncasecmp(line, "insert", 6) == 0) {
    return prepare_insert(line, statement, schema);
  }
  if (strncasecmp(line, "select", 6) == 0) {
    return prepare_select(line, statement, schema);
  }
  if (strncasecmp(line, "delete", 6) == 0) {
    return prepare_delete(line, statement, schema);
  }
  if (strncasecmp(line, "create", 6) == 0) {
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
      if (next == 0)
        break;
      c->page_num = next;
      c->cell_num = 0;
      continue;
    }

    // If selecting by key, verify we match.
    if (!statement->select_whole_table) {
      uint32_t key = *leaf_node_key(node, c->cell_num, &t->schema);
      if (key != statement->select_key)
        break;
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
