#include "schema.h"
#include "statement.h"
#include <stdlib.h>
#include <string.h>

void serialize_field(Schema *schema, uint32_t field_idx, void *val,
                     void *dest) {
  Field *f = &schema->fields[field_idx];
  if (f->type == FIELD_TEXT) {
    memset((char *)dest + f->offset, 0, f->size);
    strncpy((char *)dest + f->offset, (char *)val, f->size - 1);
  } else {
    memcpy((char *)dest + f->offset, val, f->size);
  }
}

void deserialize_field(Schema *schema, uint32_t field_idx, void *src,
                       void *dest) {
  Field *f = &schema->fields[field_idx];
  memcpy(dest, (char *)src + f->offset, f->size);
}

void serialize_row(Schema *schema, struct Statement *s, void *dest) {
  for (uint32_t i = 0; i < schema->num_fields; i++) {
    if (schema->fields[i].type == FIELD_INT) {
      serialize_field(schema, i, &s->insert_values[i], dest);
    } else {
      serialize_field(schema, i, s->insert_strings[i], dest);
    }
  }
}

void deserialize_row(Schema *schema, void *src, struct Statement *s) {
  for (uint32_t i = 0; i < schema->num_fields; i++) {
    if (schema->fields[i].type == FIELD_INT) {
      deserialize_field(schema, i, src, &s->insert_values[i]);
    } else {
      // NOTE: This assumes s->insert_strings[i] is already allocated if needed,
      // or we use a fixed buffer. For SELECT/box mode, we usually use local
      // buffers. In Statement, they are pointers.
    }
  }
}

uint32_t hash_string(const char *str) {
  uint32_t hash = 5381;
  int c;
  while ((c = *str++))
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  return hash;
}
