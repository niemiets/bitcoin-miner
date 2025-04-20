#include <stdlib.h>

#include "datatypes/compact_size_uint.h"

compact_size_uint::compact_size_uint(uint64_t value)
{
	if (value <= 0xFC)
	{
		_size_ptr = nullptr;
		_data_ptr = new uint8_t(value);
	}
	else if (value <= 0xFFFF)
	{
		const auto ptr = (uint8_t*)malloc(sizeof(uint8_t) + sizeof(uint8_t) * 2);
		
		_size_ptr = ptr;
		_data_ptr = ptr + 1;
		
		*_size_ptr = 0xFD;
		
		// TODO: set data from le value to be data
	}
	else if (value <= 0xFFFFFFFF)
	{
		const auto ptr = (uint8_t*)malloc(sizeof(uint8_t) + sizeof(uint8_t) * 4);
		
		_size_ptr = ptr;
		_data_ptr = ptr + 1;
		
		*_size_ptr = 0xFE;
		
		// TODO: set data from le value to be data
	}
	else if (value <= 0xFFFFFFFFFFFFFFFF)
	{
		const auto ptr = (uint8_t*)malloc(sizeof(uint8_t) + sizeof(uint8_t) * 8);
		
		_size_ptr = ptr;
		_data_ptr = ptr + 1;
		
		*_size_ptr = 0xFF;
		
		// TODO: set data from le value to be data
	}
}

compact_size_uint compact_size_uint::from_little_endian(uint64_t value)
{
	return compact_size_uint(value);
}