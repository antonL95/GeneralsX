#pragma once

#include "Common/SpecialPowerINI.h"
#include "GameLogic/Module/UpgradeModule.h"

class Thing;
class Player;

class CommandPointsUpgradeModuleData : public UpgradeModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	Int m_commandPoints = 0;
	SpecialPowerObjectFilter m_requiredObject;
};

class CommandPointsUpgrade final : public UpgradeModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(CommandPointsUpgrade, "CommandPointsUpgrade")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(CommandPointsUpgrade, CommandPointsUpgradeModuleData)

public:
	CommandPointsUpgrade(Thing* thing, const ModuleData* moduleData);

	void onDelete() override;
	void onCapture(Player* oldOwner, Player* newOwner) override;

protected:
	void upgradeImplementation() override;
	Bool isSubObjectsUpgrade() override { return FALSE; }
};
