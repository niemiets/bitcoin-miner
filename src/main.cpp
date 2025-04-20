#include <cstdint>
#include <iostream>
#include <span>
#include <spanstream>

#include <winsock2.h>
#include <ws2tcpip.h>

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

	char payload_buffer[4096] = {0};

	const std::span<char> payload_span(std::begin(payload_buffer), 4096);

	std::spanstream payload_stream(payload_span, std::ios::binary | std::ios::in | std::ios::out);

	// TODO: fix ts sht
	const int32_t d_ver = 0x7F110100; // 70015 in le (little endian)
	const uint64_t d_serv = 0x00; // TODO: change this to some other than 0x00
	const int64_t d_time = 0x00; // TODO: change this to real timestamp
	const uint64_t d_addr_recv_serv = 0x00;
	const char d_addr_recv_ip[16 + 1] = "::ffff:127.0.0.1"; // char[16]
	const uint16_t d_addr_recv_port = 18333;
	const uint64_t d_addr_trans_serv = d_serv;
	const char d_addr_trans_ip[16 + 1] = "::ffff:127.0.0.1"; // char[16]
	const uint16_t d_addr_trans_port = 0;
	const uint64_t d_nonce = 0; // TODO: add nonce support
	const uint8_t d_compactSize = 0x00; // TODO: add compact size
	const char *d_user_agent = ""; // TODO: probably do
	const int32_t d_start_height = 0x00; // TODO: fill
	const bool d_relay = 0x00; // TODO: idk its optional

	payload_stream
	.write((char*)&d_ver, sizeof(d_ver))
	.write((char*)&d_serv, sizeof(d_serv))
	.write((char*)&d_time, sizeof(d_time))
	.write((char*)&d_addr_recv_serv, sizeof(d_addr_recv_serv))
	.write((char*)&d_addr_recv_ip, strlen(d_addr_recv_ip))
	.write((char*)&d_addr_recv_port, sizeof(d_addr_recv_port))
	.write((char*)&d_addr_trans_serv, sizeof(d_addr_trans_serv))
	.write((char*)&d_addr_trans_ip, strlen(d_addr_trans_ip))
	.write((char*)&d_addr_trans_port, sizeof(d_addr_trans_port))
	.write((char*)&d_nonce, sizeof(d_nonce))
	.write((char*)&d_compactSize, sizeof(d_compactSize))
	.write((char*)&d_user_agent, strlen(d_user_agent))
	.write((char*)&d_start_height, sizeof(d_start_height))
	.write((char*)&d_relay, sizeof(d_relay));

	// std::cout.write(payload_stream.span().data(), static_cast<long long>(payload_stream.span().size()));
	//
	// std::cout << '\n';
	//
	// std::cout << payload_stream.span().size() << '\n';

	char header_buffer[24] = {0};

	const std::span<char> header_span(std::begin(header_buffer), 24);

	std::spanstream header_stream(header_span, std::ios::binary | std::ios::in | std::ios::out);

	const char d_start_string[4] = {0x0b, 0x11, 0x09, 0x07};
	const char d_cmd_name[12] = "version\0\0\0\0";
	const uint32_t d_payload_size = payload_stream.span().size();
	const char d_checksum[4 + 1] = "test";

	header_stream.write((char*)&d_start_string, 4);
	header_stream.write((char*)&d_cmd_name, 12);
	header_stream.write((char*)&d_payload_size, sizeof(d_payload_size));
	header_stream.write((char*)&d_checksum, 4);

	int bytes_count = send(sock, header_stream.span().data(), header_stream.span().size(), 0);

	if (bytes_count == SOCKET_ERROR) {
		std::cerr << "Couldn't send data: " << WSAGetLastError() << '\n';
		return 1;
	}

	bytes_count = send(sock, payload_stream.span().data(), payload_stream.span().size(), 0);

	if (bytes_count == SOCKET_ERROR) {
		std::cerr << "Couldn't send data: " << WSAGetLastError() << '\n';
		return 1;
	}
	
	return 0;
}

void exit_gracefully()
{

}