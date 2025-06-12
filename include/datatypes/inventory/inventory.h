#ifndef MINER_INVENTORY_H
#define MINER_INVENTORY_H

#include <cstdint>

#include "datatypes/inventory/inventory_type.h"

struct inventory
{
	inventory_type type;
	char           hash[32];
};

#endif //MINER_INVENTORY_H
