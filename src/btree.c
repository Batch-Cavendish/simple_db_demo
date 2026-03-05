#include "btree.h"
#include "database.h"
#include "schema.h"
#include "statement.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

uint32_t leaf_node_max_cells(Schema *schema) {
  return (PAGE_SIZE - LEAF_NODE_HEADER_SIZE) / leaf_node_cell_size(schema);
}
uint32_t internal_node_max_keys() { return INTERNAL_NODE_MAX_KEYS; }

void initialize_leaf_node(void *node) {
  set_node_type(node, NODE_LEAF);
  set_node_root(node, false);
  *leaf_node_num_cells(node) = 0;
  *leaf_node_next_leaf(node) = 0;
  *node_parent(node) = 0;
}
void initialize_internal_node(void *node) {
  set_node_type(node, NODE_INTERNAL);
  set_node_root(node, false);
  *internal_node_num_keys(node) = 0;
  *node_parent(node) = 0;
}

static bool verify_node(Database *db, uint32_t table_index, uint32_t pg,
                        uint32_t parent_pg, uint32_t *min_key,
                        uint32_t *max_key) {
  void *node = get_page(db->pager, pg);
  NodeType type = get_node_type(node);

  if (*node_parent(node) != parent_pg && !is_node_root(node)) {
    printf("Verify error: node %u has parent %u, expected %u\n", pg,
           *node_parent(node), parent_pg);
    return false;
  }

  if (type == NODE_LEAF) {
    uint32_t num = *leaf_node_num_cells(node);
    for (uint32_t i = 0; i < num; i++) {
      uint32_t k =
          *leaf_node_key(node, i, &db->catalog.tables[table_index].schema);
      if (min_key && k < *min_key)
        return false;
      if (max_key && k > *max_key)
        return false;
      if (i > 0 && k < *leaf_node_key(node, i - 1,
                                      &db->catalog.tables[table_index].schema))
        return false;
    }
    return true;
  } else {
    uint32_t num = *internal_node_num_keys(node);
    for (uint32_t i = 0; i < num; i++) {
      uint32_t child_pg = *internal_node_child(node, i);
      uint32_t k = *internal_node_key(node, i);
      if (!verify_node(db, table_index, child_pg, pg,
                       i == 0 ? min_key : nullptr, &k))
        return false;
      if (i > 0 && k < *internal_node_key(node, i - 1))
        return false;
    }
    return verify_node(db, table_index, *internal_node_right_child(node), pg,
                       num > 0 ? internal_node_key(node, num - 1) : min_key,
                       max_key);
  }
}

bool verify_btree(Database *db, uint32_t table_index) {
  uint32_t root_pg = db->catalog.tables[table_index].root_page_num;
  return verify_node(db, table_index, root_pg, 0, nullptr, nullptr);
}

uint32_t get_node_max_key(Database *db, uint32_t table_index, void *node) {
  NodeType type = get_node_type(node);
  if (type == NODE_INTERNAL) {
    uint32_t right_child_pg = *internal_node_right_child(node);
    return get_node_max_key(db, table_index,
                            get_page(db->pager, right_child_pg));
  } else if (type == NODE_LEAF) {
    uint32_t num = *leaf_node_num_cells(node);
    if (num == 0)
      return 0;
    uint32_t key =
        *leaf_node_key(node, num - 1, &db->catalog.tables[table_index].schema);
    return key;
  }
  return 0;
}

static Cursor *leaf_node_find(Database *db, uint32_t table_index, uint32_t pg,
                              uint32_t key) {
  void *node = get_page(db->pager, pg);
  uint32_t num = *leaf_node_num_cells(node);
  Cursor *c = malloc(sizeof(Cursor));
  c->db = db;
  c->page_num = pg;
  c->table_index = table_index;
  uint32_t min = 0, max = num;
  while (max != min) {
    uint32_t i = min + (max - min) / 2;
    uint32_t k =
        *leaf_node_key(node, i, &db->catalog.tables[table_index].schema);
    if (key <= k)
      max = i;
    else
      min = i + 1;
  }
  c->cell_num = min;
  return c;
}

static uint32_t internal_node_find_child(void *node, uint32_t key) {
  uint32_t num = *internal_node_num_keys(node);
  for (uint32_t i = 0; i < num; i++)
    if (*internal_node_key(node, i) >= key)
      return i;
  return num;
}

Cursor *find_node(Database *db, uint32_t table_index, uint32_t pg,
                  uint32_t key) {
  void *node = get_page(db->pager, pg);
  if (get_node_type(node) == NODE_LEAF)
    return leaf_node_find(db, table_index, pg, key);
  return find_node(
      db, table_index,
      *internal_node_child(node, internal_node_find_child(node, key)), key);
}

