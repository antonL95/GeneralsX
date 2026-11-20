#pragma once

#include "GameLogic/Module/UpgradeModule.h"

class CastleUpgradeModuleData : public UpgradeModuleData
{
public:
	AsciiString m_upgrade;
	Real m_wallUpgradeRadius = 0.0f;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class CastleUpgrade final : public UpgradeModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(CastleUpgrade, "CastleUpgrade")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(CastleUpgrade, CastleUpgradeModuleData)

public:
	CastleUpgrade(Thing* thing, const ModuleData* data) : UpgradeModule(thing, data) {}

	Bool isSubObjectsUpgrade() override { return FALSE; }
	const UpgradeTemplate* getGrantedUpgrade() const;

protected:
	void upgradeImplementation() override;
};
