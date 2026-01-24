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
#define TABLE_MAX_PAGES 1000
#define MAX_PAGES_IN_MEMORY 100
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
  uint32_t last_used[TABLE_MAX_PAGES];
  bool is_dirty[TABLE_MAX_PAGES];
  uint32_t pinned[TABLE_MAX_PAGES];
  uint32_t timer;
} Pager;

void pager_flush(Pager *p, uint32_t pg);
void mark_page_dirty(Pager *p, uint32_t pg);
void pin_page(Pager *p, uint32_t pg);
void unpin_page(Pager *p, uint32_t pg);
void unpin_page_all(Pager *p);
void *get_page(Pager *p, uint32_t pg);

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

/* DJB2 Hash Function */
uint32_t hash_string(const char *str) {
  uint32_t hash = 5381;
  int c;
  while ((c = *str++))
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  return hash;
}

PrepareResult prepare_insert(char *line, Statement *statement, Schema *schema) {
  statement->type = STATEMENT_INSERT;
  char *token = strtok(line, " "); // Skip "insert" keyword
  token = strtok(NULL, " ");
  
  for (uint32_t i = 0; i < schema->num_fields; i++) {
    if (token == NULL) return PREPARE_SYNTAX_ERROR;
    if (schema->fields[i].type == FIELD_INT) {
      statement->insert_values[i] = atoi(token);
    } else {
      statement->insert_strings[i] = strdup(token);
      // If the primary key (first field) is TEXT, hash it to use as the B-Tree key
      if (i == 0) {
        statement->insert_values[i] = hash_string(token);
      }
    }
    token = strtok(NULL, " ");
  }
  return PREPARE_SUCCESS;
}

PrepareResult prepare_create(char *line, Statement *statement) {
  statement->type = STATEMENT_CREATE_TABLE;
  statement->new_schema.num_fields = 0;
  statement->new_schema.row_size = 0;
  
  char *token = strtok(line, " "); // Skip "create"
  token = strtok(NULL, " ");
  
  while (token) {
    Field *f = &statement->new_schema.fields[statement->new_schema.num_fields++];
    strncpy(f->name, token, FIELD_NAME_MAX - 1);
    token = strtok(NULL, " ");
    if (token == NULL) return PREPARE_SYNTAX_ERROR;
    
    if (strcmp(token, "int") == 0) {
      f->type = FIELD_INT;
      f->size = 4;
    } else {
      f->type = FIELD_TEXT;
      f->size = 32;
    }
    f->offset = statement->new_schema.row_size;
    statement->new_schema.row_size += f->size;
    token = strtok(NULL, " ");
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
    token = strtok(NULL, " ");
    if (token == NULL) {
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
    token = strtok(NULL, " ");
    if (token == NULL) return PREPARE_SYNTAX_ERROR;
    
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

void serialize_field(Schema *schema, uint32_t field_idx, void *val,
                     void *dest) {
  Field *f = &schema->fields[field_idx];
  if (f->type == FIELD_TEXT) {
    memset((char *)dest + f->offset, 0, f->size);
    strncpy((char *)dest + f->offset, (char *)val, f->size - 1);
  } else {
    memcpy((char *)dest + f->offset, val, f->size);
  }
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
  p->timer = 0;
  for (int i = 0; i < TABLE_MAX_PAGES; i++) {
    p->pages[i] = NULL;
    p->last_used[i] = 0;
    p->is_dirty[i] = false;
    p->pinned[i] = 0;
  }
  return p;
}

void pin_page(Pager *p, uint32_t pg) {
  p->pinned[pg]++;
}

void unpin_page(Pager *p, uint32_t pg) {
  if (p->pinned[pg] > 0) p->pinned[pg]--;
}

void unpin_page_all(Pager *p) {
  for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
    p->pinned[i] = 0;
  }
}

void pager_flush(Pager *p, uint32_t pg) {
  if (p->pages[pg] && p->is_dirty[pg]) {
    lseek(p->file_descriptor, pg * PAGE_SIZE, SEEK_SET);
    write(p->file_descriptor, p->pages[pg], PAGE_SIZE);
    p->is_dirty[pg] = false;
  }
}

void mark_page_dirty(Pager *p, uint32_t pg) {
  p->is_dirty[pg] = true;
}

/*
 * get_page implements a Buffer Pool with LRU eviction and pinning.
 * Accessing RAM is orders of magnitude faster than disk, but RAM is limited.
 * When we need a new page and the buffer pool is full, we evict the
 * "Least Recently Used" page that is not currently pinned.
 */
void *get_page(Pager *p, uint32_t pg) {
  p->timer++;
  if (p->pages[pg] == NULL) {
    uint32_t pages_in_memory = 0;
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
      if (p->pages[i]) pages_in_memory++;
    }

    if (pages_in_memory >= MAX_PAGES_IN_MEMORY) {
      uint32_t victim = 0xFFFFFFFF;
      uint32_t min_time = 0xFFFFFFFF;
      for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
        if (p->pages[i] && p->pinned[i] == 0 && p->last_used[i] < min_time) {
          min_time = p->last_used[i];
          victim = i;
        }
      }
      
      if (victim == 0xFFFFFFFF) {
        printf("Buffer pool full and all pages are pinned! Cannot load page %d\n", pg);
        exit(EXIT_FAILURE);
      }

      pager_flush(p, victim);
      free(p->pages[victim]);
      p->pages[victim] = NULL;
    }

    void *page = malloc(PAGE_SIZE);
    if (pg < p->num_pages) {
      lseek(p->file_descriptor, pg * PAGE_SIZE, SEEK_SET);
      read(p->file_descriptor, page, PAGE_SIZE);
    }
    p->pages[pg] = page;
    p->is_dirty[pg] = false;
    if (pg >= p->num_pages)
      p->num_pages = pg + 1;
  }
  p->last_used[pg] = p->timer;
  pin_page(p, pg);
  return p->pages[pg];
}

