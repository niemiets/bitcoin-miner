#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iostream>
#include <random>
#include <span>
#include <spanstream>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "algorithms/crypto.h"
#include "bitcoin/block_header.h"
#include "bitcoin/message_header.h"
#include "bitcoin/network_type.h"
#include "bitcoin/tx_in.h"
#include "bitcoin/messages/message_block.h"
#include "bitcoin/messages/message_inv.h"
#include "bitcoin/messages/message_ping.h"
#include "bitcoin/messages/message_version.h"
#include "datatypes/compact_size_uint.h"

typedef uint8_t byte;

constexpr uint64_t MAX_UINT64 = 0xFFFFFFFFFFFFFFFF;

constexpr uint32_t BLOCK_SIZE = 4 + 32 + 32 + 4 + 4 + 4;

constexpr uint32_t MAX_SIZE = 0x02000000;
constexpr uint32_t MSG_HEADER_SIZE = 4 + 12 + 4 + 4;
constexpr uint32_t MSG_BLOCK_MIN_SIZE = 4 + 32 + 32 + 4 + 4 + 4 + 1;
constexpr uint32_t MSG_GETHEADERS_MIN_SIZE = 4 + 1 + 32;
constexpr uint32_t MSG_INV_MIN_SIZE = 1;
constexpr uint32_t MSG_PING_SIZE = 8;
constexpr uint32_t MSG_VERSION_MIN_SIZE = 4 + 8 + 8 + 8 + 16 + 2 + 8 + 16 + 2 + 8 + 1 + 4;

constexpr uint16_t NETWORK_BUFFER_SIZE = 1024;

constexpr uint32_t MSG_VERACK_CHECKSUM = std::byteswap(0x5df6e0e2);

constexpr int SUCCESS = 0;

int recv_all(SOCKET &sock, char* buffer, int length, int flags);

int discard(const SOCKET &sock, uint64_t length, int flags = 0);

int recv_message_block(const SOCKET &sock, message_block &msg_block);
int recv_message_header(const SOCKET &sock, message_header &msg_header);
int recv_message_inv(const SOCKET &sock, message_inv &msg_inv);
int recv_message_ping(const SOCKET &sock, message_ping &msg_ping);
int recv_message_version(const SOCKET &sock, const message_header &msg_header, message_version &msg_version);
int recv_compact_size_uint(const SOCKET &sock, compact_size_uint &cmpct);
int recv_raw_transaction(const SOCKET &sock, raw_transaction &transaction);
int recv_tx_in(const SOCKET &sock, tx_in &tx);
int recv_tx_out(const SOCKET &sock, tx_out &tx);
int recv_outpoint(const SOCKET &sock, outpoint &outpoint);

void hash_block_header(const block_header &blk_header, uint32_t *hash);
void hash_raw_transaction(const raw_transaction &transaction, uint32_t *hash);

uint8_t get_height_size(uint32_t height);

void nbits_to_target(uint32_t nbits, uint8_t *target);

bool is_data_available(const SOCKET &sock);

