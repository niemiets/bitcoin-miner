#ifndef MINER_VERSION_H
#define MINER_VERSION_H

#include <cstdint>

struct version_message
{
	int32_t version;
	uint64_t services;
	int64_t timestamp;
	uint64_t addr_recv_services;
	char addr_recv_ip[16];
	uint16_t addr_recv_port;
	uint64_t addr_trans_services;
	char addr_trans_ip[16];
	uint16_t addr_trans_port;
	uint64_t nonce;
	compact_size_uint user_agent_bytes;
	char *user_agent;
	int32_t start_height;
	bool relay;
};

#endif //MINER_VERSION_H
