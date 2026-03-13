#include "database.h"
#include "btree.h"
#include "os_portability.h"
#include <stdlib.h>
#include <string.h>

void db_save_catalog(Database *db) {
  void *page0 = get_page(db->pager, 0);
  memcpy(page0, &db->catalog, sizeof(Catalog));
  mark_page_dirty(db->pager, 0);
}

Database *db_open(const char *filename) {
  Pager *p = pager_open(filename);
  Database *db = malloc(sizeof(Database));
  db->pager = p;
  db->print_mode = PRINT_PLAIN;
  if (p->num_pages > 0) {
    void *page0 = get_page(p, 0);
    memcpy(&db->catalog, page0, sizeof(Catalog));
  } else {
    db->catalog.num_tables = 0;
    void *page0 = get_page(p, 0);
    memset(page0, 0, PAGE_SIZE);
    mark_page_dirty(p, 0);
  }
  return db;
}

void db_close(Database *db) {
  db_save_catalog(db);
  for (uint32_t i = 0; i < db->pager->num_pages; i++) {
    pager_flush(db->pager, i);
    if (db->pager->pages[i]) {
      free(db->pager->pages[i]);
    }
  }
  close(db->pager->file_descriptor);
  free(db->pager);
  free(db);
}

Cursor *table_start(Database *db, uint32_t table_index) {
  if (table_index >= db->catalog.num_tables)
    return nullptr;

  uint32_t pg = db->catalog.tables[table_index].root_page_num;
  void *node = get_page(db->pager, pg);
  while (get_node_type(node) != NODE_LEAF) {
    pg = *internal_node_child(node, 0);
    node = get_page(db->pager, pg);
  }
  Cursor *c = malloc(sizeof(Cursor));
  c->db = db;
  c->page_num = pg;
  c->cell_num = 0;
  c->table_index = table_index;
  return c;
}

int find_table(Database *db, const char *name) {
  for (uint32_t i = 0; i < db->catalog.num_tables; i++) {
    if (strcmp(db->catalog.tables[i].name, name) == 0) {
      return i;
    }
  }
  return -1;
}
