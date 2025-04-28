#ifndef MINER_COMPACT_SIZE_UINT_H
#define MINER_COMPACT_SIZE_UINT_H

#include <cstdint>

struct compact_size_uint
{
	public:
		uint8_t size();
		uint64_t data();
		
		compact_size_uint(uint64_t value);
		~compact_size_uint();
		
	private:
		uint8_t *_size_ptr;
		uint8_t *_data_ptr;
};

#endif //MINER_COMPACT_SIZE_UINT_H
