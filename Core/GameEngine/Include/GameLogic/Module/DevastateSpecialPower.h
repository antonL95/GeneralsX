#pragma once

#include "GameLogic/Module/SpecialPowerModule.h"

class DevastateSpecialPowerModuleData : public SpecialPowerModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& fields);

	Real m_radius = 0.0f;
	Real m_treeValueMultiplier = 0.0f;
	Real m_treeValueTotalCap = 0.0f;
	AsciiString m_fx;
	AsciiString m_fireWeapon;
};

class DevastateSpecialPower final : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(DevastateSpecialPower, "DevastateSpecialPower")

public:
	using SpecialPowerModule::SpecialPowerModule;

	static Module* friend_newModuleInstance(Thing* thing, const ModuleData* data)
	{
		return newInstance(DevastateSpecialPower)(thing, data);
	}

	static ModuleData* friend_newModuleData(INI* ini);

	NameKeyType getModuleNameKey() const override
	{
		static const NameKeyType key = NAMEKEY("DevastateSpecialPower");
		return key;
	}

protected:
	void applyPowerEffect(const Coord3D* location) override;
};
