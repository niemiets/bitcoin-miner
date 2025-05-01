#ifndef MINER_NETWORK_TYPE_H
#define MINER_NETWORK_TYPE_H

#include <bit>
#include <cstdint>

#include <winsock2.h>

class mainnet
{
	public:
		static constexpr uint16_t port = 8333;
		static constexpr char start_string[4] = {static_cast<char>(0xf9), static_cast<char>(0xbe), static_cast<char>(0xb4), static_cast<char>(0xd9)};
		static constexpr uint32_t max_nbits = 0x1d00ffff;
		
		static constexpr char* port_string = "8333";
		
	private:
};

class testnet
{
	public:
		static constexpr uint16_t port = 18333;
		static constexpr char start_string[4] = {static_cast<char>(0x0b), static_cast<char>(0x11), static_cast<char>(0x09), static_cast<char>(0x07)};
		static constexpr uint32_t max_nbits = 0x1d00ffff;
		
		static constexpr char* port_string = "18333";
		
	private:
};

class regtest
{
	public:
		static constexpr uint16_t port = 18444;
		static constexpr char start_string[4] = {static_cast<char>(0xfa), static_cast<char>(0xbf), static_cast<char>(0xb5), static_cast<char>(0xda)};
		static constexpr uint32_t max_nbits = 0x207fffff;
		
		static constexpr char* port_string = "18444";
		
	private:
};

#endif //MINER_NETWORK_TYPE_H
