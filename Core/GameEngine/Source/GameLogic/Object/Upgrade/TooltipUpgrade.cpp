#include "PreRTS.h"

#include "GameLogic/Module/TooltipUpgrade.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "GameLogic/Object.h"

void TooltipUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "DisplayName", INI::parseAsciiString, nullptr, offsetof(TooltipUpgradeModuleData, displayName) },
		{ "Description", INI::parseAsciiString, nullptr, offsetof(TooltipUpgradeModuleData, description) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

TooltipUpgrade::~TooltipUpgrade() = default;

const TooltipUpgradeModuleData* TooltipUpgrade::activeLabels(const Object& object)
{
	if (!RetailFileLayers::enabled() || !object.areModulesReady())
		return nullptr;

	static const NameKeyType key = NAMEKEY("TooltipUpgrade");
	UpgradeMaskType upgrades = object.getObjectCompletedUpgradeMask();
	if (const Player* controllingPlayer = object.getControllingPlayer())
		upgrades.set(controllingPlayer->getCompletedUpgradeMask());

	const TooltipUpgradeModuleData* labels = nullptr;
	for (const BehaviorModule* const* module = object.getBehaviorModules(); *module; ++module)
	{
		if ((*module)->getModuleNameKey() != key)
			continue;

		const auto* upgrade = static_cast<const TooltipUpgrade*>(*module);
		if (upgrade->isAlreadyUpgraded() ? upgrade->testUpgradeConditions(upgrades) : upgrade->wouldUpgrade(upgrades))
			labels = upgrade->getTooltipUpgradeModuleData();
	}
	return labels;
}

void TooltipUpgrade::crc(Xfer* xfer)
{
	UpgradeModule::crc(xfer);
}

void TooltipUpgrade::xfer(Xfer* xfer)
{
	UpgradeModule::xfer(xfer);
}

void TooltipUpgrade::loadPostProcess()
{
	UpgradeModule::loadPostProcess();
}
