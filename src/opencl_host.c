#include "opencl_host.h"
#include "opencl_dyn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int gpu_init(gpu_context *ctx) {
    cl_int err;
    cl_uint num_platforms;
    cl_platform_id platforms[16];
    
    memset(ctx, 0, sizeof(gpu_context));
    
    if (opencl_load() != 0) {
        return -1;
    }
    
    err = p_clGetPlatformIDs(16, platforms, &num_platforms);
    if (err != CL_SUCCESS || num_platforms == 0) {
        fprintf(stderr, "No OpenCL platforms found\n");
        return -1;
    }
    
    for (cl_uint i = 0; i < num_platforms; i++) {
        cl_uint num_devices;
        err = p_clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_GPU, 1, &ctx->device, &num_devices);
        if (err == CL_SUCCESS && num_devices > 0) {
            ctx->platform = platforms[i];
            break;
        }
    }
    
    if (ctx->device == NULL) {
        fprintf(stderr, "No GPU device found\n");
        return -1;
    }
    
    p_clGetDeviceInfo(ctx->device, CL_DEVICE_NAME, sizeof(ctx->device_name), ctx->device_name, NULL);
    p_clGetDeviceInfo(ctx->device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(ctx->compute_units), &ctx->compute_units, NULL);
    p_clGetDeviceInfo(ctx->device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(ctx->max_work_group_size), &ctx->max_work_group_size, NULL);
    
    printf("      GPU: %s\n", ctx->device_name);
    printf("      Compute units: %u\n", ctx->compute_units);
    printf("      Max work group size: %zu\n", ctx->max_work_group_size);
    
    ctx->context = p_clCreateContext(NULL, 1, &ctx->device, NULL, NULL, &err);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Failed to create context: %d\n", err);
        return -1;
    }
    
    ctx->queue = p_clCreateCommandQueue(ctx->context, ctx->device, 0, &err);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Failed to create command queue: %d\n", err);
        return -1;
    }
    
    return 0;
}

int gpu_load_kernel(gpu_context *ctx, const char *filename, const char *kernel_name) {
    cl_int err;
    FILE *f;
    char *source;
    size_t source_len;
    
    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open kernel file: %s\n", filename);
        return -1;
    }
    
    fseek(f, 0, SEEK_END);
    source_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    source = malloc(source_len + 1);
    if (!source) {
        fclose(f);
        return -1;
    }
    
    fread(source, 1, source_len, f);
    source[source_len] = '\0';
    fclose(f);
    
    ctx->program = p_clCreateProgramWithSource(ctx->context, 1, (const char **)&source, &source_len, &err);
    free(source);
    
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Failed to create program: %d\n", err);
        return -1;
    }
    
    err = p_clBuildProgram(ctx->program, 1, &ctx->device, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        size_t log_size;
        p_clGetProgramBuildInfo(ctx->program, ctx->device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char *log = malloc(log_size);
        p_clGetProgramBuildInfo(ctx->program, ctx->device, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        fprintf(stderr, "Build error:\n%s\n", log);
        free(log);
        return -1;
    }
    
    ctx->kernel = p_clCreateKernel(ctx->program, kernel_name, &err);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Failed to create kernel '%s': %d\n", kernel_name, err);
        return -1;
    }
    
    return 0;
}

void gpu_cleanup(gpu_context *ctx) {
    if (ctx->kernel) p_clReleaseKernel(ctx->kernel);
    if (ctx->program) p_clReleaseProgram(ctx->program);
    if (ctx->fa_kernel) p_clReleaseKernel(ctx->fa_kernel);
    if (ctx->fa_program) p_clReleaseProgram(ctx->fa_program);
    if (ctx->queue) p_clReleaseCommandQueue(ctx->queue);
    if (ctx->context) p_clReleaseContext(ctx->context);
    memset(ctx, 0, sizeof(gpu_context));
    opencl_unload();
}

