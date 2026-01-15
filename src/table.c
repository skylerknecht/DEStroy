#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "table.h"

int table_load(rt_table *table, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    table->num_chains = file_size / 16;
    table->data = malloc(file_size);
    
    if (!table->data) {
        fclose(f);
        return -1;
    }
    
    if (fread(table->data, 1, file_size, f) != (size_t)file_size) {
        free(table->data);
        fclose(f);
        return -1;
    }
    
    fclose(f);
    return 0;
}

void table_free(rt_table *table) {
    if (table->data) {
        free(table->data);
        table->data = NULL;
    }
    table->num_chains = 0;
}

// data layout: [start0][end0][start1][end1][start2][end2]...
// data[i*2] = start, data[i*2+1] = end

uint64_t table_search(rt_table *table, uint64_t end_index, int *found) {
    *found = 0;
    if (table->num_chains == 0) return 0;

    uint64_t left = 0;
    uint64_t right = table->num_chains - 1;

    while (left <= right) {
        uint64_t mid = left + (right - left) / 2;
        uint64_t mid_end = table->data[mid * 2 + 1];
        
        if (mid_end == end_index) {
            *found = 1;
            return table->data[mid * 2];  // return start
        }
        
        if (mid_end < end_index) {
            left = mid + 1;
        } else {
            if (mid == 0) break;
            right = mid - 1;
        }
    }
    return 0;
}