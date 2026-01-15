#ifndef OPENCL_HOST_H
#define OPENCL_HOST_H

#define CL_TARGET_OPENCL_VERSION 120

#include <stdint.h>
#include <CL/cl.h>

typedef struct {
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;
    cl_program fa_program;
    cl_kernel fa_kernel;
    char device_name[128];
    uint32_t compute_units;
    size_t max_work_group_size;
} gpu_context;

int gpu_init(gpu_context *ctx);
void gpu_cleanup(gpu_context *ctx);
int gpu_load_kernel(gpu_context *ctx, const char *source_file, const char *kernel_name);
int gpu_load_false_alarm_kernel(gpu_context *ctx, const char *source_file);
int gpu_precompute(gpu_context *ctx, const uint8_t *ciphertext, uint32_t chain_len,
                   uint32_t reduction_offset, uint64_t plaintext_space_total,
                   uint64_t *end_indices);
int gpu_check_false_alarms(gpu_context *ctx, const uint8_t *target_hash,
                           uint64_t *start_indices, uint32_t *positions,
                           uint32_t num_candidates, uint32_t reduction_offset,
                           uint64_t plaintext_space_total, uint8_t *found_key);

#endif