void create_new_root(Database *db, uint32_t table_index, uint32_t r_pg) {
  uint32_t root_page_num = db->catalog.tables[table_index].root_page_num;
  void *root = get_page(db->pager, root_page_num);
  void *right_child = get_page(db->pager, r_pg);
  uint32_t l_pg = db->pager->num_pages;
  void *left_child = get_page(db->pager, l_pg);

  /* The old root is copied to the left child */
  memcpy(left_child, root, PAGE_SIZE);
  set_node_root(left_child, false);

  /* Update children's parent pointers if they are internal nodes */
  if (get_node_type(left_child) == NODE_INTERNAL) {
    for (uint32_t i = 0; i <= *internal_node_num_keys(left_child); i++) {
      void *child = get_page(db->pager, *internal_node_child(left_child, i));
      *node_parent(child) = l_pg;
    }
  }

  /* The root becomes a new internal node */
  initialize_internal_node(root);
  set_node_root(root, true);
  *internal_node_num_keys(root) = 1;
  *internal_node_child(root, 0) = l_pg;
  uint32_t l_max_key = get_node_max_key(db, table_index, left_child);
  *internal_node_key(root, 0) = l_max_key;
  *internal_node_right_child(root) = r_pg;
  *node_parent(left_child) = root_page_num;
  *node_parent(right_child) = root_page_num;

  mark_page_dirty(db->pager, root_page_num);
  mark_page_dirty(db->pager, l_pg);
  mark_page_dirty(db->pager, r_pg);
}

void internal_node_insert(Database *db, uint32_t table_index,
                          uint32_t parent_pg, uint32_t child_pg);

void internal_node_split_and_insert(Database *db, uint32_t table_index,
                                    uint32_t parent_pg, uint32_t child_pg) {
  uint32_t old_pg = parent_pg;
  void *old_node = get_page(db->pager, old_pg);

  void *child = get_page(db->pager, child_pg);
  uint32_t child_max_key = get_node_max_key(db, table_index, child);

  uint32_t new_pg = db->pager->num_pages;
  void *new_node = get_page(db->pager, new_pg);
  initialize_internal_node(new_node);

  uint32_t num_keys = *internal_node_num_keys(old_node);
  uint32_t split_idx = num_keys / 2;

  /* Move keys to the new node */
  for (uint32_t i = split_idx + 1; i < num_keys; i++) {
    uint32_t dest_idx = i - (split_idx + 1);
    memcpy(internal_node_cell(new_node, dest_idx),
           internal_node_cell(old_node, i), INTERNAL_NODE_CELL_SIZE);
    void *moved_child =
        get_page(db->pager, *internal_node_child(new_node, dest_idx));
    *node_parent(moved_child) = new_pg;
    mark_page_dirty(db->pager, *internal_node_child(new_node, dest_idx));
    (*internal_node_num_keys(new_node))++;
  }

  /* Handle the right child */
  *internal_node_right_child(new_node) = *internal_node_right_child(old_node);
  void *moved_right_child =
      get_page(db->pager, *internal_node_right_child(new_node));
  *node_parent(moved_right_child) = new_pg;
  mark_page_dirty(db->pager, *internal_node_right_child(new_node));

  *internal_node_right_child(old_node) =
      *internal_node_child(old_node, split_idx);
  *internal_node_num_keys(old_node) = split_idx;

  mark_page_dirty(db->pager, old_pg);
  mark_page_dirty(db->pager, new_pg);

  /* Insert the new child into one of the two nodes */
  uint32_t cur_max_old = get_node_max_key(db, table_index, old_node);
  if (child_max_key > cur_max_old)
    internal_node_insert(db, table_index, new_pg, child_pg);
  else
    internal_node_insert(db, table_index, old_pg, child_pg);

  if (is_node_root(old_node))
    create_new_root(db, table_index, new_pg);
  else
    internal_node_insert(db, table_index, *node_parent(old_node), new_pg);
}

