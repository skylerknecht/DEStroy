#include "rainbow.h"
#include <string.h>

uint64_t fill_plaintext_space_table(uint32_t charset_len,
                                    uint32_t plaintext_len_min,
                                    uint32_t plaintext_len_max,
                                    uint64_t *plaintext_space_up_to_index) {
    uint64_t n = 1;
    
    plaintext_space_up_to_index[0] = 0;
    
    for (uint32_t i = 1; i <= plaintext_len_max; i++) {
        n = n * charset_len;
        if (i < plaintext_len_min)
            plaintext_space_up_to_index[i] = 0;
        else
            plaintext_space_up_to_index[i] = plaintext_space_up_to_index[i - 1] + n;
    }
    
    return plaintext_space_up_to_index[plaintext_len_max];
}

uint64_t hash_to_index(const uint8_t *hash, uint32_t reduction_offset,
                       uint64_t plaintext_space_total, uint32_t pos) {
    uint64_t ret = 0;
    
    // Read hash as little-endian 64-bit value
    ret = (uint64_t)hash[7] << 56 |
          (uint64_t)hash[6] << 48 |
          (uint64_t)hash[5] << 40 |
          (uint64_t)hash[4] << 32 |
          (uint64_t)hash[3] << 24 |
          (uint64_t)hash[2] << 16 |
          (uint64_t)hash[1] << 8  |
          (uint64_t)hash[0];
    
    return (ret + reduction_offset + pos) % plaintext_space_total;
}

void index_to_plaintext(uint64_t index, uint32_t charset_len,
                        uint32_t plaintext_len_min, uint32_t plaintext_len_max,
                        uint64_t *plaintext_space_up_to_index,
                        uint8_t *key_out, uint32_t *key_len_out) {
    uint64_t index_x;
    
    // Find the plaintext length for this index
    *key_len_out = 0;
    for (int i = plaintext_len_max - 1; i >= (int)plaintext_len_min - 1; i--) {
        if (index >= plaintext_space_up_to_index[i]) {
            *key_len_out = i + 1;
            break;
        }
    }
    
    if (*key_len_out == 0) {
        *key_len_out = plaintext_len_min;
    }
    
    // Convert index to key bytes
    index_x = index - plaintext_space_up_to_index[*key_len_out - 1];
    
    for (int i = *key_len_out - 1; i >= 0; i--) {
        key_out[i] = index_x % charset_len;
        index_x = index_x / charset_len;
    }
}