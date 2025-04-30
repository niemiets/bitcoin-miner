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

typedef uint8_t byte;

constexpr uint64_t MAX_UINT64 = 0xFFFFFFFFFFFFFFFF;

constexpr uint32_t MAX_SIZE = 0x02000000;
constexpr uint8_t  MSG_HEADER_SIZE = 4 + 12 + 4 + 4;
constexpr uint32_t MSG_VERSION_MIN_SIZE = 4 + 8 + 8 + 8 + 16 + 2 + 8 + 16 + 2 + 8 + 1 + 4 + 1;
constexpr uint32_t NET_MAINNET = std::byteswap(0xf9beb4d9);

constexpr uint16_t NETWORK_BUFFER_SIZE = 1024;

constexpr int SUCCESS = 0;

// template <uint64_t min_size = 0, uint64_t max_size = 0>
// class network_stream
// {
// 	public:
// 		network_stream();
// 		~network_stream() = default;
//
// 		size_t size();
// 		byte* data();
//
// 		template <typename T>
// 		bool write(const T *data, size_t size);
//
// 	private:
// 		std::vector<byte> _buf;
// };
//
// template <uint64_t min_size, uint64_t max_size>
// network_stream<min_size, max_size>::network_stream()
// {
// 	_buf.reserve(min_size);
// }
//
// template <uint64_t min_size, uint64_t max_size>
// size_t network_stream<min_size, max_size>::size()
// {
// 	return _buf.size();
// }
//
// template <uint64_t min_size, uint64_t max_size>
// byte* network_stream<min_size, max_size>::data()
// {
// 	return reinterpret_cast<byte*>(_buf.data());
// }
//
// template <uint64_t min_size, uint64_t max_size>
// template <typename T>
// bool network_stream<min_size, max_size>::write(const T *data, const size_t size)
// {
// 	if (max_size > 0 && this->size() + size > max_size)
// 		return false;
//
// 	_buf.insert(_buf.end(), data, data + size);
//
// 	return true;
// }

