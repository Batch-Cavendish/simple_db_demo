#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define TABLE_MAX_PAGES 100
#define MAX_FIELDS 16
#define FIELD_NAME_MAX 32

typedef enum { FIELD_INT, FIELD_TEXT } FieldType;

typedef struct {
  char name[FIELD_NAME_MAX];
  FieldType type;
  uint32_t size;
  uint32_t offset;
} Field;

typedef struct {
  uint32_t num_fields;
  Field fields[MAX_FIELDS];
  uint32_t row_size;
} Schema;

typedef enum { NODE_INTERNAL, NODE_LEAF } NodeType;

/* Common Node Header Layout */
const uint32_t NODE_TYPE_SIZE = sizeof(uint8_t);
const uint32_t NODE_TYPE_OFFSET = 0;
const uint32_t IS_ROOT_SIZE = sizeof(uint8_t);
const uint32_t IS_ROOT_OFFSET = NODE_TYPE_SIZE;
const uint32_t PARENT_POINTER_SIZE = sizeof(uint32_t);
const uint32_t PARENT_POINTER_OFFSET = IS_ROOT_OFFSET + IS_ROOT_SIZE;
const uint8_t COMMON_NODE_HEADER_SIZE =
    NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE;

/* Leaf Node Header Layout (num_cells, next_leaf) */
const uint32_t LEAF_NODE_NUM_CELLS_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;
const uint32_t LEAF_NODE_NEXT_LEAF_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_NEXT_LEAF_OFFSET =
    LEAF_NODE_NUM_CELLS_OFFSET + LEAF_NODE_NUM_CELLS_SIZE;
const uint32_t LEAF_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE +
                                       LEAF_NODE_NUM_CELLS_SIZE +
                                       LEAF_NODE_NEXT_LEAF_SIZE;

/* Internal Node Header Layout (num_keys, right_child) */
const uint32_t INTERNAL_NODE_NUM_KEYS_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_NUM_KEYS_OFFSET = COMMON_NODE_HEADER_SIZE;
const uint32_t INTERNAL_NODE_RIGHT_CHILD_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_RIGHT_CHILD_OFFSET =
    INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE;
const uint32_t INTERNAL_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE +
                                           INTERNAL_NODE_NUM_KEYS_SIZE +
                                           INTERNAL_NODE_RIGHT_CHILD_SIZE;

typedef struct {
  int file_descriptor;
  uint32_t file_length;
  uint32_t num_pages;
  void *pages[TABLE_MAX_PAGES];
} Pager;

typedef struct {
  uint32_t root_page_num;
  Pager *pager;
  Schema schema;
  bool has_schema;
} Table;

typedef struct {
  Table *table;
  uint32_t page_num;
  uint32_t cell_num;
  bool end_of_table;
} Cursor;

typedef struct {
  char *buffer;
  size_t buffer_length;
  ssize_t input_length;
} InputBuffer;

/* Node Accessors */
NodeType get_node_type(void *node) {
  return (NodeType)(*(uint8_t *)((char *)node + NODE_TYPE_OFFSET));
}
void set_node_type(void *node, NodeType type) {
  *(uint8_t *)((char *)node + NODE_TYPE_OFFSET) = (uint8_t)type;
}
bool is_node_root(void *node) {
  return (bool)(*(uint8_t *)((char *)node + IS_ROOT_OFFSET));
}
void set_node_root(void *node, bool is_root) {
  *(uint8_t *)((char *)node + IS_ROOT_OFFSET) = (uint8_t)is_root;
}
uint32_t *node_parent(void *node) {
  return (uint32_t *)((char *)node + PARENT_POINTER_OFFSET);
}

/* Leaf Node Accessors */
uint32_t *leaf_node_num_cells(void *node) {
  return (uint32_t *)((char *)node + LEAF_NODE_NUM_CELLS_OFFSET);
}
uint32_t *leaf_node_next_leaf(void *node) {
  return (uint32_t *)((char *)node + LEAF_NODE_NEXT_LEAF_OFFSET);
}

