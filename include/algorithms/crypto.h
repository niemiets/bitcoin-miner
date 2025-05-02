#ifndef MINER_CRYPTO_H
#define MINER_CRYPTO_H

#include <cstdint>

void sha256(const uint8_t* message, const uint64_t bit_count, uint32_t hash[8]);

#endif //MINER_CRYPTO_H