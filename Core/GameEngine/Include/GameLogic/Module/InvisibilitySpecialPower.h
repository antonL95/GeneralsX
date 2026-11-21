#pragma once

#include "Common/ModelState.h"
#include "GameLogic/Module/SpecialPowerModule.h"

class InvisibilitySpecialPowerModuleData : public SpecialPowerModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& fields);

	struct Nugget
	{
		ModelConditionFlags forbidden;
		Real detectionRange = 0.0f;
		Int type = 0;
	};

	Nugget m_nugget;
	SpecialPowerObjectFilter m_filter;
	Real m_broadcastRadius = 0.0f;
	UnsignedInt m_duration = 0;
};

class InvisibilitySpecialPower : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(InvisibilitySpecialPower, "InvisibilitySpecialPower")

public:
	using SpecialPowerModule::SpecialPowerModule;

	static Module* friend_newModuleInstance(Thing* thing, const ModuleData* data)
	{
		return newInstance(InvisibilitySpecialPower)(thing, data);
	}

	static ModuleData* friend_newModuleData(INI* ini);

	NameKeyType getModuleNameKey() const override
	{
		static NameKeyType key = NAMEKEY("InvisibilitySpecialPower");
		return key;
	}

protected:
	void applyPowerEffect(const Coord3D* position) override;
};
