/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: INISpecialPower.cpp //////////////////////////////////////////////////////////////////////
// Author: Colin Day, April 2002
// Desc:   Special Power INI database
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/INI.h"
#include "Common/ModelState.h"
#include "Common/SpecialPower.h"
#include "Common/SpecialPowerINI.h"
#include "Common/ThingTemplate.h"

#include <algorithm>
#include <string_view>

Bool SpecialPowerObjectFilter::matchesTemplate(const ThingTemplate& thing) const
{
	const auto matchesName = [&thing](const AsciiString& name)
	{
		return stricmp(name.str(), thing.getName().str()) == 0;
	};
	if (thing.isAnyKindOf(m_excludeKinds) ||
		std::any_of(m_excludeObjects.begin(), m_excludeObjects.end(), matchesName))
		return FALSE;

	if (m_includeKinds.any() || !m_includeObjects.empty())
		return thing.isAnyKindOf(m_includeKinds) ||
			std::any_of(m_includeObjects.begin(), m_includeObjects.end(), matchesName);

	return (m_rules & RULE_NONE) == 0;
}

void SpecialPowerObjectFilter::parse(INI* ini, void*, void* store, const void*)
{
	SpecialPowerObjectFilter filter;
	filter.addTokens(ini, ini->getNextToken());
	*static_cast<SpecialPowerObjectFilter*>(store) = std::move(filter);
}

void SpecialPowerObjectFilter::addTokens(INI* ini, const char* token)
{
	static constexpr const char* rules[] =
	{
		"SELF", "ALLIES", "ENEMIES", "NEUTRALS", "SUICIDE", "NOT_SIMILAR", "NOT_AIRBORNE",
		"PROJECTILES", "SAME_HEIGHT_ONLY", "MINES", "ALL", "ANY", "NONE", "SAME_PLAYER", nullptr
	};
	static_assert(std::string_view(rules[12]) == "NONE");
	for (; token; token = ini->getNextTokenOrNull())
	{
		if (token[0] == '+' || token[0] == '-')
		{
			if (!token[1])
				throw INI_INVALID_DATA;
			const Bool include = token[0] == '+';
			const char* name = token + 1;
			const char* const* kinds = KindOfMaskType::getBitNames();
			Int index = 0;
			while (kinds[index] && stricmp(name, kinds[index]) != 0)
				++index;
			if (kinds[index])
				(include ? m_includeKinds : m_excludeKinds).set(index);
			else
				(include ? m_includeObjects : m_excludeObjects).emplace_back(name);
		}
		else
		{
			const char* rule = stricmp(token, "NEUTRAL") == 0 ? "NEUTRALS" : token;
			m_rules |= 1u << INI::scanIndexList(rule, rules);
		}
	}
}

void SpecialPowerAnimation::parse(INI* ini, void*, void* store, const void*)
{
	SpecialPowerAnimation anim;
	anim.m_condition = INI::scanIndexList(ini->getNextSubToken("AnimState"), ModelConditionFlags::getBitNames());
	anim.m_duration = INI::scanUnsignedInt(ini->getNextSubToken("AnimTime"));
	if (const char* token = ini->getNextTokenOrNull(ini->getSepsColon()))
	{
		if (stricmp(token, "TriggerTime") != 0)
			throw INI_INVALID_DATA;
		anim.m_triggerTime = INI::scanUnsignedInt(ini->getNextToken(ini->getSepsColon()));
	}
	*static_cast<SpecialPowerAnimation*>(store) = anim;
}

void parseSpecialPowerModelCondition(INI* ini, void*, void* store, const void*)
{
	*static_cast<Int*>(store) =
		INI::scanIndexList(ini->getNextSubToken("ModelConditionState"), ModelConditionFlags::getBitNames());
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void INI::parseSpecialPowerDefinition( INI *ini )
{
	SpecialPowerStore::parseSpecialPowerDefinition(ini);
}



