#ifndef MINER_MESSAGE_HEADER_H
#define MINER_MESSAGE_HEADER_H

#include <cstdint>

enum e_network
{
	Mainnet = 0xf9beb4d9,
	Testnet = 0x0b110907,
	Regtest = 0xfabfb5da
};

struct message_header
{
	union {
		char str[4];
		uint32_t value;
	} start_str;
	char cmd_name[12];
	uint32_t payload_size;
	char checksum[4];
};

#endif //MINER_MESSAGE_HEADER_H
