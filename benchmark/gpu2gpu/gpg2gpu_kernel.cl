__kernel void dump(
	__global const int * __restrict const values,
	__global int * __restrict const out)
{
	const int idx = get_global_id(0);

	out[idx] = values[idx];
}
