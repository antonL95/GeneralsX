#include "PreRTS.h"

#include <algorithm>
#include <vector>

#include "Common/RetailFileLayers.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/RetailHordeTransport.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"
#include "WWMath/wwmath.h"

namespace
{
void placeHordeAtPosition(Object* hordeObj, const Coord3D& anchorPos, Real angle)
{
	hordeObj->setPosition(&anchorPos);
	hordeObj->setOrientation(angle);
	const HordeContain* horde = HordeContain::get(hordeObj);
	if (!horde)
		return;
	for (Object* member : *horde->getContainedItemsList())
	{
		if (!member || member->isEffectivelyDead())
			continue;
		const Coord3D slotPos = horde->getMemberPosition(member->getID(), FALSE);
		member->setPosition(&slotPos);
		member->setOrientation(angle);
	}
}

void syncHordeToContainer(Object* container, Object* rider, Bool enclosing)
{
	if (!container || !rider)
		return;
	const Coord3D* pos = container->getPosition();
	rider->setPosition(pos);
	const HordeContain* horde = HordeContain::get(rider);
	if (!horde)
		return;
	const OpenContain* openContain = container->getContain() ? container->getContain()->asOpenContain() : nullptr;
	const auto* prefixes = openContain ? &openContain->getRetailOpenContainData()->m_passengerBonePrefixes : nullptr;
	struct BoneGroup
	{
		KindOfMaskType kindOf;
		std::vector<Coord3D> positions;
		std::size_t used = 0;
	};
	std::vector<BoneGroup> groups;
	if (!enclosing && prefixes)
	{
		const Int maxBones = horde->getContainCount();
		for (const auto& prefix : *prefixes)
		{
			BoneGroup group;
			group.kindOf = prefix.kindOf;
			group.positions.resize(maxBones);
			const Int found = container->getMultiLogicalBonePosition(
				prefix.bonePrefix.str(), maxBones, group.positions.data(), nullptr);
			if (found > 0)
			{
				group.positions.resize(found);
				groups.push_back(std::move(group));
			}
		}
	}
	for (Object* member : *horde->getContainedItemsList())
	{
		if (!member || member->isEffectivelyDead())
			continue;
		const Coord3D* memberPos = pos;
		for (BoneGroup& group : groups)
		{
			if (group.used == group.positions.size() || !member->isAnyKindOf(group.kindOf))
				continue;
			memberPos = &group.positions[group.used++];
			break;
		}
		member->setPosition(memberPos);
	}
}

Bool hasNonZeroOffset(const Coord3D& offset)
{
	return offset.x != 0.0f || offset.y != 0.0f || offset.z != 0.0f;
}

Coord3D transformOffsetToWorld(const Object* container, const Coord3D& offset)
{
	Vector3 local(offset.x, offset.y, offset.z);
	Vector3 world;
	const Matrix3D* mtx = container->getTransformMatrix();
	Matrix3D::Transform_Vector(*mtx, local, &world);
	Coord3D result;
	result.x = world.X;
	result.y = world.Y;
	result.z = TheTerrainLogic->getGroundHeight(world.X, world.Y);
	return result;
}
}

Bool damageRetailHordePassengers(Object* passenger, ObjectID source, Real fraction, DeathType deathType)
{
	const HordeContain* horde = RetailFileLayers::enabled() ? HordeContain::get(passenger) : nullptr;
	if (!horde)
		return FALSE;

	const ContainedItemsList members = *horde->getContainedItemsList();
	for (Object* member : members)
	{
		if (!member || member->isEffectivelyDead())
			continue;
		DamageInfo damage;
		damage.in.m_damageType = DAMAGE_UNRESISTABLE;
		damage.in.m_deathType = deathType;
		damage.in.m_sourceID = source;
		damage.in.m_amount = member->getBodyModule()->getMaxHealth() * fraction;
		member->attemptDamage(&damage);
		if (fraction == 1.0f && !member->isEffectivelyDead())
			member->kill();
	}
	return TRUE;
}

void HordeTransportContainModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	TransportContainModuleData::buildFieldParse(p);
}

HordeTransportContain::HordeTransportContain(Thing* thing, const ModuleData* data) : TransportContain(thing, data)
{
}

HordeTransportContain::~HordeTransportContain() = default;

