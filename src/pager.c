#include "pager.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

Pager *pager_open(const char *filename) {
    int fd = open(filename, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
    if (fd == -1) {
        printf("Unable to open file\n");
        exit(EXIT_FAILURE);
    }
    off_t len = lseek(fd, 0, SEEK_END);
    Pager *p = malloc(sizeof(Pager));
    p->file_descriptor = fd;
    p->file_length = len;
    p->num_pages = len / PAGE_SIZE;
    p->timer = 0;
    for (int i = 0; i < TABLE_MAX_PAGES; i++) {
        p->pages[i] = nullptr;
        p->last_used[i] = 0;
        p->is_dirty[i] = false;
        p->pinned[i] = 0;
    }
    return p;
}

void pin_page(Pager *p, uint32_t pg) {
    p->pinned[pg]++;
}

void unpin_page(Pager *p, uint32_t pg) {
    if (p->pinned[pg] > 0) p->pinned[pg]--;
}

void unpin_page_all(Pager *p) {
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
        p->pinned[i] = 0;
    }
}

void pager_flush(Pager *p, uint32_t pg) {
    if (p->pages[pg] && p->is_dirty[pg]) {
        lseek(p->file_descriptor, pg * PAGE_SIZE, SEEK_SET);
        write(p->file_descriptor, p->pages[pg], PAGE_SIZE);
        p->is_dirty[pg] = false;
    }
}

void mark_page_dirty(Pager *p, uint32_t pg) {
    p->is_dirty[pg] = true;
}

void *get_page(Pager *p, uint32_t pg) {
    p->timer++;
    if (p->pages[pg] == nullptr) {
        uint32_t pages_in_memory = 0;
        for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
            if (p->pages[i]) pages_in_memory++;
        }

        if (pages_in_memory >= MAX_PAGES_IN_MEMORY) {
            uint32_t victim = 0xFFFFFFFF;
            uint32_t min_time = 0xFFFFFFFF;
            for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
                if (p->pages[i] && p->pinned[i] == 0 && p->last_used[i] < min_time) {
                    min_time = p->last_used[i];
                    victim = i;
                }
            }

            if (victim == 0xFFFFFFFF) {
                printf("Buffer pool full and all pages are pinned! Cannot load page %d\n", pg);
                exit(EXIT_FAILURE);
            }

            pager_flush(p, victim);
            free(p->pages[victim]);
            p->pages[victim] = nullptr;
        }

        void *page = malloc(PAGE_SIZE);
        if (pg < p->num_pages) {
            lseek(p->file_descriptor, pg * PAGE_SIZE, SEEK_SET);
            read(p->file_descriptor, page, PAGE_SIZE);
        }
        p->pages[pg] = page;
        p->is_dirty[pg] = false;
        if (pg >= p->num_pages)
            p->num_pages = pg + 1;
    }
    p->last_used[pg] = p->timer;
    pin_page(p, pg);
    return p->pages[pg];
}
