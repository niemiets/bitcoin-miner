#ifndef MINER_TX_IN_COINBASE_H
#define MINER_TX_IN_COINBASE_H

#include <cstdint>

#include "bitcoin/script.h"
#include "datatypes/compact_size_uint.h"

struct tx_in_coinbase
{
	char     hash[32] = {0};
	uint32_t index = 0xFFFFFFFF;
	
	compact_size_uint script_bytes;
	script            height;
	void             *coinbase_script;
	
	uint32_t sequence;
};

#endif //MINER_TX_IN_COINBASE_H