Bool HordeTransportContain::isValidContainerFor(const Object* rider, Bool checkCapacity) const
{
	if (!RetailFileLayers::enabled() || !rider)
		return FALSE;
	return TransportContain::isValidContainerFor(rider, checkCapacity);
}

Bool HordeTransportContain::isEnclosingContainerFor(const Object* obj) const
{
	return getHordeTransportContainModuleData()->m_objectStatusOfContained.test(OBJECT_STATUS_ENCLOSED);
}

void HordeTransportContain::positionContainedHordes()
{
	Object* self = getObject();
	if (!self || self->isEffectivelyDead())
		return;
	for (Object* rider : getContainList())
	{
		if (!rider || rider->isEffectivelyDead())
			continue;
		syncHordeToContainer(self, rider, isEnclosingContainerFor(rider));
	}
}

void HordeTransportContain::releaseHordeMembers(Object* rider)
{
	const HordeContain* horde = HordeContain::get(rider);
	if (!horde)
		return;
	for (Object* member : *horde->getContainedItemsList())
	{
		if (!member)
			continue;
		member->clearDisabled(DISABLED_HELD);
		if (AIUpdateInterface* ai = member->getAI())
			ai->aiIdle(CMD_FROM_AI);
	}
	placeHordeAtPosition(rider, *rider->getPosition(), rider->getOrientation());
}

void HordeTransportContain::containReactToTransformChange()
{
	TransportContain::containReactToTransformChange();
	if (RetailFileLayers::enabled())
		positionContainedHordes();
}

UpdateSleepTime HordeTransportContain::update()
{
	const UpdateSleepTime result = TransportContain::update();
	if (RetailFileLayers::enabled())
		positionContainedHordes();
	return result;
}

#if RTS_GENERALS
void HordeTransportContain::onContaining(Object* rider)
{
	TransportContain::onContaining(rider);
#else
void HordeTransportContain::onContaining(Object* rider, Bool wasSelected)
{
	TransportContain::onContaining(rider, wasSelected);
#endif
	if (!RetailFileLayers::enabled() || !rider)
		return;
	if (const HordeContain* horde = HordeContain::get(rider))
	{
		for (Object* member : *horde->getContainedItemsList())
		{
			if (!member || member->isEffectivelyDead())
				continue;
			member->setDisabled(DISABLED_HELD);
		}
	}
	syncHordeToContainer(getObject(), rider, isEnclosingContainerFor(rider));
}

void HordeTransportContain::onRemoving(Object* rider)
{
	TransportContain::onRemoving(rider);
	if (!RetailFileLayers::enabled() || !rider)
		return;
	releaseHordeMembers(rider);
}

#if RTS_GENERALS
Bool HordeTransportContain::isPassengerAllowedToFire() const
{
	if (RetailFileLayers::enabled() &&
		getHordeTransportContainModuleData()->m_objectStatusOfContained.test(OBJECT_STATUS_CAN_ATTACK))
		return TRUE;
	return TransportContain::isPassengerAllowedToFire();
}
#else
Bool HordeTransportContain::isPassengerAllowedToFire(ObjectID id) const
{
	if (RetailFileLayers::enabled() &&
		getHordeTransportContainModuleData()->m_objectStatusOfContained.test(OBJECT_STATUS_CAN_ATTACK))
		return TRUE;
	return TransportContain::isPassengerAllowedToFire(id);
}
#endif

void HordeTransportContain::crc(Xfer* xfer)
{
	TransportContain::crc(xfer);
}

void HordeTransportContain::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	TransportContain::xfer(xfer);
}

void HordeTransportContain::loadPostProcess()
{
	TransportContain::loadPostProcess();
}

void HordeGarrisonContainModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	GarrisonContainModuleData::buildFieldParse(p);
	static const FieldParse fields[] = {
		{ "ExitDelay", INI::parseDurationUnsignedInt, nullptr, offsetof(HordeGarrisonContainModuleData, m_exitDelay) },
		{ "MaxHordeCapacity", INI::parseInt, nullptr, offsetof(HordeGarrisonContainModuleData, m_maxHordeCapacity) },
		{ "EntryOffset", INI::parseCoord3D, nullptr, offsetof(HordeGarrisonContainModuleData, m_entryOffset) },
		{ "EntryPosition", INI::parseCoord3D, nullptr, offsetof(HordeGarrisonContainModuleData, m_entryPosition) },
		{ "ExitOffset", INI::parseCoord3D, nullptr, offsetof(HordeGarrisonContainModuleData, m_exitOffset) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

HordeGarrisonContain::HordeGarrisonContain(Thing* thing, const ModuleData* data) : GarrisonContain(thing, data)
{
}

HordeGarrisonContain::~HordeGarrisonContain() = default;

Bool HordeGarrisonContain::isValidContainerFor(const Object* obj, Bool checkCapacity) const
{
	if (!RetailFileLayers::enabled() || !obj)
		return FALSE;
	const auto* data = getHordeGarrisonContainModuleData();
	if (checkCapacity && data->m_maxHordeCapacity > 0 && HordeContain::get(obj))
	{
		Int hordeCount = 0;
		for (const Object* existing : getContainList())
		{
			if (HordeContain::get(existing))
				++hordeCount;
		}
		if (hordeCount >= data->m_maxHordeCapacity)
			return FALSE;
	}
	return GarrisonContain::isValidContainerFor(obj, checkCapacity);
}

Bool HordeGarrisonContain::isEnclosingContainerFor(const Object* obj) const
{
	return getHordeGarrisonContainModuleData()->m_objectStatusOfContained.test(OBJECT_STATUS_ENCLOSED);
}

Bool HordeGarrisonContain::isExitBusy() const
{
	return TheGameLogic->getFrame() < m_frameExitNotBusy;
}

#if RTS_GENERALS
Bool HordeGarrisonContain::isPassengerAllowedToFire() const
{
	return getHordeGarrisonContainModuleData()->m_objectStatusOfContained.test(OBJECT_STATUS_CAN_ATTACK) &&
		GarrisonContain::isPassengerAllowedToFire();
}
#else
Bool HordeGarrisonContain::isPassengerAllowedToFire(ObjectID id) const
{
	return getHordeGarrisonContainModuleData()->m_objectStatusOfContained.test(OBJECT_STATUS_CAN_ATTACK) &&
		GarrisonContain::isPassengerAllowedToFire(id);
}
#endif

void HordeGarrisonContain::positionHordeAtContainer(Object* obj)
{
	syncHordeToContainer(getObject(), obj, isEnclosingContainerFor(obj));
}

void HordeGarrisonContain::releaseHordeMembers(Object* obj)
{
	const HordeContain* horde = HordeContain::get(obj);
	if (!horde)
		return;
	for (Object* member : *horde->getContainedItemsList())
	{
		if (!member)
			continue;
#if RTS_GENERALS
		member->clearWeaponBonusCondition(WEAPONBONUSCONDITION_GARRISONED);
		member->clearDisabled(DISABLED_HELD);
#endif
		if (AIUpdateInterface* ai = member->getAI())
			ai->aiIdle(CMD_FROM_AI);
		if (PhysicsBehavior* physics = member->getPhysics())
			physics->resetDynamicPhysics();
		member->clearModelConditionState(MODELCONDITION_MOVING);
	}
}

#if RTS_GENERALS
void HordeGarrisonContain::onContaining(Object* obj)
{
	GarrisonContain::onContaining(obj);
#else
void HordeGarrisonContain::onContaining(Object* obj, Bool wasSelected)
{
	GarrisonContain::onContaining(obj, wasSelected);
#endif
	if (!RetailFileLayers::enabled() || !obj)
		return;
	positionHordeAtContainer(obj);
#if RTS_GENERALS
	if (const HordeContain* horde = HordeContain::get(obj))
	{
		for (Object* member : *horde->getContainedItemsList())
		{
			member->setDisabled(DISABLED_HELD);
			member->setWeaponBonusCondition(WEAPONBONUSCONDITION_GARRISONED);
		}
	}
#endif
}

void HordeGarrisonContain::onRemoving(Object* obj)
{
	GarrisonContain::onRemoving(obj);
	if (!RetailFileLayers::enabled() || !obj)
		return;
	const auto* data = getHordeGarrisonContainModuleData();
	m_frameExitNotBusy = TheGameLogic->getFrame() + data->m_exitDelay;
	releaseHordeMembers(obj);
	if (getObject()->isEffectivelyDead() && !obj->isEffectivelyDead())
		placeHordeAtPosition(obj, transformOffsetToWorld(getObject(), data->m_exitOffset), getObject()->getOrientation());
}

void HordeGarrisonContain::exitObjectViaDoor(Object* exitObj, ExitDoorType exitDoor)
{
	if (!RetailFileLayers::enabled())
	{
		GarrisonContain::exitObjectViaDoor(exitObj, exitDoor);
		return;
	}
	if (!exitObj || exitObj->getContainedBy() != getObject())
		return;
	removeFromContain(exitObj);
	const auto* data = getHordeGarrisonContainModuleData();
	const Object* self = getObject();
	const Coord3D& offset = hasNonZeroOffset(data->m_exitOffset) ? data->m_exitOffset : data->m_entryOffset;
	Coord3D exitPos = transformOffsetToWorld(self, offset);
	if (!hasNonZeroOffset(offset))
	{
		if (AIUpdateInterface* ai = exitObj->getAI())
			TheAI->pathfinder()->adjustToPossibleDestination(exitObj, ai->getLocomotorSet(), &exitPos);
	}

	if (PhysicsBehavior* physics = exitObj->getPhysics())
		physics->resetDynamicPhysics();
	placeHordeAtPosition(exitObj, exitPos, self->getOrientation());
	TheAI->pathfinder()->addObjectToPathfindMap(exitObj);
	recalcApparentControllingPlayer();
}

void HordeGarrisonContain::xferRetailBlocks(Xfer* xfer, Bool present)
{
	RetailSaveBlocks::transferSection(xfer, xfer->getXferMode() == XFER_LOAD, present, {
		{ "horde-garrison-exit", [this](Xfer* x) { x->xferUnsignedInt(&m_frameExitNotBusy); }, [this] { m_frameExitNotBusy = 0; } }
	});
}

void HordeGarrisonContain::crc(Xfer* xfer)
{
	GarrisonContain::crc(xfer);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, TRUE);
}

void HordeGarrisonContain::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	GarrisonContain::xfer(xfer);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, version >= 1);
}

