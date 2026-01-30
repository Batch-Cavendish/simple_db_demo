#include "table.h"
#include "btree.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void db_save_schema(Table *t) {
    void *page0 = get_page(t->pager, 0);
    memcpy(page0, &t->schema, sizeof(Schema));
    memcpy((char *)page0 + sizeof(Schema), &t->root_page_num, sizeof(uint32_t));
    mark_page_dirty(t->pager, 0);
}

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
        if (t->pager->pages[i]) {
            free(t->pager->pages[i]);
        }
    }
    close(t->pager->file_descriptor);
    free(t->pager);
    free(t);
}

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
