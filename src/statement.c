#include "statement.h"
#include "btree.h"
#include "database.h"
#include "schema.h"
#include "os_portability.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *tokens[128];
  uint32_t num_tokens;
} PrepareContext;

static void free_context(PrepareContext *ctx) {
  for (uint32_t i = 0; i < ctx->num_tokens; i++) {
    free(ctx->tokens[i]);
  }
  ctx->num_tokens = 0;
}

static char *skip_whitespace(char *str) {
  while (*str && isspace(*str))
    str++;
  return str;
}

static char *consume_token_ctx(char **str, PrepareContext *ctx) {
  char *s = skip_whitespace(*str);
  if (*s == '\0')
    return nullptr;

  char *token = nullptr;
  char *start = s;

  if (*s == '(' || *s == ')' || *s == ',' || *s == '=' || *s == ';' ||
      *s == '>' || *s == '<') {
    *str = s + 1;
    token = malloc(2);
    token[0] = *s;
    token[1] = '\0';
  } else if (*s == '\'') {
    s++;
    start = s;
    while (*s && *s != '\'')
      s++;
    size_t len = s - start;
    token = malloc(len + 1);
    memcpy(token, start, len);
    token[len] = '\0';
    if (*s == '\'')
      s++;
    *str = s;
  } else {
    while (*s && !isspace(*s) && *s != '(' && *s != ')' && *s != ',' &&
           *s != '=' && *s != ';')
      s++;
    size_t len = s - start;
    token = malloc(len + 1);
    memcpy(token, start, len);
    token[len] = '\0';
    *str = s;
  }

  if (token) {
    if (ctx->num_tokens < 128) {
      ctx->tokens[ctx->num_tokens++] = token;
    } else {
      free(token);
      return nullptr;
    }
  }
  return token;
}

static bool expect_token_ctx(char **str, const char *expected,
                             PrepareContext *ctx) {
  char *token = consume_token_ctx(str, ctx);
  if (token == nullptr)
    return false;
  return (strcasecmp(token, expected) == 0);
}

static PrepareResult prepare_insert(char *line, Statement *statement,
                                    Database *db, PrepareContext *ctx) {
  statement->type = STATEMENT_INSERT;
  char *curr = line;

  if (!expect_token_ctx(&curr, "insert", ctx))
    return PREPARE_UNRECOGNIZED_STATEMENT;
  if (!expect_token_ctx(&curr, "into", ctx))
    return PREPARE_SYNTAX_ERROR;

  char *table_name = consume_token_ctx(&curr, ctx);
  if (table_name == nullptr)
    return PREPARE_SYNTAX_ERROR;

  int table_index = find_table(db, table_name);
  if (table_index == -1) {
    return PREPARE_NO_TABLE;
  }
  strncpy(statement->table_name, table_name, TABLE_NAME_MAX - 1);
  statement->table_name[TABLE_NAME_MAX - 1] = '\0';
  statement->table_index = (uint32_t)table_index;

  Schema *schema = &db->catalog.tables[statement->table_index].schema;

  if (!expect_token_ctx(&curr, "values", ctx))
    return PREPARE_SYNTAX_ERROR;
  if (!expect_token_ctx(&curr, "(", ctx))
    return PREPARE_SYNTAX_ERROR;

  for (uint32_t i = 0; i < schema->num_fields; i++) {
    char *token = consume_token_ctx(&curr, ctx);
    if (token == nullptr)
      return PREPARE_SYNTAX_ERROR;

    if (schema->fields[i].type == FIELD_INT) {
      statement->insert_values[i] = atoi(token);
    } else {
      if (strlen(token) >= schema->fields[i].size) {
        return PREPARE_STRING_TOO_LONG;
      }
      // We must copy the string because PrepareContext will free the token
      statement->insert_strings[i] = strdup(token);
      if (i == 0) {
        statement->insert_values[i] = hash_string(token);
      }
    }

    if (i < schema->num_fields - 1) {
      if (!expect_token_ctx(&curr, ",", ctx))
        return PREPARE_SYNTAX_ERROR;
    }
  }

  if (!expect_token_ctx(&curr, ")", ctx))
    return PREPARE_SYNTAX_ERROR;

  return PREPARE_SUCCESS;
}

