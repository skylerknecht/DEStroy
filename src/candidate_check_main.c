#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "opencl_host.h"

#define MAX_CANDIDATES (4 * 1024 * 1024)

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <ciphertext_hex> [work_dir]\n", argv[0]);
        return 1;
    }

    const char *ct_hex = argv[1];
    const char *work_dir = argc > 2 ? argv[2] : "working";

    uint8_t ciphertext[8];
    if (hex_to_bytes(ct_hex, ciphertext, 8) != 8) {
        fprintf(stderr, "Invalid ciphertext\n");
        return 1;
    }

    uint64_t *start_indices = malloc(MAX_CANDIDATES * sizeof(uint64_t));
    uint32_t *positions = malloc(MAX_CANDIDATES * sizeof(uint32_t));
    if (!start_indices || !positions) {
        free(start_indices);
        free(positions);
        return 1;
    }

    uint32_t total = 0;
    if (load_candidates_from(work_dir, ct_hex, start_indices, positions, &total, MAX_CANDIDATES) != 0) {
        printf("NOTFOUND 0\n");
        free(start_indices);
        free(positions);
        return 0;
    }

    gpu_context gpu = {0};
    if (gpu_init(&gpu) != 0) {
        fprintf(stderr, "GPU init failed\n");
        free(start_indices);
        free(positions);
        return 1;
    }

    if (gpu_load_false_alarm_kernel(&gpu, "kernels/false_alarm.cl") != 0) {
        fprintf(stderr, "Kernel load failed\n");
        gpu_cleanup(&gpu);
        free(start_indices);
        free(positions);
        return 1;
    }

    uint64_t plaintext_space = get_plaintext_space();
    uint8_t key_bytes[7] = {0};

    int result = gpu_check_false_alarms(&gpu, ciphertext, start_indices, positions,
                                         total, REDUCTION_OFFSET, plaintext_space, key_bytes);

    gpu_cleanup(&gpu);
    free(start_indices);
    free(positions);

    if (result == 1) {
        char key_hex[15];
        bytes_to_hex(key_bytes, 7, key_hex, sizeof(key_hex));
        printf("%s\n", key_hex);
        
        char path[512];
        snprintf(path, sizeof(path), "%s/%s.result", work_dir, ct_hex);
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "%s\n", key_hex);
            fclose(f);
        }
        return 0;  // success, found
    } else {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s.result", work_dir, ct_hex);
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "NOTFOUND\n");
            fclose(f);
        }
        return 1;  // not found
    }

    return 0;
}