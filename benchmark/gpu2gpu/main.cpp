#include <string>
#include <fstream>
#include <random>
#include <memory>
#include <cstring>
#include <iostream>

#include "cl.hpp"
#include "bdi_compressor.hpp"


#include "../../src/timer.hpp"

static std::unique_ptr<int[]>
get_random_vector(size_t n)
{
	std::unique_ptr<int[]> res(new int[n]);

	std::random_device r;
	std::default_random_engine e1(r());
	std::uniform_int_distribution<int> uniform_dist(-1, 1);

	res[0] = uniform_dist(e1) + 1000;
	for (size_t i = 1; i < n; ++i)
		res[i] = res[i - 1] + uniform_dist(e1);


	return std::move(res);
}

static double run_common(Device& gpu, size_t n)
{
	std::unique_ptr<int[]> vec = get_random_vector(n);
	size_t data_size = n * sizeof(vec[0]);
	Program dumper = compile_program(&gpu, "kernel.cl", "dump");
	Memory mem_src(gpu, CL_MEM_READ_ONLY, data_size);
	Memory mem_dump(gpu, CL_MEM_WRITE_ONLY, data_size);

	return Benchmark::run(4000, 8000, [&](Timer& timer) {
		cl_check(clEnqueueWriteBuffer(
			gpu.queue, mem_src.mem,
			CL_FALSE, 0,
			data_size, vec.get(),
			0, nullptr, nullptr));

		const size_t glob_size[] = {n};
		const size_t local_size[] = {BDI_GROUP_SIZE};
		cl_check(clSetKernelArg(dumper.kernel, 0, sizeof(mem_src.mem), (const void *)&mem_src.mem));
		cl_check(clSetKernelArg(dumper.kernel, 1, sizeof(mem_dump.mem), (const void *)&mem_dump.mem));
		cl_check(clEnqueueNDRangeKernel(
			gpu.queue, dumper.kernel,
			1, nullptr, glob_size, local_size,
			0, nullptr, nullptr));
		cl_check(clFinish(gpu.queue));

//		float *src = (float *)clEnqueueMapBuffer(
//			gpu.queue, mem_dump.mem, CL_TRUE, CL_MAP_READ, 0, data_size, 0, nullptr, nullptr, nullptr);
//		for (int i = 0; i < n; ++i) {
//			if (src[i] != vec[i]) {
//				std::cerr << "idx: " << i << "src: " << src[i] << " dst: " << vec[i] << std::endl;
//				exit(1);
//			}
//		}
	});
}

static void
copy_memobj(Memory& mem_src, Memory& mem_dst, size_t size)
{
	void *src = clEnqueueMapBuffer(
		mem_src.dev.queue, mem_src.mem, CL_TRUE, CL_MAP_READ, 0, size, 0, nullptr, nullptr, nullptr);
	void *dst = clEnqueueMapBuffer(
		mem_dst.dev.queue, mem_dst.mem, CL_TRUE, CL_MAP_WRITE, 0, size, 0, nullptr, nullptr, nullptr);

	std::memcpy(dst, src, size);
	clEnqueueUnmapMemObject(mem_src.dev.queue, mem_src.mem, src, 0, nullptr, nullptr);
	clEnqueueUnmapMemObject(mem_dst.dev.queue, mem_dst.mem, dst, 0, nullptr, nullptr);
}

static double
run_compressed(Device& gpu, Device& igpu, size_t n)
{
	std::unique_ptr<int[]> vec = get_random_vector(n);
	BDICompressor compressor;
	compressor.set_mem(vec.get(), n);

	size_t data_size = n * sizeof(vec[0]);
	size_t compr_size = BDICompressor::get_compr_size(n);

	Program dumper = compile_program(&gpu, "kernel.cl", "bdi_decompress");
	Memory mem_tmp(gpu, CL_MEM_WRITE_ONLY | CL_MEM_ALLOC_HOST_PTR, compr_size);
	Memory mem_dst(gpu, CL_MEM_READ_ONLY, compr_size);
	Memory mem_dump(gpu, CL_MEM_WRITE_ONLY, data_size);

	double r = Benchmark::run(4000, 8000, [&](Timer& timer) {
		const size_t glob_size[] = {n / BDICompressor::coeff};
		const size_t local_size[] = {BDI_GROUP_SIZE};
		Memory& mem_comp = compressor.compress();

		timer.stop();
		copy_memobj(mem_comp, mem_tmp, compr_size);
		timer.start();

		clEnqueueCopyBuffer(
			gpu.queue, mem_tmp.mem, mem_dst.mem,
			0, 0, compr_size,
			0, nullptr, nullptr);

		clSetKernelArg(dumper.kernel, 0, sizeof(mem_dst.mem), (const void *)&mem_dst.mem);
		clSetKernelArg(dumper.kernel, 1, sizeof(mem_dump.mem), (const void *)&mem_dump.mem);
		clEnqueueNDRangeKernel(
			gpu.queue, dumper.kernel,
			1, nullptr, glob_size, local_size,
			0, nullptr, nullptr);
		clFinish(gpu.queue);
	});


	auto *src = (BDICompressor::src_type *)clEnqueueMapBuffer(
		gpu.queue, mem_dump.mem, CL_TRUE, CL_MAP_READ, 0, data_size, 0, nullptr, nullptr, nullptr);
	for (int i = 0; i < 10000; ++i) {
		if (src[i] != vec[i]) {
			std::cerr << "idx: " << i << " src: " << src[i] << " dst: " << vec[i] << std::endl;
//			exit(1);
		}
	}

	return r;
}

int main()
{
	size_t task_size = 1024 * 1024;
//	Device gpu = DeviceFinder::get_device(DT_GPU);
	Device igpu = DeviceFinder::get_device(DT_IGPU);

	double defTime;

//	defTime = run_common(gpu, task_size);
//	std::cout << "gpu: " << defTime << " ms" << std::endl;
//
//	defTime = run_common(igpu, task_size);
//	std::cout << "igpu: " << defTime << " ms" << std::endl;

	defTime = run_compressed(igpu, igpu, task_size);
	std::cout << "compressed: " << defTime << " ms" << std::endl;

	return 0;
}
