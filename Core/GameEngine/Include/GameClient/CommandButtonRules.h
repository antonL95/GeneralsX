#pragma once

#include <cctype>
#include <string>
#include <vector>

namespace CommandButtonRules
{
enum { RETAIL_COMMANDS_PER_SET = 33 };

enum RotwkOption : unsigned int
{
	ROTWK_OPTION_NONE = 0,
	ROTWK_NO_PLAY_UNIT_SPECIFIC_SOUND_FOR_AUTO_ABILITY = 1u << 0,
	ROTWK_NEEDS_CASTLE_KINDOF = 1u << 1,
	ROTWK_OK_FOR_MULTI_EXECUTE = 1u << 2,
	ROTWK_ALLOW_ROCK_TARGET = 1u << 3,
	ROTWK_HIDE_WHILE_DISABLED = 1u << 4,
	ROTWK_TOGGLE_IMAGE_ON_WEAPON = 1u << 5,
	ROTWK_TOGGLE_IMAGE_ON_WEAPONSET = 1u << 6,
	ROTWK_TOGGLE_IMAGE_ON_FORMATION = 1u << 7,
	ROTWK_MOUNTED_ONLY = 1u << 8,
	ROTWK_UNMOUNTED_ONLY = 1u << 9,
	ROTWK_NONPRESSABLE = 1u << 10,
	ROTWK_AUTO_ABILITY_TRIGGERED = 1u << 11,
	ROTWK_ON_GROUND_ONLY = 1u << 12,
	ROTWK_CANCELABLE = 1u << 13,
};

inline const char* const* rotwkOptionNames()
{
	static const char* const names[] =
	{
		"NO_PLAY_UNIT_SPECIFIC_SOUND_FOR_AUTO_ABILITY",
		"NEEDS_CASTLE_KINDOF",
		"OK_FOR_MULTI_EXECUTE",
		"ALLOW_ROCK_TARGET",
		"HIDE_WHILE_DISABLED",
		"TOGGLE_IMAGE_ON_WEAPON",
		"TOGGLE_IMAGE_ON_WEAPONSET",
		"TOGGLE_IMAGE_ON_FORMATION",
		"MOUNTED_ONLY",
		"UNMOUNTED_ONLY",
		"NONPRESSABLE",
		"AUTO_ABILITY_TRIGGERED",
		"ON_GROUND_ONLY",
		"CANCELABLE",
		nullptr
	};
	return names;
}

inline const char* const* rotwkCommandNames()
{
	static const char* const names[] =
	{
		"SECONDARY_UNIT_BUILD",
		"CASTLE_UPGRADE",
		"EVACUATE_CONTESTED",
		"AUTOCAST_WEAPON",
		"CASTLE_UNPACK",
		"CASTLE_PACK",
		"TOGGLE_WEAPON",
		"HORDE_TOGGLE_FORMATION",
		"SPECIAL_POWER_TOGGLE",
		"SPELL_BOOK",
		"ONE_RING",
		"CREW_EVACUATE",
		"BLOODTHIRSTY",
		"CLOSE_GATE",
		"OPEN_GATE",
		"TOGGLE_GATE",
		"TOGGLE_WEAPONSET",
		"REVIVE",
		"MONSTERDOCK",
		"TOGGLE_NO_AUTO_ACQUIRE",
		"WAKE_AUTO_PICKUP",
		"CASTLE_UNPACK_EXPLICIT_OBJECT",
		"START_SELF_REPAIR",
		"HORDE_SET_FORMATION",
		"PUSH_VISIBLE_COMMAND_RANGE",
		"POP_VISIBLE_COMMAND_RANGE",
		"TOGGLE_STANCE",
		"SET_STANCE",
		"START_NEIGHBORHOOD_REPAIR",
		"CANCEL_NEIGHBORHOOD",
		nullptr
	};
	return names;
}

inline int nameIndex(const char* const* names, const char* token)
{
	for (int i = 0; names[i]; ++i)
	{
		const char* a = names[i];
		const char* b = token;
		while (*a && *b && std::tolower(static_cast<unsigned char>(*a)) == std::tolower(static_cast<unsigned char>(*b)))
		{
			++a;
			++b;
		}
		if (*a == '\0' && *b == '\0')
			return i;
	}
	return -1;
}

struct OptionBits
{
	unsigned int zeroHour = 0;
	unsigned int rotwk = 0;
	std::vector<std::string> unknown;
};

inline OptionBits splitOptions(const std::vector<std::string>& tokens, const char* const* zeroHourNames)
{
	OptionBits bits;
	for (const std::string& token : tokens)
	{
		const int zh = nameIndex(zeroHourNames, token.c_str());
		if (zh >= 0)
		{
			bits.zeroHour |= 1u << zh;
			continue;
		}
		const int rotwk = nameIndex(rotwkOptionNames(), token.c_str());
		if (rotwk >= 0)
			bits.rotwk |= 1u << rotwk;
		else
			bits.unknown.push_back(token);
	}
	return bits;
}

struct VisibleRange
{
	int start = -1;
	int count = 0;

	bool active() const { return start >= 0 && count > 0; }
};

inline bool inSideColumn(int entry, const VisibleRange& range, int initialVisible)
{
	if (range.active())
		return entry >= range.start && entry < range.start + range.count;
	return initialVisible <= 0 || entry < initialVisible;
}

inline VisibleRange pushRange(int start, int count)
{
	VisibleRange range;
	if (start >= 0 && count > 0 && start < RETAIL_COMMANDS_PER_SET)
	{
		range.start = start;
		range.count = count;
	}
	return range;
}
}
