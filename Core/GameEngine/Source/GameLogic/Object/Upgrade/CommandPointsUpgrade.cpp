#include "PreRTS.h"

#include "GameLogic/Module/CommandPointsUpgrade.h"

#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/Xfer.h"
#include "GameLogic/Object.h"

void CommandPointsUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "CommandPoints", INI::parseInt, nullptr, offsetof(CommandPointsUpgradeModuleData, m_commandPoints) },
		{ "RequiredObject", SpecialPowerObjectFilter::parse, nullptr, offsetof(CommandPointsUpgradeModuleData, m_requiredObject) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

CommandPointsUpgrade::CommandPointsUpgrade(Thing* thing, const ModuleData* moduleData) :
	UpgradeModule(thing, moduleData)
{
}

CommandPointsUpgrade::~CommandPointsUpgrade() = default;

void CommandPointsUpgrade::onDelete()
{
	if (!isAlreadyUpgraded())
		return;
	Player* player = getObject()->getControllingPlayer();
	if (player)
		player->removeCommandPointUpgradeBonus(this);
	setUpgradeExecuted(FALSE);
}

void CommandPointsUpgrade::onCapture(Player* oldOwner, Player* newOwner)
{
	if (!isAlreadyUpgraded())
		return;
	const auto* data = getCommandPointsUpgradeModuleData();
	if (oldOwner)
		oldOwner->removeCommandPointUpgradeBonus(this);
	if (newOwner)
		newOwner->addCommandPointUpgradeBonus(this, data->m_commandPoints, data->m_requiredObject);
}

void CommandPointsUpgrade::upgradeImplementation()
{
	Player* player = getObject()->getControllingPlayer();
	if (!player)
		return;
	const auto* data = getCommandPointsUpgradeModuleData();
	player->addCommandPointUpgradeBonus(this, data->m_commandPoints, data->m_requiredObject);
}

void CommandPointsUpgrade::crc(Xfer* xfer)
{
	UpgradeModule::crc(xfer);
}

void CommandPointsUpgrade::xfer(Xfer* xfer)
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	UpgradeModule::xfer(xfer);
}

void CommandPointsUpgrade::loadPostProcess()
{
	UpgradeModule::loadPostProcess();
	if (isAlreadyUpgraded())
	{
		Player* player = getObject()->getControllingPlayer();
		if (player)
		{
			const auto* data = getCommandPointsUpgradeModuleData();
			player->addCommandPointUpgradeBonus(this, data->m_commandPoints, data->m_requiredObject);
		}
	}
}