void HordeGarrisonContain::loadPostProcess()
{
	GarrisonContain::loadPostProcess();
}

void EvacuateDamageModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	DamageModuleData::buildFieldParse(p);
	static const FieldParse fields[] = {
		{ "WeaponThatCausesEvacuation", INI::parseAsciiString, nullptr, offsetof(EvacuateDamageModuleData, m_weaponThatCausesEvacuation) },
		{ "DamageTypeToTrack", INI::parseIndexList, DamageTypeFlags::getBitNames(), offsetof(EvacuateDamageModuleData, m_damageTypeToTrack) },
		{ "DamageToPanicThreshold", INI::parsePercentToReal, nullptr, offsetof(EvacuateDamageModuleData, m_damageToPanicThreshold) },
		{ "TrackingTimeSpan", INI::parseDurationUnsignedInt, nullptr, offsetof(EvacuateDamageModuleData, m_trackingTimeSpan) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

EvacuateDamage::EvacuateDamage(Thing* thing, const ModuleData* data) : DamageModule(thing, data)
{
}

EvacuateDamage::~EvacuateDamage() = default;

void EvacuateDamage::evacuateContainer()
{
	ContainModuleInterface* contain = getObject()->getContain();
	if (!contain || contain->getContainCount() == 0)
		return;
#if RTS_GENERALS
	contain->orderAllPassengersToExit(CMD_FROM_AI);
#else
	contain->orderAllPassengersToExit(CMD_FROM_AI, FALSE);
#endif
}

void EvacuateDamage::onDamage(DamageInfo* damageInfo)
{
	if (!RetailFileLayers::enabled() || !damageInfo)
		return;
	const auto* data = getEvacuateDamageModuleData();
	if (!data->m_weaponThatCausesEvacuation.isEmpty())
	{
		if (damageInfo->retailWeaponName.compareNoCase(data->m_weaponThatCausesEvacuation) == 0)
		{
			evacuateContainer();
			return;
		}
	}

	if (data->m_damageTypeToTrack < DAMAGE_NUM_TYPES &&
		damageInfo->in.m_damageType == data->m_damageTypeToTrack &&
		data->m_damageToPanicThreshold > 0.0f &&
		damageInfo->out.m_actualDamageDealt > 0.0f)
	{
		const BodyModuleInterface* body = getObject()->getBodyModule();
		if (!body || body->getMaxHealth() <= 0.0f)
			return;
		const UnsignedInt now = TheGameLogic->getFrame();
		m_trackedDamage.push_back({ now, damageInfo->out.m_actualDamageDealt });
		const UnsignedInt cutoff = now > data->m_trackingTimeSpan ? now - data->m_trackingTimeSpan : 0;
		m_trackedDamage.erase(
			std::remove_if(m_trackedDamage.begin(), m_trackedDamage.end(),
				[cutoff](const TrackedDamage& entry) { return entry.frame < cutoff; }),
			m_trackedDamage.end());
		Real total = 0.0f;
		for (const TrackedDamage& entry : m_trackedDamage)
			total += entry.amount;
		if (total >= body->getMaxHealth() * data->m_damageToPanicThreshold)
		{
			m_trackedDamage.clear();
			evacuateContainer();
		}
	}
}

void EvacuateDamage::xferRetailBlocks(Xfer* xfer, Bool present)
{
	RetailSaveBlocks::transferSection(xfer, xfer->getXferMode() == XFER_LOAD, present, {
		{ "evacuate-tracked", [this](Xfer* x) {
			UnsignedInt count = static_cast<UnsignedInt>(m_trackedDamage.size());
			x->xferUnsignedInt(&count);
			if (x->getXferMode() == XFER_LOAD)
				m_trackedDamage.resize(count);
			for (UnsignedInt i = 0; i < count; ++i)
			{
				x->xferUnsignedInt(&m_trackedDamage[i].frame);
				x->xferReal(&m_trackedDamage[i].amount);
			}
		}, [this] { m_trackedDamage.clear(); } }
	});
}

void EvacuateDamage::crc(Xfer* xfer)
{
	DamageModule::crc(xfer);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, TRUE);
}

void EvacuateDamage::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	DamageModule::xfer(xfer);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, version >= 1);
}

