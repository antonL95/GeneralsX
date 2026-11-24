#include "PreRTS.h"

#include "Common/GameUtility.h"
#include "Common/Player.h"
#include "GameClient/RetailEva.h"
#include "GameLogic/Module/RemoveUpgradeUpgrade.h"
#include "GameLogic/Object.h"

namespace
{
void removeObjectUpgrade(Object* object, void* data)
{
	const auto* upgrade = static_cast<const UpgradeTemplate*>(data);
	if (upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER || object->hasUpgrade(upgrade))
		object->removeUpgrade(upgrade);
}

void announceRemoval(const Player* owner, const UpgradeTemplate* upgrade)
{
	Player* viewer = rts::getObservedOrLocalPlayer_Safe();
	if (!viewer)
		return;
	if (viewer == owner)
		RetailEva::queue(viewer, upgrade->getLocalPlayerLosesUpgradeEvaEvent());
	else if (viewer->getRelationship(owner->getDefaultTeam()) == ALLIES)
		RetailEva::queue(viewer, upgrade->getAlliedPlayerLosesUpgradeEvaEvent());
	else
		RetailEva::queue(viewer, upgrade->getEnemyPlayerLosesUpgradeEvaEvent());
}
}

void RemoveUpgradeUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "UpgradeToRemove", INI::parseAsciiStringVector, nullptr, offsetof(RemoveUpgradeUpgradeModuleData, m_upgrades) },
		{ "UpgradeGroupsToRemove", INI::parseAsciiStringVector, nullptr, offsetof(RemoveUpgradeUpgradeModuleData, m_groups) },
		{ "SuppressEvaEventForRemoval", INI::parseBool, nullptr, offsetof(RemoveUpgradeUpgradeModuleData, m_suppressEva) },
		{ "RemoveFromAllPlayerObjects", INI::parseBool, nullptr, offsetof(RemoveUpgradeUpgradeModuleData, m_removeFromAllObjects) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

RemoveUpgradeUpgrade::RemoveUpgradeUpgrade(Thing* thing, const ModuleData* moduleData)
	: UpgradeModule(thing, moduleData)
{
}

RemoveUpgradeUpgrade::~RemoveUpgradeUpgrade() = default;

void RemoveUpgradeUpgrade::upgradeImplementation()
{
	const auto* data = getRemoveUpgradeUpgradeModuleData();
	Object* object = getObject();
	Player* owner = object->getControllingPlayer();
	for (const AsciiString& name : data->m_upgrades)
	{
		const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(name);
		if (!upgrade)
			continue;
		if (upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER)
		{
			const Bool completed = owner->hasUpgradeComplete(upgrade);
			owner->removeUpgrade(upgrade);
			if (completed)
			{
				owner->iterateObjects(removeObjectUpgrade, const_cast<UpgradeTemplate*>(upgrade));
				if (!data->m_suppressEva)
					announceRemoval(owner, upgrade);
			}
		}
		else
		{
			object->removeUpgrade(upgrade);
			if (data->m_removeFromAllObjects)
				owner->iterateObjects(removeObjectUpgrade, const_cast<UpgradeTemplate*>(upgrade));
		}
	}

	if (data->m_groups.empty())
		return;
	UpgradeMaskType triggers, conflicts;
	getUpgradeActivationMasks(triggers, conflicts);
	for (const UpgradeTemplate* upgrade = TheUpgradeCenter->firstUpgradeTemplate(); upgrade; upgrade = upgrade->friend_getNext())
	{
		if (!object->hasUpgrade(upgrade) || triggers.testForAny(upgrade->getUpgradeMask()))
			continue;
		for (const AsciiString& group : data->m_groups)
		{
			if (group.compareNoCase(upgrade->getGroupName()) == 0)
			{
				object->removeUpgrade(upgrade);
				break;
			}
		}
	}
}

void RemoveUpgradeUpgrade::crc(Xfer* xfer)
{
	UpgradeModule::crc(xfer);
}

void RemoveUpgradeUpgrade::xfer(Xfer* xfer)
{
	UpgradeModule::xfer(xfer);
}

void RemoveUpgradeUpgrade::loadPostProcess()
{
	UpgradeModule::loadPostProcess();
}
