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

static void leaf_node_move_cells(void *dest_node, uint32_t dest_cell_num,
                                 void *src_node, uint32_t src_cell_num,
                                 uint32_t num_cells, Schema *schema) {
  if (num_cells == 0)
    return;
  memcpy(leaf_node_cell(dest_node, dest_cell_num, schema),
         leaf_node_cell(src_node, src_cell_num, schema),
         num_cells * leaf_node_cell_size(schema));
}

static void internal_node_move_cells(void *dest_node, uint32_t dest_cell_num,
                                     void *src_node, uint32_t src_cell_num,
                                     uint32_t num_cells) {
  if (num_cells == 0)
    return;
  memcpy(internal_node_cell(dest_node, dest_cell_num),
         internal_node_cell(src_node, src_cell_num),
         num_cells * INTERNAL_NODE_CELL_SIZE);
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
    return *leaf_node_key(node, num - 1,
                          &db->catalog.tables[table_index].schema);
  }
  return 0;
}

uint32_t internal_node_find_child(void *node, uint32_t key) {
  uint32_t num_keys = *internal_node_num_keys(node);
  uint32_t min_idx = 0;
  uint32_t max_idx = num_keys;
  while (min_idx != max_idx) {
    uint32_t idx = (min_idx + max_idx) / 2;
    uint32_t key_to_right = *internal_node_key(node, idx);
    if (key_to_right >= key)
      max_idx = idx;
    else
      min_idx = idx + 1;
  }
  return min_idx;
}

Cursor *find_node(Database *db, uint32_t table_index, uint32_t pg,
                  uint32_t key) {
  void *node = get_page(db->pager, pg);
  NodeType type = get_node_type(node);
  if (type == NODE_LEAF) {
    Cursor *c = malloc(sizeof(Cursor));
    c->db = db;
    c->page_num = pg;
    c->table_index = table_index;
    uint32_t num_cells = *leaf_node_num_cells(node);
    uint32_t min_idx = 0;
    uint32_t max_idx = num_cells;
    while (min_idx != max_idx) {
      uint32_t idx = (min_idx + max_idx) / 2;
      uint32_t key_at_index =
          *leaf_node_key(node, idx, &db->catalog.tables[table_index].schema);
      if (key_at_index >= key)
        max_idx = idx;
      else
        min_idx = idx + 1;
    }
    c->cell_num = min_idx;
    return c;
  } else {
    uint32_t child_idx = internal_node_find_child(node, key);
    uint32_t child_pg = *internal_node_child(node, child_idx);
    return find_node(db, table_index, child_pg, key);
  }
}

void create_new_root(Database *db, uint32_t table_index,
                     uint32_t right_child_pg) {
  uint32_t root_pg = db->catalog.tables[table_index].root_page_num;
  void *root = get_page(db->pager, root_pg);
  void *right_child = get_page(db->pager, right_child_pg);
  uint32_t left_child_pg = db->pager->num_pages;
  void *left_child = get_page(db->pager, left_child_pg);

  memcpy(left_child, root, PAGE_SIZE);
  set_node_root(left_child, false);

  initialize_internal_node(root);
  set_node_root(root, true);
  *internal_node_num_keys(root) = 1;
  *internal_node_child(root, 0) = left_child_pg;
  uint32_t left_child_max_key = get_node_max_key(db, table_index, left_child);
  *internal_node_key(root, 0) = left_child_max_key;
  *internal_node_right_child(root) = right_child_pg;
  *node_parent(left_child) = root_pg;
  *node_parent(right_child) = root_pg;

  mark_page_dirty(db->pager, root_pg);
  mark_page_dirty(db->pager, left_child_pg);
}

void internal_node_insert(Database *db, uint32_t table_index,
                          uint32_t parent_pg, uint32_t child_pg);

void internal_node_split_and_insert(Database *db, uint32_t table_index,
                                    uint32_t parent_pg, uint32_t child_pg) {
  uint32_t old_pg = parent_pg;
  void *old_node = get_page(db->pager, old_pg);
  uint32_t old_max_key = get_node_max_key(db, table_index, old_node);

  uint32_t new_pg = db->pager->num_pages;
  void *new_node = get_page(db->pager, new_pg);
  initialize_internal_node(new_node);

  uint32_t num_keys = *internal_node_num_keys(old_node);
  uint32_t split_idx = num_keys / 2;

  // Split keys and children between old and new internal nodes
  *internal_node_num_keys(new_node) = num_keys - split_idx - 1;
  internal_node_move_cells(new_node, 0, old_node, split_idx + 1,
                           *internal_node_num_keys(new_node));
  *internal_node_right_child(new_node) = *internal_node_right_child(old_node);
  *internal_node_right_child(old_node) =
      *internal_node_child(old_node, split_idx);
  *internal_node_num_keys(old_node) = split_idx;

  // Update parents of children that moved
  for (uint32_t i = 0; i <= *internal_node_num_keys(new_node); i++) {
    uint32_t cpg = *internal_node_child(new_node, i);
    *node_parent(get_page(db->pager, cpg)) = new_pg;
    mark_page_dirty(db->pager, cpg);
  }

  uint32_t child_max_key =
      get_node_max_key(db, table_index, get_page(db->pager, child_pg));
  if (child_max_key < get_node_max_key(db, table_index, old_node)) {
    internal_node_insert(db, table_index, old_pg, child_pg);
  } else {
    internal_node_insert(db, table_index, new_pg, child_pg);
  }

  if (is_node_root(old_node)) {
    create_new_root(db, table_index, new_pg);
  } else {
    uint32_t p_pg = *node_parent(old_node);
    void *parent = get_page(db->pager, p_pg);
    uint32_t idx = internal_node_find_child(parent, old_max_key);
    *internal_node_key(parent, idx) =
        get_node_max_key(db, table_index, old_node);
    internal_node_insert(db, table_index, p_pg, new_pg);
  }
}

