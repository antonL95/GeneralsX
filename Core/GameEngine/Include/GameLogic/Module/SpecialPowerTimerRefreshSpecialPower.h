#pragma once

#include "GameLogic/Module/SpecialPowerModule.h"

class SpecialPowerTimerRefreshSpecialPower : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(SpecialPowerTimerRefreshSpecialPower, "SpecialPowerTimerRefreshSpecialPower")

public:
	using SpecialPowerModule::SpecialPowerModule;

	static Module* friend_newModuleInstance(Thing* thing, const ModuleData* data)
	{
		return newInstance(SpecialPowerTimerRefreshSpecialPower)(thing, data);
	}

	NameKeyType getModuleNameKey() const override
	{
		static NameKeyType key = NAMEKEY("SpecialPowerTimerRefreshSpecialPower");
		return key;
	}

protected:
	void applyPowerEffect(const Coord3D* position) override;
};
