#pragma once

#include <cstdint>

#include "Lib/BaseType.h"

class Player;

namespace RetailTribute
{
std::uint64_t secondsUntilAvailable(UnsignedInt frame);
Bool isEligible(const Player& sender, const Player* recipient);
UnsignedInt amountLimit(const Player& sender, const Player& recipient);
}
