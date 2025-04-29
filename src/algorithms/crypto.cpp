#include <bit>
#include <bitset>
#include <cassert>
#include <iostream>

#include "algorithms/crypto.h"

template <typename T>
constexpr inline T ch(T x, T y, T z)
{
	return (x & y) ^ ((~x) & z);
}

template <typename T>
constexpr inline T maj(T x, T y, T z)
{
	return (x & y) ^ (x & z) ^ (y & z);
}


template <typename T>
constexpr inline T bsig0(T x)
{
	return std::rotr(x, 2) ^ std::rotr(x, 13) ^ std::rotr(x, 22);
}

template <typename T>
constexpr inline T bsig1(T x)
{
	return std::rotr(x, 6) ^ std::rotr(x, 11) ^ std::rotr(x, 25);
}


template <typename T>
constexpr inline T ssig0(T x)
{
	return std::rotr(x, 7) ^ std::rotr(x, 18) ^ (x >> 3);
}

template <typename T>
constexpr inline T ssig1(T x)
{
	return std::rotr(x, 17) ^ std::rotr(x, 19) ^ (x >> 10);
}

constexpr uint16_t SHA_BLOCK_BIT_COUNT = 512;
constexpr uint8_t SHA_BLOCK_SIZE = SHA_BLOCK_BIT_COUNT / (sizeof(uint8_t) * 8);

constexpr uint32_t h_const[] = {
	std::byteswap(static_cast<uint32_t>(0x6a09e667)),
	std::byteswap(static_cast<uint32_t>(0xbb67ae85)),
	std::byteswap(static_cast<uint32_t>(0x3c6ef372)),
	std::byteswap(static_cast<uint32_t>(0xa54ff53a)),
	std::byteswap(static_cast<uint32_t>(0x510e527f)),
	std::byteswap(static_cast<uint32_t>(0x9b05688c)),
	std::byteswap(static_cast<uint32_t>(0x1f83d9ab)),
	std::byteswap(static_cast<uint32_t>(0x5be0cd19))
};

constexpr uint32_t k[] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

uint32_t w[64];
uint32_t h[8];