int main()
{
	const char *HOSTNAME = "188.214.129.52";
	const char *PORT	 = mainnet::port_string;
	
	constexpr int32_t version = 70016;
	const char *start_string = mainnet::start_string;
	constexpr char *user_agent = "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456";
	
	std::random_device rand_dev;
	std::mt19937_64 rng(rand_dev());
	std::uniform_int_distribution<std::mt19937_64::result_type> nonce_distribution(1, MAX_UINT64);
	
	bool connected = false;

	int err_code = SUCCESS;

	WSADATA init_data;

	err_code = WSAStartup(MAKEWORD(2, 2), &init_data);

	// TODO: handle all error cases for all functions
	if (err_code != SUCCESS)
	{
		std::cerr << "Couldn't initiate winsock dll: " << err_code << '\n';

		return 1;
	}

	constexpr addrinfo hints {
		.ai_flags = 0,
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = IPPROTO_TCP,
		.ai_addrlen = 0,
		.ai_canonname = nullptr,
		.ai_addr = nullptr,
		.ai_next = nullptr
	};

	addrinfo *res = new addrinfo;

	err_code = getaddrinfo(HOSTNAME, PORT, &hints, &res);

	if (err_code != SUCCESS)
	{
		std::cerr << "Couldn't get address info: " << err_code << '\n';

		WSACleanup();

		return 1;
	}

	std::cout << "Got address info." << '\n';

	const SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

	if (sock == INVALID_SOCKET)
	{
		std::cerr << "Couldn't create socket: " << WSAGetLastError() << '\n';

		freeaddrinfo(res);
		WSACleanup();

		return 1;
	}

	std::cout << "Created socket." << '\n';

	err_code = connect(sock, res->ai_addr, static_cast<int>(res->ai_addrlen));

	if (err_code == SOCKET_ERROR)
	{
		std::cerr << "Couldn't connect to socket: " << WSAGetLastError() << '\n';

		freeaddrinfo(res);
		closesocket(sock);
		WSACleanup();

		return 1;
	}
	
	connected = true;

	std::cout << "Connected to socket." << '\n';
	
	{
		constexpr uint64_t services = 0x00;
		const int64_t timestamp = std::time(nullptr);
		
		constexpr uint64_t addr_recv_services = 0x00;
		char addr_recv_ip[16];
		constexpr uint16_t addr_recv_port = std::byteswap(mainnet::port);
		
		inet_pton(AF_INET6, ("::ffff:" + std::string(HOSTNAME)).c_str(), addr_recv_ip);
		
		constexpr uint64_t addr_trans_services = 0x00;
		char addr_trans_ip[16];
		constexpr uint16_t addr_trans_port = std::byteswap(mainnet::port);
		
		inet_pton(AF_INET6, "::ffff:127.0.0.1", addr_trans_ip);
		
		const uint64_t nonce = nonce_distribution(rng);
		
		const compact_size_uint user_agent_bytes(strnlen_s(user_agent, MAX_SIZE));
		
		assert(user_agent_bytes.data() <= 256);
		
		constexpr int32_t start_height = 0;
		
		constexpr bool relay = false;
		
		std::vector<char> payload_buffer(MSG_VERSION_MIN_SIZE - 1 + user_agent_bytes.size() + user_agent_bytes.data() + 1);
		const std::span payload_span(payload_buffer);
		std::spanstream payload_stream(payload_span, std::ios::binary | std::ios::in | std::ios::out);
		
		payload_stream
		.write(reinterpret_cast<const char *>(&version), 4)
		.write(reinterpret_cast<const char *>(&services), 8)
		.write(reinterpret_cast<const char *>(&timestamp), 8)
		.write(reinterpret_cast<const char *>(&addr_recv_services), 8)
		.write(reinterpret_cast<const char *>(addr_recv_ip), 16)
		.write(reinterpret_cast<const char *>(&addr_recv_port), 2)
		.write(reinterpret_cast<const char *>(&addr_trans_services), 8)
		.write(reinterpret_cast<const char *>(addr_trans_ip), 16)
		.write(reinterpret_cast<const char *>(&addr_trans_port), 2)
		.write(reinterpret_cast<const char *>(&nonce), 8)
		.write(reinterpret_cast<const char *>(*user_agent_bytes), user_agent_bytes.size())
		.write(reinterpret_cast<const char *>(user_agent), static_cast<std::streamsize>(user_agent_bytes.data()))
		.write(reinterpret_cast<const char *>(&start_height), 4)
		.write(reinterpret_cast<const char *>(&relay), 1);
		
		constexpr char *command_name = "version\0\0\0\0\0";
		
		const uint32_t payload_size = payload_stream.span().size();
		
		char checksum[32];
		
		sha256(reinterpret_cast<const byte *>(payload_stream.span().data()), payload_stream.span().size() * 8, reinterpret_cast<uint32_t *>(checksum));
		sha256(reinterpret_cast<const byte *>(checksum), 256, reinterpret_cast<uint32_t *>(checksum));
		
		std::vector<char> header_buffer(MSG_HEADER_SIZE);
		const std::span header_span(header_buffer);
		std::spanstream header_stream(header_span, std::ios::binary | std::ios::in | std::ios::out);
		
		header_stream
		.write(reinterpret_cast<const char *>(start_string), 4)
		.write(reinterpret_cast<const char *>(command_name), 12)
		.write(reinterpret_cast<const char *>(&payload_size), 4)
		.write(reinterpret_cast<const char *>(checksum), 4);
		
		std::cout << "Message header:\n";
		
		for (const char &c : header_stream.span())
		{
			printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
		}
		
		std::cout << "\nMessage payload:\n";
		
		for (const char &c : payload_stream.span())
		{
			printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
		}
		
		std::cout << '\n';
		
		int bytes_count;
		
		bytes_count = send(sock, header_stream.span().data(), static_cast<int>(header_stream.span().size()), 0);
		
		if (bytes_count == SOCKET_ERROR)
		{
			std::cerr << "Couldn't send message header: " << WSAGetLastError() << '\n';
			
			freeaddrinfo(res);
			closesocket(sock);
			WSACleanup();
			
			return 1;
		}
		
		std::cout << "Sent message header.\n";
		
		bytes_count = send(sock, payload_stream.span().data(), static_cast<int>(payload_stream.span().size()), 0);
		
		if (bytes_count == SOCKET_ERROR)
		{
			std::cerr << "Couldn't send message payload: " << WSAGetLastError() << '\n';
			
			freeaddrinfo(res);
			closesocket(sock);
			WSACleanup();
			
			return 1;
		}
		
		std::cout << "Sent message payload.\n";
	}
	
	uint32_t height = 0;
	
	uint8_t scrpt_height_size = 1 + get_height_size(height);
	
	uint8_t *scrpt_height = new uint8_t[scrpt_height_size];
	
	scrpt_height[0] = scrpt_height_size - 1;
	
	for (uint8_t i = 1; i < scrpt_height_size; i++)
	{
		scrpt_height[i] = height >> i;
	}
	
	uint8_t scrpt_pubkey[22] = {
		0x00,
		0x14,
		0x80,
		0xf2,
		0xe1,
		0x65,
		0xab,
		0xfe,
		0x92,
		0x3e,
		0x50,
		0x66,
		0x81,
		0x30,
		0x72,
		0x16,
		0x43,
		0x9c,
		0x7f,
		0x71,
		0x58,
		0xc9
	};
	
	raw_transaction coinbase_transaction = {
		.version = 2,
		.tx_in_count = 1,
		.tx_in = new tx_in[] {
			tx_in {
				.previous_output = {
					.hash = {0},
					.index = 0xffffffff
				},
				.script_bytes = scrpt_height_size,
				.signature_script = reinterpret_cast<char *>(scrpt_height),
				.sequence = 0xffffffff,
			}
		},
		.tx_out_count = 1,
		.tx_out = new tx_out[] {
			tx_out {
				.value = 312500000,
				.pk_script_bytes = sizeof(scrpt_pubkey),
				.pk_script = reinterpret_cast<char *>(scrpt_pubkey)
			}
		},
		.lock_time = 0
	};
	
	block_header block_latest = {
		.version = 1,
		.previous_block_header_hash = {0},
		.merkle_root_hash = {0x3b, static_cast<char>(0xa3), static_cast<char>(0xed), static_cast<char>(0xfd), 0x7a, 0x7b, 0x12, static_cast<char>(0xb2), 0x7a, static_cast<char>(0xc7), 0x2c, 0x3e, 0x67, 0x76, static_cast<char>(0x8f), 0x61, 0x7f, static_cast<char>(0xc8), 0x1b, static_cast<char>(0xc3), static_cast<char>(0x88), static_cast<char>(0x8a), 0x51, 0x32, 0x3a, static_cast<char>(0x9f), static_cast<char>(0xb8), static_cast<char>(0xaa), 0x4b, 0x1e, 0x5e, 0x4a},
		.time = 0x495fab29,
		.nbits = 0x1d00ffff,
		.nonce = 0x7c2bac1d
	}; // genesis block (height: 0)

	message_block block_candidate = {
		.block_header = {
			.version = 0,
			.previous_block_header_hash = {},
			.merkle_root_hash = {},
			.time = static_cast<uint32_t>(std::time(nullptr)),
			.nbits = block_latest.nbits,
			.nonce = 0
		},
		.txn_count = 1,
		.txns = &coinbase_transaction
	};

	hash_block_header(block_latest, reinterpret_cast<uint32_t*>(block_candidate.block_header.previous_block_header_hash));
	
	hash_raw_transaction(coinbase_transaction, reinterpret_cast<uint32_t *>(block_candidate.block_header.merkle_root_hash));
	
	uint8_t target[32];
	
	nbits_to_target(block_candidate.block_header.nbits, target);
	
	message_version msg_version;
	
	while (connected)
	{
		if (is_data_available(sock))
		{
			message_header msg_header {};
			
			std::cout << "Waiting for message header.\n";
			
			err_code = recv_message_header(sock, msg_header);
			
			if (err_code != SUCCESS)
			{
				std::cerr << "Couldn't receive message header.\n";
				
				freeaddrinfo(res);
				closesocket(sock);
				WSACleanup();
				
				return 1;
			}
			
			std::cout << "Received message header.\n";
			
			if (memcmp(msg_header.command_name, "version\0\0\0\0", 12) == 0)
			{
				err_code = recv_message_version(sock, msg_header, msg_version);
				
				if (err_code != SUCCESS)
				{
					std::cerr << "Couldn't receive message version." << '\n';
					
					freeaddrinfo(res);
					closesocket(sock);
					WSACleanup();
					
					return 1;
				}
				
				std::cout << "Received message version.\n";
			}
			else if (memcmp(msg_header.command_name, "verack\0\0\0\0\0\0", 12) == 0)
			{
				{
					constexpr char *command_name = "verack\0\0\0\0\0\0";
					
					constexpr uint32_t payload_size = 0;
					
					std::vector<char> header_buffer(MSG_HEADER_SIZE);
					const std::span header_span(header_buffer);
					std::spanstream header_stream(header_span, std::ios::binary | std::ios::in | std::ios::out);
					
					header_stream
					.write(reinterpret_cast<const char *>(start_string), 4)
					.write(reinterpret_cast<const char *>(command_name), 12)
					.write(reinterpret_cast<const char *>(&payload_size), 4)
					.write(reinterpret_cast<const char *>(&MSG_VERACK_CHECKSUM), 4);
					
					std::cout << "Message header:\n";
					
					for (const char &c : header_stream.span())
					{
						printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
					}
					
					int bytes_count = send(sock, header_stream.span().data(), static_cast<int>(header_stream.span().size()), 0);
					
					if (bytes_count == SOCKET_ERROR)
					{
						std::cerr << "Couldn't send message header: " << WSAGetLastError() << '\n';
						
						freeaddrinfo(res);
						closesocket(sock);
						WSACleanup();
						
						return 1;
					}
					
					std::cout << "\nSent message header.\n";
				}
				
				{
					char hash[32];
					
					hash_block_header(block_latest, reinterpret_cast<uint32_t *>(hash));
					
					compact_size_uint hash_count(1);
					char *stop_hash = reinterpret_cast<char *>(calloc(32, sizeof(char)));
					
					std::vector<char> payload_buffer(MSG_GETHEADERS_MIN_SIZE - 1 + hash_count.size() + (hash_count.data() * sizeof(inventory)));
					const std::span payload_span(payload_buffer);
					std::spanstream payload_stream(payload_span, std::ios::binary | std::ios::in | std::ios::out);
					
					payload_stream
					.write(reinterpret_cast<const char *>(&version), 4)
					.write(reinterpret_cast<const char *>(*hash_count), hash_count.size())
					.write(reinterpret_cast<const char *>(hash), 32)
					.write(reinterpret_cast<const char *>(stop_hash), 32);
					
					constexpr char *command_name = "getblocks\0\0\0";
					
					const uint32_t payload_size = payload_stream.span().size();
					
					char checksum[32];
					
					sha256(reinterpret_cast<const byte *>(payload_stream.span().data()), payload_stream.span().size() * 8, reinterpret_cast<uint32_t *>(checksum));
					sha256(reinterpret_cast<const byte *>(checksum), 256, reinterpret_cast<uint32_t *>(checksum));
					
					std::vector<char> header_buffer(MSG_HEADER_SIZE);
					const std::span header_span(header_buffer);
					std::spanstream header_stream(header_span, std::ios::binary | std::ios::in | std::ios::out);
					
					header_stream
					.write(reinterpret_cast<const char *>(start_string), 4)
					.write(reinterpret_cast<const char *>(command_name), 12)
					.write(reinterpret_cast<const char *>(&payload_size), 4)
					.write(reinterpret_cast<const char *>(&checksum), 4);
					
					std::cout << "Message header:\n";
					
					for (const char &c : header_stream.span())
					{
						printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
					}
					
					std::cout << "\nMessage payload:\n";
					
					for (const char &c : payload_stream.span())
					{
						printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
					}
					
					std::cout << '\n';
					
					int bytes_count = send(sock, header_stream.span().data(), static_cast<int>(header_stream.span().size()), 0);
					
					if (bytes_count == SOCKET_ERROR)
					{
						std::cerr << "Couldn't send message header: " << WSAGetLastError() << '\n';
						
						freeaddrinfo(res);
						closesocket(sock);
						WSACleanup();
						
						return 1;
					}
					
					std::cout << "Sent message header.\n";
					
					bytes_count = send(sock, payload_stream.span().data(), static_cast<int>(payload_stream.span().size()), 0);
					
					if (bytes_count == SOCKET_ERROR)
					{
						std::cerr << "Couldn't send message payload: " << WSAGetLastError() << '\n';
						
						freeaddrinfo(res);
						closesocket(sock);
						WSACleanup();
						
						return 1;
					}
					
					std::cout << "Sent message payload.\n";
				}
			}
			else if (memcmp(msg_header.command_name, "inv\0\0\0\0\0\0\0\0\0", 12) == 0)
			{
				message_inv msg_inv;
				
				err_code = recv_message_inv(sock, msg_inv);
				
				if (err_code != SUCCESS)
				{
					std::cerr << "Couldn't receive message inv.\n";
					
					freeaddrinfo(res);
					closesocket(sock);
					WSACleanup();
					
					return 1;
				}
				
				std::cout << "Received message inv.\n";
				
				{
					std::vector<char> payload_buffer(MSG_INV_MIN_SIZE - 1 + msg_inv.count.size() + (msg_inv.count.data() * sizeof(inventory)));
					const std::span payload_span(payload_buffer);
					std::spanstream payload_stream(payload_span, std::ios::binary | std::ios::in | std::ios::out);
					
					payload_stream
					.write(reinterpret_cast<const char *>(*(msg_inv.count)), msg_inv.count.size());
					
					for (uint64_t i = 0; i < msg_inv.count.data(); i++)
					{
						payload_stream
						.write(reinterpret_cast<const char *>(&(msg_inv.inventory[i].type)), 4)
						.write(reinterpret_cast<const char *>(msg_inv.inventory[i].hash), 32);
					}
					
					constexpr char *command_name = "getdata\0\0\0\0\0";
					
					const uint32_t payload_size = payload_stream.span().size();
					
					char checksum[32];
					
					sha256(reinterpret_cast<const byte *>(payload_stream.span().data()), payload_stream.span().size() * 8, reinterpret_cast<uint32_t *>(checksum));
					sha256(reinterpret_cast<const byte *>(checksum), 256, reinterpret_cast<uint32_t *>(checksum));
					
					std::vector<char> header_buffer(MSG_HEADER_SIZE);
					const std::span header_span(header_buffer);
					std::spanstream header_stream(header_span, std::ios::binary | std::ios::in | std::ios::out);
					
					header_stream
					.write(reinterpret_cast<const char *>(start_string), 4)
					.write(reinterpret_cast<const char *>(command_name), 12)
					.write(reinterpret_cast<const char *>(&payload_size), 4)
					.write(reinterpret_cast<const char *>(&checksum), 4);
					
					std::cout << "Message header:\n";
					
					for (const char &c : header_stream.span())
					{
						printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
					}
					
					std::cout << "\nMessage payload:\n";
					
					for (const char &c : payload_stream.span())
					{
						printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
					}
					
					std::cout << '\n';
					
					int bytes_count = send(sock, header_stream.span().data(), static_cast<int>(header_stream.span().size()), 0);
					
					if (bytes_count == SOCKET_ERROR)
					{
						std::cerr << "Couldn't send message header: " << WSAGetLastError() << '\n';
						
						freeaddrinfo(res);
						closesocket(sock);
						WSACleanup();
						
						return 1;
					}
					
					std::cout << "Sent message header.\n";
					
					bytes_count = send(sock, payload_stream.span().data(), static_cast<int>(payload_stream.span().size()), 0);
					
					if (bytes_count == SOCKET_ERROR)
					{
						std::cerr << "Couldn't send message payload: " << WSAGetLastError() << '\n';
						
						freeaddrinfo(res);
						closesocket(sock);
						WSACleanup();
						
						return 1;
					}
					
					std::cout << "Sent message payload.\n";
				}
			}
			else if (memcmp(msg_header.command_name, "block\0\0\0\0\0\0\0", 12) == 0)
			{
				message_block msg_block;
				
				err_code = recv_message_block(sock, msg_block);
				
				if (err_code != SUCCESS)
				{
					std::cerr << "Couldn't receive message block.\n";
					
					freeaddrinfo(res);
					closesocket(sock);
					WSACleanup();
					
					return 1;
				}
				
				std::cout << "Received message block.\n";
				
				block_latest = msg_block.block_header;
				
				hash_block_header(block_latest, reinterpret_cast<uint32_t *>(block_candidate.block_header.previous_block_header_hash));
				
				block_candidate.block_header.time = static_cast<uint32_t>(std::time(nullptr));
				block_candidate.block_header.nbits = block_latest.nbits;
				block_candidate.block_header.nonce = 0;
				
				nbits_to_target(block_candidate.block_header.nbits, target);
			}
			else if (memcmp(msg_header.command_name, "ping\0\0\0\0\0\0\0\0", 12) == 0)
			{
				message_ping msg_ping{};
				
				err_code = recv_message_ping(sock, msg_ping);
				
				if (err_code != SUCCESS)
				{
					std::cerr << "Couldn't receive message ping.\n";
					
					freeaddrinfo(res);
					closesocket(sock);
					WSACleanup();
					
					return 1;
				}
				
				std::cout << "Received message ping.\n";
				
				{
					std::vector<char> payload_buffer(MSG_PING_SIZE);
					const std::span payload_span(payload_buffer);
					std::spanstream payload_stream(payload_span, std::ios::binary | std::ios::in | std::ios::out);
					
					payload_stream
					.write(reinterpret_cast<const char *>(&msg_ping.nonce), 8);
					
					constexpr char *command_name = "pong\0\0\0\0\0\0\0\0";
					
					const uint32_t payload_size = payload_stream.span().size();
					
					char checksum[32];
					
					sha256(reinterpret_cast<const byte *>(payload_stream.span().data()), payload_stream.span().size() * 8, reinterpret_cast<uint32_t *>(checksum));
					sha256(reinterpret_cast<const byte *>(checksum), 256, reinterpret_cast<uint32_t *>(checksum));
					
					std::vector<char> header_buffer(MSG_HEADER_SIZE);
					const std::span header_span(header_buffer);
					std::spanstream header_stream(header_span, std::ios::binary | std::ios::in | std::ios::out);
					
					header_stream
					.write(reinterpret_cast<const char *>(start_string), 4)
					.write(reinterpret_cast<const char *>(command_name), 12)
					.write(reinterpret_cast<const char *>(&payload_size), 4)
					.write(reinterpret_cast<const char *>(&checksum), 4);
					
					std::cout << "Message header:\n";
					
					for (const char &c : header_stream.span())
					{
						printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
					}
					
					std::cout << "\nMessage payload:\n";
					
					for (const char &c : payload_stream.span())
					{
						printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
					}
					
					std::cout << '\n';
					
					int bytes_count = send(sock, header_stream.span().data(), static_cast<int>(header_stream.span().size()), 0);
					
					if (bytes_count == SOCKET_ERROR)
					{
						std::cerr << "Couldn't send message header: " << WSAGetLastError() << '\n';
						
						freeaddrinfo(res);
						closesocket(sock);
						WSACleanup();
						
						return 1;
					}
					
					std::cout << "Sent message header.\n";
					
					bytes_count = send(sock, payload_stream.span().data(), static_cast<int>(payload_stream.span().size()), 0);
					
					if (bytes_count == SOCKET_ERROR)
					{
						std::cerr << "Couldn't send message payload: " << WSAGetLastError() << '\n';
						
						freeaddrinfo(res);
						closesocket(sock);
						WSACleanup();
						
						return 1;
					}
					
					std::cout << "Sent message payload.\n";
				}
			}
			else if (memcmp(msg_header.command_name, "getdata\0\0\0\0\0", 12) == 0)
			{
				message_inv msg_getdata;
				
				err_code = recv_message_inv(sock, msg_getdata);
				
				if (err_code != SUCCESS)
				{
					std::cerr << "Couldn't receive message getdata.\n";
					
					freeaddrinfo(res);
					closesocket(sock);
					WSACleanup();
					
					return 1;
				}
				
				std::cout << "Received message getdata.\n";
				
				{
					std::vector<char> payload_buffer(
						MSG_BLOCK_MIN_SIZE +
						4 +
						block_candidate.txns[0].tx_in_count.size() +
						32 +
						4 +
						block_candidate.txns[0].tx_in[0].script_bytes.size() +
						block_candidate.txns[0].tx_in[0].script_bytes.data() +
						4 +
						block_candidate.txns[0].tx_out_count.size() +
						8 +
						block_candidate.txns[0].tx_out[0].pk_script_bytes.size() +
						block_candidate.txns[0].tx_out[0].pk_script_bytes.data() +
						4
					);
					const std::span payload_span(payload_buffer);
					std::spanstream payload_stream(payload_span, std::ios::binary | std::ios::in | std::ios::out);
					
					payload_stream
					.write(reinterpret_cast<const char *>(&block_candidate.block_header.version), 4)
					.write(reinterpret_cast<const char *>(block_candidate.block_header.previous_block_header_hash), 32)
					.write(reinterpret_cast<const char *>(block_candidate.block_header.merkle_root_hash), 32)
					.write(reinterpret_cast<const char *>(&block_candidate.block_header.time), 4)
					.write(reinterpret_cast<const char *>(&block_candidate.block_header.nbits), 4)
					.write(reinterpret_cast<const char *>(&block_candidate.block_header.nonce), 4)
					.write(reinterpret_cast<const char *>(*block_candidate.txn_count), block_candidate.txn_count.size())
					.write(reinterpret_cast<const char *>(&block_candidate.txns[0].version), 4)
					.write(reinterpret_cast<const char *>(*block_candidate.txns[0].tx_in_count), block_candidate.txns[0].tx_in_count.size())
					.write(reinterpret_cast<const char *>(block_candidate.txns[0].tx_in[0].previous_output.hash), 32)
					.write(reinterpret_cast<const char *>(&block_candidate.txns[0].tx_in[0].previous_output.index), 4)
					.write(reinterpret_cast<const char *>(*block_candidate.txns[0].tx_in[0].script_bytes), block_candidate.txns[0].tx_in[0].script_bytes.size())
					.write(reinterpret_cast<const char *>(block_candidate.txns[0].tx_in[0].signature_script), static_cast<std::streamsize>(block_candidate.txns[0].tx_in[0].script_bytes.data()))
					.write(reinterpret_cast<const char *>(&block_candidate.txns[0].tx_in[0].sequence), 4)
					.write(reinterpret_cast<const char *>(*block_candidate.txns[0].tx_out_count), block_candidate.txns[0].tx_out_count.size())
					.write(reinterpret_cast<const char *>(&block_candidate.txns[0].tx_out[0].value), 8)
					.write(reinterpret_cast<const char *>(*block_candidate.txns[0].tx_out[0].pk_script_bytes), block_candidate.txns[0].tx_out[0].pk_script_bytes.size())
					.write(reinterpret_cast<const char *>(block_candidate.txns[0].tx_out[0].pk_script), static_cast<std::streamsize>(block_candidate.txns[0].tx_out[0].pk_script_bytes.data()))
					.write(reinterpret_cast<const char *>(&block_candidate.txns[0].lock_time), 4);
					
					constexpr char *command_name = "block\0\0\0\0\0\0\0";
					
					const uint32_t payload_size = payload_stream.span().size();
					
					char checksum[32];
					
					sha256(reinterpret_cast<const byte *>(payload_stream.span().data()), payload_stream.span().size() * 8, reinterpret_cast<uint32_t *>(checksum));
					sha256(reinterpret_cast<const byte *>(checksum), 256, reinterpret_cast<uint32_t *>(checksum));
					
					std::vector<char> header_buffer(MSG_HEADER_SIZE);
					const std::span header_span(header_buffer);
					std::spanstream header_stream(header_span, std::ios::binary | std::ios::in | std::ios::out);
					
					header_stream
					.write(reinterpret_cast<const char *>(start_string), 4)
					.write(reinterpret_cast<const char *>(command_name), 12)
					.write(reinterpret_cast<const char *>(&payload_size), 4)
					.write(reinterpret_cast<const char *>(&checksum), 4);
					
					std::cout << "Message header:\n";
					
					for (const char &c : header_stream.span())
					{
						printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
					}
					
					std::cout << "\nMessage payload:\n";
					
					for (const char &c : payload_stream.span())
					{
						printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
					}
					
					std::cout << '\n';
					
					int bytes_count = send(sock, header_stream.span().data(), static_cast<int>(header_stream.span().size()), 0);
					
					if (bytes_count == SOCKET_ERROR)
					{
						std::cerr << "Couldn't send message header: " << WSAGetLastError() << '\n';
						
						freeaddrinfo(res);
						closesocket(sock);
						WSACleanup();
						
						return 1;
					}
					
					std::cout << "Sent message header.\n";
					
					bytes_count = send(sock, payload_stream.span().data(), static_cast<int>(payload_stream.span().size()), 0);
					
					if (bytes_count == SOCKET_ERROR)
					{
						std::cerr << "Couldn't send message payload: " << WSAGetLastError() << '\n';
						
						freeaddrinfo(res);
						closesocket(sock);
						WSACleanup();
						
						return 1;
					}
					
					std::cout << "Sent message payload.\n";
				}
			}
			else
			{
				{
					char tmp[13];
					tmp[12] = '\0';
					memcpy(tmp, msg_header.command_name, 12);
					
					std::cout << "Got " << tmp << " message.\n";
				}
				
				std::cout << "Discarding message payload.\n";
				
				err_code = discard(sock, msg_header.payload_size);
				
				if (err_code != SUCCESS)
				{
					std::cerr << "Couldn't discard message payload.\n";
					
					freeaddrinfo(res);
					closesocket(sock);
					WSACleanup();
					
					return 1;
				}
				
				std::cout << "Discarded message payload.\n";
			}
		}
		
		char hash[32] = {0};

		hash_block_header(block_candidate.block_header, reinterpret_cast<uint32_t *>(hash));
		
		for (const char &c : block_candidate.block_header.previous_block_header_hash)
		{
			printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
		}

		std::cout << "Trying nonce: " << block_candidate.block_header.nonce << '\n';

		if (memcmp(hash, target, 32) < 0)
		{
			compact_size_uint inv_count = 1;
			inventory_type inv_type = inventory_type::MSG_BLOCK;
			
			std::vector<char> payload_buffer(1 + 4 + 32);
			const std::span payload_span(payload_buffer);
			std::spanstream payload_stream(payload_span, std::ios::binary | std::ios::in | std::ios::out);
			
			payload_stream
			.write(reinterpret_cast<const char *>(*inv_count), inv_count.size())
			.write(reinterpret_cast<const char *>(&inv_type), 4)
			.write(reinterpret_cast<const char *>(&hash), 32);
			
			constexpr char *command_name = "inv\0\0\0\0\0\0\0\0\0";
			
			const uint32_t payload_size = payload_stream.span().size();
			
			char checksum[32];
			
			sha256(reinterpret_cast<const byte *>(payload_stream.span().data()), payload_stream.span().size() * 8, reinterpret_cast<uint32_t *>(checksum));
			sha256(reinterpret_cast<const byte *>(checksum), 256, reinterpret_cast<uint32_t *>(checksum));
			
			std::vector<char> header_buffer(MSG_HEADER_SIZE);
			const std::span header_span(header_buffer);
			std::spanstream header_stream(header_span, std::ios::binary | std::ios::in | std::ios::out);
			
			header_stream
			.write(reinterpret_cast<const char *>(start_string), 4)
			.write(reinterpret_cast<const char *>(command_name), 12)
			.write(reinterpret_cast<const char *>(&payload_size), 4)
			.write(reinterpret_cast<const char *>(&checksum), 4);
			
			std::cout << "Message header:\n";
			
			for (const char &c : header_stream.span())
			{
				printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
			}
			
			std::cout << "\nMessage payload:\n";
			
			for (const char &c : payload_stream.span())
			{
				printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
			}
			
			std::cout << '\n';
			
			int bytes_count = send(sock, header_stream.span().data(), static_cast<int>(header_stream.span().size()), 0);
			
			if (bytes_count == SOCKET_ERROR)
			{
				std::cerr << "Couldn't send message header: " << WSAGetLastError() << '\n';
				
				freeaddrinfo(res);
				closesocket(sock);
				WSACleanup();
				
				return 1;
			}
			
			std::cout << "Sent message header.\n";
			
			bytes_count = send(sock, payload_stream.span().data(), static_cast<int>(payload_stream.span().size()), 0);
			
			if (bytes_count == SOCKET_ERROR)
			{
				std::cerr << "Couldn't send message payload: " << WSAGetLastError() << '\n';
				
				freeaddrinfo(res);
				closesocket(sock);
				WSACleanup();
				
				return 1;
			}
			
			std::cout << "Sent message payload.\n";
			
			std::cout << "Mined btc.";
		}

		block_candidate.block_header.nonce++;
	}

	freeaddrinfo(res);
	closesocket(sock);
	WSACleanup();

	return 0;
}

