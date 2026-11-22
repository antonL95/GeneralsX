#include "PreRTS.h"

#include "GameLogic/Module/RetailTransportTeleportModules.h"

#include <algorithm>

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/Team.h"
#include "Common/Xfer.h"
#include "GameClient/ControlBar.h"
#include "GameClient/FXList.h"
#include "GameLogic/AI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"
#include "WWMath/wwmath.h"

namespace
{
constexpr UnsignedInt MAX_SERIALIZED_ENTRIES = 256;

void teleportObjectWithMembers(Object* unit, const Coord3D& dest)
{
	if (!unit || unit->isEffectivelyDead() || unit->isDestroyed())
		return;
	const Coord3D oldPos = *unit->getPosition();
	const Real dx = dest.x - oldPos.x;
	const Real dy = dest.y - oldPos.y;
	unit->setPosition(&dest);
	if (AIUpdateInterface* ai = unit->getAI())
		ai->aiIdle(CMD_FROM_AI);
	if (unit->isKindOf(KINDOF_HORDE) && unit->getContain())
	{
		const ContainedItemsList* items = unit->getContain()->getContainedItemsList();
		if (items)
		{
			std::vector<Object*> members;
			for (Object* member : *items)
				if (member && !member->isEffectivelyDead() && !member->isDestroyed())
					members.push_back(member);
			std::sort(members.begin(), members.end(), [](const Object* a, const Object* b) { return a->getID() < b->getID(); });
			for (Object* member : members)
			{
				Coord3D memberPos = *member->getPosition();
				memberPos.x += dx;
				memberPos.y += dy;
				memberPos.z = TheTerrainLogic->getGroundHeight(memberPos.x, memberPos.y);
				member->setPosition(&memberPos);
				if (AIUpdateInterface* memberAI = member->getAI())
					memberAI->aiIdle(CMD_FROM_AI);
			}
		}
	}
}

void convertObjectOwnership(Object* victim, Team* newTeam)
{
	if (!victim || !newTeam || victim->isEffectivelyDead() || victim->isDestroyed())
		return;
	if (victim->isKindOf(KINDOF_HORDE) && victim->getContain())
	{
		victim->setTeam(newTeam);
		victim->handlePartitionCellMaintenance();
		if (AIUpdateInterface* ai = victim->getAI())
			ai->aiIdle(CMD_FROM_AI);
		const ContainedItemsList* items = victim->getContain()->getContainedItemsList();
		if (items)
		{
			std::vector<Object*> members;
			for (Object* member : *items)
				if (member && !member->isEffectivelyDead() && !member->isDestroyed())
					members.push_back(member);
			std::sort(members.begin(), members.end(), [](const Object* a, const Object* b) { return a->getID() < b->getID(); });
			for (Object* member : members)
			{
				member->setTeam(newTeam);
				member->handlePartitionCellMaintenance();
				if (AIUpdateInterface* memberAI = member->getAI())
					memberAI->aiIdle(CMD_FROM_AI);
			}
		}
		return;
	}
	victim->defect(newTeam, 0);
}

Bool isValidTeleportPoint(const Coord3D& pos, Bool waterOk)
{
	Region3D extent;
	TheTerrainLogic->getExtent(&extent);
	if (pos.x < extent.lo.x || pos.x > extent.hi.x || pos.y < extent.lo.y || pos.y > extent.hi.y)
		return FALSE;
	if (!waterOk && TheTerrainLogic->isUnderwater(pos.x, pos.y))
		return FALSE;
	return TRUE;
}

Bool hasObjectFilter(const SpecialPowerObjectFilter& filter)
{
	return filter.m_rules != 0 || filter.m_includeKinds.any() || filter.m_excludeKinds.any() ||
		!filter.m_includeObjects.empty() || !filter.m_excludeObjects.empty();
}
}

void DominateEnemySpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] = {
		{ "AttributeModifierAffects", [](INI* ini, void* instance, void* store, const void* data) {
			SpecialPowerObjectFilter::parse(ini, instance, store, data);
			static_cast<DominateEnemySpecialPowerModuleData*>(instance)->m_hasFilter = TRUE;
		}, nullptr, offsetof(DominateEnemySpecialPowerModuleData, m_filter) },
		{ "DominateRadius", INI::parseReal, nullptr, offsetof(DominateEnemySpecialPowerModuleData, m_dominateRadius) },
		{ "DominatedFX", INI::parseFXList, nullptr, offsetof(DominateEnemySpecialPowerModuleData, m_dominatedFX) },
		{ "TriggerFX", INI::parseFXList, nullptr, offsetof(DominateEnemySpecialPowerModuleData, m_triggerFX) },
		{ "PermanentlyConvert", INI::parseBool, nullptr, offsetof(DominateEnemySpecialPowerModuleData, m_permanentlyConvert) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

DominateEnemySpecialPower::~DominateEnemySpecialPower() = default;

Bool DominateEnemySpecialPower::matchesDominateFilter(const Object* victim) const
{
	const Object* self = getObject();
	if (!victim || victim == self || victim->isEffectivelyDead() || victim->isDestroyed())
		return FALSE;
	if (victim->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) || victim->testStatus(OBJECT_STATUS_SOLD))
		return FALSE;
	if (self->getRelationship(victim) == ALLIES)
		return FALSE;
	const auto* data = getDominateEnemySpecialPowerModuleData();
	if (data->m_hasFilter && !data->m_filter.matchesObject(self, victim))
		return FALSE;
	const SpecialPowerTemplate* spTemplate = getTemplate();
	if (spTemplate && spTemplate->hasRetailFlag(SpecialPowerTemplate::RETAIL_NEEDS_OBJECT_FILTER) &&
		!spTemplate->getObjectFilter().matchesObject(self, victim))
		return FALSE;
	return TRUE;
}

Bool DominateEnemySpecialPower::acceptsIntent(const Object* target, const Coord3D* position) const
{
	if (!usesRotwkRules() || !SpecialAbilityUpdate::acceptsIntent(target, position))
		return FALSE;
	if (target)
	{
		const Object* effective = target;
		if (const Object* container = target->getContainedBy())
			if (container->isKindOf(KINDOF_HORDE))
				effective = container;
		return matchesDominateFilter(effective);
	}
	const auto* data = getDominateEnemySpecialPowerModuleData();
	const Real radius = data->m_dominateRadius > 0.0f ? data->m_dominateRadius : getTemplate()->getRadiusCursorRadius();
	return position != nullptr && radius > 0.0f;
}

void DominateEnemySpecialPower::revertExpiredEntries(UnsignedInt now)
{
	if (m_dominated.empty())
		return;
	auto it = m_dominated.begin();
	while (it != m_dominated.end())
	{
		if (it->revertFrame != 0 && now < it->revertFrame)
		{
			++it;
			continue;
		}
		Object* victim = TheGameLogic->findObjectByID(it->objectID);
		Team* originalTeam = TheTeamFactory ? TheTeamFactory->findTeamByID(it->originalTeamID) : nullptr;
		if (victim && originalTeam && !victim->isEffectivelyDead() && !victim->isDestroyed())
			convertObjectOwnership(victim, originalTeam);
		it = m_dominated.erase(it);
	}
}

UpdateSleepTime DominateEnemySpecialPower::update()
{
	const UpdateSleepTime baseSleep = SpecialAbilityUpdate::update();
	if (!m_dominated.empty())
	{
		const UnsignedInt now = TheGameLogic->getFrame();
		revertExpiredEntries(now);
		if (!m_dominated.empty())
		{
			UnsignedInt earliest = 0;
			for (const DominatedEntry& entry : m_dominated)
			{
				if (entry.revertFrame > now && (earliest == 0 || entry.revertFrame < earliest))
					earliest = entry.revertFrame;
			}
			if (earliest > now)
			{
				const UpdateSleepTime wait = UPDATE_SLEEP(earliest - now);
				return wait < baseSleep ? wait : baseSleep;
			}
		}
	}
	return baseSleep;
}

void DominateEnemySpecialPower::triggerAbilityEffect()
{
	SpecialAbilityUpdate::triggerAbilityEffect();
	Object* self = getObject();
	Player* owner = self->getControllingPlayer();
	Team* newTeam = owner ? owner->getDefaultTeam() : nullptr;
	if (!newTeam)
		return;
	const auto* data = getDominateEnemySpecialPowerModuleData();
	if (data->m_triggerFX)
		FXList::doFXObj(data->m_triggerFX, self);

	const Real radius = data->m_dominateRadius > 0.0f ? data->m_dominateRadius : getTemplate()->getRadiusCursorRadius();
	std::vector<Object*> candidates;
	if (getAbilityTargetID() != INVALID_ID && radius <= 0.0f)
	{
		if (Object* target = TheGameLogic->findObjectByID(getAbilityTargetID()))
			candidates.push_back(target);
	}
	else
	{
		Coord3D center = getAbilityTargetPos();
		if (getAbilityTargetID() != INVALID_ID)
		{
			if (Object* target = TheGameLogic->findObjectByID(getAbilityTargetID()))
				center = *target->getPosition();
		}
		if (radius > 0.0f)
		{
			SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(&center, radius, FROM_CENTER_2D);
			MemoryPoolObjectHolder holder(iter);
			for (Object* obj = iter->first(); obj; obj = iter->next())
				candidates.push_back(obj);
		}
	}

	std::sort(candidates.begin(), candidates.end(), [](const Object* a, const Object* b) { return a->getID() < b->getID(); });
	std::vector<ObjectID> seen;
	for (Object* candidate : candidates)
	{
		if (!candidate)
			continue;
		Object* victim = candidate;
		if (Object* container = candidate->getContainedBy())
			if (container->isKindOf(KINDOF_HORDE))
				victim = container;
		if (std::find(seen.begin(), seen.end(), victim->getID()) != seen.end())
			continue;
		seen.push_back(victim->getID());
		if (!matchesDominateFilter(victim))
			continue;
		const TeamID originalTeamID = victim->getTeam() ? victim->getTeam()->getID() : TEAM_ID_INVALID;
		convertObjectOwnership(victim, newTeam);
		if (data->m_dominatedFX)
			FXList::doFXObj(data->m_dominatedFX, victim);
		if (!data->m_permanentlyConvert && data->m_effectDuration > 0 && originalTeamID != TEAM_ID_INVALID &&
			m_dominated.size() < MAX_SERIALIZED_ENTRIES)
		{
			DominatedEntry entry;
			entry.objectID = victim->getID();
			entry.originalTeamID = originalTeamID;
			entry.revertFrame = TheGameLogic->getFrame() + data->m_effectDuration;
			m_dominated.push_back(entry);
		}
	}
}

void DominateEnemySpecialPower::xferDominateState(Xfer* xfer)
{
	UnsignedInt count = static_cast<UnsignedInt>(m_dominated.size());
	xfer->xferUnsignedInt(&count);
	if (xfer->getXferMode() == XFER_LOAD)
	{
		if (count > MAX_SERIALIZED_ENTRIES)
			count = MAX_SERIALIZED_ENTRIES;
		m_dominated.resize(count);
	}
	for (UnsignedInt i = 0; i < count; ++i)
	{
		xfer->xferObjectID(&m_dominated[i].objectID);
		xfer->xferUser(&m_dominated[i].originalTeamID, sizeof(TeamID));
		xfer->xferUnsignedInt(&m_dominated[i].revertFrame);
	}
}

void DominateEnemySpecialPower::crc(Xfer* xfer)
{
	SpecialAbilityUpdate::crc(xfer);
	xferDominateState(xfer);
}

void DominateEnemySpecialPower::xfer(Xfer* xfer)
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	SpecialAbilityUpdate::xfer(xfer);
	xferDominateState(xfer);
}

