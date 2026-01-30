#ifndef PAGER_H
#define PAGER_H

#include "common.h"

typedef struct {
    int file_descriptor;
    uint32_t file_length;
    uint32_t num_pages;
    void *pages[TABLE_MAX_PAGES];
    uint32_t last_used[TABLE_MAX_PAGES];
    bool is_dirty[TABLE_MAX_PAGES];
    uint32_t pinned[TABLE_MAX_PAGES];
    uint32_t timer;
} Pager;

/**
 * The Pager is the heart of the storage engine. It manages the abstraction 
 * of "pages" so the rest of the database doesn't have to deal with file offsets.
 * Databases use fixed-size pages to match the physical blocks on disk, which
 * optimizes I/O performance.
 */
Pager *pager_open(const char *filename);
void pager_flush(Pager *p, uint32_t pg);
void mark_page_dirty(Pager *p, uint32_t pg);
void pin_page(Pager *p, uint32_t pg);
void unpin_page(Pager *p, uint32_t pg);
void unpin_page_all(Pager *p);
void *get_page(Pager *p, uint32_t pg);

#endif