int recv_all(const SOCKET &sock, char* buffer, int length, int flags)
{
	int bytes_total = 0;

	while (bytes_total < length)
	{
		const int bytes_count = recv(sock, buffer + bytes_total, length - bytes_total, flags);

		if (bytes_count == SOCKET_ERROR)
		{
			std::cerr << "Couldn't receive all data: " << WSAGetLastError() << '\n';

			return SOCKET_ERROR;
		}
		else if (bytes_count == 0)
		{
			return 0;
		}

		bytes_total += bytes_count;
	}

	return bytes_total;
}

int discard(const SOCKET &sock, uint64_t length, int flags)
{
	int bytes_count;
	char tmp[4096];
	
	for (; length > 0; length -= bytes_count)
	{
		bytes_count = recv_all(sock, tmp, static_cast<int>(std::min(length, 4096ULL)), flags);
	
		if (bytes_count == SOCKET_ERROR)
		{
			std::cerr << "Couldn't receive: " << WSAGetLastError() << '\n';
	
			return 1;
		}
		
		std::cout << "Received " << bytes_count << " bytes.\n";
	}
	
	return SUCCESS;
}

// TODO: handle 0 bytes_count aka. disconnected
int recv_message_block(const SOCKET &sock, message_block &msg_block)
{
	int bytes_count = recv_all(sock, reinterpret_cast<char*>(&msg_block.block_header.version), 4, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive version: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	std::cout << "Received version:\n";

	for (uint8_t i = 0; i < 4; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t*>(&msg_block.block_header.version) + i) & 0xFF);
	}

	std::cout << '\n';
	
	bytes_count = recv_all(sock, reinterpret_cast<char*>(&msg_block.block_header.previous_block_header_hash), 32, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive previous block header hash: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	std::cout << "Received previous block header hash:\n";

	for (uint8_t i = 0; i < 32; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t*>(&msg_block.block_header.previous_block_header_hash) + i) & 0xFF);
	}

	std::cout << '\n';
	
	bytes_count = recv_all(sock, reinterpret_cast<char*>(&msg_block.block_header.merkle_root_hash), 32, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive merkle root hash: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	std::cout << "Received merkle root hash:\n";

	for (uint8_t i = 0; i < 32; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t*>(&msg_block.block_header.merkle_root_hash) + i) & 0xFF);
	}

	std::cout << '\n';
	
	bytes_count = recv_all(sock, reinterpret_cast<char*>(&msg_block.block_header.time), 4, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive time: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	std::cout << "Received time:\n";

	for (uint8_t i = 0; i < 4; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t*>(&msg_block.block_header.time) + i) & 0xFF);
	}

	std::cout << '\n';
	
	bytes_count = recv_all(sock, reinterpret_cast<char*>(&msg_block.block_header.nbits), 4, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive nbits: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	std::cout << "Received nbits:\n";

	for (uint8_t i = 0; i < 4; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t*>(&msg_block.block_header.nbits) + i) & 0xFF);
	}

	std::cout << '\n';
	
	bytes_count = recv_all(sock, reinterpret_cast<char*>(&msg_block.block_header.nonce), 4, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive nonce: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	std::cout << "Received nonce:\n";

	for (uint8_t i = 0; i < 4; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t*>(&msg_block.block_header.nonce) + i) & 0xFF);
	}

	std::cout << '\n';
	
	int err_code = recv_compact_size_uint(sock, msg_block.txn_count);
	
	if (err_code != SUCCESS)
	{
		std::cerr << "Couldn't receive transactions count.\n";
		
		return 1;
	}
	
	std::cout << "Received transactions count:\n";
	
	for (uint8_t i = 0; i < msg_block.txn_count.size(); i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t *>(*msg_block.txn_count) + i) & 0xFF);
	}
	
	std::cout << '\n';
	
	msg_block.txns = new raw_transaction[msg_block.txn_count.data()];
	
	for (uint64_t i = 0; i < msg_block.txn_count.data(); i++)
	{
		err_code = recv_raw_transaction(sock, msg_block.txns[i]);
	
		if (err_code != SUCCESS)
		{
			std::cerr << "Couldn't receive transaction.\n";
			
			return 1;
		}
		
		std::cout << "Received transaction.\n";
	}
	
	return SUCCESS;
}

