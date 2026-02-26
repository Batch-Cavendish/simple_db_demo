#ifndef PAGER_H
#define PAGER_H

#include "common.h"

/**
 * The Pager is the heart of the storage engine. It manages the abstraction
 * of "pages" so the rest of the database doesn't have to deal with file offsets.
 * Databases use fixed-size pages to match the physical blocks on disk, which
 * optimizes I/O performance.
 */

typedef struct {
    // File descriptor for the database file
    int file_descriptor;
    // Length of the database file in bytes
    uint32_t file_length;
    // Number of pages in the database file
    uint32_t num_pages;
    // Array of pointers to pages in memory
    void *pages[TABLE_MAX_PAGES];
    // Array of timestamps for last use of each page
    uint32_t last_used[TABLE_MAX_PAGES];
    // Array of flags indicating whether a page has been modified
    bool is_dirty[TABLE_MAX_PAGES];
    // Array of reference counts for pins on each page
    uint32_t pinned[TABLE_MAX_PAGES];
    // Timer for tracking page usage
    uint32_t timer;
} Pager;

Pager *pager_open(const char *filename);
void pager_flush(Pager *p, uint32_t pg);
void mark_page_dirty(Pager *p, uint32_t pg);
void pin_page(Pager *p, uint32_t pg);
void unpin_page(Pager *p, uint32_t pg);
void unpin_page_all(Pager *p);
void *get_page(Pager *p, uint32_t pg);
void pager_close(Pager *p);

#endif