static PrepareResult prepare_create(char *line, Statement *statement,
                                    Database *db, PrepareContext *ctx) {
  statement->type = STATEMENT_CREATE_TABLE;
  statement->new_schema.num_fields = 0;
  statement->new_schema.row_size = 0;

  char *curr = line;
  if (!expect_token_ctx(&curr, "create", ctx))
    return PREPARE_UNRECOGNIZED_STATEMENT;
  if (!expect_token_ctx(&curr, "table", ctx))
    return PREPARE_SYNTAX_ERROR;

  char *table_name = consume_token_ctx(&curr, ctx);
  if (table_name == nullptr)
    return PREPARE_SYNTAX_ERROR;

  if (find_table(db, table_name) != -1) {
    return PREPARE_TABLE_ALREADY_EXISTS;
  }
  if (db->catalog.num_tables >= MAX_TABLES) {
    return PREPARE_CATALOG_FULL;
  }

  strncpy(statement->table_name, table_name, TABLE_NAME_MAX - 1);
  statement->table_name[TABLE_NAME_MAX - 1] = '\0';

  if (!expect_token_ctx(&curr, "(", ctx))
    return PREPARE_SYNTAX_ERROR;

  while (true) {
    char *name = consume_token_ctx(&curr, ctx);
    if (name == nullptr || strcmp(name, ")") == 0) {
      break;
    }

    if (statement->new_schema.num_fields >= MAX_FIELDS) {
      return PREPARE_SYNTAX_ERROR;
    }

    Field *f =
        &statement->new_schema.fields[statement->new_schema.num_fields++];
    if (strlen(name) >= FIELD_NAME_MAX) {
      return PREPARE_SYNTAX_ERROR;
    }
    strncpy(f->name, name, FIELD_NAME_MAX - 1);
    f->name[FIELD_NAME_MAX - 1] = '\0';

    char *type = consume_token_ctx(&curr, ctx);
    if (type == nullptr)
      return PREPARE_SYNTAX_ERROR;

    if (strcasecmp(type, "int") == 0) {
      f->type = FIELD_INT;
      f->size = 4;
    } else {
      f->type = FIELD_TEXT;
      f->size = 32;
    }

    f->offset = statement->new_schema.row_size;
    statement->new_schema.row_size += f->size;

    char *next = consume_token_ctx(&curr, ctx);
    if (next == nullptr)
      return PREPARE_SYNTAX_ERROR;
    if (strcmp(next, ")") == 0) {
      break;
    }
    if (strcmp(next, ",") != 0) {
      return PREPARE_SYNTAX_ERROR;
    }
  }

  return PREPARE_SUCCESS;
}