int recv_message_header(const SOCKET &sock, message_header &msg_header)
{
	int bytes_count = recv_all(sock, msg_header.start_string, 4, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive start string: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	std::cout << "Received start string:\n";

	for (const char &c : msg_header.start_string)
	{
		printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
	}

	std::cout << '\n';

	bytes_count = recv_all(sock, msg_header.command_name, 12, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive command name: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	std::cout << "Received command name:\n";

	for (const char &c : msg_header.command_name)
	{
		printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
	}

	std::cout << '\n';

	bytes_count = recv_all(sock, reinterpret_cast<char*>(&msg_header.payload_size), 4, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive payload size: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	std::cout << "Received payload size:\n";

	for (uint8_t i = 0; i < 4; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t*>(&msg_header.payload_size) + i) & 0xFF);
	}

	std::cout << '\n';

	bytes_count = recv_all(sock, msg_header.checksum, 4, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive checksum: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	std::cout << "Received checksum:\n";

	for (const char &c : msg_header.checksum)
	{
		printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
	}

	std::cout << '\n';

	return SUCCESS;
}

int recv_message_inv(const SOCKET &sock, message_inv &msg_inv)
{
	int err_code = recv_compact_size_uint(sock, msg_inv.count);
	
	if (err_code != SUCCESS)
	{
		std::cerr << "Couldn't receive count.\n";
		
		return 1;
	}
	
	std::cout << "Received count:\n";
	
	for (uint8_t i = 0; i < msg_inv.count.size(); i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t *>(*msg_inv.count) + i) & 0xFF);
	}
	
	std::cout << '\n';
	
	msg_inv.inventory = new inventory[msg_inv.count.data()];
	
	std::cout << '\n';
	
	for (uint64_t i = 0; i < msg_inv.count.data(); i++)
	{
		int bytes_count = recv_all(sock, reinterpret_cast<char*>(&(msg_inv.inventory[i].type)), 4, 0);

		if (bytes_count == SOCKET_ERROR)
		{
			std::cerr << "Couldn't receive inventory type: " << WSAGetLastError() << '\n';
	
			return 1;
		}
	
		std::cout << "Received " << bytes_count << " bytes.\n";
	
		std::cout << "Received inventory type:\n";
	
		for (uint8_t j = 0; j < 4; j++)
		{
			printf("%02x ", *(reinterpret_cast<uint8_t*>(&(msg_inv.inventory[i].type)) + j) & 0xFF);
		}
	
		std::cout << '\n';
		
		bytes_count = recv_all(sock, reinterpret_cast<char*>(&(msg_inv.inventory[i].hash)), 32, 0);

		if (bytes_count == SOCKET_ERROR)
		{
			std::cerr << "Couldn't receive inventory hash: " << WSAGetLastError() << '\n';
	
			return 1;
		}
	
		std::cout << "Received " << bytes_count << " bytes.\n";
	
		std::cout << "Received inventory hash:\n";
	
		for (uint8_t j = 0; j < 32; j++)
		{
			printf("%02x ", *(reinterpret_cast<uint8_t*>(&(msg_inv.inventory[i].hash)) + j) & 0xFF);
		}
	
		std::cout << '\n';
	}
	
	return SUCCESS;
}

