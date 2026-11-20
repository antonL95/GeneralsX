#pragma once

#include <vector>

#include "Common/AsciiString.h"
#include "Common/KindOf.h"

class INI;

struct SpecialPowerObjectFilter
{
	static constexpr UnsignedInt RULE_NONE = 1u << 12;

	UnsignedInt m_rules = 0;
	KindOfMaskType m_includeKinds;
	KindOfMaskType m_excludeKinds;
	std::vector<AsciiString> m_includeObjects;
	std::vector<AsciiString> m_excludeObjects;

	static void parse(INI* ini, void* instance, void* store, const void* userData);
};

struct SpecialPowerAnimation
{
	Int m_condition = -1;
	UnsignedInt m_duration = 0;
	UnsignedInt m_triggerTime = 0;

	static void parse(INI* ini, void* instance, void* store, const void* userData);
};

void parseSpecialPowerModelCondition(INI* ini, void* instance, void* store, const void* userData);
