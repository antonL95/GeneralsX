#pragma once

#include "Common/AudioEventRTS.h"
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

class AudioLoopUpgradeModuleData : public UpdateModuleData
{
public:
	UpgradeMuxData m_upgrade;
	AsciiString m_sound;
	UnsignedInt m_duration = 0;
	Bool m_killOnDeath = FALSE;
	static void buildFieldParse(MultiIniFieldParse& p);
};

class AudioLoopUpgrade final : public UpdateModule, public UpgradeMux
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(AudioLoopUpgrade, "AudioLoopUpgrade")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(AudioLoopUpgrade, AudioLoopUpgradeModuleData)

public:
	AudioLoopUpgrade(Thing* thing, const ModuleData* data);
	static Int getInterfaceMask() { return MODULEINTERFACE_UPGRADE | MODULEINTERFACE_UPDATE; }
	UpgradeModuleInterface* getUpgrade() override { return this; }
	DisabledMaskType getDisabledTypesToProcess() const override { return DISABLEDMASK_ALL; }
	UpdateSleepTime update() override;
	Bool resetUpgrade(const UpgradeMaskType& mask) override;
	void onDelete() override;

private:
	void upgradeImplementation() override;
	void getUpgradeActivationMasks(UpgradeMaskType& activation, UpgradeMaskType& conflicting) const override;
	void performUpgradeFX() override;
	void processUpgradeRemoval() override;
	Bool requiresAllActivationUpgrades() const override;
	Bool isSubObjectsUpgrade() override { return FALSE; }
	void xferRetailBlocks(Xfer* xfer, Bool present);

	UnsignedInt m_playUntil = 0;
	AudioHandle m_handle = 0;
	Bool m_requested = FALSE;
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
