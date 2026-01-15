#ifndef DES_H
#define DES_H

#include <stdint.h>

// DES encrypt NetNTLMv1 challenge with 7-byte key
void des_encrypt_ntlmv1(const uint8_t *key_56, uint8_t *output);

#endif