#ifndef MINER_RAW_TRANSACTION_H
#define MINER_RAW_TRANSACTION_H

#include <cstdint>

#include "bitcoin/tx_in.h"
#include "bitcoin/tx_out.h"
#include "datatypes/compact_size_uint.h"

struct raw_transaction
{
	int32_t version;
	
	compact_size_uint tx_in_count;
	tx_in            *tx_in;
	
	compact_size_uint tx_out_count;
	tx_out           *tx_out;
	
	uint32_t lock_time;
};

#endif //MINER_RAW_TRANSACTION_H