void EvacuateDamage::loadPostProcess()
{
	DamageModule::loadPostProcess();
}

void HordeTransportContainDamageModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	DamageModuleData::buildFieldParse(p);
}

HordeTransportContainDamage::HordeTransportContainDamage(Thing* thing, const ModuleData* data) : DamageModule(thing, data)
{
}

HordeTransportContainDamage::~HordeTransportContainDamage() = default;

void HordeTransportContainDamage::onDamage(DamageInfo* damageInfo)
{
	if (!RetailFileLayers::enabled() || !damageInfo || damageInfo->out.m_actualDamageDealt <= 0.0f)
		return;
	Object* self = getObject();
	const BodyModuleInterface* body = self ? self->getBodyModule() : nullptr;
	ContainModuleInterface* contain = self ? self->getContain() : nullptr;
	if (!body || !contain)
		return;
	const Real maxHealth = body->getMaxHealth();
	if (maxHealth <= 0.0f)
		return;
	const Real health = std::max(0.0f, body->getHealth());
	const Real oldHealth = std::min(maxHealth, health + damageInfo->out.m_actualDamageDealt);

	std::vector<ObjectID> victimsToKill;
	for (Object* passenger : *contain->getContainedItemsList())
	{
		if (!passenger || passenger->isEffectivelyDead())
			continue;
		const HordeContain* horde = HordeContain::get(passenger);
		if (!horde)
			continue;
		const Int currentCount = static_cast<Int>(horde->getContainCount());
		const Int maxCount = horde->getContainMax();
		if (currentCount <= 1 || maxCount <= 0)
			continue;
		const Int oldExpected = std::min(maxCount, static_cast<Int>(WWMath::Floor((oldHealth / maxHealth) * static_cast<Real>(maxCount))) + 1);
		const Int newExpected = std::min(maxCount, static_cast<Int>(WWMath::Floor((health / maxHealth) * static_cast<Real>(maxCount))) + 1);
		if (newExpected >= currentCount || oldExpected <= newExpected)
			continue;
		Int toKill = std::min(oldExpected - newExpected, currentCount - newExpected);
		for (Object* member : *horde->getContainedItemsList())
		{
			if (toKill <= 0)
				break;
			if (!member || member->isEffectivelyDead())
				continue;
			victimsToKill.push_back(member->getID());
			--toKill;
		}
	}
	for (ObjectID id : victimsToKill)
	{
		if (Object* victim = TheGameLogic->findObjectByID(id))
		{
			if (!victim->isEffectivelyDead())
				victim->kill();
		}
	}
}

void HordeTransportContainDamage::crc(Xfer* xfer)
{
	DamageModule::crc(xfer);
}

void HordeTransportContainDamage::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	DamageModule::xfer(xfer);
}

void HordeTransportContainDamage::loadPostProcess()
{
	DamageModule::loadPostProcess();
}