void DominateEnemySpecialPower::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}

void FlingPassengerSpecialAbilityUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] = {
		{ "FlingPassengerVelocity", INI::parseCoord3D, nullptr, offsetof(FlingPassengerSpecialAbilityUpdateModuleData, m_flingVelocity) },
		{ "FlingPassengerLandingWarhead", INI::parseWeaponTemplate, nullptr, offsetof(FlingPassengerSpecialAbilityUpdateModuleData, m_landingWarhead) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

FlingPassengerSpecialAbilityUpdate::~FlingPassengerSpecialAbilityUpdate() = default;

Bool FlingPassengerSpecialAbilityUpdate::acceptsIntent(const Object* target, const Coord3D* position) const
{
	if (!usesRotwkRules() || !SpecialAbilityUpdate::acceptsIntent(target, position))
		return FALSE;
	const ContainModuleInterface* contain = getObject()->getContain();
	return contain != nullptr && contain->getContainCount() > 0;
}

void FlingPassengerSpecialAbilityUpdate::triggerAbilityEffect()
{
	SpecialAbilityUpdate::triggerAbilityEffect();
	Object* self = getObject();
	ContainModuleInterface* contain = self->getContain();
	if (!contain || contain->getContainCount() == 0)
		return;
	const ContainedItemsList* items = contain->getContainedItemsList();
	if (!items || items->empty())
		return;
	Object* passenger = nullptr;
	for (Object* item : *items)
	{
		if (item && !item->isDestroyed())
		{
			passenger = item;
			break;
		}
	}
	if (!passenger)
		return;

	contain->removeFromContain(passenger, TRUE);

	const Coord3D& selfPos = *self->getPosition();
	const Coord3D& dir = *self->getUnitDirectionVector2D();
	const Real offset = self->getGeometryInfo().getMajorRadius() + passenger->getGeometryInfo().getMajorRadius();
	Coord3D dropPos;
	dropPos.x = selfPos.x + dir.x * offset;
	dropPos.y = selfPos.y + dir.y * offset;
	dropPos.z = TheTerrainLogic->getGroundHeight(dropPos.x, dropPos.y);
	passenger->setPosition(&dropPos);
	passenger->setOrientation(self->getOrientation());

	const auto* data = getFlingPassengerSpecialAbilityUpdateModuleData();
	const Coord3D& localVel = data->m_flingVelocity;
	if (localVel.x != 0.0f || localVel.y != 0.0f || localVel.z != 0.0f)
	{
		if (PhysicsBehavior* physics = passenger->getPhysics())
		{
			Coord3D worldVel;
			worldVel.x = dir.x * localVel.x - dir.y * localVel.y;
			worldVel.y = dir.y * localVel.x + dir.x * localVel.y;
			worldVel.z = localVel.z;
			physics->addVelocityTo(&worldVel);
		}
	}
	if (data->m_landingWarhead && TheWeaponStore && !passenger->isEffectivelyDead() && !passenger->isDestroyed())
		TheWeaponStore->createAndFireTempWeapon(data->m_landingWarhead, self, passenger, TRUE);
}

void FlingPassengerSpecialAbilityUpdate::crc(Xfer* xfer)
{
	SpecialAbilityUpdate::crc(xfer);
}

void FlingPassengerSpecialAbilityUpdate::xfer(Xfer* xfer)
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	SpecialAbilityUpdate::xfer(xfer);
}

void FlingPassengerSpecialAbilityUpdate::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}

void StoreObjectsSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] = {
		{ "Radius", INI::parseReal, nullptr, offsetof(StoreObjectsSpecialPowerModuleData, m_radius) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

StoreObjectsSpecialPower::~StoreObjectsSpecialPower() = default;

StoreObjectsSpecialPower* StoreObjectsSpecialPower::findForCaster(Object* caster, const SpecialPowerTemplate* teleportPower)
{
	if (!caster)
		return nullptr;
	static const NameKeyType storeKey = NAMEKEY("StoreObjectsSpecialPower");
	StoreObjectsSpecialPower* fallback = nullptr;
	for (BehaviorModule** module = caster->getBehaviorModules(); *module; ++module)
	{
		if ((*module)->getModuleNameKey() != storeKey)
			continue;
		auto* store = static_cast<StoreObjectsSpecialPower*>(*module);
		const AsciiString& chained = store->getStoreObjectsSpecialPowerModuleData()->m_chainedButton;
		if (teleportPower && TheRetailCommandCatalog && chained.isNotEmpty())
		{
			const CommandButton* btn = TheRetailCommandCatalog->findCommandButton(chained);
			if (btn && btn->getSpecialPowerTemplate() == teleportPower)
				return store;
		}
		if (!fallback || (fallback->getStoredObjects().empty() && !store->getStoredObjects().empty()))
			fallback = store;
	}
	return fallback;
}

Bool StoreObjectsSpecialPower::acceptsIntent(const Object* target, const Coord3D* position) const
{
	if (!usesRotwkRules() || !SpecialAbilityUpdate::acceptsIntent(target, position))
		return FALSE;
	return target != nullptr || position != nullptr;
}

void StoreObjectsSpecialPower::triggerAbilityEffect()
{
	SpecialAbilityUpdate::triggerAbilityEffect();
	Object* self = getObject();
	const auto* data = getStoreObjectsSpecialPowerModuleData();
	const SpecialPowerTemplate* spTemplate = getTemplate();
	const Real radius = data->m_radius > 0.0f ? data->m_radius : spTemplate->getRadiusCursorRadius();
	m_storedObjects.clear();
	if (radius <= 0.0f)
		return;

	Coord3D center = getAbilityTargetPos();
	if (getAbilityTargetID() != INVALID_ID)
	{
		if (Object* target = TheGameLogic->findObjectByID(getAbilityTargetID()))
			center = *target->getPosition();
	}

	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(&center, radius, FROM_CENTER_2D);
	MemoryPoolObjectHolder holder(iter);
	std::vector<Object*> candidates;
	for (Object* obj = iter->first(); obj; obj = iter->next())
		candidates.push_back(obj);
	std::sort(candidates.begin(), candidates.end(), [](const Object* a, const Object* b) { return a->getID() < b->getID(); });

	const Bool hasFilter = hasObjectFilter(spTemplate->getObjectFilter());
	for (Object* candidate : candidates)
	{
		if (!candidate || candidate == self || candidate->isEffectivelyDead() || candidate->isDestroyed())
			continue;
		if (candidate->isKindOf(KINDOF_INERT) || candidate->isKindOf(KINDOF_STRUCTURE) || candidate->isKindOf(KINDOF_IMMOBILE))
			continue;
		Object* effective = candidate;
		if (Object* container = candidate->getContainedBy())
		{
			if (!container->isKindOf(KINDOF_HORDE))
				continue;
			effective = container;
		}
		if (std::find(m_storedObjects.begin(), m_storedObjects.end(), effective->getID()) != m_storedObjects.end())
			continue;
		if (hasFilter && !spTemplate->getObjectFilter().matchesObject(self, effective))
			continue;
		if (m_storedObjects.size() >= MAX_SERIALIZED_ENTRIES)
			break;
		m_storedObjects.push_back(effective->getID());
	}
}

void StoreObjectsSpecialPower::xferStoredState(Xfer* xfer)
{
	UnsignedInt count = static_cast<UnsignedInt>(m_storedObjects.size());
	xfer->xferUnsignedInt(&count);
	if (xfer->getXferMode() == XFER_LOAD)
	{
		if (count > MAX_SERIALIZED_ENTRIES)
			count = MAX_SERIALIZED_ENTRIES;
		m_storedObjects.resize(count);
	}
	for (UnsignedInt i = 0; i < count; ++i)
		xfer->xferObjectID(&m_storedObjects[i]);
}

void StoreObjectsSpecialPower::crc(Xfer* xfer)
{
	SpecialAbilityUpdate::crc(xfer);
	xferStoredState(xfer);
}

void StoreObjectsSpecialPower::xfer(Xfer* xfer)
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	SpecialAbilityUpdate::xfer(xfer);
	xferStoredState(xfer);
}

void StoreObjectsSpecialPower::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}

void TeleportSpecialAbilityUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] = {
		{ "BusyForDuration", INI::parseDurationUnsignedInt, nullptr, offsetof(TeleportSpecialAbilityUpdateModuleData, m_busyForDuration) },
		{ "DestinationWeaponName", INI::parseWeaponTemplate, nullptr, offsetof(TeleportSpecialAbilityUpdateModuleData, m_destinationWeapon) },
		{ "SourceWeaponName", INI::parseWeaponTemplate, nullptr, offsetof(TeleportSpecialAbilityUpdateModuleData, m_sourceWeapon) },
		{ "MaxDistance", INI::parseReal, nullptr, offsetof(TeleportSpecialAbilityUpdateModuleData, m_maxDistance) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

TeleportSpecialAbilityUpdate::~TeleportSpecialAbilityUpdate() = default;

Bool TeleportSpecialAbilityUpdate::acceptsIntent(const Object* target, const Coord3D* position) const
{
	if (!usesRotwkRules() || !SpecialAbilityUpdate::acceptsIntent(target, position))
		return FALSE;
	const Coord3D* pos = position ? position : (target ? target->getPosition() : nullptr);
	if (!pos)
		return FALSE;
	const Bool waterOk = getTemplate()->hasRetailFlag(SpecialPowerTemplate::RETAIL_WATER_OK);
	return isValidTeleportPoint(*pos, waterOk);
}

UnsignedInt TeleportSpecialAbilityUpdate::holdFramesAfterTrigger() const
{
	return std::max(SpecialAbilityUpdate::holdFramesAfterTrigger(),
		getTeleportSpecialAbilityUpdateModuleData()->m_busyForDuration);
}

void TeleportSpecialAbilityUpdate::triggerAbilityEffect()
{
	SpecialAbilityUpdate::triggerAbilityEffect();
	Object* self = getObject();
	const auto* data = getTeleportSpecialAbilityUpdateModuleData();
	const Coord3D oldPos = *self->getPosition();
	Coord3D dest = getAbilityTargetPos();
	if (getAbilityTargetID() != INVALID_ID)
	{
		if (Object* target = TheGameLogic->findObjectByID(getAbilityTargetID()))
			dest = *target->getPosition();
	}
	if (data->m_maxDistance > 0.0f)
	{
		const Real dx = dest.x - oldPos.x;
		const Real dy = dest.y - oldPos.y;
		const Real dist = WWMath::Sqrt(dx * dx + dy * dy);
		if (dist > data->m_maxDistance && dist > 0.0f)
		{
			const Real scale = data->m_maxDistance / dist;
			dest.x = oldPos.x + dx * scale;
			dest.y = oldPos.y + dy * scale;
		}
	}
	dest.z = TheTerrainLogic->getGroundHeight(dest.x, dest.y);

	if (data->m_sourceWeapon && TheWeaponStore)
		TheWeaponStore->createAndFireTempWeapon(data->m_sourceWeapon, self, &oldPos);
	teleportObjectWithMembers(self, dest);
	if (data->m_destinationWeapon && TheWeaponStore)
		TheWeaponStore->createAndFireTempWeapon(data->m_destinationWeapon, self, &dest);
}

void TeleportSpecialAbilityUpdate::crc(Xfer* xfer)
{
	SpecialAbilityUpdate::crc(xfer);
}

void TeleportSpecialAbilityUpdate::xfer(Xfer* xfer)
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	SpecialAbilityUpdate::xfer(xfer);
}

void TeleportSpecialAbilityUpdate::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}

void TeleportToCasterSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] = {
		{ "Radius", INI::parseReal, nullptr, offsetof(TeleportToCasterSpecialPowerModuleData, m_radius) },
		{ "TargetFX", INI::parseFXList, nullptr, offsetof(TeleportToCasterSpecialPowerModuleData, m_targetFX) },
		{ "TriggerFX", INI::parseFXList, nullptr, offsetof(TeleportToCasterSpecialPowerModuleData, m_triggerFX) },
		{ "MinDestinationRadius", INI::parseReal, nullptr, offsetof(TeleportToCasterSpecialPowerModuleData, m_minDestinationRadius) },
		{ "MaxDestinationRadius", INI::parseReal, nullptr, offsetof(TeleportToCasterSpecialPowerModuleData, m_maxDestinationRadius) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

TeleportToCasterSpecialPower::~TeleportToCasterSpecialPower() = default;

Bool TeleportToCasterSpecialPower::acceptsIntent(const Object* target, const Coord3D* position) const
{
	if (!usesRotwkRules() || !SpecialAbilityUpdate::acceptsIntent(target, position))
		return FALSE;
	const Coord3D* pos = position ? position : (target ? target->getPosition() : nullptr);
	if (!pos)
		return FALSE;
	const Bool waterOk = getTemplate()->hasRetailFlag(SpecialPowerTemplate::RETAIL_WATER_OK);
	if (!isValidTeleportPoint(*pos, waterOk))
		return FALSE;
	if (getTemplate()->getSpecialPowerType() == SPECIAL_TELEPORT_LIST_TO_POSITION)
	{
		const StoreObjectsSpecialPower* store = StoreObjectsSpecialPower::findForCaster(
			const_cast<Object*>(getObject()), getTemplate());
		if (!store)
			return FALSE;
		for (const ObjectID id : store->getStoredObjects())
		{
			const Object* obj = TheGameLogic->findObjectByID(id);
			if (obj && !obj->isEffectivelyDead() && !obj->isDestroyed())
				return TRUE;
		}
		return FALSE;
	}
	return TRUE;
}

void TeleportToCasterSpecialPower::triggerAbilityEffect()
{
	SpecialAbilityUpdate::triggerAbilityEffect();
	Object* self = getObject();
	const auto* data = getTeleportToCasterSpecialPowerModuleData();
	const SpecialPowerTemplate* spTemplate = getTemplate();
	const Bool fromStoredList = spTemplate->getSpecialPowerType() == SPECIAL_TELEPORT_LIST_TO_POSITION;

	Coord3D targetCenter = getAbilityTargetPos();
	if (getAbilityTargetID() != INVALID_ID)
	{
		if (Object* target = TheGameLogic->findObjectByID(getAbilityTargetID()))
			targetCenter = *target->getPosition();
	}

	std::vector<Object*> units;
	Coord3D destCenter = *self->getPosition();
	if (fromStoredList)
	{
		destCenter = targetCenter;
		StoreObjectsSpecialPower* store = StoreObjectsSpecialPower::findForCaster(self, spTemplate);
		if (store)
		{
			for (const ObjectID id : store->getStoredObjects())
			{
				Object* obj = TheGameLogic->findObjectByID(id);
				if (obj && !obj->isEffectivelyDead() && !obj->isDestroyed())
					units.push_back(obj);
			}
			store->clearStoredObjects();
		}
	}
	else
	{
		const Real radius = data->m_radius > 0.0f ? data->m_radius : spTemplate->getRadiusCursorRadius();
		if (radius > 0.0f)
		{
			SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(&targetCenter, radius, FROM_CENTER_2D);
			MemoryPoolObjectHolder holder(iter);
			std::vector<Object*> candidates;
			for (Object* obj = iter->first(); obj; obj = iter->next())
				candidates.push_back(obj);
			std::sort(candidates.begin(), candidates.end(), [](const Object* a, const Object* b) { return a->getID() < b->getID(); });
			const Bool hasFilter = hasObjectFilter(spTemplate->getObjectFilter());
			std::vector<ObjectID> seen;
			for (Object* candidate : candidates)
			{
				if (!candidate || candidate == self || candidate->isEffectivelyDead() || candidate->isDestroyed())
					continue;
				if (candidate->isKindOf(KINDOF_INERT) || candidate->isKindOf(KINDOF_STRUCTURE) || candidate->isKindOf(KINDOF_IMMOBILE))
					continue;
				Object* effective = candidate;
				if (Object* container = candidate->getContainedBy())
				{
					if (!container->isKindOf(KINDOF_HORDE))
						continue;
					effective = container;
				}
				if (std::find(seen.begin(), seen.end(), effective->getID()) != seen.end())
					continue;
				seen.push_back(effective->getID());
				if (hasFilter && !spTemplate->getObjectFilter().matchesObject(self, effective))
					continue;
				units.push_back(effective);
			}
		}
	}

	if (data->m_triggerFX)
		FXList::doFXPos(data->m_triggerFX, &destCenter);

	const Real minR = std::max(0.0f, data->m_minDestinationRadius);
	const Real maxR = std::max(minR, data->m_maxDestinationRadius);
	const Real ringR = minR + (maxR - minR) * 0.5f;
	const size_t total = units.size();
	for (size_t i = 0; i < total; ++i)
	{
		Object* unit = units[i];
		const Coord3D oldPos = *unit->getPosition();
		const Real angle = total > 0 ? (2.0f * PI * static_cast<Real>(i)) / static_cast<Real>(total) : 0.0f;
		Coord3D dest;
		dest.x = destCenter.x + ringR * WWMath::CosTrig(angle);
		dest.y = destCenter.y + ringR * WWMath::SinTrig(angle);
		dest.z = TheTerrainLogic->getGroundHeight(dest.x, dest.y);
		if (data->m_targetFX)
			FXList::doFXPos(data->m_targetFX, &oldPos);
		teleportObjectWithMembers(unit, dest);
		if (data->m_targetFX)
			FXList::doFXPos(data->m_targetFX, &dest);
	}
}

void TeleportToCasterSpecialPower::crc(Xfer* xfer)
{
	SpecialAbilityUpdate::crc(xfer);
}

void TeleportToCasterSpecialPower::xfer(Xfer* xfer)
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	SpecialAbilityUpdate::xfer(xfer);
}

void TeleportToCasterSpecialPower::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}
