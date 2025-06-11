#ifndef MINER_MESSAGE_BLOCK_H
#define MINER_MESSAGE_BLOCK_H

#include "bitcoin/block_header.h"
#include "bitcoin/raw_transaction.h"
#include "datatypes/compact_size_uint.h"

struct message_block
{
	block_header block_header;
	
	compact_size_uint txn_count;
	raw_transaction  *txns;
};

#endif //MINER_MESSAGE_BLOCK_H
