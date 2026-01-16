#ifndef RAINBOW_H
#define RAINBOW_H

#include <stdint.h>

// Table parameters (from your filename: netntlmv1_byte#7-7_0_881689x134217668_0-4096)
typedef struct {
    uint32_t plaintext_len_min;  // 7
    uint32_t plaintext_len_max;  // 7
    uint32_t table_index;        // 0
    uint32_t chain_len;          // 134217668
    uint32_t num_chains;         // 881689
    uint32_t reduction_offset;   // 0
    uint32_t charset_len;        // 256 (byte)
} rt_params;

// Reduction function: ciphertext → index
uint64_t hash_to_index(const uint8_t *hash, uint32_t reduction_offset, 
                       uint64_t plaintext_space_total, uint32_t pos);

// Index to 7-byte key
void index_to_plaintext(uint64_t index, uint32_t charset_len,
                        uint32_t plaintext_len_min, uint32_t plaintext_len_max,
                        uint64_t *plaintext_space_up_to_index,
                        uint8_t *key_out, uint32_t *key_len_out);

// Precompute the plaintext space table
uint64_t fill_plaintext_space_table(uint32_t charset_len,
                                    uint32_t plaintext_len_min,
                                    uint32_t plaintext_len_max,
                                    uint64_t *plaintext_space_up_to_index);

#endif