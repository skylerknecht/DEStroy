#include "lookup.h"
#include "netntlmv1.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>

void lookup_init_params(lookup_params *params, uint32_t chain_len) {
    params->charset_len = 256;
    params->plaintext_len_min = 7;
    params->plaintext_len_max = 7;
    params->chain_len = chain_len;
    params->reduction_offset = 0;
    
    params->plaintext_space_total = fill_plaintext_space_table(
        params->charset_len,
        params->plaintext_len_min,
        params->plaintext_len_max,
        params->plaintext_space_up_to_index
    );
}

// Walk chain from start_index, looking for target ciphertext
// Returns 1 if found (and sets key_out), 0 if false alarm
static int verify_chain(lookup_params *params, uint64_t start_index,
                        uint32_t expected_pos, const uint8_t *target_ct,
                        uint8_t *key_out) {
    uint64_t index = start_index;
    uint8_t key_56[7];
    uint8_t key_64[8];
    uint8_t hash[8];
    uint32_t key_len;
    
    for (uint32_t pos = 0; pos <= expected_pos; pos++) {
        index_to_plaintext(index, params->charset_len,
                           params->plaintext_len_min, params->plaintext_len_max,
                           params->plaintext_space_up_to_index,
                           key_56, &key_len);
        
        netntlmv1_hash(key_64, hash);
        
        if (pos == expected_pos) {
            // Check if this is our target
            if (memcmp(hash, target_ct, 8) == 0) {
                memcpy(key_out, key_56, 7);
                return 1;
            }
            return 0;  // False alarm
        }
        
        index = hash_to_index(hash, params->reduction_offset,
                              params->plaintext_space_total, pos);
    }
    
    return 0;
}

int lookup_crack(lookup_params *params, rt_table *table,
                 const uint8_t *ciphertext, uint8_t *key_out) {
    uint8_t hash[8];
    uint64_t index;
    int found;
    
    memcpy(hash, ciphertext, 8);
    
    // Try each possible position (walk backwards from end of chain)
    for (uint32_t pos = params->chain_len - 2; pos > 0; pos--) {
        // Compute what end_index would be if ciphertext was at position pos
        index = hash_to_index(hash, params->reduction_offset,
                              params->plaintext_space_total, pos);
        
        // Walk to end of chain
        for (uint32_t p = pos + 1; p < params->chain_len - 1; p++) {
            uint8_t key_56[7], key_64[8], h[8];
            uint32_t key_len;
            
            index_to_plaintext(index, params->charset_len,
                               params->plaintext_len_min, params->plaintext_len_max,
                               params->plaintext_space_up_to_index,
                               key_56, &key_len);
            
            setup_des_key(key_56, key_64);
            netntlmv1_hash(key_64, h);
            index = hash_to_index(h, params->reduction_offset,
                                  params->plaintext_space_total, p);
        }
        
        // Search for this end index
        uint64_t start_index = table_search(table, index, &found);
        
        if (found) {
            printf("  Potential match at pos=%u, verifying...\n", pos);
            if (verify_chain(params, start_index, pos, ciphertext, key_out)) {
                return 0;  // Success!
            }
            printf("  False alarm.\n");
        }
        
        // Progress indicator
        if (pos % 10000 == 0) {
            printf("  Checked position %u / %u\n", params->chain_len - pos, params->chain_len);
        }
    }
    
    return -1;  // Not found
}