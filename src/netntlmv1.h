#ifndef NETNTLMV1_H
#define NETNTLMV1_H

#include <stdint.h>

// Static challenge used for NetNTLMv1 tables
extern const uint8_t NTLMV1_CHALLENGE[8];

// Convert 7-byte key material to 8-byte DES key with parity bits
void setup_des_key(const uint8_t *key_56, uint8_t *key_64);

// Compute NetNTLMv1 hash (DES encrypt challenge with 7-byte key)
// key_56: 7 bytes of key material
// out: 8 bytes of ciphertext
void netntlmv1_hash(const uint8_t *key_56, uint8_t *out);

#endif