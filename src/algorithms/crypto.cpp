#include <cstdint>

constexpr int SHA_BLOCK_BIT_COUNT = 512;
constexpr int SHA_BLOCK_SIZE = SHA_BLOCK_BIT_COUNT / 8;

template <typename T>
uint8_t** sha256(T* input, size_t bit_count)
{
	size_t input_size = (((bit_count - 1) / 8) + 1);
	
	uint64_t block_count = ((bit_count + 1 + 64 - 1) / 512) + 1; // bit_count + the one 1 bit + 64 bit length - 1 because we want it to increment when its above 512
	
	uint8_t **digest = new uint8_t*[block_count];
	
	for (size_t i = 0; i < block_count; i++)
	{
		digest[i] = new uint8_t[SHA_BLOCK_SIZE];
	}
	
	for (size_t i = 0; i < input_size; i++)
	{
		digest[i / SHA_BLOCK_SIZE][i % SHA_BLOCK_SIZE] = input[i];
	}
	
	// append 1
	const size_t append_byte_index = ((bit_count - 1 + 1) / 8) + 1 - 1;
	const size_t append_bit_index = (bit_count - 1 + 1) % 8;
	
	digest[append_byte_index / SHA_BLOCK_SIZE][append_byte_index % SHA_BLOCK_SIZE] |= (0b10000000 >> append_bit_index);
	
	
	
	return digest;
}