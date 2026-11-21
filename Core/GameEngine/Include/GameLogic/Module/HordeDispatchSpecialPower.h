#pragma once

#include "GameLogic/Module/SpecialPowerModule.h"

class HordeDispatchSpecialPower : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(HordeDispatchSpecialPower, "HordeDispatchSpecialPower")

public:
	using SpecialPowerModule::SpecialPowerModule;

	static Module* friend_newModuleInstance(Thing* thing, const ModuleData* data)
	{
		return newInstance(HordeDispatchSpecialPower)(thing, data);
	}

	NameKeyType getModuleNameKey() const override
	{
		static NameKeyType key = NAMEKEY("HordeDispatchSpecialPower");
		return key;
	}

	void doSpecialPower(UnsignedInt commandOptions) override;
	void doSpecialPowerAtObject(Object* target, UnsignedInt commandOptions) override;
	void doSpecialPowerAtLocation(const Coord3D* position, Real angle, UnsignedInt commandOptions) override;
};