/* Internal Node Accessors */
uint32_t *internal_node_num_keys(void *node) {
  return (uint32_t *)((char *)node + INTERNAL_NODE_NUM_KEYS_OFFSET);
}
uint32_t *internal_node_right_child(void *node) {
  return (uint32_t *)((char *)node + INTERNAL_NODE_RIGHT_CHILD_OFFSET);
}

const uint32_t INTERNAL_NODE_KEY_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_CHILD_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_CELL_SIZE =
    INTERNAL_NODE_KEY_SIZE + INTERNAL_NODE_CHILD_SIZE;

uint32_t *internal_node_cell(void *node, uint32_t cell_num) {
  return (uint32_t *)((char *)node + INTERNAL_NODE_HEADER_SIZE +
                      cell_num * INTERNAL_NODE_CELL_SIZE);
}
uint32_t *internal_node_child(void *node, uint32_t child_num) {
  uint32_t num_keys = *internal_node_num_keys(node);
  if (child_num == num_keys)
    return internal_node_right_child(node);
  return internal_node_cell(node, child_num);
}
uint32_t *internal_node_key(void *node, uint32_t key_num) {
  return (uint32_t *)((char *)internal_node_cell(node, key_num) +
                      INTERNAL_NODE_CHILD_SIZE);
}

/* Dynamic Cell Accessors */
uint32_t leaf_node_cell_size(Schema *schema) {
  return sizeof(uint32_t) + schema->row_size;
}
void *leaf_node_cell(void *node, uint32_t cell_num, Schema *schema) {
  return (char *)node + LEAF_NODE_HEADER_SIZE +
         cell_num * leaf_node_cell_size(schema);
}
uint32_t *leaf_node_key(void *node, uint32_t cell_num, Schema *schema) {
  return (uint32_t *)leaf_node_cell(node, cell_num, schema);
}
void *leaf_node_value(void *node, uint32_t cell_num, Schema *schema) {
  return (char *)leaf_node_cell(node, cell_num, schema) + sizeof(uint32_t);
}

void initialize_leaf_node(void *node) {
  set_node_type(node, NODE_LEAF);
  set_node_root(node, false);
  *leaf_node_num_cells(node) = 0;
  *leaf_node_next_leaf(node) = 0;
}
void initialize_internal_node(void *node) {
  set_node_type(node, NODE_INTERNAL);
  set_node_root(node, false);
  *internal_node_num_keys(node) = 0;
}

uint32_t get_node_max_key(void *node, Schema *schema) {
  switch (get_node_type(node)) {
  case NODE_INTERNAL:
    return *internal_node_key(node, *internal_node_num_keys(node) - 1);
  case NODE_LEAF:
    return *leaf_node_key(node, *leaf_node_num_cells(node) - 1, schema);
  }
  return 0;
}

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

typedef struct {
  StatementType type;
  uint32_t delete_id;
  uint32_t insert_values[MAX_FIELDS];
  char *insert_strings[MAX_FIELDS];
} Statement;

void serialize_field(Schema *schema, uint32_t field_idx, void *val,
                     void *dest) {
  Field *f = &schema->fields[field_idx];
  memcpy((char *)dest + f->offset, val, f->size);
}

void deserialize_field(Schema *schema, uint32_t field_idx, void *src,
                       void *dest) {
  Field *f = &schema->fields[field_idx];
  memcpy(dest, (char *)src + f->offset, f->size);
}

/* 
 * The Pager is the heart of the storage engine. It manages the abstraction 
 * of "pages" so the rest of the database doesn't have to deal with file offsets.
 * Databases use fixed-size pages to match the physical blocks on disk, which
 * optimizes I/O performance.
 */
