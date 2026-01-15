#include "netntlmv1.h"
#include "des.h"

const uint8_t NTLMV1_CHALLENGE[8] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88
};

void setup_des_key(const uint8_t *key_56, uint8_t *key_64) {
    key_64[0] = (((key_56[0] >> 1) & 0x7f) << 1);
    key_64[1] = (((key_56[0] & 0x01) << 6 | ((key_56[1] >> 2) & 0x3f)) << 1);
    key_64[2] = (((key_56[1] & 0x03) << 5 | ((key_56[2] >> 3) & 0x1f)) << 1);
    key_64[3] = (((key_56[2] & 0x07) << 4 | ((key_56[3] >> 4) & 0x0f)) << 1);
    key_64[4] = (((key_56[3] & 0x0f) << 3 | ((key_56[4] >> 5) & 0x07)) << 1);
    key_64[5] = (((key_56[4] & 0x1f) << 2 | ((key_56[5] >> 6) & 0x03)) << 1);
    key_64[6] = (((key_56[5] & 0x3f) << 1 | ((key_56[6] >> 7) & 0x01)) << 1);
    key_64[7] = ((key_56[6] & 0x7f) << 1);
}

void netntlmv1_hash(const uint8_t *key_56, uint8_t *out) {
    des_encrypt_ntlmv1(key_56, out);
}