int gpu_precompute(gpu_context *ctx, const uint8_t *hash,
                   uint32_t chain_len, uint32_t reduction_offset,
                   uint64_t plaintext_space_total,
                   uint64_t *output) {
    cl_int err;
    cl_mem hash_buf, output_buf;
    size_t global_work_size;
    
    uint32_t num_indices = chain_len - 1;
    global_work_size = num_indices;
    
    hash_buf = p_clCreateBuffer(ctx->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                8, (void *)hash, &err);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Failed to create hash buffer: %d\n", err);
        return -1;
    }
    
    output_buf = p_clCreateBuffer(ctx->context, CL_MEM_WRITE_ONLY,
                                  num_indices * sizeof(cl_ulong), NULL, &err);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Failed to create output buffer: %d\n", err);
        p_clReleaseMemObject(hash_buf);
        return -1;
    }
    
    p_clSetKernelArg(ctx->kernel, 0, sizeof(cl_mem), &hash_buf);
    p_clSetKernelArg(ctx->kernel, 1, sizeof(cl_uint), &chain_len);
    p_clSetKernelArg(ctx->kernel, 2, sizeof(cl_uint), &reduction_offset);
    p_clSetKernelArg(ctx->kernel, 3, sizeof(cl_ulong), &plaintext_space_total);
    p_clSetKernelArg(ctx->kernel, 4, sizeof(cl_mem), &output_buf);
    
    printf("      Running on GPU (please wait, ~1-2 minutes)...\n");
    fflush(stdout);
    
    double start = (double)clock() / CLOCKS_PER_SEC;
    
    err = p_clEnqueueNDRangeKernel(ctx->queue, ctx->kernel, 1, NULL,
                                    &global_work_size, NULL, 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Failed to enqueue kernel: %d\n", err);
        p_clReleaseMemObject(hash_buf);
        p_clReleaseMemObject(output_buf);
        return -1;
    }
    
    p_clFinish(ctx->queue);
    
    double elapsed = (double)clock() / CLOCKS_PER_SEC - start;
    printf("      GPU computation finished in %.1f seconds\n", elapsed);
    
    err = p_clEnqueueReadBuffer(ctx->queue, output_buf, CL_TRUE, 0,
                                 num_indices * sizeof(cl_ulong), output, 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Failed to read output buffer: %d\n", err);
        p_clReleaseMemObject(hash_buf);
        p_clReleaseMemObject(output_buf);
        return -1;
    }
    
    p_clReleaseMemObject(hash_buf);
    p_clReleaseMemObject(output_buf);
    
    return num_indices;
}

int gpu_load_false_alarm_kernel(gpu_context *ctx, const char *filename) {
    cl_int err;
    FILE *f;
    char *source;
    size_t source_len;
    
    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open kernel file: %s\n", filename);
        return -1;
    }
    
    fseek(f, 0, SEEK_END);
    source_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    source = malloc(source_len + 1);
    if (!source) {
        fclose(f);
        return -1;
    }
    
    fread(source, 1, source_len, f);
    source[source_len] = '\0';
    fclose(f);
    
    ctx->fa_program = p_clCreateProgramWithSource(ctx->context, 1, (const char **)&source, &source_len, &err);
    free(source);
    
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Failed to create false alarm program: %d\n", err);
        return -1;
    }
    
    err = p_clBuildProgram(ctx->fa_program, 1, &ctx->device, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        size_t log_size;
        p_clGetProgramBuildInfo(ctx->fa_program, ctx->device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char *log = malloc(log_size);
        p_clGetProgramBuildInfo(ctx->fa_program, ctx->device, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        fprintf(stderr, "False alarm kernel build error:\n%s\n", log);
        free(log);
        return -1;
    }
    
    ctx->fa_kernel = p_clCreateKernel(ctx->fa_program, "check_false_alarms", &err);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Failed to create false alarm kernel: %d\n", err);
        return -1;
    }
    
    return 0;
}

