#ifndef MINER_OUTPOINT_H
#define MINER_OUTPOINT_H

#include <cstdint>

struct outpoint
{
	char hash[32];
	
	uint32_t index;
};

#endif //MINER_OUTPOINT_H
