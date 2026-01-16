#ifndef LOOKUP_H
#define LOOKUP_H

#include <stdint.h>
#include "table.h"
#include "rainbow.h"

typedef struct {
    uint32_t charset_len;
    uint32_t plaintext_len_min;
    uint32_t plaintext_len_max;
    uint32_t chain_len;
    uint32_t reduction_offset;
    uint64_t plaintext_space_total;
    uint64_t plaintext_space_up_to_index[8];
} lookup_params;

// Initialize params for byte#7-7 table
void lookup_init_params(lookup_params *params, uint32_t chain_len);

// Attempt to crack a ciphertext
// Returns 0 on success (key found), -1 on not found
// key_out must be at least 7 bytes
int lookup_crack(lookup_params *params, rt_table *table,
                 const uint8_t *ciphertext, uint8_t *key_out);

#endif