void internal_node_insert(Database *db, uint32_t table_index,
                          uint32_t parent_pg, uint32_t child_pg) {
  void *parent = get_page(db->pager, parent_pg);
  void *child = get_page(db->pager, child_pg);
  uint32_t child_max_key = get_node_max_key(db, table_index, child);
  uint32_t index = internal_node_find_child(parent, child_max_key);

  uint32_t num_keys = *internal_node_num_keys(parent);
  if (num_keys >= INTERNAL_NODE_MAX_KEYS) {
    internal_node_split_and_insert(db, table_index, parent_pg, child_pg);
    return;
  }

  uint32_t right_child_pg = *internal_node_right_child(parent);
  void *right_child = get_page(db->pager, right_child_pg);

  if (child_max_key > get_node_max_key(db, table_index, right_child)) {
    // New child becomes the new right child
    *internal_node_child(parent, num_keys) = right_child_pg;
    *internal_node_key(parent, num_keys) =
        get_node_max_key(db, table_index, right_child);
    *internal_node_right_child(parent) = child_pg;
  } else {
    // Shift keys/children to make room
    for (uint32_t i = num_keys; i > index; i--) {
      *internal_node_cell(parent, i) = *internal_node_cell(parent, i - 1);
    }
    *internal_node_child(parent, index) = child_pg;
    *internal_node_key(parent, index) = child_max_key;
  }
  *internal_node_num_keys(parent) += 1;
  *node_parent(child) = parent_pg;
  mark_page_dirty(db->pager, parent_pg);
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
  uint32_t split_idx = (max_cells + 1) / 2;

  // Use a temporary buffer to avoid corruption during split
  void *temp_cells = malloc(PAGE_SIZE);
  uint32_t total_cells = max_cells + 1;

  for (uint32_t i = 0; i < total_cells; i++) {
    void *dest = (char *)temp_cells + i * leaf_node_cell_size(schema);
    if (i == c->cell_num) {
      *(uint32_t *)dest = key;
      serialize_row(schema, s, (char *)dest + sizeof(uint32_t));
    } else {
      uint32_t src_idx = (i > c->cell_num) ? i - 1 : i;
      memcpy(dest, leaf_node_cell(old_node, src_idx, schema),
             leaf_node_cell_size(schema));
    }
  }

  // Copy from temp back to old and new nodes
  *leaf_node_num_cells(old_node) = split_idx;
  memcpy(leaf_node_cell(old_node, 0, schema), temp_cells,
         split_idx * leaf_node_cell_size(schema));

  *leaf_node_num_cells(new_node) = total_cells - split_idx;
  memcpy(leaf_node_cell(new_node, 0, schema),
         (char *)temp_cells + split_idx * leaf_node_cell_size(schema),
         *leaf_node_num_cells(new_node) * leaf_node_cell_size(schema));

  free(temp_cells);
  mark_page_dirty(c->db->pager, c->page_num);
  mark_page_dirty(c->db->pager, new_pg);

  if (is_node_root(old_node))
    create_new_root(c->db, c->table_index, new_pg);
  else {
    uint32_t parent_pg = *node_parent(old_node);
    void *parent = get_page(c->db->pager, parent_pg);
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
  if (num >= leaf_node_max_cells(schema)) {
    leaf_node_split_and_insert(c, key, s);
    return;
  }
  if (c->cell_num < num) {
    leaf_node_move_cells(node, c->cell_num + 1, node, c->cell_num,
                         num - c->cell_num, schema);
  }
  *leaf_node_num_cells(node) += 1;
  *leaf_node_key(node, c->cell_num, schema) = key;
  serialize_row(schema, s, leaf_node_value(node, c->cell_num, schema));
  mark_page_dirty(c->db->pager, c->page_num);
}

void leaf_node_delete(Cursor *c) {
  void *node = get_page(c->db->pager, c->page_num);
  uint32_t num = *leaf_node_num_cells(node);
  Schema *schema = &c->db->catalog.tables[c->table_index].schema;
  if (c->cell_num >= num)
    return;
  leaf_node_move_cells(node, c->cell_num, node, c->cell_num + 1,
                       num - c->cell_num - 1, schema);
  *leaf_node_num_cells(node) -= 1;
  mark_page_dirty(c->db->pager, c->page_num);
}