int recv_message_ping(const SOCKET &sock, message_ping &msg_ping)
{
	int bytes_count = recv_all(sock, reinterpret_cast<char *>(&msg_ping.nonce), 8, 0);
	
	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive nonce: " << WSAGetLastError() << '\n';
		
		return 1;
	}
	
	std::cout << "Received " << bytes_count << " bytes.\n";
	
	std::cout << "Received nonce:\n";
	
	for (uint8_t i = 0; i < 8; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t *>(&msg_ping.nonce) + i) & 0xFF);
	}
	
	std::cout << '\n';
	
	return SUCCESS;
}

int recv_message_version(const SOCKET &sock, const message_header &msg_header, message_version &msg_version)
{
	int bytes_count = recv_all(sock, reinterpret_cast<char *>(&msg_version.version), 4, 0);
	
	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive version: " << WSAGetLastError() << '\n';
		
		return 1;
	}
	
	std::cout << "Received " << bytes_count << " bytes.\n";
	
	std::cout << "Received version:\n";
	
	for (uint8_t i = 0; i < 4; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t *>(&msg_version.version) + i) & 0xFF);
	}
	
	std::cout << '\n';
	
	bytes_count = recv_all(sock, reinterpret_cast<char *>(&msg_version.services), 8, 0);
	
	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive services: " << WSAGetLastError() << '\n';
		
		return 1;
	}
	
	std::cout << "Received " << bytes_count << " bytes.\n";
	
	std::cout << "Received services:\n";
	
	for (uint8_t i = 0; i < 8; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t *>(&msg_version.services) + i) & 0xFF);
	}
	
	std::cout << '\n';
	
	bytes_count = recv_all(sock, reinterpret_cast<char *>(&msg_version.timestamp), 8, 0);
	
	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive timestamp: " << WSAGetLastError() << '\n';
		
		return 1;
	}
	
	std::cout << "Received " << bytes_count << " bytes.\n";
	
	std::cout << "Received timestamp:\n";
	
	for (uint8_t i = 0; i < 8; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t *>(&msg_version.timestamp) + i) & 0xFF);
	}
	
	std::cout << '\n';
	
	bytes_count = recv_all(sock, reinterpret_cast<char *>(&msg_version.addr_recv_services), 8, 0);
	
	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive receiver services: " << WSAGetLastError() << '\n';
		
		return 1;
	}
	
	std::cout << "Received " << bytes_count << " bytes.\n";
	
	std::cout << "Received receiver services:\n";
	
	for (uint8_t i = 0; i < 8; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t *>(&msg_version.addr_recv_services) + i) & 0xFF);
	}
	
	std::cout << '\n';
	
	bytes_count = recv_all(sock, msg_version.addr_recv_ip, 16, 0);
	
	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive receiver ip: " << WSAGetLastError() << '\n';
		
		return 1;
	}
	
	std::cout << "Received " << bytes_count << " bytes.\n";
	
	std::cout << "Received receiver ip:\n";
	
	for (const char &c : msg_version.addr_recv_ip)
	{
		printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
	}
	
	std::cout << '\n';
	
	bytes_count = recv_all(sock, reinterpret_cast<char *>(&msg_version.addr_recv_port), 2, 0);
	
	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive receiver port: " << WSAGetLastError() << '\n';
		
		return 1;
	}
	
	std::cout << "Received " << bytes_count << " bytes.\n";
	
	std::cout << "Received receiver port:\n";
	
	for (uint8_t i = 0; i < 2; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t *>(&msg_version.addr_recv_port) + i) & 0xFF);
	}
	
	std::cout << '\n';
	
	bytes_count = recv_all(sock, reinterpret_cast<char *>(&msg_version.addr_trans_services), 8, 0);
	
	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive transmitter services: " << WSAGetLastError() << '\n';
		
		return 1;
	}
	
	std::cout << "Received " << bytes_count << " bytes.\n";
	
	std::cout << "Received transmitter services:\n";
	
	for (uint8_t i = 0; i < 8; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t *>(&msg_version.addr_trans_services) + i) & 0xFF);
	}
	
	std::cout << '\n';
	
	bytes_count = recv_all(sock, msg_version.addr_trans_ip, 16, 0);
	
	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive transmitter ip: " << WSAGetLastError() << '\n';
		
		return 1;
	}
	
	std::cout << "Received " << bytes_count << " bytes.\n";
	
	std::cout << "Received transmitter ip:\n";
	
	for (const char &c : msg_version.addr_trans_ip)
	{
		printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
	}
	
	std::cout << '\n';
	
	bytes_count = recv_all(sock, reinterpret_cast<char *>(&msg_version.addr_trans_port), 2, 0);
	
	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive transmitter port: " << WSAGetLastError() << '\n';
		
		return 1;
	}
	
	std::cout << "Received " << bytes_count << " bytes.\n";
	
	std::cout << "Received transmitter port:\n";
	
	for (uint8_t i = 0; i < 2; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t *>(&msg_version.addr_trans_port) + i) & 0xFF);
	}
	
	std::cout << '\n';
	
	bytes_count = recv_all(sock, reinterpret_cast<char *>(&msg_version.nonce), 8, 0);
	
	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive nonce: " << WSAGetLastError() << '\n';
		
		return 1;
	}
	
	std::cout << "Received " << bytes_count << " bytes.\n";
	
	std::cout << "Received nonce:\n";
	
	for (uint8_t i = 0; i < 8; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t *>(&msg_version.nonce) + i) & 0xFF);
	}
	
	std::cout << '\n';
	
	int err_code = recv_compact_size_uint(sock, msg_version.user_agent_bytes);
	
	if (err_code != SUCCESS)
	{
		std::cerr << "Couldn't receive user agent bytes.\n";
		
		return 1;
	}
	
	std::cout << "Received user agent bytes:\n";
	
	for (uint8_t i = 0; i < msg_version.user_agent_bytes.size(); i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t *>(*msg_version.user_agent_bytes) + i) & 0xFF);
	}
	
	std::cout << '\n';
	
	msg_version.user_agent = new char[msg_version.user_agent_bytes.data()];

	bytes_count = recv_all(sock, msg_version.user_agent, (int32_t)msg_version.user_agent_bytes.data(), 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive user agent: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	std::cout << "Received user agent:\n";

	for (uint64_t i = 0; i < msg_version.user_agent_bytes.data(); i++)
	{
		printf("%02x ", static_cast<uint8_t>(msg_version.user_agent[i]) & 0xFF);
	}

	std::cout << '\n';

	bytes_count = recv_all(sock, reinterpret_cast<char*>(&msg_version.start_height), 4, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive start height: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	std::cout << "Received start height:\n";

	for (uint8_t i = 0; i < 4; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t*>(&msg_version.start_height) + i) & 0xFF);
	}

	std::cout << '\n';
	
	if (msg_header.payload_size - (MSG_VERSION_MIN_SIZE + msg_version.user_agent_bytes.size() - 1) > 0)
	{
		bytes_count = recv_all(sock, reinterpret_cast<char*>(&msg_version.relay), 1, 0);
	
		if (bytes_count == SOCKET_ERROR)
		{
			std::cerr << "Couldn't receive relay: " << WSAGetLastError() << '\n';
	
			return 1;
		}
	
		std::cout << "Received " << bytes_count << " bytes.\n";
	
		std::cout << "Received relay:\n";
	
		for (uint8_t i = 0; i < 1; i++)
		{
			printf("%02x ", *(reinterpret_cast<uint8_t*>(&msg_version.relay) + i) & 0xFF);
		}
	
		std::cout << '\n';
	}
	
	return SUCCESS;
}

