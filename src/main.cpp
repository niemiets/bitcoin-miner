#include <cstdint>
#include <ctime>
#include <memory>
#include <iostream>
#include <span>
#include <spanstream>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>

typedef uint8_t byte;

constexpr uint32_t MAX_SIZE = 0x02000000;
constexpr uint32_t VERSION_MSG_MIN_SIZE = 4 + 8 + 8 + 8 + 16 + 2 + 8 + 16 + 2 + 8 + 1 + 4 + 1;

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

	const SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

	if (sock == INVALID_SOCKET)
	{
		std::cerr << "Couldn't create socket: " << WSAGetLastError() << '\n';
		freeaddrinfo(res);
		WSACleanup();
		return 1;
	}

	err_code = connect(sock, res->ai_addr, static_cast<int>(res->ai_addrlen));

	if (err_code == SOCKET_ERROR)
	{
		std::cerr << "Couldn't connect to socket: " << WSAGetLastError() << '\n';
		freeaddrinfo(res);
		closesocket(sock);
		WSACleanup();
		return 1;
	}

	constexpr int32_t  version = 70016;
	constexpr uint64_t services = 0x00;
	const     int64_t  timestamp = std::time(nullptr);

	constexpr uint64_t addr_recv_services = 0x00;
	constexpr char *addr_recv_ip = "";
	constexpr uint16_t addr_recv_port = 8333;

	constexpr uint64_t addr_trans_services = 0x00;
	constexpr char *addr_trans_ip = "";
	constexpr uint16_t addr_trans_port = 8333;

	constexpr uint64_t nonce = 0x000072616E646F6D;

	constexpr char *user_agent = "";
	const     uint8_t user_agent_bytes = strnlen_s(user_agent, MAX_SIZE);

	constexpr int32_t start_height = 0;

	constexpr bool relay = false;

	std::vector<char> payload_buffer(VERSION_MSG_MIN_SIZE + user_agent_bytes);

	const std::span<char> payload_span(payload_buffer);

	std::spanstream payload_stream(payload_span, std::ios::binary | std::ios::in | std::ios::out);

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

	for (const char &c : payload_stream.span())
	{
		std::cout << std::hex << (static_cast<int>(c) & 0xFF) << ' ';
	}

	freeaddrinfo(res);
	closesocket(sock);
	WSACleanup();
}