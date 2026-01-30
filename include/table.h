#ifndef TABLE_H
#define TABLE_H

#include "common.h"
#include "pager.h"

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

/**
 * db_open initializes the database state. If the file exists, it 
 * reconstructs the schema; if not, it initializes a new B-Tree root.
 */
Table *db_open(const char *filename);
void db_close(Table *t);
void db_save_schema(Table *t);

/**
 * table_start returns a cursor at the very first record of the table.
 * Since the B-Tree is ordered, this is the leftmost cell of the leftmost leaf.
 */
Cursor *table_start(Table *t);

#endif
