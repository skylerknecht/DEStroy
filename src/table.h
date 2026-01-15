#ifndef TABLE_H
#define TABLE_H

#include <stdint.h>

typedef struct {
    uint64_t *data;        // raw interleaved data
    uint64_t num_chains;
} rt_table;

int table_load(rt_table *table, const char *filename);
void table_free(rt_table *table);
uint64_t table_search(rt_table *table, uint64_t end_index, int *found);

#endif