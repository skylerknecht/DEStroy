#include "opencl_dyn.h"
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
static HMODULE opencl_lib = NULL;
#define LOADLIB(name) LoadLibraryA(name)
#define GETFUNC(lib, name) ((void*)GetProcAddress(lib, name))
#define FREELIB(lib) FreeLibrary(lib)
#define OPENCL_LIB "OpenCL.dll"
#else
#include <dlfcn.h>
static void *opencl_lib = NULL;
#define LOADLIB(name) dlopen(name, RTLD_NOW)
#define GETFUNC(lib, name) dlsym(lib, name)
#define FREELIB(lib) dlclose(lib)
#define OPENCL_LIB "libOpenCL.so.1"
#endif

// Function pointers
clGetPlatformIDs_fn p_clGetPlatformIDs;
clGetPlatformInfo_fn p_clGetPlatformInfo;
clGetDeviceIDs_fn p_clGetDeviceIDs;
clGetDeviceInfo_fn p_clGetDeviceInfo;
clCreateContext_fn p_clCreateContext;
clCreateCommandQueue_fn p_clCreateCommandQueue;
clCreateProgramWithSource_fn p_clCreateProgramWithSource;
clBuildProgram_fn p_clBuildProgram;
clGetProgramBuildInfo_fn p_clGetProgramBuildInfo;
clCreateKernel_fn p_clCreateKernel;
clCreateBuffer_fn p_clCreateBuffer;
clSetKernelArg_fn p_clSetKernelArg;
clEnqueueNDRangeKernel_fn p_clEnqueueNDRangeKernel;
clEnqueueReadBuffer_fn p_clEnqueueReadBuffer;
clFinish_fn p_clFinish;
clReleaseMemObject_fn p_clReleaseMemObject;
clReleaseKernel_fn p_clReleaseKernel;
clReleaseProgram_fn p_clReleaseProgram;
clReleaseCommandQueue_fn p_clReleaseCommandQueue;
clReleaseContext_fn p_clReleaseContext;

int opencl_load(void) {
    opencl_lib = LOADLIB(OPENCL_LIB);
    
#ifndef _WIN32
    if (!opencl_lib) {
        opencl_lib = LOADLIB("libOpenCL.so");
    }
    if (!opencl_lib) {
        opencl_lib = LOADLIB("/usr/lib/x86_64-linux-gnu/libOpenCL.so.1");
    }
#endif

    if (!opencl_lib) {
        fprintf(stderr, "Failed to load OpenCL library\n");
        return -1;
    }

    p_clGetPlatformIDs = (clGetPlatformIDs_fn)GETFUNC(opencl_lib, "clGetPlatformIDs");
    p_clGetPlatformInfo = (clGetPlatformInfo_fn)GETFUNC(opencl_lib, "clGetPlatformInfo");
    p_clGetDeviceIDs = (clGetDeviceIDs_fn)GETFUNC(opencl_lib, "clGetDeviceIDs");
    p_clGetDeviceInfo = (clGetDeviceInfo_fn)GETFUNC(opencl_lib, "clGetDeviceInfo");
    p_clCreateContext = (clCreateContext_fn)GETFUNC(opencl_lib, "clCreateContext");
    p_clCreateCommandQueue = (clCreateCommandQueue_fn)GETFUNC(opencl_lib, "clCreateCommandQueue");
    p_clCreateProgramWithSource = (clCreateProgramWithSource_fn)GETFUNC(opencl_lib, "clCreateProgramWithSource");
    p_clBuildProgram = (clBuildProgram_fn)GETFUNC(opencl_lib, "clBuildProgram");
    p_clGetProgramBuildInfo = (clGetProgramBuildInfo_fn)GETFUNC(opencl_lib, "clGetProgramBuildInfo");
    p_clCreateKernel = (clCreateKernel_fn)GETFUNC(opencl_lib, "clCreateKernel");
    p_clCreateBuffer = (clCreateBuffer_fn)GETFUNC(opencl_lib, "clCreateBuffer");
    p_clSetKernelArg = (clSetKernelArg_fn)GETFUNC(opencl_lib, "clSetKernelArg");
    p_clEnqueueNDRangeKernel = (clEnqueueNDRangeKernel_fn)GETFUNC(opencl_lib, "clEnqueueNDRangeKernel");
    p_clEnqueueReadBuffer = (clEnqueueReadBuffer_fn)GETFUNC(opencl_lib, "clEnqueueReadBuffer");
    p_clFinish = (clFinish_fn)GETFUNC(opencl_lib, "clFinish");
    p_clReleaseMemObject = (clReleaseMemObject_fn)GETFUNC(opencl_lib, "clReleaseMemObject");
    p_clReleaseKernel = (clReleaseKernel_fn)GETFUNC(opencl_lib, "clReleaseKernel");
    p_clReleaseProgram = (clReleaseProgram_fn)GETFUNC(opencl_lib, "clReleaseProgram");
    p_clReleaseCommandQueue = (clReleaseCommandQueue_fn)GETFUNC(opencl_lib, "clReleaseCommandQueue");
    p_clReleaseContext = (clReleaseContext_fn)GETFUNC(opencl_lib, "clReleaseContext");

    if (!p_clGetPlatformIDs || !p_clCreateContext || !p_clCreateKernel) {
        fprintf(stderr, "Failed to load OpenCL functions\n");
        opencl_unload();
        return -1;
    }

    return 0;
}

void opencl_unload(void) {
    if (opencl_lib) {
        FREELIB(opencl_lib);
        opencl_lib = NULL;
    }
}