#ifndef DLM_CL_HPP
#define DLM_CL_HPP

#include <string.h>
#include <iostream>
#include <CL/cl.h>

#include "compressor.hpp"

#define cl_check(expr) do { \
	cl_int __internal_return_code = (cl_int)(expr); \
	if (__internal_return_code != CL_SUCCESS) { \
		std::cerr << #expr << " failed (" << __internal_return_code << ")\n"; \
		exit(1); \
	} \
} while(0)

extern int64_t pagesize;

struct Program
{
	cl_program program;
	cl_kernel kernel;

	~Program() {
		clReleaseKernel(kernel);
		clReleaseProgram(program);
	}
	Program() = default;

	Program(Program&& pr) noexcept
	{
		memcpy(this, &pr, sizeof(pr));
		memset(&pr, 0, sizeof(pr));
	}
};

Program compile_program(Device *device, const char *path, const char *kernel_name);

#endif // DLM_CL_HPP
