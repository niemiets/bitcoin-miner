#ifndef MINER_CRYPTO_H
#define MINER_CRYPTO_H

#include <cstdint>

uint32_t* sha256(const uint8_t* message, size_t bit_count);

#endif //MINER_CRYPTO_H