static PrepareResult prepare_select(char *line, Statement *statement,
                                    Database *db, PrepareContext *ctx) {
  statement->type = STATEMENT_SELECT;
  char *curr = line;

  if (!expect_token_ctx(&curr, "select", ctx))
    return PREPARE_UNRECOGNIZED_STATEMENT;
  if (!expect_token_ctx(&curr, "*", ctx))
    return PREPARE_SYNTAX_ERROR;
  if (!expect_token_ctx(&curr, "from", ctx))
    return PREPARE_SYNTAX_ERROR;

  char *table_name = consume_token_ctx(&curr, ctx);
  if (table_name == nullptr)
    return PREPARE_SYNTAX_ERROR;

  int table_index = find_table(db, table_name);
  if (table_index == -1) {
    return PREPARE_NO_TABLE;
  }
  strncpy(statement->table_name, table_name, TABLE_NAME_MAX - 1);
  statement->table_name[TABLE_NAME_MAX - 1] = '\0';
  statement->table_index = (uint32_t)table_index;

  Schema *schema = &db->catalog.tables[statement->table_index].schema;

  char *where = consume_token_ctx(&curr, ctx);
  if (where == nullptr || strcmp(where, ";") == 0) {
    statement->where_condition = WHERE_NONE;
    return PREPARE_SUCCESS;
  }

  if (strcasecmp(where, "where") != 0) {
    return PREPARE_SYNTAX_ERROR;
  }

  consume_token_ctx(&curr, ctx); // Skip column name
  char *op = consume_token_ctx(&curr, ctx);
  if (op == nullptr)
    return PREPARE_SYNTAX_ERROR;

  if (strcmp(op, "=") == 0) {
    statement->where_condition = WHERE_EQUALS;
  } else if (strcmp(op, ">") == 0) {
    statement->where_condition = WHERE_GREATER_THAN;
  } else if (strcmp(op, "<") == 0) {
    statement->where_condition = WHERE_LESS_THAN;
  } else {
    return PREPARE_SYNTAX_ERROR;
  }

  char *val = consume_token_ctx(&curr, ctx);
  if (val == nullptr)
    return PREPARE_SYNTAX_ERROR;

  if (schema->num_fields > 0 && schema->fields[0].type == FIELD_TEXT) {
    statement->where_key = hash_string(val);
  } else {
    statement->where_key = atoi(val);
  }

  return PREPARE_SUCCESS;
}

static PrepareResult prepare_delete(char *line, Statement *statement,
                                    Database *db, PrepareContext *ctx) {
  statement->type = STATEMENT_DELETE;
  char *curr = line;

  if (!expect_token_ctx(&curr, "delete", ctx))
    return PREPARE_UNRECOGNIZED_STATEMENT;
  if (!expect_token_ctx(&curr, "from", ctx))
    return PREPARE_SYNTAX_ERROR;

  char *table_name = consume_token_ctx(&curr, ctx);
  if (table_name == nullptr)
    return PREPARE_SYNTAX_ERROR;

  int table_index = find_table(db, table_name);
  if (table_index == -1) {
    return PREPARE_NO_TABLE;
  }
  strncpy(statement->table_name, table_name, TABLE_NAME_MAX - 1);
  statement->table_name[TABLE_NAME_MAX - 1] = '\0';
  statement->table_index = (uint32_t)table_index;

  Schema *schema = &db->catalog.tables[statement->table_index].schema;

  if (!expect_token_ctx(&curr, "where", ctx))
    return PREPARE_SYNTAX_ERROR;
  consume_token_ctx(&curr, ctx); // Skip column name
  if (!expect_token_ctx(&curr, "=", ctx))
    return PREPARE_SYNTAX_ERROR;

  char *val = consume_token_ctx(&curr, ctx);
  if (val == nullptr)
    return PREPARE_SYNTAX_ERROR;

  if (schema->num_fields > 0 && schema->fields[0].type == FIELD_TEXT) {
    statement->delete_id = hash_string(val);
  } else {
    statement->delete_id = atoi(val);
  }

  return PREPARE_SUCCESS;
}

