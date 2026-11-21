#include "PreRTS.h"

#include "GameClient/ControlBar.h"
#include "GameLogic/Module/DoCommandUpgrade.h"
#include "GameLogic/Object.h"

void DoCommandUpgradeModuleData::buildFieldParse(MultiIniFieldParse& fields)
{
	UpgradeModuleData::buildFieldParse(fields);
	static const FieldParse commands[] =
	{
		{ "GetUpgradeCommandButtonName", INI::parseAsciiString, nullptr, offsetof(DoCommandUpgradeModuleData, m_grantCommand) },
		{ "RemoveUpgradeCommandButtonName", INI::parseAsciiString, nullptr, offsetof(DoCommandUpgradeModuleData, m_removeCommand) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	fields.add(commands);
}

DoCommandUpgrade::~DoCommandUpgrade() = default;

void DoCommandUpgrade::issueCommand(const AsciiString& name)
{
	if (TheControlBar && name.isNotEmpty() && !getObject()->isDestroyed())
	{
		if (const CommandButton* button = TheControlBar->findCommandButton(name))
			getObject()->doCommandButton(button, CMD_FROM_SCRIPT);
	}
}

void DoCommandUpgrade::upgradeImplementation()
{
	setUpgradeExecuted(TRUE);
	issueCommand(getDoCommandUpgradeModuleData()->m_grantCommand);
}

Bool DoCommandUpgrade::resetUpgrade(const UpgradeMaskType& mask)
{
	if (!UpgradeModule::resetUpgrade(mask))
		return FALSE;
	if (getDoCommandUpgradeModuleData()->m_upgradeMuxData.m_permanent)
	{
		setUpgradeExecuted(TRUE);
		return FALSE;
	}
	issueCommand(getDoCommandUpgradeModuleData()->m_removeCommand);
	return TRUE;
}

void DoCommandUpgrade::crc(Xfer* xfer)
{
	UpgradeModule::crc(xfer);
}

void DoCommandUpgrade::xfer(Xfer* xfer)
{
	UpgradeModule::xfer(xfer);
}

void DoCommandUpgrade::loadPostProcess()
{
	UpgradeModule::loadPostProcess();
}
