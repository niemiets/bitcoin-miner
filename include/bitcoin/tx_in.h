#ifndef MINER_TX_IN_H
#define MINER_TX_IN_H

#include <cstdint>

#include "bitcoin/outpoint.h"
#include "datatypes/compact_size_uint.h"

struct tx_in
{
	outpoint previous_output;
	
	compact_size_uint script_bytes;
	char             *signature_script;
	
	uint32_t sequence;
};

#endif //MINER_TX_IN_H
