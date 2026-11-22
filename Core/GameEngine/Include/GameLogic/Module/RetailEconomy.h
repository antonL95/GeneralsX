#pragma once

#include <optional>

#include "Common/SpecialPowerINI.h"
#include "GameLogic/Module/DieModule.h"

class Player;

namespace RetailEconomy
{
Int applyDepositUpgrade(const Player& player, const AsciiString& name, Real multiplier, Int amount);
}

class PillageModuleData : public BehaviorModuleData
{
public:
	UnsignedInt m_amount = 0;
	UnsignedInt m_eventsPerPillage = 0;
	SpecialPowerObjectFilter m_filter;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class PillageModule : public BehaviorModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(PillageModule, "PillageModule")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(PillageModule, PillageModuleData)

public:
	PillageModule(Thing* thing, const ModuleData* data) : BehaviorModule(thing, data) {}

	static void onDamageDealt(const Object& source, const Object& victim);

private:
	void pillage(const Object& victim);

	UnsignedInt m_damageEvents = 0;
};

class RefundDieModuleData : public DieModuleData
{
public:
	AsciiString m_upgrade;
	std::optional<SpecialPowerObjectFilter> m_building;
	Real m_percent = 0.0f;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class RefundDie : public DieModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(RefundDie, "RefundDie")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(RefundDie, RefundDieModuleData)

public:
	RefundDie(Thing* thing, const ModuleData* data) : DieModule(thing, data) {}

	void onDie(const DamageInfo* damageInfo) override;
};
