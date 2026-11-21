#pragma once

#include "GameLogic/Module/SpecialPowerModule.h"

class ProductionSpeedBonusModuleData : public SpecialPowerModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& fields);

	std::vector<AsciiString> m_types;
	UnsignedInt m_numberOfFrames = 0;
	Real m_speedMultiplier = 1.0f;
};

class ProductionSpeedBonus : public SpecialPowerModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ProductionSpeedBonus, "ProductionSpeedBonus")

public:
	using SpecialPowerModule::SpecialPowerModule;

	static Module* friend_newModuleInstance(Thing* thing, const ModuleData* data)
	{
		return newInstance(ProductionSpeedBonus)(thing, data);
	}

	static ModuleData* friend_newModuleData(INI* ini);

	NameKeyType getModuleNameKey() const override
	{
		static NameKeyType key = NAMEKEY("ProductionSpeedBonus");
		return key;
	}

protected:
	void applyPowerEffect(const Coord3D*) override;
};