uint32_t get_node_max_key(Table *t, void *node) {
  NodeType type = get_node_type(node);
  if (type == NODE_INTERNAL) {
    uint32_t right_child_pg = *internal_node_right_child(node);
    return get_node_max_key(t, get_page(t->pager, right_child_pg));
  } else if (type == NODE_LEAF) {
    uint32_t num = *leaf_node_num_cells(node);
    if (num == 0)
      return 0;
    uint32_t key = *leaf_node_key(node, num - 1, &t->schema);
    return key;
  }
  return 0;
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
  mark_page_dirty(t->pager, 0);
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
    mark_page_dirty(p, 1);
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
    if (key <= k)
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

const uint32_t INTERNAL_NODE_MAX_KEYS = 510;

/*
 * create_new_root handles the height increase of the B-Tree.
 * When the old root splits, we create a new internal node to be the parent
 * of the two resulting nodes, effectively growing the tree upwards.
 */
void create_new_root(Table *t, uint32_t r_pg) {
  void *root = get_page(t->pager, t->root_page_num);
  void *right_child = get_page(t->pager, r_pg);
  uint32_t l_pg = t->pager->num_pages;
  void *left_child = get_page(t->pager, l_pg);

  /* The old root is copied to the left child */
  memcpy(left_child, root, PAGE_SIZE);
  set_node_root(left_child, false);

  /* Update children's parent pointers if they are internal nodes */
  if (get_node_type(left_child) == NODE_INTERNAL) {
    for (uint32_t i = 0; i <= *internal_node_num_keys(left_child); i++) {
      void *child = get_page(t->pager, *internal_node_child(left_child, i));
      *node_parent(child) = l_pg;
    }
  }

  /* The root becomes a new internal node */
  initialize_internal_node(root);
  set_node_root(root, true);
  *internal_node_num_keys(root) = 1;
  *internal_node_child(root, 0) = l_pg;
  uint32_t l_max_key = get_node_max_key(t, left_child);
  *internal_node_key(root, 0) = l_max_key;
  *internal_node_right_child(root) = r_pg;
  *node_parent(left_child) = t->root_page_num;
  *node_parent(right_child) = t->root_page_num;

  mark_page_dirty(t->pager, t->root_page_num);
  mark_page_dirty(t->pager, l_pg);
  mark_page_dirty(t->pager, r_pg);
}

void internal_node_insert(Table *t, uint32_t parent_pg, uint32_t child_pg);

/*
 * internal_node_split_and_insert handles splitting an internal node when it 
 * exceeds INTERNAL_NODE_MAX_KEYS. This is how the B-Tree maintains its 
 * logarithmic height and balance.
 */
void internal_node_split_and_insert(Table *t, uint32_t parent_pg,
                                    uint32_t child_pg) {
  uint32_t old_pg = parent_pg;
  void *old_node = get_page(t->pager, old_pg);

  void *child = get_page(t->pager, child_pg);
  uint32_t child_max_key = get_node_max_key(t, child);

  uint32_t new_pg = t->pager->num_pages;
  void *new_node = get_page(t->pager, new_pg);
  initialize_internal_node(new_node);

  uint32_t num_keys = *internal_node_num_keys(old_node);
  uint32_t split_idx = num_keys / 2;

  /* Move keys to the new node */
  for (uint32_t i = split_idx + 1; i < num_keys; i++) {
    uint32_t dest_idx = i - (split_idx + 1);
    memcpy(internal_node_cell(new_node, dest_idx),
           internal_node_cell(old_node, i), INTERNAL_NODE_CELL_SIZE);
    void *moved_child = get_page(t->pager, *internal_node_child(new_node, dest_idx));
    *node_parent(moved_child) = new_pg;
    mark_page_dirty(t->pager, *internal_node_child(new_node, dest_idx));
    (*internal_node_num_keys(new_node))++;
  }

  /* Handle the right child */
  *internal_node_right_child(new_node) = *internal_node_right_child(old_node);
  void *moved_right_child = get_page(t->pager, *internal_node_right_child(new_node));
  *node_parent(moved_right_child) = new_pg;
  mark_page_dirty(t->pager, *internal_node_right_child(new_node));

  *internal_node_right_child(old_node) = *internal_node_child(old_node, split_idx);
  *internal_node_num_keys(old_node) = split_idx;

  mark_page_dirty(t->pager, old_pg);
  mark_page_dirty(t->pager, new_pg);

  /* Insert the new child into one of the two nodes */
  uint32_t cur_max_old = get_node_max_key(t, old_node);
  if (child_max_key > cur_max_old)
    internal_node_insert(t, new_pg, child_pg);
  else
    internal_node_insert(t, old_pg, child_pg);

  if (is_node_root(old_node))
    create_new_root(t, new_pg);
  else
    internal_node_insert(t, *node_parent(old_node), new_pg);
}

/*
 * internal_node_insert adds a new child pointer to an internal node.
 * If the node is full, it triggers a split.
 */
void internal_node_insert(Table *t, uint32_t parent_pg, uint32_t child_pg) {
  void *parent = get_page(t->pager, parent_pg);
  void *child = get_page(t->pager, child_pg);
  uint32_t child_max_key = get_node_max_key(t, child);
  uint32_t index = internal_node_find_child(parent, child_max_key);

  uint32_t original_num_keys = *internal_node_num_keys(parent);
  if (original_num_keys >= INTERNAL_NODE_MAX_KEYS) {
    internal_node_split_and_insert(t, parent_pg, child_pg);
    return;
  }

  uint32_t right_child_pg = *internal_node_right_child(parent);
  void *right_child = get_page(t->pager, right_child_pg);

  *internal_node_num_keys(parent) = original_num_keys + 1;

  if (child_max_key > get_node_max_key(t, right_child)) {
    /* Replace right child */
    *internal_node_child(parent, original_num_keys) = right_child_pg;
    *internal_node_key(parent, original_num_keys) =
        get_node_max_key(t, right_child);
    *internal_node_right_child(parent) = child_pg;
  } else {
    /* Make room for new cell */
    for (uint32_t i = original_num_keys; i > index; i--) {
      memcpy(internal_node_cell(parent, i), internal_node_cell(parent, i - 1),
             INTERNAL_NODE_CELL_SIZE);
    }
    *internal_node_child(parent, index) = child_pg;
    *internal_node_key(parent, index) = child_max_key;
  }
  *node_parent(child) = parent_pg;
  mark_page_dirty(t->pager, parent_pg);
  mark_page_dirty(t->pager, child_pg);
}

/*
 * leaf_node_split_and_insert is the most complex part of B-Tree maintenance.
 * When a page is full, we must split it to make room for new data.
 * This ensures that no single page exceeds the 4KB limit and keeps
 * the tree balanced for consistent lookup times.
 */
void leaf_node_split_and_insert(Cursor *c, uint32_t key, Statement *s) {
  void *old_node = get_page(c->table->pager, c->page_num);
  uint32_t old_max_key = get_node_max_key(c->table, old_node);
  uint32_t new_pg = c->table->pager->num_pages;
  void *new_node = get_page(c->table->pager, new_pg);
  initialize_leaf_node(new_node);
  *node_parent(new_node) = *node_parent(old_node);
  *leaf_node_next_leaf(new_node) = *leaf_node_next_leaf(old_node);
  *leaf_node_next_leaf(old_node) = new_pg;

  uint32_t max_cells = (PAGE_SIZE - LEAF_NODE_HEADER_SIZE) /
                       leaf_node_cell_size(&c->table->schema);
  for (int32_t i = max_cells; i >= 0; i--) {
    void *dest_node =
        (i >= (int32_t)((max_cells + 1) / 2)) ? new_node : old_node;
    uint32_t dest_idx = i - ((i >= (int32_t)((max_cells + 1) / 2)) ? (int32_t)((max_cells + 1) / 2) : 0);
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
  *leaf_node_num_cells(new_node) =
      (max_cells + 1) - *leaf_node_num_cells(old_node);

  mark_page_dirty(c->table->pager, c->page_num);
  mark_page_dirty(c->table->pager, new_pg);

  if (is_node_root(old_node))
    create_new_root(c->table, new_pg);
  else {
    uint32_t parent_pg = *node_parent(old_node);
    void *parent = get_page(c->table->pager, parent_pg);

    /* Update the key in the parent for the old node if needed */
    uint32_t old_node_idx = internal_node_find_child(parent, old_max_key);
    if (old_node_idx < *internal_node_num_keys(parent)) {
      *internal_node_key(parent, old_node_idx) =
          get_node_max_key(c->table, old_node);
      mark_page_dirty(c->table->pager, parent_pg);
    }

    internal_node_insert(c->table, parent_pg, new_pg);
  }
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
  mark_page_dirty(c->table->pager, c->page_num);
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
  mark_page_dirty(c->table->pager, c->page_num);
}

/*
 * table_start returns a cursor at the very first record of the table.
 * Since the B-Tree is ordered, this is the leftmost cell of the leftmost leaf.
 */
Cursor *table_start(Table *t) {
  uint32_t pg = t->root_page_num;
  void *node = get_page(t->pager, pg);
  while (get_node_type(node) != NODE_LEAF) {
    pg = *internal_node_child(node, 0);
    node = get_page(t->pager, pg);
  }
  Cursor *c = malloc(sizeof(Cursor));
  c->table = t;
  c->page_num = pg;
  c->cell_num = 0;
  return c;
}

/*
 * execute_insert runs the B-Tree insertion logic.
 */
ExecuteResult execute_insert(Statement *statement, Table *t) {
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

ExecuteResult execute_select(Statement *statement, Table *t) {
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
    // Since find_node is lower_bound, we might land on a key > target if target doesn't exist,
    // or on the first duplicate. We stop if key > target.
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
  // Unpinning is handled by get_page's simplistic LRU or manual unpin_page_all calls if we had them. 
  // For this version (simple LRU), we rely on eventual eviction.
  return EXECUTE_SUCCESS;
}

ExecuteResult execute_delete(Statement *statement, Table *t) {
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

ExecuteResult execute_create(Statement *statement, Table *t) {
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

int main(int argc, char *argv[]) {
  if (argc < 2)
    exit(EXIT_FAILURE);
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

    Statement statement;
    PrepareResult prepare_result = prepare_statement(line, &statement, &t->schema);

    switch (prepare_result) {
      case PREPARE_SUCCESS:
        break;
      case PREPARE_SYNTAX_ERROR:
        printf("Syntax error. Could not parse statement.\n");
        continue;
      case PREPARE_UNRECOGNIZED_STATEMENT:
        printf("Unrecognized keyword at start of '%s'.\n", line);
        continue;
    }

    ExecuteResult execute_result = execute_statement(&statement, t);
    switch (execute_result) {
      case EXECUTE_SUCCESS:
        // Already printed internal messages if any
        break;
      case EXECUTE_TABLE_FULL:
        printf("Error: Table full.\n");
        break;
      case EXECUTE_DUPLICATE_KEY:
        printf("Error: Duplicate key.\n");
        break;
      case EXECUTE_UNKNOWN_ERROR:
        printf("Unknown error.\n");
        break;
    }
  }
  db_close(t);
  return 0;
}
