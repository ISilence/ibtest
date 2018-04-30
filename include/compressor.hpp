#ifndef COMPRESSOR_HPP
#define COMPRESSOR_HPP

#include <CL/cl.h>

/* ====================================================
 * 		OpenCL wrappers
 */

enum E_DEVICE_TYPE {
    DT_CPU,
    DT_GPU,
    DT_IGPU
};

struct Device {
    cl_device_id id;
    cl_platform_id platform_id;
    cl_context context;
    cl_command_queue queue;

    ~Device();
    Device() = default;
    Device(Device&& dev) noexcept;
};

struct Memory {
    cl_mem mem;
    size_t size;
    Device& dev;

    Memory(Device& dev, cl_mem_flags flags, size_t sz, void *ptr = nullptr);
    ~Memory();
};


/* ====================================================
 * 		Compressor
 */

class DeviceFinder
{
public:
	static Device get_device(enum E_DEVICE_TYPE type);
	static Device& getIGPU();

	DeviceFinder() = delete;
	DeviceFinder(DeviceFinder const&) = delete;
	void operator=(DeviceFinder const&)  = delete;
};


#define BDI_GROUP_SIZE 32

class GPUCompressor
{
public:
	virtual void set_mem(void *va, size_t size) = 0;
	virtual size_t get_max_size(size_t inbytes) = 0;

	virtual Memory& compress() = 0;
};





#endif // COMPRESSOR_HPP