int recv_compact_size_uint(const SOCKET &sock, compact_size_uint &cmpct)
{
	uint8_t size;

	uint64_t value;

	int bytes_count = recv_all(sock, reinterpret_cast<char*>(&value), 1, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive compact size uint: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	switch (*(reinterpret_cast<uint8_t*>(&value)))
	{
		case 0xFF:
			size = 8;
			break;
		case 0xFE:
			size = 4;
			break;
		case 0xFD:
			size = 2;
			break;
		default:
			size = 0;
	}

	if (size != 0)
	{
		bytes_count = recv_all(sock, reinterpret_cast<char*>(&value), size, 0);

		if (bytes_count == SOCKET_ERROR)
		{
			std::cerr << "Couldn't receive compact size uint: " << WSAGetLastError() << '\n';

			return 1;
		}

		std::cout << "Received " << bytes_count << " bytes.\n";
	}
	
	switch (size)
	{
		case 8:
			cmpct = compact_size_uint(value);
			break;
		case 4:
			cmpct = compact_size_uint(static_cast<uint64_t>(*(reinterpret_cast<uint32_t*>(&value))));
			break;
		case 2:
			cmpct = compact_size_uint(static_cast<uint64_t>(*(reinterpret_cast<uint16_t*>(&value))));
			break;
		case 0:
			cmpct = compact_size_uint(static_cast<uint64_t>(*(reinterpret_cast<uint8_t*>(&value))));
			break;
		default:
			assert(false);
	}

	std::cout << "Received compact size uint:\n";

	for (uint8_t i = 0; i < cmpct.size(); i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t *>(*cmpct) + i) & 0xFF);
	}

	std::cout << '\n';

	return SUCCESS;
}

