#ifndef MINER_MESSAGE_GETBLOCKS_H
#define MINER_MESSAGE_GETBLOCKS_H

#include <cstdint>

#include "datatypes/compact_size_uint.h"

struct message_getblock
{
	uint32_t version;
	
	compact_size_uint hash_count;
	char            **block_header_hashes;
	char              stop_hash[32];
};

#endif //MINER_MESSAGE_GETBLOCKS_H