static PrepareResult prepare_update(char *line, Statement *statement,
                                    Database *db, PrepareContext *ctx) {
  statement->type = STATEMENT_UPDATE;
  for (int i = 0; i < MAX_FIELDS; i++)
    statement->update_mask[i] = false;

  char *curr = line;
  if (!expect_token_ctx(&curr, "update", ctx))
    return PREPARE_UNRECOGNIZED_STATEMENT;

  char *table_name = consume_token_ctx(&curr, ctx);
  if (table_name == nullptr)
    return PREPARE_SYNTAX_ERROR;

  int table_index = find_table(db, table_name);
  if (table_index == -1) {
    return PREPARE_NO_TABLE;
  }
  strncpy(statement->table_name, table_name, TABLE_NAME_MAX - 1);
  statement->table_name[TABLE_NAME_MAX - 1] = '\0';
  statement->table_index = (uint32_t)table_index;

  Schema *schema = &db->catalog.tables[statement->table_index].schema;

  if (!expect_token_ctx(&curr, "set", ctx))
    return PREPARE_SYNTAX_ERROR;

  while (true) {
    char *name = consume_token_ctx(&curr, ctx);
    if (name == nullptr)
      return PREPARE_SYNTAX_ERROR;

    int field_idx = -1;
    for (uint32_t i = 0; i < schema->num_fields; i++) {
      if (strcasecmp(name, schema->fields[i].name) == 0) {
        field_idx = i;
        break;
      }
    }

    if (field_idx == -1)
      return PREPARE_SYNTAX_ERROR;

    if (!expect_token_ctx(&curr, "=", ctx))
      return PREPARE_SYNTAX_ERROR;

    char *val = consume_token_ctx(&curr, ctx);
    if (val == nullptr)
      return PREPARE_SYNTAX_ERROR;

    statement->update_mask[field_idx] = true;
    if (schema->fields[field_idx].type == FIELD_INT) {
      statement->insert_values[field_idx] = atoi(val);
    } else {
      if (strlen(val) >= schema->fields[field_idx].size) {
        return PREPARE_STRING_TOO_LONG;
      }
      statement->insert_strings[field_idx] = strdup(val);
    }

    char *next = consume_token_ctx(&curr, ctx);
    if (next == nullptr)
      return PREPARE_SYNTAX_ERROR;
    if (strcasecmp(next, "where") == 0) {
      break;
    }
    if (strcmp(next, ",") != 0) {
      return PREPARE_SYNTAX_ERROR;
    }
  }

  consume_token_ctx(&curr, ctx); // Skip column name (assume it's the PK)
  if (!expect_token_ctx(&curr, "=", ctx))
    return PREPARE_SYNTAX_ERROR;

  char *val = consume_token_ctx(&curr, ctx);
  if (val == nullptr)
    return PREPARE_SYNTAX_ERROR;

  if (schema->num_fields > 0 && schema->fields[0].type == FIELD_TEXT) {
    statement->update_key = hash_string(val);
  } else {
    statement->update_key = atoi(val);
  }

  return PREPARE_SUCCESS;
}

PrepareResult prepare_statement(char *line, Statement *statement,
                                Database *db) {
  PrepareContext ctx = {.num_tokens = 0};
  PrepareResult result;

  if (strncasecmp(line, "create", 6) == 0) {
    result = prepare_create(line, statement, db, &ctx);
  } else if (strncasecmp(line, "insert", 6) == 0) {
    result = prepare_insert(line, statement, db, &ctx);
  } else if (strncasecmp(line, "select", 6) == 0) {
    result = prepare_select(line, statement, db, &ctx);
  } else if (strncasecmp(line, "delete", 6) == 0) {
    result = prepare_delete(line, statement, db, &ctx);
  } else if (strncasecmp(line, "update", 6) == 0) {
    result = prepare_update(line, statement, db, &ctx);
  } else if (strcasecmp(line, "begin") == 0) {
    statement->type = STATEMENT_BEGIN;
    result = PREPARE_SUCCESS;
  } else if (strcasecmp(line, "commit") == 0) {
    statement->type = STATEMENT_COMMIT;
    result = PREPARE_SUCCESS;
  } else if (strcasecmp(line, "rollback") == 0) {
    statement->type = STATEMENT_ROLLBACK;
    result = PREPARE_SUCCESS;
  } else {
    result = PREPARE_UNRECOGNIZED_STATEMENT;
  }

  free_context(&ctx);
  return result;
}

