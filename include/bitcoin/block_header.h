#ifndef MINER_BLOCK_HEADER_H
#define MINER_BLOCK_HEADER_H

#include <cstdint>

struct block_header
{
	int32_t  version; // TODO: maybe change it to some bitfield because of BIP 9
	char     previous_block_header_hash[32];
	char     merkle_root_hash[32];
	uint32_t time;
	uint32_t nbits;
	uint32_t nonce;
};

#endif //MINER_BLOCK_HEADER_H
