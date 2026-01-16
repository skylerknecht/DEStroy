#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "table.h"

#define MAX_BATCH_CANDIDATES 100000

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <ciphertext_hex> <work_dir> <table1.rt> [table2.rt ...]\n", argv[0]);
        return 1;
    }

    const char *ct_hex = argv[1];
    const char *work_dir = argv[2];

    uint32_t num_indices = CHAIN_LEN - 1;
    uint64_t *end_indices = malloc(num_indices * sizeof(uint64_t));
    if (!end_indices) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    if (load_endpoints_from(work_dir, ct_hex, end_indices, &num_indices) != 0) {
        fprintf(stderr, "No endpoints\n");
        free(end_indices);
        return 1;
    }

    uint64_t *start_indices = malloc(MAX_BATCH_CANDIDATES * sizeof(uint64_t));
    uint32_t *positions = malloc(MAX_BATCH_CANDIDATES * sizeof(uint32_t));
    if (!start_indices || !positions) {
        fprintf(stderr, "malloc failed\n");
        free(end_indices);
        free(start_indices);
        free(positions);
        return 1;
    }

    int num_tables = argc - 3;
    uint32_t total_count = 0;

    for (int t = 0; t < num_tables; t++) {
        const char *table_path = argv[3 + t];
        
        rt_table table = {0};
        if (table_load(&table, table_path) != 0) {
            fprintf(stderr, "Table load failed: %s\n", table_path);
            continue;
        }

        uint32_t count = 0;
        for (uint32_t pos = 0; pos < num_indices; pos++) {
            int found = 0;
            uint64_t start_index = table_search(&table, end_indices[pos], &found);
            if (found && count < MAX_BATCH_CANDIDATES) {
                start_indices[count] = start_index;
                positions[count] = pos;
                printf("%016llX:%08X\n", (unsigned long long)start_index, pos);
                count++;
            }
        }

        table_free(&table);
        total_count += count;

        if (count > 0) {
            append_candidates_to(work_dir, ct_hex, start_indices, positions, count);
        }
    }

    free(end_indices);
    free(start_indices);
    free(positions);
    return 0;
}