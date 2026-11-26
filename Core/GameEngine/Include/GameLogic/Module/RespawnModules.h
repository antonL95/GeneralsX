#pragma once

#include <vector>

#include "Common/HeroRules.h"
#include "Common/SpecialPowerINI.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/DieModule.h"
#include "GameLogic/Module/UpdateModule.h"

class FXList;

class RespawnBodyModuleData : public ActiveBodyModuleData
{
public:
	SpecialPowerObjectFilter m_permanentlyKilledByFilter;
	Bool m_canRespawn = TRUE;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class RespawnBody : public ActiveBody
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( RespawnBody, "RespawnBody" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( RespawnBody, RespawnBodyModuleData )

public:
	RespawnBody(Thing *thing, const ModuleData *moduleData);

	virtual void attemptDamage(DamageInfo *damageInfo) override;

private:
	Bool isKilledByFilter(const DamageInfo *damageInfo) const;
};

struct RespawnUpdateModuleData : public UpdateModuleData
{
	Int m_deathAnim = -1;
	const FXList *m_deathFX = nullptr;
	UnsignedInt m_deathAnimationTime = 0;
	Int m_initialSpawnAnim = -1;
	const FXList *m_initialSpawnFX = nullptr;
	UnsignedInt m_initialSpawnAnimationTime = 0;
	Int m_respawnAnim = -1;
	const FXList *m_respawnFX = nullptr;
	UnsignedInt m_respawnAnimationTime = 0;
	SpecialPowerObjectFilter m_autoRespawnAtObjectFilter;
	Bool m_autoSpawn = FALSE;
	HeroRules::Price m_rules;
	Int m_respawnHealthPercent = 100;
	std::vector<HeroRules::ReviveEntry> m_entries;
	AsciiString m_buttonImage;
	AsciiString m_respawnAsTemplate;

	static void buildFieldParse(MultiIniFieldParse& p);
	static void parseRespawnRules(INI *ini, void *instance, void *store, const void *userData);
	static void parseRespawnEntry(INI *ini, void *instance, void *store, const void *userData);
};

class RespawnUpdate : public UpdateModule, public DieModuleInterface
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( RespawnUpdate, "RespawnUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( RespawnUpdate, RespawnUpdateModuleData )

public:
	RespawnUpdate(Thing *thing, const ModuleData *moduleData);

	static Int getInterfaceMask() { return UpdateModule::getInterfaceMask() | MODULEINTERFACE_DIE; }
	virtual DieModuleInterface *getDie() override { return this; }

	virtual UpdateSleepTime update() override;
	virtual void onDie(const DamageInfo *damageInfo) override;

	void markRevived(Int healthPercent);
	const FXList* spawnFX() const;

private:
	Int m_animCondition = -1;
	Bool m_spawnHandled = FALSE;
	Bool m_revived = FALSE;
};
