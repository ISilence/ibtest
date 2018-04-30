#ifndef GPU_COMP_BDI_COMPRESSOR_HPP
#define GPU_COMP_BDI_COMPRESSOR_HPP

#include <optional>
#include "compressor.hpp"
#include "cl.hpp"

class BDICompressor: public GPUCompressor
{
public:
	Device& comp_device;
	std::optional<Memory> mem_comp;
	Program compressor;
	int * __restrict va;
	size_t n;

	typedef int src_type;
	typedef char trg_type;
	constexpr static int coeff = sizeof(src_type) / sizeof(trg_type);

	static size_t get_compr_size(size_t elem)
	{
		return elem * sizeof(trg_type) + elem / BDI_GROUP_SIZE / coeff * sizeof(src_type);
	}

	BDICompressor();

	void set_mem(void *va, size_t size) override;
	size_t get_max_size(size_t inbytes) override;

	Memory& compress() override;
};

#endif //GPU_COMP_BDI_COMPRESSOR_HPP