static ExecuteResult execute_insert(Statement *statement, Database *db) {
  uint32_t table_index = statement->table_index;
  TableDefinition *td = &db->catalog.tables[table_index];
  Cursor *c = find_node(db, table_index, td->root_page_num,
                        statement->insert_values[0]);
  void *node = get_page(db->pager, c->page_num);
  if (c->cell_num < *leaf_node_num_cells(node)) {
    uint32_t key_at_index = *leaf_node_key(node, c->cell_num, &td->schema);
    if (key_at_index == statement->insert_values[0]) {
      free_statement(statement);
      free(c);
      unpin_page_all(db->pager);
      return EXECUTE_DUPLICATE_KEY;
    }
  }

  leaf_node_insert(c, statement->insert_values[0], statement);

  free_statement(statement);
  free(c);
  unpin_page_all(db->pager);
  return EXECUTE_SUCCESS;
}

static void print_box_header(Schema *schema, uint32_t *widths) {
  printf("┌");
  for (uint32_t i = 0; i < schema->num_fields; i++) {
    for (uint32_t j = 0; j < widths[i] + 2; j++)
      printf("─");
    if (i < schema->num_fields - 1)
      printf("┬");
  }
  printf("┐\n");

  printf("│");
  for (uint32_t i = 0; i < schema->num_fields; i++) {
    printf(" %-*s │", widths[i], schema->fields[i].name);
  }
  printf("\n");

  printf("├");
  for (uint32_t i = 0; i < schema->num_fields; i++) {
    for (uint32_t j = 0; j < widths[i] + 2; j++)
      printf("─");
    if (i < schema->num_fields - 1)
      printf("┼");
  }
  printf("┤\n");
}

static void print_box_footer(Schema *schema, uint32_t *widths) {
  printf("└");
  for (uint32_t i = 0; i < schema->num_fields; i++) {
    for (uint32_t j = 0; j < widths[i] + 2; j++)
      printf("─");
    if (i < schema->num_fields - 1)
      printf("┴");
  }
  printf("┘\n");
}

