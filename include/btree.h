#ifndef BTREE_H
#define BTREE_H

#include "common.h"
#include "table.h"

/* Node Accessors */
NodeType get_node_type(void *node);
void set_node_type(void *node, NodeType type);
bool is_node_root(void *node);
void set_node_root(void *node, bool is_root);
uint32_t *node_parent(void *node);

/* Leaf Node Accessors */
uint32_t *leaf_node_num_cells(void *node);
uint32_t *leaf_node_next_leaf(void *node);

/* Internal Node Accessors */
uint32_t *internal_node_num_keys(void *node);
uint32_t *internal_node_right_child(void *node);

uint32_t *internal_node_cell(void *node, uint32_t cell_num);
uint32_t *internal_node_child(void *node, uint32_t child_num);
uint32_t *internal_node_key(void *node, uint32_t key_num);

/* Dynamic Cell Accessors */
uint32_t leaf_node_cell_size(Schema *schema);
void *leaf_node_cell(void *node, uint32_t cell_num, Schema *schema);
uint32_t *leaf_node_key(void *node, uint32_t cell_num, Schema *schema);
void *leaf_node_value(void *node, uint32_t cell_num, Schema *schema);

void initialize_leaf_node(void *node);
void initialize_internal_node(void *node);

uint32_t get_node_max_key(Table *t, void *node);

/**
 * find_node traverses the B-Tree to find the leaf page containing a specific key.
 * B-Trees provide O(log n) search time, which is essential for maintaining
 * performance as the database grows to millions of records.
 */
Cursor *find_node(Table *t, uint32_t pg, uint32_t key);

struct Statement;
void leaf_node_insert(Cursor *c, uint32_t key, struct Statement *s);
void leaf_node_delete(Cursor *c);

#endif
