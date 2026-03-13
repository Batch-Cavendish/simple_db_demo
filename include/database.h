#ifndef TABLE_H
#define TABLE_H

#include "common.h"
#include "pager.h"

typedef struct {
  char name[TABLE_NAME_MAX];
  uint32_t root_page_num;
  Schema schema;
} TableDefinition;

typedef struct {
  uint32_t num_tables;
  TableDefinition tables[MAX_TABLES];
} Catalog;

typedef enum {
  PRINT_PLAIN,
  PRINT_BOX
} PrintMode;

typedef struct {
  Pager *pager;
  Catalog catalog;
  PrintMode print_mode;
} Database;

typedef struct {
  Database *db;
  uint32_t page_num;
  uint32_t cell_num;
  uint32_t table_index; // Index into catalog
} Cursor;

/**
 * db_open initializes the database state. If the file exists, it
 * reconstructs the catalog; if not, it initializes an empty one.
 */
Database *db_open(const char *filename);
void db_close(Database *db);
void db_save_catalog(Database *db);

/**
 * table_start returns a cursor at the very first record of the table.
 */
Cursor *table_start(Database *db, uint32_t table_index);
int find_table(Database *db, const char *name);

#endif
