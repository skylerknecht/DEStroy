#include "utils.h"
#include <string.h>

int hex_char_to_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int hex_to_bytes(const char *hex, uint8_t *out, size_t out_len) {
    size_t hex_len = strlen(hex);
    
    // Must be even length
    if (hex_len % 2 != 0) return -1;
    
    size_t num_bytes = hex_len / 2;
    if (num_bytes > out_len) return -1;
    
    for (size_t i = 0; i < num_bytes; i++) {
        int high = hex_char_to_nibble(hex[i * 2]);
        int low = hex_char_to_nibble(hex[i * 2 + 1]);
        
        if (high < 0 || low < 0) return -1;
        
        out[i] = (high << 4) | low;
    }
    
    return (int)num_bytes;
}

int bytes_to_hex(const uint8_t *bytes, size_t len, char *out, size_t out_len) {
    // Need 2 chars per byte plus null terminator
    if (out_len < (len * 2) + 1) return -1;
    
    const char *hex_chars = "0123456789abcdef";
    
    for (size_t i = 0; i < len; i++) {
        out[i * 2]     = hex_chars[(bytes[i] >> 4) & 0x0f];
        out[i * 2 + 1] = hex_chars[bytes[i] & 0x0f];
    }
    out[len * 2] = '\0';
    
    return 0;
}