static ExecuteResult execute_select(Statement *statement, Database *db) {
  uint32_t table_index = statement->table_index;
  TableDefinition *td = &db->catalog.tables[table_index];
  Cursor *c;
  if (statement->where_condition == WHERE_NONE ||
      statement->where_condition == WHERE_LESS_THAN) {
    c = table_start(db, table_index);
  } else {
    c = find_node(db, table_index, td->root_page_num, statement->where_key);
  }

  if (db->print_mode == PRINT_BOX) {
    // In a real DB we wouldn't load everything into memory, but for education
    // and small tables it's fine.
    uint32_t widths[MAX_FIELDS];
    for (uint32_t i = 0; i < td->schema.num_fields; i++) {
      widths[i] = (uint32_t)strlen(td->schema.fields[i].name);
    }

    // First pass: calculate widths
    Cursor *temp_c = malloc(sizeof(Cursor));
    memcpy(temp_c, c, sizeof(Cursor));
    while (true) {
      void *node = get_page(db->pager, temp_c->page_num);
      if (temp_c->cell_num >= *leaf_node_num_cells(node)) {
        uint32_t next = *leaf_node_next_leaf(node);
        if (next == 0)
          break;
        temp_c->page_num = next;
        temp_c->cell_num = 0;
        continue;
      }

      uint32_t key = *leaf_node_key(node, temp_c->cell_num, &td->schema);
      if (statement->where_condition == WHERE_EQUALS &&
          key != statement->where_key)
        break;
      if (statement->where_condition == WHERE_GREATER_THAN &&
          key <= statement->where_key) {
        temp_c->cell_num++;
        continue;
      }
      if (statement->where_condition == WHERE_LESS_THAN &&
          key >= statement->where_key)
        break;

      void *val = leaf_node_value(node, temp_c->cell_num, &td->schema);
      for (uint32_t i = 0; i < td->schema.num_fields; i++) {
        char buf[64];
        if (td->schema.fields[i].type == FIELD_INT) {
          uint32_t v;
          deserialize_field(&td->schema, i, val, &v);
          snprintf(buf, sizeof(buf), "%u", v);
        } else {
          deserialize_field(&td->schema, i, val, buf);
        }
        uint32_t len = (uint32_t)strlen(buf);
        if (len > widths[i])
          widths[i] = len;
      }
      temp_c->cell_num++;
    }
    free(temp_c);

    print_box_header(&td->schema, widths);

    // Second pass: print rows
    while (true) {
      void *node = get_page(db->pager, c->page_num);
      if (c->cell_num >= *leaf_node_num_cells(node)) {
        uint32_t next = *leaf_node_next_leaf(node);
        if (next == 0)
          break;
        c->page_num = next;
        c->cell_num = 0;
        continue;
      }

      uint32_t key = *leaf_node_key(node, c->cell_num, &td->schema);
      if (statement->where_condition == WHERE_EQUALS &&
          key != statement->where_key)
        break;
      if (statement->where_condition == WHERE_GREATER_THAN &&
          key <= statement->where_key) {
        c->cell_num++;
        continue;
      }
      if (statement->where_condition == WHERE_LESS_THAN &&
          key >= statement->where_key)
        break;

      void *val = leaf_node_value(node, c->cell_num, &td->schema);
      printf("│");
      for (uint32_t i = 0; i < td->schema.num_fields; i++) {
        char buf[64];
        if (td->schema.fields[i].type == FIELD_INT) {
          uint32_t v;
          deserialize_field(&td->schema, i, val, &v);
          snprintf(buf, sizeof(buf), "%u", v);
        } else {
          deserialize_field(&td->schema, i, val, buf);
        }
        printf(" %-*s │", widths[i], buf);
      }
      printf("\n");
      c->cell_num++;
    }
    print_box_footer(&td->schema, widths);
  } else {
    // PLAIN MODE
    while (true) {
      void *node = get_page(db->pager, c->page_num);
      if (c->cell_num >= *leaf_node_num_cells(node)) {
        uint32_t next = *leaf_node_next_leaf(node);
        if (next == 0)
          break;
        c->page_num = next;
        c->cell_num = 0;
        continue;
      }

      uint32_t key = *leaf_node_key(node, c->cell_num, &td->schema);

      if (statement->where_condition == WHERE_EQUALS) {
        if (key != statement->where_key)
          break;
      } else if (statement->where_condition == WHERE_GREATER_THAN) {
        if (key <= statement->where_key) {
          c->cell_num++;
          continue;
        }
      } else if (statement->where_condition == WHERE_LESS_THAN) {
        if (key >= statement->where_key)
          break;
      }

      void *val = leaf_node_value(node, c->cell_num, &td->schema);
      printf("(");
      for (uint32_t i = 0; i < td->schema.num_fields; i++) {
        if (td->schema.fields[i].type == FIELD_INT) {
          uint32_t v;
          deserialize_field(&td->schema, i, val, &v);
          printf("%u", v);
        } else {
          char v[33] = {0};
          deserialize_field(&td->schema, i, val, v);
          printf("%s", v);
        }
        if (i < td->schema.num_fields - 1)
          printf(", ");
      }
      printf(")\n");
      c->cell_num++;
    }
  }
  free(c);
  return EXECUTE_SUCCESS;
}

static ExecuteResult execute_delete(Statement *statement, Database *db) {
  uint32_t table_index = statement->table_index;
  TableDefinition *td = &db->catalog.tables[table_index];
  uint32_t id = statement->delete_id;
  Cursor *c = find_node(db, table_index, td->root_page_num, id);
  void *node = get_page(db->pager, c->page_num);
  ExecuteResult res = EXECUTE_SUCCESS;
  if (c->cell_num < *leaf_node_num_cells(node) &&
      *leaf_node_key(node, c->cell_num, &td->schema) == id) {
    leaf_node_delete(c);
    printf("Deleted.\n");
  } else {
    res = EXECUTE_KEY_NOT_FOUND;
  }
  free(c);
  unpin_page_all(db->pager);
  return res;
}

