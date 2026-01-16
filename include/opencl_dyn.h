#ifndef OPENCL_DYN_H
#define OPENCL_DYN_H

#define CL_TARGET_OPENCL_VERSION 120

#include <CL/cl.h>

// Function pointer types
typedef cl_int (*clGetPlatformIDs_fn)(cl_uint, cl_platform_id *, cl_uint *);
typedef cl_int (*clGetPlatformInfo_fn)(cl_platform_id, cl_platform_info, size_t, void *, size_t *);
typedef cl_int (*clGetDeviceIDs_fn)(cl_platform_id, cl_device_type, cl_uint, cl_device_id *, cl_uint *);
typedef cl_int (*clGetDeviceInfo_fn)(cl_device_id, cl_device_info, size_t, void *, size_t *);
typedef cl_context (*clCreateContext_fn)(const cl_context_properties *, cl_uint, const cl_device_id *, void (*)(const char *, const void *, size_t, void *), void *, cl_int *);
typedef cl_command_queue (*clCreateCommandQueue_fn)(cl_context, cl_device_id, cl_command_queue_properties, cl_int *);
typedef cl_program (*clCreateProgramWithSource_fn)(cl_context, cl_uint, const char **, const size_t *, cl_int *);
typedef cl_int (*clBuildProgram_fn)(cl_program, cl_uint, const cl_device_id *, const char *, void (*)(cl_program, void *), void *);
typedef cl_int (*clGetProgramBuildInfo_fn)(cl_program, cl_device_id, cl_program_build_info, size_t, void *, size_t *);
typedef cl_kernel (*clCreateKernel_fn)(cl_program, const char *, cl_int *);
typedef cl_mem (*clCreateBuffer_fn)(cl_context, cl_mem_flags, size_t, void *, cl_int *);
typedef cl_int (*clSetKernelArg_fn)(cl_kernel, cl_uint, size_t, const void *);
typedef cl_int (*clEnqueueNDRangeKernel_fn)(cl_command_queue, cl_kernel, cl_uint, const size_t *, const size_t *, const size_t *, cl_uint, const cl_event *, cl_event *);
typedef cl_int (*clEnqueueReadBuffer_fn)(cl_command_queue, cl_mem, cl_bool, size_t, size_t, void *, cl_uint, const cl_event *, cl_event *);
typedef cl_int (*clFinish_fn)(cl_command_queue);
typedef cl_int (*clReleaseMemObject_fn)(cl_mem);
typedef cl_int (*clReleaseKernel_fn)(cl_kernel);
typedef cl_int (*clReleaseProgram_fn)(cl_program);
typedef cl_int (*clReleaseCommandQueue_fn)(cl_command_queue);
typedef cl_int (*clReleaseContext_fn)(cl_context);

// Function pointers
extern clGetPlatformIDs_fn p_clGetPlatformIDs;
extern clGetPlatformInfo_fn p_clGetPlatformInfo;
extern clGetDeviceIDs_fn p_clGetDeviceIDs;
extern clGetDeviceInfo_fn p_clGetDeviceInfo;
extern clCreateContext_fn p_clCreateContext;
extern clCreateCommandQueue_fn p_clCreateCommandQueue;
extern clCreateProgramWithSource_fn p_clCreateProgramWithSource;
extern clBuildProgram_fn p_clBuildProgram;
extern clGetProgramBuildInfo_fn p_clGetProgramBuildInfo;
extern clCreateKernel_fn p_clCreateKernel;
extern clCreateBuffer_fn p_clCreateBuffer;
extern clSetKernelArg_fn p_clSetKernelArg;
extern clEnqueueNDRangeKernel_fn p_clEnqueueNDRangeKernel;
extern clEnqueueReadBuffer_fn p_clEnqueueReadBuffer;
extern clFinish_fn p_clFinish;
extern clReleaseMemObject_fn p_clReleaseMemObject;
extern clReleaseKernel_fn p_clReleaseKernel;
extern clReleaseProgram_fn p_clReleaseProgram;
extern clReleaseCommandQueue_fn p_clReleaseCommandQueue;
extern clReleaseContext_fn p_clReleaseContext;

int opencl_load(void);
void opencl_unload(void);

#endif