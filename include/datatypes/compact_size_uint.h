#ifndef MINER_COMPACT_SIZE_UINT_H
#define MINER_COMPACT_SIZE_UINT_H

#include <cstdint>

struct compact_size_uint
{
	public:
		uint8_t size();
		uint64_t data();
		
		compact_size_uint(uint64_t value); // from little endian
		~compact_size_uint();
		
		static compact_size_uint from_little_endian(uint64_t value);
		static compact_size_uint from_big_endian(uint64_t value);
		
	private:
		uint8_t *_size_ptr;
		uint8_t *_data_ptr; // array of bytes stored in big endian
};

#endif //MINER_COMPACT_SIZE_UINT_H
