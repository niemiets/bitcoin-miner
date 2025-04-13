#include <iostream>

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
	
	
	
	return 0;
}

void exit_gracefully()
{

}