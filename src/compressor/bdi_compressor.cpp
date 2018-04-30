#include <memory>

#include "cl.hpp"
#include "bdi_compressor.hpp"

#define BDI_GROUP_SIZE 32

BDICompressor::BDICompressor()
	: comp_device(DeviceFinder::getIGPU())
	, compressor(compile_program(&comp_device, "kernel.cl", "bdi_compress"))
{}


void BDICompressor::set_mem(void *uva, size_t size)
{
	va = (int *)uva;
	n = size;
	mem_comp.emplace(comp_device, CL_MEM_WRITE_ONLY, get_compr_size(size), nullptr);
}


size_t BDICompressor::get_max_size(size_t inbytes)
{
	size_t elem = (inbytes + sizeof(src_type) - 1) / sizeof(src_type);

	return get_compr_size(elem);
}

Memory& BDICompressor::compress()
{
	Memory mem_src(comp_device, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(src_type) * n, va);

	const size_t glob_size[] = {n / coeff};
	const size_t local_size[] = {BDI_GROUP_SIZE};

	clSetKernelArg(compressor.kernel, 0, sizeof(mem_src.mem), (const void *)&mem_src.mem);
	clSetKernelArg(compressor.kernel, 1, sizeof(mem_comp.value().mem), (const void *)&mem_comp.value().mem);
	clEnqueueNDRangeKernel(
		comp_device.queue, compressor.kernel,
		1, nullptr, glob_size, local_size,
		0, nullptr, nullptr);

	clFinish(comp_device.queue);
	return mem_comp.value();
}