Pager *pager_open(const char *filename) {
  int fd = open(filename, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
  if (fd == -1)
    exit(EXIT_FAILURE);
  off_t len = lseek(fd, 0, SEEK_END);
  Pager *p = malloc(sizeof(Pager));
  p->file_descriptor = fd;
  p->file_length = len;
  p->num_pages = len / PAGE_SIZE;
  for (int i = 0; i < TABLE_MAX_PAGES; i++)
    p->pages[i] = NULL;
  return p;
}

/*
 * get_page implements a simple page cache (Buffer Pool). 
 * Accessing RAM is orders of magnitude faster than disk, so we keep 
 * frequently accessed pages in memory to avoid redundant disk reads.
 */
void *get_page(Pager *p, uint32_t pg) {
  if (p->pages[pg] == NULL) {
    void *page = malloc(PAGE_SIZE);
    if (pg < p->num_pages) {
      lseek(p->file_descriptor, pg * PAGE_SIZE, SEEK_SET);
      read(p->file_descriptor, page, PAGE_SIZE);
    }
    p->pages[pg] = page;
    if (pg >= p->num_pages)
      p->num_pages = pg + 1;
  }
  return p->pages[pg];
}

/*
 * Flushing ensures durability (the 'D' in ACID). 
 * We write dirty pages back to the filesystem so that data persists 
 * even if the application or system crashes later.
 */
void pager_flush(Pager *p, uint32_t pg) {
  if (p->pages[pg]) {
    lseek(p->file_descriptor, pg * PAGE_SIZE, SEEK_SET);
    write(p->file_descriptor, p->pages[pg], PAGE_SIZE);
  }
}

/*
 * We store the schema at the very beginning of the file (Page 0).
 * This makes the database file "self-describing," allowing the engine
 * to know how to parse rows without needing external configuration.
 */
void db_save_schema(Table *t) {
  void *page0 = get_page(t->pager, 0);
  memcpy(page0, &t->schema, sizeof(Schema));
  memcpy((char *)page0 + sizeof(Schema), &t->root_page_num, sizeof(uint32_t));
}

/*
 * db_open initializes the database state. If the file exists, it 
 * reconstructs the schema; if not, it initializes a new B-Tree root.
 */
Table *db_open(const char *filename) {
  Pager *p = pager_open(filename);
  Table *t = malloc(sizeof(Table));
  t->pager = p;
  if (p->num_pages > 0) {
    void *page0 = get_page(p, 0);
    memcpy(&t->schema, page0, sizeof(Schema));
    memcpy(&t->root_page_num, (char *)page0 + sizeof(Schema), sizeof(uint32_t));
    t->has_schema = (t->schema.num_fields > 0);
  } else {
    t->has_schema = false;
    t->root_page_num = 1;
    void *root = get_page(p, 1);
    initialize_leaf_node(root);
    set_node_root(root, true);
  }
  return t;
}

void db_close(Table *t) {
  db_save_schema(t);
  for (uint32_t i = 0; i < t->pager->num_pages; i++) {
    pager_flush(t->pager, i);
    free(t->pager->pages[i]);
  }
  close(t->pager->file_descriptor);
  free(t->pager);
  free(t);
}

Cursor *leaf_node_find(Table *t, uint32_t pg, uint32_t key) {
  void *node = get_page(t->pager, pg);
  uint32_t num = *leaf_node_num_cells(node);
  Cursor *c = malloc(sizeof(Cursor));
  c->table = t;
  c->page_num = pg;
  uint32_t min = 0, max = num;
  while (max != min) {
    uint32_t i = min + (max - min) / 2;
    uint32_t k = *leaf_node_key(node, i, &t->schema);
    if (key == k) {
      c->cell_num = i;
      return c;
    }
    if (key < k)
      max = i;
    else
      min = i + 1;
  }
  c->cell_num = min;
  return c;
}

uint32_t internal_node_find_child(void *node, uint32_t key) {
  uint32_t num = *internal_node_num_keys(node);
  for (uint32_t i = 0; i < num; i++)
    if (*internal_node_key(node, i) >= key)
      return i;
  return num;
}

/*
 * find_node traverses the B-Tree to find the leaf page containing a specific key.
 * B-Trees provide O(log n) search time, which is essential for maintaining
 * performance as the database grows to millions of records.
 */
Cursor *find_node(Table *t, uint32_t pg, uint32_t key) {
  void *node = get_page(t->pager, pg);
  if (get_node_type(node) == NODE_LEAF)
    return leaf_node_find(t, pg, key);
  return find_node(
      t, *internal_node_child(node, internal_node_find_child(node, key)), key);
}

/*
 * create_new_root handles the height increase of the B-Tree.
 * When the old root splits, we create a new internal node to be the parent
 * of the two resulting nodes, effectively growing the tree upwards.
 */
void create_new_root(Table *t, uint32_t r_pg) {
  void *root = get_page(t->pager, t->root_page_num);
  uint32_t l_pg = t->pager->num_pages;
  void *left = get_page(t->pager, l_pg);
  memcpy(left, root, PAGE_SIZE);
  set_node_root(left, false);
  initialize_internal_node(root);
  set_node_root(root, true);
  *internal_node_num_keys(root) = 1;
  *internal_node_child(root, 0) = l_pg;
  *internal_node_key(root, 0) = get_node_max_key(left, &t->schema);
  *internal_node_right_child(root) = r_pg;
}

/*
 * leaf_node_split_and_insert is the most complex part of B-Tree maintenance.
 * When a page is full, we must split it to make room for new data.
 * This ensures that no single page exceeds the 4KB limit and keeps
 * the tree balanced for consistent lookup times.
 */
void leaf_node_split_and_insert(Cursor *c, uint32_t key, Statement *s) {
  void *old_node = get_page(c->table->pager, c->page_num);
  uint32_t new_pg = c->table->pager->num_pages;
  void *new_node = get_page(c->table->pager, new_pg);
  initialize_leaf_node(new_node);
  *leaf_node_next_leaf(new_node) = *leaf_node_next_leaf(old_node);
  *leaf_node_next_leaf(old_node) = new_pg;

  uint32_t max_cells = (PAGE_SIZE - LEAF_NODE_HEADER_SIZE) /
                       leaf_node_cell_size(&c->table->schema);
  for (int32_t i = max_cells; i >= 0; i--) {
    void *dest_node =
        (i >= (int32_t)((max_cells + 1) / 2)) ? new_node : old_node;
    uint32_t dest_idx = i % ((max_cells + 1) / 2);
    if (i == (int32_t)c->cell_num) {
      *leaf_node_key(dest_node, dest_idx, &c->table->schema) = key;
      void *val_dest = leaf_node_value(dest_node, dest_idx, &c->table->schema);
      for (uint32_t f = 0; f < c->table->schema.num_fields; f++) {
        if (c->table->schema.fields[f].type == FIELD_INT)
          serialize_field(&c->table->schema, f, &s->insert_values[f], val_dest);
        else
          serialize_field(&c->table->schema, f, s->insert_strings[f], val_dest);
      }
    } else {
      int32_t src_idx = (i > (int32_t)c->cell_num) ? i - 1 : i;
      memcpy(leaf_node_cell(dest_node, dest_idx, &c->table->schema),
             leaf_node_cell(old_node, src_idx, &c->table->schema),
             leaf_node_cell_size(&c->table->schema));
    }
  }
  *leaf_node_num_cells(old_node) = (max_cells + 1) / 2;
  *leaf_node_num_cells(new_node) =
      (max_cells + 1) - *leaf_node_num_cells(old_node);
  if (is_node_root(old_node))
    create_new_root(c->table, new_pg);
  else
    printf("Internal split not implemented in dynamic mode.\n");
}

void leaf_node_insert(Cursor *c, uint32_t key, Statement *s) {
  void *node = get_page(c->table->pager, c->page_num);
  uint32_t num = *leaf_node_num_cells(node);
  uint32_t max_cells = (PAGE_SIZE - LEAF_NODE_HEADER_SIZE) /
                       leaf_node_cell_size(&c->table->schema);
  if (num >= max_cells) {
    leaf_node_split_and_insert(c, key, s);
    return;
  }
  if (c->cell_num < num) {
    for (uint32_t i = num; i > c->cell_num; i--)
      memcpy(leaf_node_cell(node, i, &c->table->schema),
             leaf_node_cell(node, i - 1, &c->table->schema),
             leaf_node_cell_size(&c->table->schema));
  }
  *leaf_node_num_cells(node) += 1;
  *leaf_node_key(node, c->cell_num, &c->table->schema) = key;
  void *val_dest = leaf_node_value(node, c->cell_num, &c->table->schema);
  for (uint32_t f = 0; f < c->table->schema.num_fields; f++) {
    if (c->table->schema.fields[f].type == FIELD_INT)
      serialize_field(&c->table->schema, f, &s->insert_values[f], val_dest);
    else
      serialize_field(&c->table->schema, f, s->insert_strings[f], val_dest);
  }
}

void leaf_node_delete(Cursor *c) {
  void *node = get_page(c->table->pager, c->page_num);
  uint32_t num = *leaf_node_num_cells(node);
  if (c->cell_num >= num)
    return;
  for (uint32_t i = c->cell_num; i < num - 1; i++) {
    memcpy(leaf_node_cell(node, i, &c->table->schema),
           leaf_node_cell(node, i + 1, &c->table->schema),
           leaf_node_cell_size(&c->table->schema));
  }
  *leaf_node_num_cells(node) -= 1;
}

int main(int argc, char *argv[]) {
  if (argc < 2)
    exit(EXIT_FAILURE);
  Table *t = db_open(argv[1]);
  char line[1024];
  while (printf("db > "), fgets(line, 1024, stdin)) {
    line[strcspn(line, "\n")] = 0;
    if (line[0] == '.') {
      if (strcmp(line, ".exit") == 0)
        break;
      continue;
    }
    if (strncmp(line, "create", 6) == 0) {
      t->schema.num_fields = 0;
      t->schema.row_size = 0;
      char *token = strtok(line + 7, " ");
      while (token) {
        Field *f = &t->schema.fields[t->schema.num_fields++];
        strcpy(f->name, token);
        token = strtok(NULL, " ");
        if (strcmp(token, "int") == 0) {
          f->type = FIELD_INT;
          f->size = 4;
        } else {
          f->type = FIELD_TEXT;
          f->size = 32;
        }
        f->offset = t->schema.row_size;
        t->schema.row_size += f->size;
        token = strtok(NULL, " ");
      }
      t->has_schema = true;
      printf("Table created.\n");
      continue;
    }
    if (strncmp(line, "insert", 6) == 0) {
      Statement s = {.type = STATEMENT_INSERT};
      char *token = strtok(line + 7, " ");
      for (uint32_t i = 0; i < t->schema.num_fields; i++) {
        if (token == NULL)
          break;
        if (t->schema.fields[i].type == FIELD_INT)
          s.insert_values[i] = atoi(token);
        else
          s.insert_strings[i] = strdup(token);
        token = strtok(NULL, " ");
      }
      Cursor *c = find_node(t, t->root_page_num, s.insert_values[0]);
      leaf_node_insert(c, s.insert_values[0], &s);
      free(c);
      printf("Executed.\n");
      continue;
    }
    if (strncmp(line, "delete", 6) == 0) {
      uint32_t id = atoi(line + 7);
      Cursor *c = find_node(t, t->root_page_num, id);
      void *node = get_page(t->pager, c->page_num);
      if (c->cell_num < *leaf_node_num_cells(node) &&
          *leaf_node_key(node, c->cell_num, &t->schema) == id) {
        leaf_node_delete(c);
        printf("Deleted.\n");
      } else
        printf("Key not found.\n");
      free(c);
      continue;
    }
    if (strncmp(line, "select", 6) == 0) {
      Cursor *c = find_node(t, t->root_page_num, 0);
      while (true) {
        void *node = get_page(t->pager, c->page_num);
        if (c->cell_num >= *leaf_node_num_cells(node))
          break;
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
        if (c->cell_num >= *leaf_node_num_cells(node)) {
          uint32_t next = *leaf_node_next_leaf(node);
          if (next == 0)
            break;
          c->page_num = next;
          c->cell_num = 0;
        }
      }
      free(c);
      printf("Executed.\n");
    }
  }
  db_close(t);
  return 0;
}
