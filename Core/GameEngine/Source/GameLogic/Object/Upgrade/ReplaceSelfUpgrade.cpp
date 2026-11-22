#include "PreRTS.h"

#include "Common/BuildAssistant.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/CreateModule.h"
#include "GameLogic/Module/GeometryUpgrade.h"
#include "GameLogic/Module/ReplaceSelfUpgrade.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"

namespace
{
	Bool isWallPiece(const Object* object)
	{
		return object->isKindOf(KINDOF_WALL_SEGMENT) || object->isKindOf(KINDOF_WALL_GATE) ||
			object->isKindOf(KINDOF_WALL_HUB) || object->isKindOf(KINDOF_DEFENSIVE_WALL);
	}

	Bool fitsWallGate(const Object& source, const ThingTemplate& gate)
	{
		const Coord3D& center = *source.getPosition();
		const Real angle = source.getOrientation();
		const auto& data = gate.getRotWKObjectData();
		const auto& offset = data.primaryGeometry.offset;
		const Bool extended = !data.additionalGeometries.empty() || offset.x != 0.0f || offset.y != 0.0f || offset.z != 0.0f;
		PartitionFilterWouldCollide primary(center, gate.getTemplateGeometryInfo(), angle, TRUE);
		for (Object* other = TheGameLogic->getFirstObject(); other; other = other->getNextObject())
		{
			if (other == &source || !other->isKindOf(KINDOF_STRUCTURE) || other->isKindOf(KINDOF_NO_COLLIDE) ||
				other->isDestroyed() || other->isEffectivelyDead())
				continue;
			if (extended ? GeometryUpgrade::constructionFootprintsOverlap(gate, center, angle, *other) : primary.allow(other))
				return FALSE;
		}
		return TRUE;
	}
}

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
		const ThingTemplate* definition = TheThingFactory->findTemplate(name, FALSE);
		const auto* type = definition ? static_cast<const ThingTemplate*>(definition->getFinalOverride()) : nullptr;
		if (!type || type->isKindOf(KINDOF_DRAWABLE_ONLY))
			return FALSE;
		sites.push_back({type, *source->getPosition()});
		offset += type->getTemplateGeometryInfo().getMajorRadius();
	}

	if (sites.size() == 1 && isWallPiece(source))
		return !RetailFileLayers::enabled() || !sites.front().type->isKindOf(KINDOF_BLOCKING_GATE) || fitsWallGate(*source, *sites.front().type);

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

const ThingTemplate* ReplaceSelfUpgrade::findReplacementTemplate(const Object* obj, const UpgradeTemplate* upgrade)
{
	static const NameKeyType key = NAMEKEY("ReplaceSelfUpgrade");
	if (!obj || !upgrade || !TheThingFactory)
		return nullptr;
	const UpgradeMaskType mask = upgrade->getUpgradeMask();
	for (BehaviorModule** module = const_cast<Object*>(obj)->getBehaviorModules(); *module; ++module)
	{
		if ((*module)->getModuleNameKey() != key)
			continue;
		const auto* replace = static_cast<const ReplaceSelfUpgrade*>(*module);
		UpgradeMaskType activation;
		UpgradeMaskType conflicting;
		replace->getUpgradeActivationMasks(activation, conflicting);
		if (!activation.testForAny(mask))
			continue;
		const auto& replacements = replace->getReplaceSelfUpgradeModuleData()->m_replacements;
		if (!replacements.empty())
		{
			if (const ThingTemplate* tmpl = TheThingFactory->findTemplate(replacements.front()))
				return tmpl;
		}
	}
	return nullptr;
}

void ReplaceSelfUpgrade::upgradeImplementation()
{
	std::vector<Site> sites;
	if (!findSites(sites))
		return;

	Object* source = getObject();
	const AsciiString name = source->getName();
	const Matrix3D transform = *source->getTransformMatrix();
	const Real angle = source->getOrientation();
	const Bool inPlace = sites.size() == 1 && isWallPiece(source);
	Team* team = source->getTeam();
	TheAI->pathfinder()->removeObjectFromPathfindMap(source);
	TheGameLogic->destroyObject(source);

	const auto construction = MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNDER_CONSTRUCTION);
	for (const Site& site : sites)
	{
		const Bool structure = site.type->isKindOf(KINDOF_STRUCTURE);
		Object* replacement = TheThingFactory->newObjectFromExactTemplate(site.type, team,
			inPlace && structure ? construction : OBJECT_STATUS_MASK_NONE);
		if (inPlace)
		{
			replacement->setTransformMatrix(&transform);
			if (structure)
				replacement->clearStatus(construction);
		}
		else
		{
			replacement->setPosition(&site.position);
			replacement->setOrientation(angle);
		}
		if (&site == &sites.front())
			replacement->setName(name);
		for (BehaviorModule** module = replacement->getBehaviorModules(); *module; ++module)
		{
			CreateModuleInterface* create = (*module)->getCreate();
			if (create && (!inPlace || create->shouldDoOnBuildComplete()))
				create->onBuildComplete();
		}
		if (structure && replacement->getControllingPlayer())
			replacement->getControllingPlayer()->onStructureConstructionComplete(nullptr, replacement, TRUE);
		else if (inPlace)
			TheAI->pathfinder()->addObjectToPathfindMap(replacement);
	}
}

void ReplaceSelfUpgrade::crc(Xfer* xfer)
{
	UpgradeModule::crc(xfer);
}

void ReplaceSelfUpgrade::xfer(Xfer* xfer)
{
	if (isWallPiece(getObject()))
	{
		XferVersion version = 1;
		xfer->xferVersion(&version, 1);
	}
	UpgradeModule::xfer(xfer);
}

void ReplaceSelfUpgrade::loadPostProcess()
{
	UpgradeModule::loadPostProcess();
}
