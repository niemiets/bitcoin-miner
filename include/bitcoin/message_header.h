#ifndef MINER_MESSAGE_HEADER_H
#define MINER_MESSAGE_HEADER_H

#include <cstdint>

struct message_header
{
	char start_string[4];
	char command_name[12];
	uint32_t payload_size;
	char checksum[4];
};

#endif //MINER_MESSAGE_HEADER_H
