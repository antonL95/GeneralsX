#include "PreRTS.h"

#include "GameLogic/Module/GrabPassengerSpecialPower.h"

#include "Common/INI.h"
#include "Common/INIException.h"
#include "Common/RetailFileLayers.h"
#include "Common/SpecialPower.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#if !RTS_GENERALS
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ThingFactory.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/TransportContain.h"
#endif

void GrabPassengerSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& fields)
{
	SpecialPowerModuleData::buildFieldParse(fields);
	static const FieldParse table[] =
	{
		{ "GrabRadius", INI::parseReal, nullptr, offsetof(GrabPassengerSpecialPowerModuleData, m_grabRadius) },
		{ "AllowTree", INI::parseBool, nullptr, offsetof(GrabPassengerSpecialPowerModuleData, m_allowTree) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	fields.add(table);
}

ModuleData* GrabPassengerSpecialPower::friend_newModuleData(INI* ini)
{
	auto data = std::unique_ptr<GrabPassengerSpecialPowerModuleData>(MSGNEW("AllModuleData") GrabPassengerSpecialPowerModuleData);
	if (ini)
	{
		ini->initFromINIMultiProc(data.get(), GrabPassengerSpecialPowerModuleData::buildFieldParse);
		if (!data->m_specialPowerTemplate)
			throw INIException("GrabPassengerSpecialPower requires a defined SpecialPowerTemplate");
	}
	return data.release();
}

GrabPassengerSpecialPower::~GrabPassengerSpecialPower() = default;

#if !RTS_GENERALS
static Bool isGrabVendor(const Object* target, SpecialPowerType power)
{
	return RetailFileLayers::enabled() &&
		((power == SPECIAL_GRAB_CHUNK && target->isKindOf(KINDOF_CHUNK_VENDOR)) ||
		 (power == SPECIAL_GRAB_PASSENGER && target->isKindOf(KINDOF_ROCK_VENDOR)));
}

static const ActiveBodyModuleData* grabVendorData(const Object* target)
{
	const auto* body = dynamic_cast<const ActiveBody*>(target->getBodyModule());
	return body ? body->getRetailBodyData() : nullptr;
}
#endif

GrabPassengerSpecialPower* GrabPassengerSpecialPower::findUsable(Object* object, const Object* target)
{
	for (BehaviorModule** module = object->getBehaviorModules(); *module; ++module)
	{
		if ((*module)->getModuleNameKey() == NAMEKEY("GrabPassengerSpecialPower"))
		{
			auto* grab = static_cast<GrabPassengerSpecialPower*>(*module);
			if ((!target || grab->isValidPowerTarget(target)) &&
				TheSpecialPowerStore->canUseSpecialPower(object, grab->getSpecialPowerTemplate()))
				return grab;
		}
	}
	return nullptr;
}

Bool GrabPassengerSpecialPower::canGrab(const Object* target) const
{
	const Object* caster = getObject();
	const auto* data = static_cast<const GrabPassengerSpecialPowerModuleData*>(getModuleData());
	const ContainModuleInterface* contain = caster->getContain();
	if (!target || target == caster || target->isEffectivelyDead() || !contain)
		return FALSE;
#if !RTS_GENERALS
	if (isGrabVendor(target, getSpecialPowerTemplate()->getSpecialPowerType()))
	{
		const auto* vendor = grabVendorData(target);
		const auto* transport = dynamic_cast<const TransportContain*>(contain);
		if (!vendor || vendor->m_grabObject.isEmpty() || !transport || contain->isHordeContain())
			return FALSE;
		const ThingTemplate* product = TheThingFactory->findTemplate(vendor->m_grabObject, FALSE);
		return product && transport->canCarryNewPassenger(*product, *ThePlayerList->getNeutralPlayer()->getDefaultTeam());
	}
#endif
	if (target->isContained() && !target->getContainedBy()->isKindOf(KINDOF_HORDE))
		return FALSE;
	if (!data->m_allowTree && (target->isKindOf(KINDOF_TREE) || target->isKindOf(KINDOF_SHRUBBERY)))
		return FALSE;
	return contain->isValidContainerFor(target, TRUE);
}

void GrabPassengerSpecialPower::doSpecialPower(UnsignedInt commandOptions)
{
	doSpecialPowerAtLocation(getObject()->getPosition(), INVALID_ANGLE, commandOptions);
}

void GrabPassengerSpecialPower::doSpecialPowerAtLocation(const Coord3D* position, Real, UnsignedInt commandOptions)
{
	m_lastCastAccepted = FALSE;
	if (!position || m_pausedCount > 0 || getObject()->isDisabled() || !isRetailCastAllowed(commandOptions))
		return;

	const auto* data = static_cast<const GrabPassengerSpecialPowerModuleData*>(getModuleData());
	const Real radius = data->m_grabRadius;
	SimpleObjectIterator* objects = ThePartitionManager->iterateObjectsInRange(position, radius, FROM_CENTER_2D,
		nullptr, ITER_SORTED_NEAR_TO_FAR);
	MemoryPoolObjectHolder hold(objects);
	for (Object* target = objects->first(); target; target = objects->next())
	{
		if (canGrab(target))
		{
			doSpecialPowerAtObject(target, commandOptions);
			return;
		}
	}
}

const Object* GrabPassengerSpecialPower::findGrabTarget(const Object* target) const
{
	if (!target)
		return nullptr;
	const Object* caster = getObject();
	if (target->isKindOf(KINDOF_HORDE) && target->getContain())
	{
		const ContainedItemsList* members = target->getContain()->getContainedItemsList();
		target = nullptr;
		Real distance = 0.0f;
		if (members)
		{
			for (Object* member : *members)
			{
				if (!canGrab(member))
					continue;
				const Real d = ThePartitionManager->getDistanceSquared(caster, member, FROM_CENTER_2D);
				if (!target || d < distance)
				{
					target = member;
					distance = d;
				}
			}
		}
	}
	return canGrab(target) ? target : nullptr;
}

Bool GrabPassengerSpecialPower::isValidPowerTarget(const Object* target) const
{
	return findGrabTarget(target) != nullptr;
}

void GrabPassengerSpecialPower::doSpecialPowerAtObject(Object* requested, UnsignedInt commandOptions)
{
	requestGrab(requested, commandOptions);
}

Bool GrabPassengerSpecialPower::requestGrab(Object* requested, UnsignedInt commandOptions)
{
	m_lastCastAccepted = FALSE;
	Object* caster = getObject();
	if (m_pausedCount > 0 || caster->isDisabled() || !isRetailCastAllowed(commandOptions))
		return FALSE;
	const Object* target = findGrabTarget(requested);
	if (!target)
		return FALSE;

	const ObjectID previousTarget = m_target;
	m_target = target->getID();
	const Bool accepted = initiateIntentToDoSpecialPower(target, nullptr, nullptr, commandOptions);
	if (getPowerModuleData()->m_updateModuleStartsAttack)
	{
		if (!accepted)
			m_target = previousTarget;
	}
	else
	{
		triggerSpecialPower(target->getPosition());
	}
	if (accepted && TheFXListStore)
	{
		const FXList* fx = TheFXListStore->findFXList(getPowerModuleData()->m_initiateFX.str());
		if (RetailFileLayers::enabled())
			FXList::doFXPos(fx, caster->getPosition(), caster->getTransformMatrix(), 0.0f,
				target->getPosition(), 0.0f, caster, target);
		else
			FXList::doFXObj(fx, caster);
	}
	m_lastCastAccepted = accepted || !getPowerModuleData()->m_updateModuleStartsAttack;
	return m_lastCastAccepted;
}

void GrabPassengerSpecialPower::applyPowerEffect(const Coord3D*)
{
	Object* caster = getObject();
	Object* target = TheGameLogic->findObjectByID(m_target);
	m_target = INVALID_ID;
	if (!canGrab(target))
		return;

#if !RTS_GENERALS
	if (isGrabVendor(target, getSpecialPowerTemplate()->getSpecialPowerType()))
	{
		const auto* vendor = grabVendorData(target);
		const ThingTemplate* chunkTemplate = TheThingFactory->findTemplate(vendor->m_grabObject, FALSE);
		Object* chunk = TheThingFactory->newObject(chunkTemplate, ThePlayerList->getNeutralPlayer()->getDefaultTeam());
		if (!chunk)
			return;
		chunk->setProducer(caster);
		const Coord2D& offset = vendor->m_grabOffset;
		Vector3 world;
		Matrix3D::Transform_Vector(*caster->getTransformMatrix(), Vector3(offset.x, offset.y, 0.0f), &world);
		const Coord3D position{world.X, world.Y, world.Z};
		chunk->setPosition(&position);
		ContainModuleInterface* contain = caster->getContain();
		if (contain->isValidContainerFor(chunk, TRUE))
			contain->addToContain(chunk);
		if (chunk->getContainedBy() != caster)
		{
			TheGameLogic->destroyObject(chunk);
			return;
		}
		DamageInfo damage;
		damage.in.m_sourceID = caster->getID();
		damage.in.m_damageType = DAMAGE_STRUCTURAL;
		damage.in.m_amount = vendor->m_grabDamage;
		target->attemptDamage(&damage);
		FXList::doFXPos(vendor->m_grabFX, &position);
		return;
	}
#endif

	if (Object* horde = target->getContainedBy())
		horde->getContain()->removeFromContain(target);
	caster->getContain()->addToContain(target);
	if (target->getContainedBy() != caster || target->isKindOf(KINDOF_CLUB))
		return;
#if !RTS_GENERALS
	if (RetailFileLayers::enabled() && target->isKindOf(KINDOF_ROCK))
		return;
#endif

	const SpecialAbilityUpdate* update = caster->findSpecialAbilityUpdate(getSpecialPowerTemplate()->getSpecialPowerType());
	if (update)
		caster->schedulePassengerConsumption(target->getID(), update->getGrabPassengerAnimation(), update->getGrabPassengerHealFraction());
}

void GrabPassengerSpecialPower::crc(Xfer* xfer)
{
	SpecialPowerModule::crc(xfer);
	xfer->xferObjectID(&m_target);
}

void GrabPassengerSpecialPower::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	SpecialPowerModule::xfer(xfer);
	xfer->xferObjectID(&m_target);
}

void GrabPassengerSpecialPower::loadPostProcess()
{
	SpecialPowerModule::loadPostProcess();
}
