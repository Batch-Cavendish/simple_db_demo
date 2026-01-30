#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

constexpr size_t PAGE_SIZE = 4096;
constexpr int TABLE_MAX_PAGES = 1000;
constexpr int MAX_PAGES_IN_MEMORY = 100;
constexpr int MAX_FIELDS = 16;
constexpr size_t FIELD_NAME_MAX = 32;

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
constexpr size_t NODE_TYPE_SIZE = sizeof(uint8_t);
constexpr size_t NODE_TYPE_OFFSET = 0;
constexpr size_t IS_ROOT_SIZE = sizeof(uint8_t);
constexpr size_t IS_ROOT_OFFSET = NODE_TYPE_SIZE;
constexpr size_t PARENT_POINTER_SIZE = sizeof(uint32_t);
constexpr size_t PARENT_POINTER_OFFSET = IS_ROOT_OFFSET + IS_ROOT_SIZE;
constexpr size_t COMMON_NODE_HEADER_SIZE = NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE;

/* Leaf Node Header Layout (num_cells, next_leaf) */
constexpr size_t LEAF_NODE_NUM_CELLS_SIZE = sizeof(uint32_t);
constexpr size_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;
constexpr size_t LEAF_NODE_NEXT_LEAF_SIZE = sizeof(uint32_t);
constexpr size_t LEAF_NODE_NEXT_LEAF_OFFSET = LEAF_NODE_NUM_CELLS_OFFSET + LEAF_NODE_NUM_CELLS_SIZE;
constexpr size_t LEAF_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE + LEAF_NODE_NEXT_LEAF_SIZE;

/* Internal Node Header Layout (num_keys, right_child) */
constexpr size_t INTERNAL_NODE_NUM_KEYS_SIZE = sizeof(uint32_t);
constexpr size_t INTERNAL_NODE_NUM_KEYS_OFFSET = COMMON_NODE_HEADER_SIZE;
constexpr size_t INTERNAL_NODE_RIGHT_CHILD_SIZE = sizeof(uint32_t);
constexpr size_t INTERNAL_NODE_RIGHT_CHILD_OFFSET = INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE;
constexpr size_t INTERNAL_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE + INTERNAL_NODE_NUM_KEYS_SIZE + INTERNAL_NODE_RIGHT_CHILD_SIZE;

constexpr size_t INTERNAL_NODE_KEY_SIZE = sizeof(uint32_t);
constexpr size_t INTERNAL_NODE_CHILD_SIZE = sizeof(uint32_t);
constexpr size_t INTERNAL_NODE_CELL_SIZE = INTERNAL_NODE_KEY_SIZE + INTERNAL_NODE_CHILD_SIZE;
constexpr size_t INTERNAL_NODE_MAX_KEYS = 510;

#endif
