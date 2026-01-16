#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "opencl_host.h"

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

    gpu_context gpu = {0};
    if (gpu_init(&gpu) != 0) {
        fprintf(stderr, "GPU init failed\n");
        return 1;
    }

    if (gpu_load_kernel(&gpu, "kernels/precompute.cl", "precompute") != 0) {
        fprintf(stderr, "Kernel load failed\n");
        gpu_cleanup(&gpu);
        return 1;
    }

    uint32_t num_indices = CHAIN_LEN - 1;
    uint64_t *end_indices = malloc(num_indices * sizeof(uint64_t));
    if (!end_indices) {
        fprintf(stderr, "malloc failed\n");
        gpu_cleanup(&gpu);
        return 1;
    }

    uint64_t plaintext_space = get_plaintext_space();
    int result = gpu_precompute(&gpu, ciphertext, CHAIN_LEN, REDUCTION_OFFSET,
                                 plaintext_space, end_indices);

    if (result < 0) {
        fprintf(stderr, "Precompute failed\n");
        free(end_indices);
        gpu_cleanup(&gpu);
        return 1;
    }

    if (save_endpoints_to(work_dir, ct_hex, end_indices, num_indices) != 0) {
        fprintf(stderr, "Save failed\n");
        free(end_indices);
        gpu_cleanup(&gpu);
        return 1;
    }

    printf("OK %u\n", num_indices);

    free(end_indices);
    gpu_cleanup(&gpu);
    return 0;
}