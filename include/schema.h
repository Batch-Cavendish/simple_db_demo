#ifndef SCHEMA_H
#define SCHEMA_H

#include "common.h"

void serialize_field(Schema *schema, uint32_t field_idx, void *val, void *dest);
void deserialize_field(Schema *schema, uint32_t field_idx, void *src, void *dest);
uint32_t hash_string(const char *str);

#endif