int gpu_check_false_alarms(gpu_context *ctx,
                           const uint8_t *target_hash,
                           uint64_t *start_indices,
                           uint32_t *positions,
                           uint32_t num_candidates,
                           uint32_t reduction_offset,
                           uint64_t plaintext_space_total,
                           uint8_t *found_key) {
    cl_int err;
    
    if (num_candidates == 0) {
        return 0;
    }
    
    // Create buffers
    cl_mem hash_buf = p_clCreateBuffer(ctx->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                        8, (void *)target_hash, &err);
    if (err != CL_SUCCESS) return -1;
    
    cl_mem start_buf = p_clCreateBuffer(ctx->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                         num_candidates * sizeof(uint64_t), start_indices, &err);
    if (err != CL_SUCCESS) { p_clReleaseMemObject(hash_buf); return -1; }
    
    cl_mem pos_buf = p_clCreateBuffer(ctx->context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                       num_candidates * sizeof(uint32_t), positions, &err);
    if (err != CL_SUCCESS) { p_clReleaseMemObject(hash_buf); p_clReleaseMemObject(start_buf); return -1; }
    
    int found_idx_init = -1;
    cl_mem found_idx_buf = p_clCreateBuffer(ctx->context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                             sizeof(int), &found_idx_init, &err);
    if (err != CL_SUCCESS) { 
        p_clReleaseMemObject(hash_buf); 
        p_clReleaseMemObject(start_buf); 
        p_clReleaseMemObject(pos_buf); 
        return -1; 
    }
    
    cl_mem found_key_buf = p_clCreateBuffer(ctx->context, CL_MEM_WRITE_ONLY, 7, NULL, &err);
    if (err != CL_SUCCESS) { 
        p_clReleaseMemObject(hash_buf); 
        p_clReleaseMemObject(start_buf); 
        p_clReleaseMemObject(pos_buf); 
        p_clReleaseMemObject(found_idx_buf);
        return -1; 
    }
    
    // Set kernel arguments
    p_clSetKernelArg(ctx->fa_kernel, 0, sizeof(cl_mem), &hash_buf);
    p_clSetKernelArg(ctx->fa_kernel, 1, sizeof(cl_mem), &start_buf);
    p_clSetKernelArg(ctx->fa_kernel, 2, sizeof(cl_mem), &pos_buf);
    p_clSetKernelArg(ctx->fa_kernel, 3, sizeof(cl_uint), &num_candidates);
    p_clSetKernelArg(ctx->fa_kernel, 4, sizeof(cl_uint), &reduction_offset);
    p_clSetKernelArg(ctx->fa_kernel, 5, sizeof(cl_ulong), &plaintext_space_total);
    p_clSetKernelArg(ctx->fa_kernel, 6, sizeof(cl_mem), &found_idx_buf);
    p_clSetKernelArg(ctx->fa_kernel, 7, sizeof(cl_mem), &found_key_buf);
    
    // Execute
    size_t global_work_size = num_candidates;
    err = p_clEnqueueNDRangeKernel(ctx->queue, ctx->fa_kernel, 1, NULL,
                                    &global_work_size, NULL, 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Failed to enqueue false alarm kernel: %d\n", err);
        p_clReleaseMemObject(hash_buf);
        p_clReleaseMemObject(start_buf);
        p_clReleaseMemObject(pos_buf);
        p_clReleaseMemObject(found_idx_buf);
        p_clReleaseMemObject(found_key_buf);
        return -1;
    }
    
    p_clFinish(ctx->queue);
    
    // Read results
    int found_idx;
    p_clEnqueueReadBuffer(ctx->queue, found_idx_buf, CL_TRUE, 0, sizeof(int), &found_idx, 0, NULL, NULL);
    
    int result = 0;
    if (found_idx >= 0) {
        p_clEnqueueReadBuffer(ctx->queue, found_key_buf, CL_TRUE, 0, 7, found_key, 0, NULL, NULL);
        result = 1;
    }
    
    p_clReleaseMemObject(hash_buf);
    p_clReleaseMemObject(start_buf);
    p_clReleaseMemObject(pos_buf);
    p_clReleaseMemObject(found_idx_buf);
    p_clReleaseMemObject(found_key_buf);
    
    return result;
}
