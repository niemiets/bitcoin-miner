#ifndef MINER_MESSAGE_HEADER_H
#define MINER_MESSAGE_HEADER_H

#include <cstdint>

enum e_network
{
	
};

struct message_header
{
	char start_str[4];
	char cmd_name[12];
	uint32_t payload_size;
	char checksum[4];
};

#endif //MINER_MESSAGE_HEADER_H
