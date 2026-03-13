#ifndef SCHEMA_H
#define SCHEMA_H

#include "common.h"

struct Statement;
void serialize_row(Schema *schema, struct Statement *s, void *dest);
void deserialize_row(Schema *schema, void *src, struct Statement *s);
void serialize_field(Schema *schema, uint32_t field_idx, void *val, void *dest);
void deserialize_field(Schema *schema, uint32_t field_idx, void *src, void *dest);
uint32_t hash_string(const char *str);

#endif