int recv_raw_transaction(const SOCKET &sock, raw_transaction &transaction)
{
	int bytes_count = recv_all(sock, reinterpret_cast<char*>(&transaction.version), 4, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive transaction version: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	std::cout << "Received transaction version:\n";

	for (uint8_t i = 0; i < 4; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t*>(&transaction.version) + i) & 0xFF);
	}

	std::cout << '\n';
	
	int err_code = recv_compact_size_uint(sock, transaction.tx_in_count);
	
	if (err_code != SUCCESS)
	{
		std::cerr << "Couldn't receive transaction inputs count.\n";
		
		return 1;
	}
	
	std::cout << "Received transaction inputs count:\n";
	
	for (uint8_t i = 0; i < transaction.tx_in_count.size(); i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t *>(*transaction.tx_in_count) + i) & 0xFF);
	}
	
	std::cout << '\n';
	
	transaction.tx_in = new tx_in[transaction.tx_in_count.data()];
	
	for (uint64_t i = 0; i < transaction.tx_in_count.data(); i++)
	{
		err_code = recv_tx_in(sock, transaction.tx_in[i]);
	
		if (err_code != SUCCESS)
		{
			std::cerr << "Couldn't receive transaction input.\n";
			
			return 1;
		}
		
		std::cout << "Received transaction input.\n";
	}
	
	err_code = recv_compact_size_uint(sock, transaction.tx_out_count);
	
	if (err_code != SUCCESS)
	{
		std::cerr << "Couldn't receive transaction outputs count.\n";
		
		return 1;
	}
	
	std::cout << "Received transaction outputs count:\n";
	
	for (uint8_t i = 0; i < transaction.tx_out_count.size(); i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t *>(*transaction.tx_out_count) + i) & 0xFF);
	}
	
	std::cout << '\n';
	
	transaction.tx_out = new tx_out[transaction.tx_out_count.data()];
	
	for (uint64_t i = 0; i < transaction.tx_out_count.data(); i++)
	{
		err_code = recv_tx_out(sock, transaction.tx_out[i]);
	
		if (err_code != SUCCESS)
		{
			std::cerr << "Couldn't receive transaction output.\n";
			
			return 1;
		}
		
		std::cout << "Received transaction output.\n";
	}
	
	bytes_count = recv_all(sock, reinterpret_cast<char*>(&transaction.lock_time), 4, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive lock time: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	std::cout << "Received lock time:\n";

	for (uint8_t i = 0; i < 4; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t*>(&transaction.lock_time) + i) & 0xFF);
	}

	std::cout << '\n';
	
	return SUCCESS;
}

int recv_tx_in(const SOCKET &sock, tx_in &tx)
{
	int err_code = recv_outpoint(sock, tx.previous_output);
	
	if (err_code != SUCCESS)
	{
		std::cerr << "Couldn't receive previous output.\n";
		
		return 1;
	}
	
	std::cout << "Received previous output.\n";
	
	err_code = recv_compact_size_uint(sock, tx.script_bytes);
	
	if (err_code != SUCCESS)
	{
		std::cerr << "Couldn't receive script bytes.\n";
		
		return 1;
	}
	
	std::cout << "Received script bytes:\n";
	
	for (uint8_t i = 0; i < tx.script_bytes.size(); i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t *>(*tx.script_bytes) + i) & 0xFF);
	}
	
	std::cout << '\n';
	
	tx.signature_script = new char[tx.script_bytes.data()];
	
	int bytes_count = recv_all(sock, reinterpret_cast<char*>(tx.signature_script), static_cast<int>(tx.script_bytes.data()), 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive signature script: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	std::cout << "Received signature script:\n";

	for (uint64_t i = 0; i < tx.script_bytes.data(); i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t*>(&tx.signature_script) + i) & 0xFF);
	}

	std::cout << '\n';
	
	bytes_count = recv_all(sock, reinterpret_cast<char*>(&tx.sequence), 4, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive sequence: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	std::cout << "Received sequence:\n";

	for (uint8_t i = 0; i < 4; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t*>(&tx.sequence) + i) & 0xFF);
	}

	std::cout << '\n';
	
	return SUCCESS;
}

