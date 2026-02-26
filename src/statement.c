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
      if (strlen(token) >= schema->fields[i].size) {
        free(token);
        // Free already allocated strings
        for (uint32_t j = 0; j < i; j++) {
          if (schema->fields[j].type == FIELD_TEXT)
            free(statement->insert_strings[j]);
        }
        return PREPARE_STRING_TOO_LONG;
      }
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

    if (statement->new_schema.num_fields >= MAX_FIELDS) {
      free(name);
      return PREPARE_SYNTAX_ERROR; // Or a more specific error
    }

    Field *f =
        &statement->new_schema.fields[statement->new_schema.num_fields++];
    if (strlen(name) >= FIELD_NAME_MAX) {
      free(name);
      return PREPARE_SYNTAX_ERROR;
    }
    strncpy(f->name, name, FIELD_NAME_MAX - 1);
    f->name[FIELD_NAME_MAX - 1] = '\0';
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

static PrepareResult prepare_update(char *line, Statement *statement,
                                    Schema *schema) {
  statement->type = STATEMENT_UPDATE;
  for (int i = 0; i < MAX_FIELDS; i++)
    statement->update_mask[i] = false;

  char *curr = line;
  if (!expect_token(&curr, "update"))
    return PREPARE_UNRECOGNIZED_STATEMENT;
  free(consume_token(&curr)); // Skip table name

  if (!expect_token(&curr, "set"))
    return PREPARE_SYNTAX_ERROR;

  while (true) {
    char *name = consume_token(&curr);
    if (name == nullptr)
      return PREPARE_SYNTAX_ERROR;

    int field_idx = -1;
    for (uint32_t i = 0; i < schema->num_fields; i++) {
      if (strcasecmp(name, schema->fields[i].name) == 0) {
        field_idx = i;
        break;
      }
    }
    free(name);

    if (field_idx == -1)
      return PREPARE_SYNTAX_ERROR;

    if (!expect_token(&curr, "="))
      return PREPARE_SYNTAX_ERROR;

    char *val = consume_token(&curr);
    if (val == nullptr)
      return PREPARE_SYNTAX_ERROR;

    statement->update_mask[field_idx] = true;
    if (schema->fields[field_idx].type == FIELD_INT) {
      statement->insert_values[field_idx] = atoi(val);
      free(val);
    } else {
      if (strlen(val) >= schema->fields[field_idx].size) {
        free(val);
        // Free strings already in this statement
        for (int i = 0; i < MAX_FIELDS; i++) {
          if (statement->update_mask[i] && schema->fields[i].type == FIELD_TEXT &&
              statement->insert_strings[i]) {
            free(statement->insert_strings[i]);
            statement->insert_strings[i] = nullptr;
          }
        }
        return PREPARE_STRING_TOO_LONG;
      }
      statement->insert_strings[field_idx] = val;
    }

    char *next = consume_token(&curr);
    if (next == nullptr)
      return PREPARE_SYNTAX_ERROR;
    if (strcasecmp(next, "where") == 0) {
      free(next);
      break;
    }
    if (strcmp(next, ",") != 0) {
      free(next);
      return PREPARE_SYNTAX_ERROR;
    }
    free(next);
  }

  free(consume_token(&curr)); // Skip column name (assume it's the PK)
  if (!expect_token(&curr, "="))
    return PREPARE_SYNTAX_ERROR;

  char *val = consume_token(&curr);
  if (val == nullptr)
    return PREPARE_SYNTAX_ERROR;

  if (schema->num_fields > 0 && schema->fields[0].type == FIELD_TEXT) {
    statement->update_key = hash_string(val);
  } else {
    statement->update_key = atoi(val);
  }
  free(val);

  char *semi = consume_token(&curr);
  if (semi)
    free(semi);

  return PREPARE_SUCCESS;
}

