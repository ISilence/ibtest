#include <memory>
#include <fstream>
#include <unistd.h>

#include "compressor.hpp"
#include "cl.hpp"

int64_t pagesize = getpagesize();

Device::~Device()
{
	clFinish(queue);
	clReleaseCommandQueue(queue);
	clReleaseContext(context);
}

Device::Device(Device&& dev) noexcept
{
	memcpy(this, &dev, sizeof(dev));
	memset(&dev, 0, sizeof(dev));
}

Memory::Memory(Device& dev, cl_mem_flags flags, size_t sz, void *ptr)
	: dev(dev)
{
	cl_int err;

	size = sz;
	mem = clCreateBuffer(dev.context, flags, size, ptr, &err);
	cl_check(err);
}

Memory::~Memory()
{
	clReleaseMemObject(mem);
}

Device DeviceFinder::get_device(enum E_DEVICE_TYPE type)
{
	cl_int ret;
	cl_uint num_devices, num_platforms;
	cl_device_type cltype = (type == DT_CPU) ? CL_DEVICE_TYPE_CPU : CL_DEVICE_TYPE_GPU;
	bool unified = (type != DT_GPU);

	clGetPlatformIDs(0, nullptr, &num_platforms);
	cl_platform_id platforms[num_platforms];
	clGetPlatformIDs(num_platforms, platforms, nullptr);

	for (int i = 0; i < num_platforms; ++i) {
		cl_check(clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, 0, nullptr, &num_devices));
		std::unique_ptr<cl_device_id[]> devices(new cl_device_id[num_devices]);
		cl_check(clGetDeviceIDs(platforms[i], cltype, num_devices, devices.get(), nullptr));

		for (int j = 0; j < num_devices; j++) {
			cl_bool sma;
			cl_check(clGetDeviceInfo(devices[j], CL_DEVICE_HOST_UNIFIED_MEMORY, sizeof(sma), &sma, nullptr));

			if (sma == unified) {
				Device device = {};

				device.id = devices[j];
				device.platform_id = platforms[i];
				device.context = clCreateContext(nullptr, 1, &device.id, nullptr, nullptr, &ret);
				cl_check(ret);
				device.queue = clCreateCommandQueue(device.context, device.id, 0, &ret);
				cl_check(ret);
				return std::move(device);
			}
		}
	}

	throw std::exception();
}

Device& DeviceFinder::getIGPU()
{
	static Device device = DeviceFinder::get_device(DT_IGPU);

	return device;
}

static std::string read_file(const char *path)
{
	std::ifstream fd(path);
	std::istreambuf_iterator<char> endIt;

	return std::string(std::istreambuf_iterator<char>(fd), endIt);
}

Program compile_program(Device *device, const char *path, const char *kernel_name)
{
	cl_int ret;
	Program program = {};

	std::string source = read_file(path);
	if (source.length() == 0)
		throw std::exception();

	const char *kernel_str = source.c_str();
	size_t size = source.size();

	program.program = clCreateProgramWithSource(device->context, 1, &kernel_str, &size, &ret);
	cl_check(ret);

	ret = clBuildProgram(program.program, 1, &device->id, nullptr, nullptr, nullptr);
	if (ret != CL_SUCCESS) {
		size_t build_log_len = 1000;
		char buffer[1000];

		clGetProgramBuildInfo(program.program, device->id, CL_PROGRAM_BUILD_LOG, 0, buffer, &build_log_len);
		std::cerr << buffer << "(" << ret << ")" << std::endl;
		exit(1);
	}

	program.kernel = clCreateKernel(program.program, kernel_name, &ret);
	cl_check(ret);

	return program;
}
