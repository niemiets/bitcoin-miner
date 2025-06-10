#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iostream>
#include <memory>
#include <random>
#include <span>
#include <spanstream>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "algorithms/crypto.h"
#include "bitcoin/message_header.h"
#include "bitcoin/network_type.h"
#include "bitcoin/messages/message_inv.h"
#include "bitcoin/messages/message_version.h"
#include "datatypes/compact_size_uint.h"

typedef uint8_t byte;

constexpr uint64_t MAX_UINT64 = 0xFFFFFFFFFFFFFFFF;

constexpr uint32_t MAX_SIZE = 0x02000000;
constexpr uint8_t  MSG_HEADER_SIZE = 4 + 12 + 4 + 4;
constexpr uint32_t MSG_VERSION_MIN_SIZE = 4 + 8 + 8 + 8 + 16 + 2 + 8 + 16 + 2 + 8 + 1 + 4;
constexpr uint32_t MSG_GETHEADERS_MIN_SIZE = 4 + 1 + 32;
constexpr uint32_t MSG_INV_MIN_SIZE = 1;

constexpr uint16_t NETWORK_BUFFER_SIZE = 1024;

constexpr uint32_t MSG_VERACK_CHECKSUM = std::byteswap(0x5df6e0e2);

constexpr int SUCCESS = 0;

int discard(const SOCKET &sock, uint64_t length, int flags = 0);
int recv_message_header(const SOCKET &sock, message_header &msg_header);
int recv_message_inv(const SOCKET &sock, message_inv &msg_inv);
int recv_message_version(const SOCKET &sock, const message_header &msg_header, message_version &msg_version);
int recv_compact_size_uint(const SOCKET &sock, compact_size_uint &cmpct);

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
		
		constexpr bool relay = true;
		
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
		.write(reinterpret_cast<const char *>(user_agent), (long long)user_agent_bytes.data())
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
	
	while (connected)
	{
		message_header msg_header{};
		
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
			message_version msg_version;

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
				
				int
				bytes_count = send(sock, header_stream.span().data(), static_cast<int>(header_stream.span().size()), 0);
				
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
				compact_size_uint hash_count(0);
				char *stop_hash = (char *)calloc(32, sizeof(char));
				
				std::vector<char> payload_buffer(MSG_GETHEADERS_MIN_SIZE - 1 + hash_count.size() + (hash_count.data() * sizeof(inventory)));
				const std::span payload_span(payload_buffer);
				std::spanstream payload_stream(payload_span, std::ios::binary | std::ios::in | std::ios::out);
				
				payload_stream
				.write(reinterpret_cast<const char *>(&version), 4)
				.write(reinterpret_cast<const char *>(*hash_count), hash_count.size())
				.write(reinterpret_cast<const char *>(stop_hash), 32);
				
				constexpr char *command_name = "getheaders\0\0";
				
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
				
				bytes_count =
				send(sock, payload_stream.span().data(), static_cast<int>(payload_stream.span().size()), 0);
				
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
			
			std::vector<char> payload_buffer(MSG_INV_MIN_SIZE - 1 + msg_inv.count.size() + (msg_inv.count.data() * sizeof(inventory)));
			const std::span payload_span(payload_buffer);
			std::spanstream payload_stream(payload_span, std::ios::binary | std::ios::in | std::ios::out);
			
			payload_stream
			.write(reinterpret_cast<const char*>(*(msg_inv.count)), msg_inv.count.size());
			
			std::cout << msg_inv.count.data() << '\n';
			
			for (uint64_t i = 0; i < msg_inv.count.data(); i++)
			{
				payload_stream
				.write(reinterpret_cast<const char*>(&(msg_inv.inventory[i].type)), 4)
				.write(reinterpret_cast<const char*>(msg_inv.inventory[i].hash), 32);
			}
			
			// TODO: rest
			
			constexpr char *command_name = "getdata\0\0\0\0\0";
		
			const uint32_t payload_size = payload_stream.span().size();
			
			char checksum[32];
		
			sha256(reinterpret_cast<const byte*>(payload_stream.span().data()), payload_stream.span().size() * 8, reinterpret_cast<uint32_t*>(checksum));
			sha256(reinterpret_cast<const byte*>(checksum), 256, reinterpret_cast<uint32_t*>(checksum));
		
			std::vector<char> header_buffer(MSG_HEADER_SIZE);
			const std::span header_span(header_buffer);
			std::spanstream header_stream(header_span, std::ios::binary | std::ios::in | std::ios::out);
			
			header_stream
			.write(reinterpret_cast<const char*>(start_string), 4)
			.write(reinterpret_cast<const char*>(command_name), 12)
			.write(reinterpret_cast<const char*>(&payload_size), 4)
			.write(reinterpret_cast<const char*>(&checksum), 4);
			
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

	freeaddrinfo(res);
	closesocket(sock);
	WSACleanup();

	return 0;
}

