#ifndef MINER_INVENTORY_TYPE_H
#define MINER_INVENTORY_TYPE_H

#include <cstdint>

enum class inventory_type: uint32_t
{
	MSG_TX = 1,
	MSG_BLOCK,
	MSG_FILTERED_BLOCK,
	MSG_CMPCT_BLOCK,
	// MSG_WITNESS_TX = 0x01000040, // TODO: understand which bits to set
	// MSG_WITNESS_BLOCK,
	// MSG_FILTERED_WITNESS_BLOCK
};

#endif //MINER_INVENTORY_TYPE_H