static ExecuteResult execute_create(Statement *statement, Database *db) {
  uint32_t table_index = db->catalog.num_tables;
  TableDefinition *td = &db->catalog.tables[table_index];

  strncpy(td->name, statement->table_name, TABLE_NAME_MAX);
  td->schema = statement->new_schema;

  // Allocate new root page
  uint32_t root_page_num = db->pager->num_pages;
  if (root_page_num == 0)
    root_page_num = 1; // Page 0 is catalog

  td->root_page_num = root_page_num;
  void *root = get_page(db->pager, root_page_num);
  initialize_leaf_node(root);
  set_node_root(root, true);
  mark_page_dirty(db->pager, root_page_num);

  db->catalog.num_tables++;
  db_save_catalog(db);

  return EXECUTE_SUCCESS;
}

static ExecuteResult execute_update(Statement *statement, Database *db) {
  uint32_t table_index = statement->table_index;
  TableDefinition *td = &db->catalog.tables[table_index];
  Cursor *c =
      find_node(db, table_index, td->root_page_num, statement->update_key);
  void *node = get_page(db->pager, c->page_num);
  ExecuteResult res = EXECUTE_SUCCESS;
  if (c->cell_num < *leaf_node_num_cells(node) &&
      *leaf_node_key(node, c->cell_num, &td->schema) == statement->update_key) {
    void *val = leaf_node_value(node, c->cell_num, &td->schema);
    for (uint32_t i = 0; i < td->schema.num_fields; i++) {
      if (statement->update_mask[i]) {
        if (td->schema.fields[i].type == FIELD_INT) {
          serialize_field(&td->schema, i, &statement->insert_values[i], val);
        } else {
          serialize_field(&td->schema, i, statement->insert_strings[i], val);
          if (i == 0) {
            *leaf_node_key(node, c->cell_num, &td->schema) =
                hash_string(statement->insert_strings[i]);
          }
        }
        if (i == 0 && td->schema.fields[i].type == FIELD_INT) {
          *leaf_node_key(node, c->cell_num, &td->schema) =
              statement->insert_values[i];
        }
      }
    }
    mark_page_dirty(db->pager, c->page_num);
    printf("Updated.\n");
  } else {
    res = EXECUTE_KEY_NOT_FOUND;
  }

  free_statement(statement);
  free(c);
  unpin_page_all(db->pager);
  return res;
}

ExecuteResult execute_statement(Statement *statement, Database *db) {
  switch (statement->type) {
  case STATEMENT_INSERT:
    return execute_insert(statement, db);
  case STATEMENT_SELECT:
    return execute_select(statement, db);
  case STATEMENT_DELETE:
    return execute_delete(statement, db);
  case STATEMENT_UPDATE:
    return execute_update(statement, db);
  case STATEMENT_CREATE_TABLE:
    return execute_create(statement, db);
  case STATEMENT_BEGIN:
    printf("Transaction started.\n");
    return EXECUTE_SUCCESS;
  case STATEMENT_COMMIT:
    db_save_catalog(db);
    printf("Transaction committed.\n");
    return EXECUTE_SUCCESS;
  case STATEMENT_ROLLBACK:
    printf("Rollback requested. Note: ROLLBACK is currently not supported in "
           "this educational version. Every statement is auto-committed.\n");
    return EXECUTE_SUCCESS;
  }
  return EXECUTE_UNKNOWN_ERROR;
}

void free_statement(Statement *statement) {
  for (int i = 0; i < MAX_FIELDS; i++) {
    if (statement->insert_strings[i]) {
      free(statement->insert_strings[i]);
      statement->insert_strings[i] = nullptr;
    }
  }
}