PrepareResult prepare_statement(char *line, Statement *statement, Table *t) {
  if (strncasecmp(line, "create", 6) == 0) {
    if (t->has_schema)
      return PREPARE_TABLE_ALREADY_EXISTS;
    return prepare_create(line, statement);
  }

  if (!t->has_schema)
    return PREPARE_NO_SCHEMA;

  if (strncasecmp(line, "insert", 6) == 0) {
    return prepare_insert(line, statement, &t->schema);
  }
  if (strncasecmp(line, "select", 6) == 0) {
    return prepare_select(line, statement, &t->schema);
  }
  if (strncasecmp(line, "delete", 6) == 0) {
    return prepare_delete(line, statement, &t->schema);
  }
  if (strncasecmp(line, "update", 6) == 0) {
    return prepare_update(line, statement, &t->schema);
  }
  return PREPARE_UNRECOGNIZED_STATEMENT;
}

static ExecuteResult execute_insert(Statement *statement, Table *t) {
  Cursor *c = find_node(t, t->root_page_num, statement->insert_values[0]);
  void *node = get_page(t->pager, c->page_num);
  if (c->cell_num < *leaf_node_num_cells(node)) {
    uint32_t key_at_index = *leaf_node_key(node, c->cell_num, &t->schema);
    if (key_at_index == statement->insert_values[0]) {
      // Free strings
      for (uint32_t i = 0; i < t->schema.num_fields; i++) {
        if (t->schema.fields[i].type == FIELD_TEXT)
          free(statement->insert_strings[i]);
      }
      free(c);
      unpin_page_all(t->pager);
      return EXECUTE_DUPLICATE_KEY;
    }
  }

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
  ExecuteResult res = EXECUTE_SUCCESS;
  if (c->cell_num < *leaf_node_num_cells(node) &&
      *leaf_node_key(node, c->cell_num, &t->schema) == id) {
    leaf_node_delete(c);
    printf("Deleted.\n");
  } else {
    res = EXECUTE_KEY_NOT_FOUND;
  }
  free(c);
  unpin_page_all(t->pager);
  return res;
}

static ExecuteResult execute_create(Statement *statement, Table *t) {
  t->schema = statement->new_schema;
  t->has_schema = true;
  return EXECUTE_SUCCESS;
}

static ExecuteResult execute_update(Statement *statement, Table *t) {
  Cursor *c = find_node(t, t->root_page_num, statement->update_key);
  void *node = get_page(t->pager, c->page_num);
  ExecuteResult res = EXECUTE_SUCCESS;
  if (c->cell_num < *leaf_node_num_cells(node) &&
      *leaf_node_key(node, c->cell_num, &t->schema) == statement->update_key) {
    void *val = leaf_node_value(node, c->cell_num, &t->schema);
    for (uint32_t i = 0; i < t->schema.num_fields; i++) {
      if (statement->update_mask[i]) {
        if (t->schema.fields[i].type == FIELD_INT) {
          serialize_field(&t->schema, i, &statement->insert_values[i], val);
        } else {
          serialize_field(&t->schema, i, statement->insert_strings[i], val);
          // Special case: if we update the first field and it's a TEXT, we
          // should update the key too
          if (i == 0) {
            *leaf_node_key(node, c->cell_num, &t->schema) =
                hash_string(statement->insert_strings[i]);
          }
        }
        // If we update the first field and it's an INT, update the key
        if (i == 0 && t->schema.fields[i].type == FIELD_INT) {
          *leaf_node_key(node, c->cell_num, &t->schema) =
              statement->insert_values[i];
        }
      }
    }
    mark_page_dirty(t->pager, c->page_num);
    printf("Updated.\n");
  } else {
    res = EXECUTE_KEY_NOT_FOUND;
  }

  // Free strings
  for (uint32_t i = 0; i < t->schema.num_fields; i++) {
    if (t->schema.fields[i].type == FIELD_TEXT && statement->update_mask[i])
      free(statement->insert_strings[i]);
  }
  free(c);
  unpin_page_all(t->pager);
  return res;
}

ExecuteResult execute_statement(Statement *statement, Table *t) {
  switch (statement->type) {
  case STATEMENT_INSERT:
    return execute_insert(statement, t);
  case STATEMENT_SELECT:
    return execute_select(statement, t);
  case STATEMENT_DELETE:
    return execute_delete(statement, t);
  case STATEMENT_UPDATE:
    return execute_update(statement, t);
  case STATEMENT_CREATE_TABLE:
    return execute_create(statement, t);
  }
  return EXECUTE_UNKNOWN_ERROR;
}
