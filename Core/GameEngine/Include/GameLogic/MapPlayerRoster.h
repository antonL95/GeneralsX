#pragma once

#include <string>
#include <string_view>

namespace MapPlayerRoster
{
	inline bool isRotwk(unsigned version)
	{
		return version >= 5;
	}

	inline bool keepsSide(std::string_view name)
	{
		return name.empty() || name == "PlyrCivilian" || name == "PlyrCreeps";
	}

	inline std::string slotName(bool rotwk, int slot, int startPosition, bool observer)
	{
		if (!rotwk)
			return "player" + std::to_string(slot);
		if (observer)
			return "Observer" + std::to_string(slot + 1);
		return "Player_" + std::to_string(startPosition + 1);
	}

	inline std::string_view qualifiedTeam(std::string_view owner)
	{
		const auto slash = owner.find('/');
		return slash == std::string_view::npos ? std::string_view() : owner.substr(slash + 1);
	}
}
