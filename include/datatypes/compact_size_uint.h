#ifndef MINER_COMPACT_SIZE_UINT_H
#define MINER_COMPACT_SIZE_UINT_H

#include <bitset>
#include <cstdint>
#include <stdexcept>
#include <stdlib.h>
#include <winsock2.h>

struct compact_size_uint
{
	public:
		uint8_t  size() const;
		uint64_t data() const;

		compact_size_uint();
		compact_size_uint(uint64_t value);
		~compact_size_uint();

		compact_size_uint& operator=(const compact_size_uint &other);
		compact_size_uint& operator=(compact_size_uint &&other) noexcept;

		uint8_t* operator*() { return _size_ptr; }
	    const uint8_t* operator*() const { return _size_ptr; }
		
	private:
		uint8_t  *_size_ptr;
		uint64_t *_data_ptr;
};

#endif //MINER_COMPACT_SIZE_UINT_H