void internal_node_insert(Database *db, uint32_t table_index,
                          uint32_t parent_pg, uint32_t child_pg) {
  assert(parent_pg != CATALOG_PAGE_NUM);
  void *parent = get_page(db->pager, parent_pg);
  void *child = get_page(db->pager, child_pg);
  uint32_t child_max_key = get_node_max_key(db, table_index, child);
  uint32_t index = internal_node_find_child(parent, child_max_key);

  uint32_t original_num_keys = *internal_node_num_keys(parent);
  if (original_num_keys >= internal_node_max_keys()) {
    internal_node_split_and_insert(db, table_index, parent_pg, child_pg);
    return;
  }

  uint32_t right_child_pg = *internal_node_right_child(parent);
  void *right_child = get_page(db->pager, right_child_pg);

  *internal_node_num_keys(parent) = original_num_keys + 1;

  if (child_max_key > get_node_max_key(db, table_index, right_child)) {
    /* Replace right child */
    *internal_node_child(parent, original_num_keys) = right_child_pg;
    *internal_node_key(parent, original_num_keys) =
        get_node_max_key(db, table_index, right_child);
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
  mark_page_dirty(db->pager, parent_pg);
  mark_page_dirty(db->pager, child_pg);
}

void leaf_node_split_and_insert(Cursor *c, uint32_t key, Statement *s) {
  void *old_node = get_page(c->db->pager, c->page_num);
  uint32_t old_max_key = get_node_max_key(c->db, c->table_index, old_node);
  uint32_t new_pg = c->db->pager->num_pages;
  void *new_node = get_page(c->db->pager, new_pg);
  initialize_leaf_node(new_node);
  *node_parent(new_node) = *node_parent(old_node);
  *leaf_node_next_leaf(new_node) = *leaf_node_next_leaf(old_node);
  *leaf_node_next_leaf(old_node) = new_pg;

  Schema *schema = &c->db->catalog.tables[c->table_index].schema;

  uint32_t max_cells = leaf_node_max_cells(schema);
  for (int32_t i = max_cells; i >= 0; i--) {
    void *dest_node =
        (i >= (int32_t)((max_cells + 1) / 2)) ? new_node : old_node;
    uint32_t dest_idx = i - ((i >= (int32_t)((max_cells + 1) / 2))
                                 ? (int32_t)((max_cells + 1) / 2)
                                 : 0);
    if (i == (int32_t)c->cell_num) {
      *leaf_node_key(dest_node, dest_idx, schema) = key;
      void *val_dest = leaf_node_value(dest_node, dest_idx, schema);
      for (uint32_t f = 0; f < schema->num_fields; f++) {
        if (schema->fields[f].type == FIELD_INT)
          serialize_field(schema, f, &s->insert_values[f], val_dest);
        else
          serialize_field(schema, f, s->insert_strings[f], val_dest);
      }
    } else {
      int32_t src_idx = (i > (int32_t)c->cell_num) ? i - 1 : i;
      memcpy(leaf_node_cell(dest_node, dest_idx, schema),
             leaf_node_cell(old_node, src_idx, schema),
             leaf_node_cell_size(schema));
    }
  }
  *leaf_node_num_cells(old_node) = (max_cells + 1) / 2;
  *leaf_node_num_cells(new_node) = (max_cells + 1) - *leaf_node_num_cells(old_node);

  mark_page_dirty(c->db->pager, c->page_num);
  mark_page_dirty(c->db->pager, new_pg);

  if (is_node_root(old_node))
    create_new_root(c->db, c->table_index, new_pg);
  else {
    uint32_t parent_pg = *node_parent(old_node);
    void *parent = get_page(c->db->pager, parent_pg);

    /* Update the key in the parent for the old node if needed */
    uint32_t old_node_idx = internal_node_find_child(parent, old_max_key);
    if (old_node_idx < *internal_node_num_keys(parent)) {
      *internal_node_key(parent, old_node_idx) =
          get_node_max_key(c->db, c->table_index, old_node);
      mark_page_dirty(c->db->pager, parent_pg);
    }

    internal_node_insert(c->db, c->table_index, parent_pg, new_pg);
  }
}

void leaf_node_insert(Cursor *c, uint32_t key, Statement *s) {
  void *node = get_page(c->db->pager, c->page_num);
  uint32_t num = *leaf_node_num_cells(node);
  Schema *schema = &c->db->catalog.tables[c->table_index].schema;
  uint32_t max_cells = leaf_node_max_cells(schema);
  if (num >= max_cells) {
    leaf_node_split_and_insert(c, key, s);
    return;
  }
  if (c->cell_num < num) {
    for (uint32_t i = num; i > c->cell_num; i--)
      memcpy(leaf_node_cell(node, i, schema),
             leaf_node_cell(node, i - 1, schema), leaf_node_cell_size(schema));
  }
  *leaf_node_num_cells(node) += 1;
  *leaf_node_key(node, c->cell_num, schema) = key;
  void *val_dest = leaf_node_value(node, c->cell_num, schema);
  for (uint32_t f = 0; f < schema->num_fields; f++) {
    if (schema->fields[f].type == FIELD_INT)
      serialize_field(schema, f, &s->insert_values[f], val_dest);
    else
      serialize_field(schema, f, s->insert_strings[f], val_dest);
  }
  mark_page_dirty(c->db->pager, c->page_num);
}

void leaf_node_delete(Cursor *c) {
  void *node = get_page(c->db->pager, c->page_num);
  uint32_t num = *leaf_node_num_cells(node);
  Schema *schema = &c->db->catalog.tables[c->table_index].schema;
  if (c->cell_num >= num)
    return;
  for (uint32_t i = c->cell_num; i < num - 1; i++) {
    memcpy(leaf_node_cell(node, i, schema), leaf_node_cell(node, i + 1, schema),
           leaf_node_cell_size(schema));
  }
  *leaf_node_num_cells(node) -= 1;
  mark_page_dirty(c->db->pager, c->page_num);
}
