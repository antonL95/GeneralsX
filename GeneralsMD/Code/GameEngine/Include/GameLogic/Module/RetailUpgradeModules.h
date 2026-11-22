#pragma once

#include "Common/SpecialPowerINI.h"
#include "GameLogic/Module/UpdateModule.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Module/UpgradeModule.h"

class InheritUpgradeCreateModuleData : public UpdateModuleData
{
public:
	Real m_radius = 0.0f;
	std::vector<AsciiString> m_upgrades;
	SpecialPowerObjectFilter m_filter;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class InheritUpgradeCreate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(InheritUpgradeCreate, "InheritUpgradeCreate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(InheritUpgradeCreate, InheritUpgradeCreateModuleData)

public:
	InheritUpgradeCreate(Thing* thing, const ModuleData* data);
	UpdateSleepTime update() override;
};

class BuildableHeroListUpgrade final : public UpgradeModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(BuildableHeroListUpgrade, "BuildableHeroListUpgrade")
	MAKE_STANDARD_MODULE_MACRO(BuildableHeroListUpgrade)

public:
	BuildableHeroListUpgrade(Thing* thing, const ModuleData* data) : UpgradeModule(thing, data) {}
	Bool isSubObjectsUpgrade() override { return FALSE; }
	static void setRingHeroesBuildable(Player* player, Bool buildable);

protected:
	void upgradeImplementation() override;
};

class GiveUpgradeUpdateModuleData : public SpecialAbilityUpdateModuleData
{
public:
	Bool m_deliverUpgrade = FALSE;
	Real m_fadeOutSpeed = 0.1f;
	const FXList* m_spawnOutFX = nullptr;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class GiveUpgradeUpdate final : public SpecialAbilityUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(GiveUpgradeUpdate, "GiveUpgradeUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(GiveUpgradeUpdate, GiveUpgradeUpdateModuleData)

public:
	GiveUpgradeUpdate(Thing* thing, const ModuleData* data) : SpecialAbilityUpdate(thing, data) {}
	Bool initiateIntentToDoSpecialPower(const SpecialPowerTemplate* power, const Object* target,
		const Coord3D* position, const Waypoint* waypoint, UnsignedInt options) override;
	UpdateSleepTime update() override;
	void crcRetailState(Xfer* xfer);

protected:
	Bool acceptsIntent(const Object* target, const Coord3D* position) const override;
	void triggerAbilityEffect() override;

private:
	const UpgradeTemplate* carriedUpgrade() const;
	Bool canDeliverTo(const Object* target, const UpgradeTemplate* upgrade) const;

	UnsignedInt m_removeAt = 0;
};
