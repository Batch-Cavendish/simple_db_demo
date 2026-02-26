#include "btree.h"
#include "schema.h"
#include "statement.h"
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

Cursor *find_node(Table *t, uint32_t pg, uint32_t key) {
  auto node = get_page(t->pager, pg);
  if (get_node_type(node) == NODE_LEAF)
    return leaf_node_find(t, pg, key);
  return find_node(
      t, *internal_node_child(node, internal_node_find_child(node, key)), key);
}

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
    void *moved_child =
        get_page(t->pager, *internal_node_child(new_node, dest_idx));
    *node_parent(moved_child) = new_pg;
    mark_page_dirty(t->pager, *internal_node_child(new_node, dest_idx));
    (*internal_node_num_keys(new_node))++;
  }

  /* Handle the right child */
  *internal_node_right_child(new_node) = *internal_node_right_child(old_node);
  void *moved_right_child =
      get_page(t->pager, *internal_node_right_child(new_node));
  *node_parent(moved_right_child) = new_pg;
  mark_page_dirty(t->pager, *internal_node_right_child(new_node));

  *internal_node_right_child(old_node) =
      *internal_node_child(old_node, split_idx);
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
    uint32_t dest_idx = i - ((i >= (int32_t)((max_cells + 1) / 2))
                                 ? (int32_t)((max_cells + 1) / 2)
                                 : 0);
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