uint32_t* sha256(const uint8_t* message, uint64_t bit_count)
{
	assert(std::endian::native == std::endian::little); // if you have for some reason big endian integers then rewrite this function and make push request

	if (bit_count == 0)
		return new uint32_t[] {
			0xe3b0c442,
			0x98fc1c14,
			0x9afbf4c8,
			0x996fb924,
			0x27ae41e4,
			0x649b934c,
			0xa495991b,
			0x7852b855
		};
	
	uint64_t message_size = (((bit_count - 1) / 8) + 1);
	
	uint64_t block_count = ((bit_count + 1 + 64 - 1) / SHA_BLOCK_BIT_COUNT) + 1; // bit_count + the one 1 bit + 64 bit length - 1 because we want it to increment when its above 512
	
	uint8_t **blocks = new uint8_t*[block_count];
	
	for (uint64_t i = 0; i < block_count; i++)
	{
		blocks[i] = new uint8_t[SHA_BLOCK_SIZE];
	}
	
	// copy message
	for (uint64_t i = 0; i < (message_size - 1) / sizeof(uint8_t); i++)
	{
		blocks[i / SHA_BLOCK_SIZE][i % SHA_BLOCK_SIZE] = message[i];
	}
	
	blocks[((message_size - 1) / sizeof(uint8_t)) / SHA_BLOCK_SIZE][((message_size - 1) / sizeof(uint8_t)) % SHA_BLOCK_SIZE] = message[message_size - 1] & (0xFF << (((sizeof(uint8_t) * 8 - 1) - (bit_count - 1)) % (sizeof(uint8_t) * 8)));
	
	// fill 0 from message to length
	for (uint64_t i = 0; i < block_count * SHA_BLOCK_SIZE - message_size / sizeof(uint8_t) - 8 / sizeof(uint8_t); i++)
	{
		blocks[(message_size / sizeof(uint8_t) + i) / SHA_BLOCK_SIZE][(message_size / sizeof(uint8_t) + i) % SHA_BLOCK_SIZE] = 0;
	}
	
	// append 1 after message
	const uint64_t append_index = ((bit_count - 1 + 1) / (sizeof(uint8_t) * 8)) + 1 - 1;
	const uint64_t append_bit_index = (bit_count - 1 + 1) % (sizeof(uint8_t) * 8);
	
	blocks[append_index / SHA_BLOCK_SIZE][append_index % SHA_BLOCK_SIZE] |= (0x80 >> append_bit_index);
	
	// set length
	*reinterpret_cast<uint64_t*>(blocks[block_count - 1] + SHA_BLOCK_SIZE - static_cast<uint64_t>(8 / sizeof(uint8_t))) = std::byteswap(bit_count);
	
	// initialize working variables
	uint32_t *vars = new uint32_t[8];
	memcpy(h, h_const, 8 * sizeof(uint32_t));
	
	// extend
	for (uint64_t i = 0; i < block_count; i++)
	{
		memcpy(w, blocks[i], SHA_BLOCK_SIZE);
		
		for (uint8_t j = 16; j < 64; j++)
		{
			w[j] = std::byteswap(std::byteswap(w[j - 16]) + ssig0(std::byteswap(w[j - 15])) + std::byteswap(w[j - 7]) + ssig1(std::byteswap(w[j - 2]))); // TODO: write custom big endian operators so it doesn't have to use byteswap twice
		}
		
		// print message schedule
		// std::cout << "w: \n";
		// for (uint16_t j = 0; j < (64 * 4); j++)
		// {
		// 	if (j % 4 == 0)
		// 	{
		// 		std::cout << '\n';
		// 	}
		//
		// 	std::cout << std::bitset<8>(reinterpret_cast<uint8_t*>(w)[j]);
		// }
		
		memcpy(vars, h, 8 * sizeof(uint32_t));
		
		// std::cout << "vars: \n";
		// for (uint16_t j = 0; j < (8 * 4); j++)
		// {
		// 	if (j % 4 == 0)
		// 	{
		// 		std::cout << '\n';
		// 	}
		//
		// 	std::cout << std::bitset<8>(reinterpret_cast<uint8_t*>(vars)[j]) << ' ';
		// }
		
		// calculate hash
		for (uint8_t j = 0; j < 64; j++)
		{
			const uint32_t t1 = std::byteswap(std::byteswap(vars[7]) + bsig1(std::byteswap(vars[4])) + ch(std::byteswap(vars[4]), std::byteswap(vars[5]), std::byteswap(vars[6])) + k[j] + std::byteswap(w[j]));
			const uint32_t t2 = std::byteswap(bsig0(std::byteswap(vars[0])) + maj(std::byteswap(vars[0]), std::byteswap(vars[1]), std::byteswap(vars[2])));
			
			vars[7] = vars[6];
			vars[6] = vars[5];
			vars[5] = vars[4];
			vars[4] = std::byteswap(std::byteswap(vars[3]) + std::byteswap(t1));
			vars[3] = vars[2];
			vars[2] = vars[1];
			vars[1] = vars[0];
			vars[0] = std::byteswap(std::byteswap(t1) + std::byteswap(t2));
		}
		
		// std::cout << "vars2: \n";
		// for (uint16_t j = 0; j < (8 * 4); j++)
		// {
		// 	if (j % 4 == 0)
		// 	{
		// 		std::cout << '\n';
		// 	}
		//
		// 	std::cout << std::bitset<8>(reinterpret_cast<uint8_t*>(vars)[j]) << ' ';
		// }
		
		for (uint8_t j = 0; j < 8; j++)
		{
			h[j] = std::byteswap(std::byteswap(vars[j]) + std::byteswap(h[j]));
		}
		
		// std::cout << "hash: \n";
		// for (uint16_t j = 0; j < (8 * 4); j++)
		// {
		// 	if (j % 4 == 0)
		// 	{
		// 		std::cout << '\n';
		// 	}
		//
		// 	std::cout << std::bitset<8>(reinterpret_cast<uint8_t*>(h)[j]) << ' ';
		// }
	}
	
	// // print blocks
	// std::cout << "blocks: \n";
	// for (uint64_t i = 0; i < block_count; i++)
	// {
	// 	for (uint8_t j = 0; j < SHA_BLOCK_SIZE; j++)
	// 	{
	// 		if (j % 4 == 0)
	// 		{
	// 			std::cout << '\n';
	// 		}
	//
	// 		std::cout << std::bitset<8>(blocks[i][j]) << ' ';
	// 	}
	// }
	
	for (uint64_t i = 0; i < block_count; i++)
	{
		delete[] blocks[i];
	}
	
	delete[] blocks;
	
	return h;
}