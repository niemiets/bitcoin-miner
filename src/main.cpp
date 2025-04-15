#include <iostream>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <spanstream>

#include "bitcoin/message_header.h"

constexpr std::string operator""_no_null(const char *str, std::size_t len)
{
	char *no_null_buff = new char[len];
	
	std::copy(str, str + len, no_null_buff);
	
	std::string s(no_null_buff);
	
	delete[] no_null_buff;
	
	return s;
}

template <size_t N>
constexpr size_t no_null_length(const char (&)[N])
{
	return N - 1;
}

int main()
{
	const char *HOSTNAME = "135.181.137.135";
	const char *PORT = "18333";
	
	int err_code = ERROR_SUCCESS;
	
	WSADATA init_data;
	
	err_code = WSAStartup(MAKEWORD(2, 2), &init_data);
	
	if (err_code != ERROR_SUCCESS)
	{
		std::cerr << "Couldn't initiate winsock dll: " << err_code << '\n';
		return 1;
	}
	
	addrinfo hints {};
	hints.ai_addrlen = 0;
	hints.ai_canonname = nullptr;
	hints.ai_addr = nullptr;
	hints.ai_next = nullptr;
	
	addrinfo *res = new addrinfo;
	
	err_code = getaddrinfo(HOSTNAME, PORT, &hints, &res);
	
	if (err_code != ERROR_SUCCESS)
	{
		std::cerr << "Couldn't get address info: " << err_code << '\n';
		return 1;
	}
	
	SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	
	if (sock == INVALID_SOCKET)
	{
		std::cerr << "Couldn't create socket: " << WSAGetLastError() << '\n';
		return 1;
	}
	
	if (connect(sock, res->ai_addr, static_cast<int>(res->ai_addrlen)) == SOCKET_ERROR)
	{
		std::cerr << "Couldn't connect to socket: " << WSAGetLastError() << '\n';
		return 1;
	}
	
	char buffff[1024];

	std::span buf(buffff);

	std::spanstream stream(buf);

	const int32_t version = 0x7F110100;
	const uint64_t services = 0;
	const int64_t timestamp = 0;
	const uint64_t addr_recv_services = 0;
	const auto addr_recv_ip = "::ffff:127.0.0.1"_no_null;
	const uint16_t addr_recv_port = 0;
	const uint64_t addr_trans_services = 0;
	const auto addr_trans_ip = "::ffff:127.0.0.1"_no_null;
	const uint16_t addr_trans_port = 0;
	const uint64_t nonce = 0;
	const uint8_t user_agent_bytes = 0;
	const char user_agent[] = "";
	const int32_t start_height = 0;
	const bool relay = true;

	stream
	<< version
	<< services
	<< timestamp
	<< addr_recv_services
	<< addr_recv_ip
	<< addr_recv_port
	<< addr_trans_services
	<< addr_trans_ip
	<< addr_trans_port
	<< nonce
	<< user_agent_bytes;

	if (user_agent_bytes > 0)
		stream << user_agent;

	stream
	<< start_height
	<< relay;

	const int bytes_sent = send(sock, stream.rdbuf()->span().data(), static_cast<int>(stream.rdbuf()->span().size()), 0);

	if (bytes_sent == SOCKET_ERROR) {
		std::cerr << "Couldn't send data: " << WSAGetLastError() << '\n';
		return 1;
	}

	char *buff = new char[4096];

	const int bytes_recv = recv(sock, buff, 4096, 0);

	if (bytes_recv == SOCKET_ERROR)
	{
		std::cerr << "Couldn't receive data: " << WSAGetLastError() << '\n';
		return 1;
	}

	std::cout << bytes_recv << '\n';
	
	return 0;
}

void exit_gracefully()
{

}