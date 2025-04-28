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
		const auto ptr = (uint8_t*)malloc(sizeof(uint8_t) + sizeof(uint16_t));
		
		_size_ptr = ptr;
		_data_ptr = ptr + sizeof(uint8_t);
		
		*_size_ptr = 0xFD;
		*_data_ptr = static_cast<uint16_t>(value);
	}
	else if (value <= 0xFFFFFFFF)
	{
		const auto ptr = (uint8_t*)malloc(sizeof(uint8_t) + sizeof(uint32_t));
		
		_size_ptr = ptr;
		_data_ptr = ptr + sizeof(uint8_t);
		
		*_size_ptr = 0xFE;
		*_data_ptr = static_cast<uint32_t>(value);
	}
	else if (value <= 0xFFFFFFFFFFFFFFFF)
	{
		const auto ptr = (uint8_t*)malloc(sizeof(uint8_t) + sizeof(uint64_t));
		
		_size_ptr = ptr;
		_data_ptr = ptr + sizeof(uint8_t);
		
		*_size_ptr = 0xFF;
		*_data_ptr = static_cast<uint64_t>(value);
	}
}