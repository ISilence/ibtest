#define OMP_COEFF (sizeof(int) / sizeof(char))

__kernel void dump(
	__global const int * __restrict const values,
	__global int * __restrict const out)
{
	const int idx = get_global_id(0);

	out[idx] = values[idx];
}


__kernel void bdi_decompress(
	__global const char * __restrict const values,
	__global int * __restrict const dst)
{
	const int idx = get_global_id(0) * OMP_COEFF;
	const int n = get_global_size(0) * OMP_COEFF;
	const int block_size = get_local_size(0) * OMP_COEFF;
	__global int * __restrict const bs = (__global int *)(values + n);

	__global const char4 * __restrict const values2 = (__global char4 *)values;
	__global int4 * __restrict const dst2 = (__global int4 *)dst;

	int base = bs[idx / block_size];
	char4 value = values2[idx / OMP_COEFF];

	int4 res;
	res.x = base + value.x;
	res.y = base + value.y;
	res.z = base + value.z;
	res.w = base + value.w;
	dst2[idx / OMP_COEFF] = res;
}

__kernel void bdi_compress(
	__global const int * __restrict const src,
	__global char * __restrict const dst)
{
	const int idx = get_global_id(0) * OMP_COEFF;
	const int n = get_global_size(0) * OMP_COEFF;
	const int block_size = get_local_size(0) * OMP_COEFF;
	__global int * __restrict const bs = (__global int *)(dst + n);

	__global const int4 * __restrict const src2 = (__global int4 *)src;
	__global char4 * __restrict const dst2 = (__global char4 *)dst;

	int base = src[idx / block_size * block_size];
	int4 value = src2[idx / OMP_COEFF];

	char4 res;
	res.x = value.x - base;
	res.y = value.y - base;
	res.z = value.z - base;
	res.w = value.w - base;

	dst2[idx / OMP_COEFF] = res;
	if (get_local_id(0) == 0)
		bs[idx / block_size] = base;
}

