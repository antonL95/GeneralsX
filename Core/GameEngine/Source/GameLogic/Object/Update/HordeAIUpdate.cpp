#include "PreRTS.h"

#include "GameLogic/Module/HordeAIUpdate.h"

#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponSet.h"
#include "Lib/trig.h"

#include <algorithm>

namespace
{
Real distanceSquared(const Coord3D& a, const Coord3D& b)
{
	const Real x = a.x - b.x;
	const Real y = a.y - b.y;
	return x * x + y * y;
}
}

void HordeAIUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse fields[] =
	{
		{ "ComboLocomotorSet", INI::parseAsciiString, nullptr, offsetof(HordeAIUpdateModuleData, m_comboLocomotorSet) },
		{ "ComboLocoAttackDistance", INI::parseReal, nullptr, offsetof(HordeAIUpdateModuleData, m_comboLocoAttackDistance) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	AIUpdateModuleData::buildFieldParse(p);
	p.add(fields);
}

HordeAIUpdate::HordeAIUpdate(Thing* thing, const ModuleData* data) : AIUpdateInterface(thing, data)
{
}

HordeAIUpdate::~HordeAIUpdate() = default;

void HordeAIUpdate::aiDoCommand(const AICommandParms* parms)
{
	if (!isAllowedToRespondToAiCommands(parms))
		return;

	if (parms->m_cmd == AICMD_ATTACK_OBJECT || parms->m_cmd == AICMD_FORCE_ATTACK_OBJECT)
	{
		if (!parms->m_obj || parms->m_obj->isEffectivelyDead())
			return;
		m_attackTarget = parms->m_obj->getID();
		m_attackSource = parms->m_cmdSource;
		m_maxShots = parms->m_intValue;
		m_forceAttack = parms->m_cmd == AICMD_FORCE_ATTACK_OBJECT;
		m_engaging = FALSE;
		privateIdle(m_attackSource);
		return;
	}

	m_attackTarget = INVALID_ID;
	m_engaging = FALSE;
	AIUpdateInterface::aiDoCommand(parms);
}

UpdateSleepTime HordeAIUpdate::update()
{
	AIUpdateInterface::update();
	Object* horde = getObject();
	const HordeContain* contain = HordeContain::get(horde);
	if (!contain || horde->isEffectivelyDead())
		return UPDATE_SLEEP_NONE;

	Object* target = TheGameLogic->findObjectByID(m_attackTarget);
	if (target && target->isEffectivelyDead())
		target = nullptr;
	if (!target && m_attackTarget != INVALID_ID)
	{
		m_attackTarget = INVALID_ID;
		m_engaging = FALSE;
		privateIdle(m_attackSource);
	}

	const auto& members = *contain->getContainedItemsList();
	const Real leash = contain->getHordeContainModuleData()->m_meleeAttackLeashDistance;
	const Bool refresh = (TheGameLogic->getFrame() + horde->getID()) % 8 == 0;
	if (target && (refresh || isIdle()))
	{
		Real approachRange = 0.0f;
		for (const Object* member : members)
		{
			if (!member->isEffectivelyDead() && contain->rankCanAttack(member))
			{
				const Weapon* weapon = member->getCurrentWeapon();
				const Real range = weapon ? weapon->getAttackRange(member) : 0.0f;
				approachRange = std::max(approachRange, range);
			}
		}
		const auto* data = contain->getHordeContainModuleData();
		const Real contactDistance = std::max(8.0f, std::max(leash, data->m_meleeBehavior.innerRange));
		const Real distance = std::max(approachRange, contactDistance);
		const Real range = distance + target->getGeometryInfo().getBoundingCircleRadius();
		m_engaging = distanceSquared(*horde->getPosition(), *target->getPosition()) <= range * range;
		if (m_engaging && isMoving())
			privateIdle(m_attackSource);
		else if (!m_engaging && (isIdle() || distanceSquared(*getGoalPosition(), *target->getPosition()) > 100.0f))
			privateMoveToPosition(target->getPosition(), m_attackSource);
	}

	for (Object* member : members)
	{
		AIUpdateInterface* ai = member->getAI();
		if (!ai || member->isEffectivelyDead())
			continue;

		const Coord3D slot = contain->getMemberPosition(member->getID(), isMoving() && !m_engaging);
		if (target && m_engaging && contain->rankCanAttack(member))
		{
			const Weapon* weapon = member->getCurrentWeapon();
			const Real range = (weapon ? weapon->getAttackRange(member) : 0.0f) +
				std::max(0.0f, leash) + target->getGeometryInfo().getBoundingCircleRadius();
			if (leash <= 0.0f || distanceSquared(slot, *target->getPosition()) <= range * range)
			{
				if (ai->getGoalObject() == target && ai->isAttacking())
					continue;
				if (refresh && member->isAbleToAttack())
				{
					const auto attackType = m_forceAttack ? ATTACK_NEW_TARGET_FORCED : ATTACK_NEW_TARGET;
					const auto result = member->getAbleToAttackSpecificObject(attackType, target, m_attackSource);
					if (result == ATTACKRESULT_POSSIBLE || result == ATTACKRESULT_POSSIBLE_AFTER_MOVING)
					{
						if (m_forceAttack)
							ai->aiForceAttackObject(target, m_maxShots, m_attackSource);
						else
							ai->aiAttackObject(target, m_maxShots, m_attackSource);
						continue;
					}
				}
			}
		}

		const Bool wasAttacking = ai->isAttacking();
		if (distanceSquared(*member->getPosition(), slot) > 16.0f)
		{
			if (wasAttacking || ((refresh || ai->isIdle()) &&
				distanceSquared(*ai->getGoalPosition(), slot) > 16.0f))
				ai->aiMoveToPositionEvenIfSleeping(&slot, CMD_FROM_SCRIPT);
		}
		else if (wasAttacking)
			ai->aiIdle(CMD_FROM_SCRIPT);
	}
	return UPDATE_SLEEP_NONE;
}

void HordeAIUpdate::crc(Xfer* xfer)
{
	AIUpdateInterface::crc(xfer);
	xfer->xferObjectID(&m_attackTarget);
	xfer->xferUser(&m_attackSource, sizeof(m_attackSource));
	xfer->xferInt(&m_maxShots);
	xfer->xferBool(&m_forceAttack);
	xfer->xferBool(&m_engaging);
}

void HordeAIUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	AIUpdateInterface::xfer(xfer);
	xfer->xferObjectID(&m_attackTarget);
	xfer->xferUser(&m_attackSource, sizeof(m_attackSource));
	xfer->xferInt(&m_maxShots);
	xfer->xferBool(&m_forceAttack);
	xfer->xferBool(&m_engaging);
}

void HordeAIUpdate::loadPostProcess()
{
	AIUpdateInterface::loadPostProcess();
}
