#include "PreRTS.h"

#include "GameLogic/Module/GrabPassengerSpecialPower.h"

#include "Common/INI.h"
#include "Common/INIException.h"
#include "Common/SpecialPower.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"

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

Bool GrabPassengerSpecialPower::canGrab(const Object* target) const
{
	const Object* caster = getObject();
	const auto* data = static_cast<const GrabPassengerSpecialPowerModuleData*>(getModuleData());
	const ContainModuleInterface* contain = caster->getContain();
	if (!target || target == caster || target->isEffectivelyDead() || !contain)
		return FALSE;
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
	if (!position || m_pausedCount > 0 || getObject()->isDisabled())
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
	Object* caster = getObject();
	if (m_pausedCount > 0 || caster->isDisabled())
		return;
	const Object* target = findGrabTarget(requested);
	if (!target)
		return;

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
		FXList::doFXObj(TheFXListStore->findFXList(getPowerModuleData()->m_initiateFX.str()), caster);
}

void GrabPassengerSpecialPower::applyPowerEffect(const Coord3D*)
{
	Object* caster = getObject();
	Object* target = TheGameLogic->findObjectByID(m_target);
	m_target = INVALID_ID;
	if (!canGrab(target))
		return;

	if (Object* horde = target->getContainedBy())
		horde->getContain()->removeFromContain(target);
	caster->getContain()->addToContain(target);
	if (target->getContainedBy() != caster || target->isKindOf(KINDOF_CLUB))
		return;

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