int discard(const SOCKET &sock, uint64_t length, int flags)
{
	int bytes_count;
	char tmp[4096];
	
	for (; length > 0; length -= bytes_count)
	{
		bytes_count = recv(sock, tmp, static_cast<int>(std::min(length, 4096ULL)), flags);
	
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
int recv_message_header(const SOCKET &sock, message_header &msg_header)
{
	int bytes_count = recv(sock, msg_header.start_string, 4, 0);

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

	bytes_count = recv(sock, msg_header.command_name, 12, 0);

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

	bytes_count = recv(sock, reinterpret_cast<char*>(&msg_header.payload_size), 4, 0);

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

	bytes_count = recv(sock, msg_header.checksum, 4, 0);

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
	
	for (uint8_t i = 0; i <msg_inv.count.size(); i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t *>(*msg_inv.count) + i) & 0xFF);
	}
	
	std::cout << '\n';
	
	msg_inv.inventory = new inventory[msg_inv.count.data()];
	
	std::cout << '\n';
	
	for (uint64_t i = 0; i < msg_inv.count.data(); i++)
	{
		int bytes_count = recv(sock, reinterpret_cast<char*>(&(msg_inv.inventory[i].type)), 4, 0);

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
		
		bytes_count = recv(sock, reinterpret_cast<char*>(&(msg_inv.inventory[i].hash)), 32, 0);

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

int recv_message_version(const SOCKET &sock, const message_header &msg_header, message_version &msg_version)
{
	int bytes_count = recv(sock, reinterpret_cast<char *>(&msg_version.version), 4, 0);
	
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
	
	bytes_count = recv(sock, reinterpret_cast<char *>(&msg_version.services), 8, 0);
	
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
	
	bytes_count = recv(sock, reinterpret_cast<char *>(&msg_version.timestamp), 8, 0);
	
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
	
	bytes_count = recv(sock, reinterpret_cast<char *>(&msg_version.addr_recv_services), 8, 0);
	
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
	
	bytes_count = recv(sock, msg_version.addr_recv_ip, 16, 0);
	
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
	
	bytes_count = recv(sock, reinterpret_cast<char *>(&msg_version.addr_recv_port), 2, 0);
	
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
	
	bytes_count = recv(sock, reinterpret_cast<char *>(&msg_version.addr_trans_services), 8, 0);
	
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
	
	bytes_count = recv(sock, msg_version.addr_trans_ip, 16, 0);
	
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
	
	bytes_count = recv(sock, reinterpret_cast<char *>(&msg_version.addr_trans_port), 2, 0);
	
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
	
	bytes_count = recv(sock, reinterpret_cast<char *>(&msg_version.nonce), 8, 0);
	
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

	bytes_count = recv(sock, msg_version.user_agent, (int32_t)msg_version.user_agent_bytes.data(), 0);

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

	bytes_count = recv(sock, reinterpret_cast<char*>(&msg_version.start_height), 4, 0);

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
		bytes_count = recv(sock, reinterpret_cast<char*>(&msg_version.relay), 1, 0);
	
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

// TODO: replace with this func
int recv_compact_size_uint(const SOCKET &sock, compact_size_uint &cmpct)
{
	uint8_t size;

	uint64_t value;

	int bytes_count = recv(sock, reinterpret_cast<char*>(&value), 1, 0);

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
		bytes_count = recv(sock, reinterpret_cast<char*>(&value), size, 0);

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