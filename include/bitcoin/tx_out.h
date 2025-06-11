#ifndef MINER_TX_OUT_H
#define MINER_TX_OUT_H

#include <cstdint>

#include "datatypes/compact_size_uint.h"

struct tx_out
{
	int64_t value;
	
	compact_size_uint pk_script_bytes;
	char             *pk_script;
};

#endif //MINER_TX_OUT_H
