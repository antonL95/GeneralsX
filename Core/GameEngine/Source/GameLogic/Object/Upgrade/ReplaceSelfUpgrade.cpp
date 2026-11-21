#include "PreRTS.h"

#include "Common/BuildAssistant.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Module/ReplaceSelfUpgrade.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"

void ReplaceSelfUpgradeModuleData::buildFieldParse(MultiIniFieldParse& fields)
{
	UpgradeModuleData::buildFieldParse(fields);
	static const FieldParse replacements[] =
	{
		{ "ReplaceWith", INI::parseAsciiStringVectorAppend, nullptr, offsetof(ReplaceSelfUpgradeModuleData, m_replacements) },
		{ "AndThenAddA", INI::parseAsciiStringVectorAppend, nullptr, offsetof(ReplaceSelfUpgradeModuleData, m_replacements) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	fields.add(replacements);
}

ReplaceSelfUpgrade::~ReplaceSelfUpgrade() = default;

Bool ReplaceSelfUpgrade::findSites(std::vector<Site>& sites) const
{
	const Object* source = getObject();
	const auto& replacements = getReplaceSelfUpgradeModuleData()->m_replacements;
	if (replacements.empty() || source->isDestroyed() || source->isEffectivelyDead())
		return FALSE;

	Real offset = 0.0f;
	for (const AsciiString& name : replacements)
	{
		const ThingTemplate* type = TheThingFactory->findTemplate(name, FALSE);
		if (!type)
			return FALSE;
		sites.push_back({type, *source->getPosition()});
		offset += type->getTemplateGeometryInfo().getMajorRadius();
	}

	const Vector3 axis = source->getTransformMatrix()->Get_Y_Vector();
	for (Site& site : sites)
	{
		const GeometryInfo& geometry = site.type->getTemplateGeometryInfo();
		offset -= geometry.getMajorRadius();
		site.position.x += axis.X * offset;
		site.position.y += axis.Y * offset;
		site.position.z += axis.Z * offset;
		offset -= geometry.getMajorRadius();

		if (TheBuildAssistant->isLocationLegalToBuild(&site.position, site.type, source->getOrientation(),
			BuildAssistant::TERRAIN_RESTRICTIONS, source, source->getControllingPlayer()) != LBC_OK)
			return FALSE;
		if (!site.type->isKindOf(KINDOF_STRUCTURE))
			continue;
		ObjectIterator* collisions = ThePartitionManager->iteratePotentialCollisions(&site.position, geometry, source->getOrientation());
		MemoryPoolObjectHolder hold(collisions);
		for (const Object* other = collisions->first(); other; other = collisions->next())
		{
			if (other != source && other->isKindOf(KINDOF_STRUCTURE) && !other->isDestroyed() && !other->isEffectivelyDead())
				return FALSE;
		}
	}
	return TRUE;
}

Bool ReplaceSelfUpgrade::wouldUpgrade(const UpgradeMaskType& mask) const
{
	if (!UpgradeModule::wouldUpgrade(mask))
		return FALSE;
	std::vector<Site> sites;
	return findSites(sites);
}

Bool ReplaceSelfUpgrade::blocks(const UpgradeMaskType& mask, const UpgradeMaskType& upgrade) const
{
	UpgradeMaskType activation, conflicting;
	getUpgradeActivationMasks(activation, conflicting);
	if (!activation.testForAny(upgrade) || !UpgradeModule::wouldUpgrade(mask))
		return FALSE;
	std::vector<Site> sites;
	return !findSites(sites);
}

Bool ReplaceSelfUpgrade::isBlocked(const Object* obj, const UpgradeTemplate* upgrade)
{
	static const NameKeyType key = NAMEKEY("ReplaceSelfUpgrade");
	if (!obj || !upgrade || upgrade->getUpgradeType() != UPGRADE_TYPE_OBJECT)
		return FALSE;
	UpgradeMaskType mask = obj->getControllingPlayer()->getCompletedUpgradeMask();
	mask.set(obj->getObjectCompletedUpgradeMask());
	mask.set(upgrade->getUpgradeMask());
	for (BehaviorModule** module = const_cast<Object*>(obj)->getBehaviorModules(); *module; ++module)
	{
		if ((*module)->getModuleNameKey() == key && static_cast<const ReplaceSelfUpgrade*>(*module)->blocks(mask, upgrade->getUpgradeMask()))
			return TRUE;
	}
	return FALSE;
}

void ReplaceSelfUpgrade::upgradeImplementation()
{
	std::vector<Site> sites;
	if (!findSites(sites))
		return;

	Object* source = getObject();
	const AsciiString name = source->getName();
	const Real angle = source->getOrientation();
	Team* team = source->getTeam();
	TheAI->pathfinder()->removeObjectFromPathfindMap(source);
	TheGameLogic->destroyObject(source);

	for (const Site& site : sites)
	{
		Object* replacement = TheThingFactory->newObjectFromExactTemplate(site.type, team, OBJECT_STATUS_MASK_NONE);
		replacement->setPosition(&site.position);
		replacement->setOrientation(angle);
		if (&site == &sites.front())
			replacement->setName(name);
		for (BehaviorModule** module = replacement->getBehaviorModules(); *module; ++module)
		{
			if (CreateModuleInterface* create = (*module)->getCreate())
				create->onBuildComplete();
		}
		if (replacement->isKindOf(KINDOF_STRUCTURE) && replacement->getControllingPlayer())
			replacement->getControllingPlayer()->onStructureConstructionComplete(nullptr, replacement, TRUE);
	}
}

void ReplaceSelfUpgrade::crc(Xfer* xfer)
{
	UpgradeModule::crc(xfer);
}

void ReplaceSelfUpgrade::xfer(Xfer* xfer)
{
	UpgradeModule::xfer(xfer);
}

void ReplaceSelfUpgrade::loadPostProcess()
{
	UpgradeModule::loadPostProcess();
}
