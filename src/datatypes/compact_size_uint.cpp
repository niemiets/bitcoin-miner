#include <stdlib.h>
#include <bitset>
#include <stdexcept>

#include "datatypes/compact_size_uint.h"

uint8_t compact_size_uint::size()
{
	switch (*_size_ptr)
	{
		case 0xFD:
			return sizeof(uint8_t) + sizeof(uint16_t); // 3
		case 0xFE:
			return sizeof(uint8_t) + sizeof(uint32_t); // 5
		case 0xFF:
			return sizeof(uint8_t) + sizeof(uint64_t); // 9
		default:
			return sizeof(uint8_t);                    // 1
	}
}

uint64_t compact_size_uint::data()
{
	switch (size())
	{
		case sizeof(uint8_t):
			return *_size_ptr;
		case sizeof(uint8_t) + sizeof(uint16_t):
			return *_data_ptr & 0xFFFF;
		case sizeof(uint8_t) + sizeof(uint32_t):
			return *_data_ptr & 0xFFFFFFFF;
		case sizeof(uint8_t) + sizeof(uint64_t):
			return *_data_ptr;
		default:
			throw std::runtime_error("Impossible value of size: " + std::to_string(size()));
	}
}

compact_size_uint::compact_size_uint(uint64_t value)
{
	if (value <= 0xFC)
	{
		_size_ptr = new uint8_t(value);
		_data_ptr = (uint64_t*)_size_ptr;
	}
	else if (value <= 0xFFFF)
	{
		const auto ptr = (uint8_t*)malloc(sizeof(uint8_t) + sizeof(uint16_t));
		
		_size_ptr = ptr;
		_data_ptr = (uint64_t*)(ptr + sizeof(uint8_t));
		
		*_size_ptr = 0xFD;
		*_data_ptr = static_cast<uint16_t>(value);
	}
	else if (value <= 0xFFFFFFFF)
	{
		const auto ptr = (uint8_t*)malloc(sizeof(uint8_t) + sizeof(uint32_t));
		
		_size_ptr = ptr;
		_data_ptr = (uint64_t*)(ptr + sizeof(uint8_t));
		
		*_size_ptr = 0xFE;
		*_data_ptr = static_cast<uint32_t>(value);
	}
	else if (value <= 0xFFFFFFFFFFFFFFFF)
	{
		const auto ptr = (uint8_t*)malloc(sizeof(uint8_t) + sizeof(uint64_t));
		
		_size_ptr = ptr;
		_data_ptr = (uint64_t*)(ptr + sizeof(uint8_t));
		
		*_size_ptr = 0xFF;
		*_data_ptr = static_cast<uint64_t>(value);
	}
}

compact_size_uint::~compact_size_uint()
{
	free(_size_ptr);
}