int main()
{
	// network_stream<0, MAX_SIZE> ns;
	//
	// const char *e = "test";
	//
	// std::cout << ns.write(reinterpret_cast<const byte *>(e), 4) << '\n';
	//
	// std::cout.write(reinterpret_cast<char*>(ns.data()), static_cast<std::streamsize>(ns.size()));

	const char *HOSTNAME = "16.16.139.80";
	const char *PORT	 = "8333";

	std::random_device rand_dev;
	std::mt19937_64 rng(rand_dev());
	std::uniform_int_distribution<std::mt19937_64::result_type> nonce_distribution(1, MAX_UINT64);

	int err_code = SUCCESS;

	WSADATA init_data;

	err_code = WSAStartup(MAKEWORD(2, 2), &init_data);

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
	
	std::cout << "Connected to socket." << '\n';

	constexpr int32_t  version = 70016;
	constexpr uint64_t services = 0x00;
	const     int64_t  timestamp = std::time(nullptr);

	constexpr uint64_t addr_recv_services = 0x00;
	constexpr char    *addr_recv_ip = "";
	constexpr uint16_t addr_recv_port = 8333;

	constexpr uint64_t addr_trans_services = 0x00;
	constexpr char    *addr_trans_ip = "";
	constexpr uint16_t addr_trans_port = 8333;

	const     uint64_t nonce = nonce_distribution(rng);

	constexpr char   *user_agent = "";
	const     uint8_t user_agent_bytes = strnlen_s(user_agent, MAX_SIZE);

	constexpr int32_t start_height = 0;

	constexpr bool relay = false;

	std::vector<char>     payload_buffer(MSG_VERSION_MIN_SIZE + user_agent_bytes);
	const std::span<char> payload_span(payload_buffer);
	std::spanstream       payload_stream(payload_span, std::ios::binary | std::ios::in | std::ios::out);

	payload_stream
	.write(reinterpret_cast<const char*>(&version), 4)
	.write(reinterpret_cast<const char*>(&services), 8)
	.write(reinterpret_cast<const char*>(&timestamp), 8)
	.write(reinterpret_cast<const char*>(&addr_recv_services), 8)
	.write(reinterpret_cast<const char*>(&addr_recv_ip), 16)
	.write(reinterpret_cast<const char*>(&addr_recv_port), 2)
	.write(reinterpret_cast<const char*>(&addr_trans_services), 8)
	.write(reinterpret_cast<const char*>(&addr_trans_ip), 16)
	.write(reinterpret_cast<const char*>(&addr_trans_port), 2)
	.write(reinterpret_cast<const char*>(&nonce), 8)
	.write(reinterpret_cast<const char*>(&user_agent_bytes), sizeof(user_agent_bytes))
	.write(reinterpret_cast<const char*>(&user_agent), user_agent_bytes)
	.write(reinterpret_cast<const char*>(&start_height), 4)
	.write(reinterpret_cast<const char*>(&relay), 1);
	
	const     char *start_string = reinterpret_cast<const char*>(&NET_MAINNET);
	constexpr char *command_name = "version\0\0\0\0\0";
	
	const     uint32_t payload_size = payload_stream.span().size();
	
	char *checksum = reinterpret_cast<char*>(sha256(reinterpret_cast<const byte*>(payload_stream.span().data()), payload_stream.span().size() * 8));
	checksum = reinterpret_cast<char*>(sha256(reinterpret_cast<const byte*>(checksum), 256));
	
	std::vector<char>     header_buffer(MSG_HEADER_SIZE);
	const std::span<char> header_span(header_buffer);
	std::spanstream       header_stream(header_span, std::ios::binary | std::ios::in | std::ios::out);
	
	header_stream
	.write(reinterpret_cast<const char*>(start_string), 4)
	.write(reinterpret_cast<const char*>(command_name), 12)
	.write(reinterpret_cast<const char*>(&payload_size), 4)
	.write(reinterpret_cast<const char*>(checksum), 4);

	std::cout << "Message header:\n";
	
	for (const char &c : header_stream.span())
	{
		printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
	}
	
	std::cout << "\nMessage payload:\n";;

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
	
	std::cout << "Sent message header." << '\n';
	
	bytes_count = send(sock, payload_stream.span().data(), static_cast<int>(payload_stream.span().size()), 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't send message payload: " << WSAGetLastError() << '\n';
		freeaddrinfo(res);
		closesocket(sock);
		WSACleanup();
		return 1;
	}
	
	std::cout << "Sent message payload." << '\n';
	
	char recv_start_string[4];
	
	bytes_count = recv(sock, recv_start_string, 4, 0);
	
	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive start string: " << WSAGetLastError() << '\n';
		freeaddrinfo(res);
		closesocket(sock);
		WSACleanup();
		return 1;
	}
	
	std::cout << "Recieved " << bytes_count << " bytes.\n";

	std::cout << "Recieved start string:\n";

	for (const char &c : recv_start_string)
	{
		printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
	}

	std::cout << '\n';

	char recv_command_name[12];

	bytes_count = recv(sock, recv_command_name, 12, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive command name: " << WSAGetLastError() << '\n';
		freeaddrinfo(res);
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	std::cout << "Recieved " << bytes_count << " bytes.\n";

	std::cout << "Recieved command name:\n";

	for (const char &c : recv_command_name)
	{
		printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
	}

	std::cout << '\n';

	uint32_t recv_payload_size;

	bytes_count = recv(sock, reinterpret_cast<char*>(&recv_payload_size), 4, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive payload size: " << WSAGetLastError() << '\n';
		freeaddrinfo(res);
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	std::cout << "Recieved " << bytes_count << " bytes.\n";

	std::cout << "Recieved payload size:\n";

	for (uint8_t i = 0; i < 4; i++)
	{
		printf("%02x ", *(reinterpret_cast<uint8_t*>(&recv_payload_size) + i) & 0xFF);
	}

	std::cout << '\n';

	char recv_checksum[4];

	bytes_count = recv(sock, recv_checksum, 4, 0);

	if (bytes_count == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive checksum: " << WSAGetLastError() << '\n';
		freeaddrinfo(res);
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	std::cout << "Recieved " << bytes_count << " bytes.\n";

	std::cout << "Recieved checksum:\n";

	for (const char &c : recv_checksum)
	{
		printf("%02x ", static_cast<uint8_t>(c) & 0xFF);
	}

	std::cout << '\n';

	std::cout << "Recieved message header.\n";

	if (memcmp(recv_command_name, "version\0\0\0\0\0", 12) != 0)
	{
		std::cerr << "Didn't receive correct command name (version): " << WSAGetLastError() << '\n';
		freeaddrinfo(res);
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(res);
	closesocket(sock);
	WSACleanup();
}