#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stddef.h>

// Convert hex char to nibble value: 'a' -> 10, 'F' -> 15, '0' -> 0
// Returns -1 on invalid input
int hex_char_to_nibble(char c);

// Hex string to bytes: "aabbccdd" -> {0xaa, 0xbb, 0xcc, 0xdd}
// Returns number of bytes written, -1 on error
int hex_to_bytes(const char *hex, uint8_t *out, size_t out_len);

// Bytes to hex string (lowercase, null-terminated)
// Returns 0 on success, -1 if buffer too small
int bytes_to_hex(const uint8_t *bytes, size_t len, char *out, size_t out_len);

#endif