#ifndef MINER_MESSAGE_INV_H
#define MINER_MESSAGE_INV_H

#include <cstdint>

#include "datatypes/compact_size_uint.h"
#include "datatypes/inventory/inventory.h"

struct message_inv
{
	compact_size_uint count;
	inventory        *inventory;
};

#endif //MINER_MESSAGE_INV_H
