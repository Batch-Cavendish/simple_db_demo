#include "btree.h"
#include "common.h"
#include "database.h"
#include "pager.h"
#include "statement.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_FILE "test.db"

void test_pager_open_close() {
  printf("Running test_pager_open_close...\n");
  Pager *p = pager_open(TEST_FILE);
  assert(p != NULL);
  assert(p->file_length == 0);
  assert(p->num_pages == 0);
  pager_close(p);
  unlink(TEST_FILE);
  printf("Passed!\n");
}

void test_pager_get_page() {
  printf("Running test_pager_get_page...\n");
  Pager *p = pager_open(TEST_FILE);
  void *page0 = get_page(p, 0);
  assert(page0 != NULL);
  assert(p->num_pages == 1);

  void *page1 = get_page(p, 1);
  assert(page1 != NULL);
  assert(p->num_pages == 2);

  // Check if we get the same pointer if we call get_page again
  void *page0_again = get_page(p, 0);
  assert(page0 == page0_again);

  pager_close(p);
  unlink(TEST_FILE);
  printf("Passed!\n");
}

void test_pager_dirty_tracking() {
  printf("Running test_pager_dirty_tracking...\n");
  Pager *p = pager_open(TEST_FILE);
  get_page(p, 0);
  assert(p->is_dirty[0] == false);

  mark_page_dirty(p, 0);
  assert(p->is_dirty[0] == true);

  pager_flush(p, 0);
  assert(p->is_dirty[0] == false);

  pager_close(p);
  unlink(TEST_FILE);
  printf("Passed!\n");
}

void test_pager_read_write() {
  printf("Running test_pager_read_write...\n");
  // Write something to page 0
  {
    Pager *p = pager_open(TEST_FILE);
    char *page0 = get_page(p, 0);
    strcpy(page0, "Hello, Pager!");
    mark_page_dirty(p, 0);
    pager_close(p);
  }

  // Read it back
  {
    Pager *p = pager_open(TEST_FILE);
    char *page0 = get_page(p, 0);
    assert(strcmp(page0, "Hello, Pager!") == 0);
    pager_close(p);
  }
  unlink(TEST_FILE);
  printf("Passed!\n");
}

void test_pager_lru_eviction() {
  printf("Running test_pager_lru_eviction...\n");
  Pager *p = pager_open(TEST_FILE);

  // Load MAX_PAGES_IN_MEMORY pages
  for (int i = 0; i < MAX_PAGES_IN_MEMORY; i++) {
    get_page(p, i);
    unpin_page(p, i); // Unpin so they can be evicted
  }

  // Page 0 should be the oldest used
  // Now load one more page, it should evict page 0
  get_page(p, MAX_PAGES_IN_MEMORY);

  assert(p->pages[0] == NULL);
  assert(p->pages[MAX_PAGES_IN_MEMORY] != NULL);

  pager_close(p);
  unlink(TEST_FILE);
  printf("Passed!\n");
}

void test_btree_node_initialization() {
  printf("Running test_btree_node_initialization...\n");
  Pager *p = pager_open(TEST_FILE);
  void *node = get_page(p, 0);

  initialize_leaf_node(node);
  assert(get_node_type(node) == NODE_LEAF);
  assert(is_node_root(node) == false);
  assert(*leaf_node_num_cells(node) == 0);

  initialize_internal_node(node);
  assert(get_node_type(node) == NODE_INTERNAL);
  assert(is_node_root(node) == false);
  assert(*internal_node_num_keys(node) == 0);

  pager_close(p);
  unlink(TEST_FILE);
  printf("Passed!\n");
}

void test_btree_insert_lookup() {
  printf("Running test_btree_insert_lookup...\n");
  Database *db = db_open(TEST_FILE);
  // Setup a simple schema: id INT, name TEXT in the catalog
  db->catalog.num_tables = 1;
  TableDefinition *td = &db->catalog.tables[0];
  strcpy(td->name, "users");
  td->root_page_num = 1;
  td->schema.num_fields = 2;
  strcpy(td->schema.fields[0].name, "id");
  td->schema.fields[0].type = FIELD_INT;
  td->schema.fields[0].size = 4;
  td->schema.fields[0].offset = 0;

  strcpy(td->schema.fields[1].name, "name");
  td->schema.fields[1].type = FIELD_TEXT;
  td->schema.fields[1].size = 32;
  td->schema.fields[1].offset = 4;
  td->schema.row_size = 36;

  void *root = get_page(db->pager, 1);
  initialize_leaf_node(root);
  set_node_root(root, true);
  mark_page_dirty(db->pager, 1);

  Statement s = {};
  s.type = STATEMENT_INSERT;
  s.table_index = 0;
  s.insert_values[0] = 1;
  s.insert_strings[1] = "Alice";

  Cursor *c = find_node(db, 0, td->root_page_num, 1);
  leaf_node_insert(c, 1, &s);
  free(c);

  // Lookup
  c = find_node(db, 0, td->root_page_num, 1);
  assert(c->cell_num == 0);
  void *node = get_page(db->pager, c->page_num);
  assert(*leaf_node_key(node, c->cell_num, &td->schema) == 1);
  void *value = leaf_node_value(node, c->cell_num, &td->schema);
  assert(strcmp((char *)value + 4, "Alice") ==
         0); // +4 because id is first 4 bytes
  free(c);

  db_close(db);
  unlink(TEST_FILE);
  printf("Passed!\n");
}

int main() {
  test_pager_open_close();
  test_pager_get_page();
  test_pager_dirty_tracking();
  test_pager_read_write();
  test_pager_lru_eviction();
  test_btree_node_initialization();
  test_btree_insert_lookup();
  printf("All unit tests passed!\n");
  return 0;
}
