#include "schema.h"
#include <string.h>

void serialize_field(Schema *schema, uint32_t field_idx, void *val, void *dest) {
    Field *f = &schema->fields[field_idx];
    if (f->type == FIELD_TEXT) {
        memset((char *)dest + f->offset, 0, f->size);
        strncpy((char *)dest + f->offset, (char *)val, f->size - 1);
    } else {
        memcpy((char *)dest + f->offset, val, f->size);
    }
}

void deserialize_field(Schema *schema, uint32_t field_idx, void *src, void *dest) {
    Field *f = &schema->fields[field_idx];
    memcpy(dest, (char *)src + f->offset, f->size);
}

uint32_t hash_string(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash;
}
