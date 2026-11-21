#pragma once

#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/WeaponSetFlags.h"

class WeaponChangeSpecialPowerModuleData : public SpecialPowerModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& fields);

	WeaponSetFlags m_toggleFlags;
	UnsignedInt m_onSleepFrames = 0;
	UnsignedInt m_offSleepFrames = 0;
	AsciiString m_onModifier;
	AsciiString m_offModifier;
};

class WeaponChangeSpecialPowerModule : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(WeaponChangeSpecialPowerModule, "WeaponChangeSpecialPowerModule")

public:
	using SpecialPowerModule::SpecialPowerModule;

	static Module* friend_newModuleInstance(Thing* thing, const ModuleData* data)
	{
		return newInstance(WeaponChangeSpecialPowerModule)(thing, data);
	}

	static ModuleData* friend_newModuleData(INI* ini);

	NameKeyType getModuleNameKey() const override
	{
		static NameKeyType key = NAMEKEY("WeaponChangeSpecialPowerModule");
		return key;
	}

	void doSpecialPowerAtObject(Object* object, UnsignedInt commandOptions) override;
	void doSpecialPowerAtLocation(const Coord3D* position, Real angle, UnsignedInt commandOptions) override;

protected:
	void applyPowerEffect(const Coord3D*) override;
};