int recv_tx_out(const SOCKET &sock, tx_out &tx)
{
	int bytes_count = recv_all(sock, reinterpret_cast<char*>(&tx.value), 8, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive value: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	std::cout << "Received value:\n";

	for (uint8_t i = 0; i < 4; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t*>(&tx.value) + i) & 0xFF);
	}

	std::cout << '\n';
	
	int err_code = recv_compact_size_uint(sock, tx.pk_script_bytes);
	
	if (err_code != SUCCESS)
	{
		std::cerr << "Couldn't receive pubkey script bytes.\n";
		
		return 1;
	}
	
	std::cout << "Received pubkey script bytes:\n";
	
	for (uint8_t i = 0; i < tx.pk_script_bytes.size(); i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t *>(*tx.pk_script_bytes) + i) & 0xFF);
	}
	
	std::cout << '\n';
	
	tx.pk_script = new char[tx.pk_script_bytes.data()];
	
	bytes_count = recv_all(sock, reinterpret_cast<char*>(tx.pk_script), static_cast<int>(tx.pk_script_bytes.data()), 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive pubkey script: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	std::cout << "Received pubkey script:\n";

	for (uint64_t i = 0; i < tx.pk_script_bytes.data(); i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t*>(&tx.pk_script) + i) & 0xFF);
	}

	std::cout << '\n';
	
	return SUCCESS;
}

int recv_outpoint(const SOCKET &sock, outpoint &outpoint)
{
	int bytes_count = recv_all(sock, reinterpret_cast<char*>(outpoint.hash), 32, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive outpoint hash: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	std::cout << "Received outpoint hash:\n";

	for (uint8_t i = 0; i < 32; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t*>(&outpoint.hash) + i) & 0xFF);
	}

	std::cout << '\n';
	
	bytes_count = recv_all(sock, reinterpret_cast<char*>(&outpoint.index), 4, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive outpoint index: " << WSAGetLastError() << '\n';

		return 1;
	}

	std::cout << "Received " << bytes_count << " bytes.\n";

	std::cout << "Received outpoint index:\n";

	for (uint8_t i = 0; i < 4; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t*>(&outpoint.index) + i) & 0xFF);
	}

	std::cout << '\n';
	
	return SUCCESS;
}

void hash_block_header(const block_header &blk_header, uint32_t *hash)
{
	std::vector<char> block_header_buffer(BLOCK_SIZE);
	const std::span block_header_span(block_header_buffer);
	std::spanstream block_header_stream(block_header_span, std::ios::binary | std::ios::in | std::ios::out);
	
	block_header_stream
	.write(reinterpret_cast<const char *>(&blk_header.version), 4)
	.write(reinterpret_cast<const char *>(&blk_header.previous_block_header_hash), 32)
	.write(reinterpret_cast<const char *>(&blk_header.merkle_root_hash), 32)
	.write(reinterpret_cast<const char *>(&blk_header.time), 4)
	.write(reinterpret_cast<const char *>(&blk_header.nbits), 4)
	.write(reinterpret_cast<const char *>(&blk_header.nonce), 4);
	
	sha256(reinterpret_cast<const uint8_t *>(block_header_stream.span().data()), block_header_stream.span().size() * 8, reinterpret_cast<uint32_t *>(hash));
	sha256(reinterpret_cast<const uint8_t *>(hash), 256, reinterpret_cast<uint32_t *>(hash));
}

void hash_raw_transaction(const raw_transaction &transaction, uint32_t *hash)
{
	std::vector<char> raw_transaction_buffer(BLOCK_SIZE);
	const std::span raw_transaction_span(raw_transaction_buffer);
	std::spanstream raw_transaction_stream(raw_transaction_span, std::ios::binary | std::ios::in | std::ios::out);

	raw_transaction_stream
	.write(reinterpret_cast<const char *>(&transaction.version), 4)
	.write(reinterpret_cast<const char *>(*transaction.tx_in_count), transaction.tx_in_count.size());
	
	for (uint64_t i = 0; i < transaction.tx_in_count.data(); i++)
	{
		raw_transaction_stream
		.write(reinterpret_cast<const char *>(&transaction.tx_in[i].previous_output.hash), 32)
		.write(reinterpret_cast<const char *>(&transaction.tx_in[i].previous_output.index), 4)
		.write(reinterpret_cast<const char *>(*transaction.tx_in[i].script_bytes), transaction.tx_in[i].script_bytes.size())
		.write(reinterpret_cast<const char *>(&transaction.tx_in[i].signature_script), static_cast<std::streamsize>(transaction.tx_in[i].script_bytes.data()))
		.write(reinterpret_cast<const char *>(&transaction.tx_in[i].sequence), 4);
	}
	
	raw_transaction_stream
	.write(reinterpret_cast<const char *>(*transaction.tx_out_count), transaction.tx_out_count.size());
	
	for (uint64_t i = 0; i < transaction.tx_out_count.data(); i++)
	{
		raw_transaction_stream
		.write(reinterpret_cast<const char *>(&transaction.tx_out[i].value), 8)
		.write(reinterpret_cast<const char *>(*transaction.tx_out[i].pk_script_bytes), transaction.tx_out[i].pk_script_bytes.size())
		.write(reinterpret_cast<const char *>(&transaction.tx_out[i].pk_script), static_cast<std::streamsize>(transaction.tx_out[i].pk_script_bytes.data()));
	}
	
	raw_transaction_stream
	.write(reinterpret_cast<const char *>(&transaction.lock_time), 4);
	
	sha256(reinterpret_cast<const uint8_t *>(raw_transaction_stream.span().data()), raw_transaction_stream.span().size() * 8, reinterpret_cast<uint32_t *>(hash));
	sha256(reinterpret_cast<const uint8_t *>(hash), 256, reinterpret_cast<uint32_t *>(hash));
}

uint8_t get_height_size(const uint32_t height)
{
	if (height > 0xFFFFFF)
	{
		return 4;
	}
	else if (height > 0xFFFF)
	{
		return 3;
	}
	else if (height > 0xFF)
	{
		return 2;
	}
	else
	{
		return 1;
	}
}

void nbits_to_target(const uint32_t nbits, uint8_t *target)
{
	memset(target, 0, 32);
	
	uint8_t n = nbits >> 3;
	
	target[31 - n + 0] = (nbits & 0xFF0000);
	target[31 - n + 1] = (nbits & 0x00FF00);
	target[31 - n + 2] = (nbits & 0x0000FF);
}

bool is_data_available(const SOCKET &sock)
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    timeval timeout{
		.tv_sec = 0,
		.tv_usec = 0
	};

    int result = select(0, &readfds, nullptr, nullptr, &timeout);
    return result > 0 && FD_ISSET(sock, &readfds);
}

int recv_all(SOCKET &sock, char* buffer, int length, int flags)
{
	int bytes_total = 0;

	while (bytes_total < length)
	{
		int bytes_count = recv(sock, buffer + bytes_total, length - bytes_total, flags);

		if (bytes_count == SOCKET_ERROR)
		{
			std::cerr << "Couldn't receive all data: " << WSAGetLastError() << '\n';

			return SOCKET_ERROR;
		}
		else if (bytes_count == 0)
		{
			return 0;
		}

		bytes_total += bytes_count;
	}

	return bytes_total;
}