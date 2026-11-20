#include "PreRTS.h"

#include "GameLogic/Module/HordeAIUpdate.h"

#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/RetailTestCommands.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponSet.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
Real distanceSquared(const Coord3D& a, const Coord3D& b)
{
	const Real x = a.x - b.x;
	const Real y = a.y - b.y;
	return x * x + y * y;
}

Object* liveTarget(ObjectID id)
{
	Object* target = id != INVALID_ID ? TheGameLogic->findObjectByID(id) : nullptr;
	return target && !target->isEffectivelyDead() ? target : nullptr;
}

Object* enemyHordeOrUnit(Object* target)
{
	if (Object* anchor = HordeContain::getAnchor(target))
		return anchor;
	return target;
}

Bool isGuardOrder(AICommandType order)
{
	return order == AICMD_GUARD_POSITION || order == AICMD_GUARD_OBJECT || order == AICMD_GUARD_AREA;
}

class HordeGuardFilter final : public PartitionFilter
{
public:
	HordeGuardFilter(const Coord3D& center, Real range, const PolygonTrigger* area, GuardMode mode)
		: m_center(center), m_rangeSquared(range * range), m_area(area), m_mode(mode)
	{
	}

	Bool allow(Object* object) override
	{
		if (m_mode == GUARDMODE_GUARD_FLYING_UNITS_ONLY && !object->isAirborneTarget())
			return FALSE;
		if (m_area)
		{
			const Coord3D& pos = *object->getPosition();
			ICoord3D point{ (Int)pos.x, (Int)pos.y, (Int)pos.z };
			return m_area->pointInTrigger(point);
		}
		return distanceSquared(m_center, *object->getPosition()) <= m_rangeSquared;
	}

#if defined(RTS_DEBUG)
	const char* debugGetName() override { return "HordeGuardFilter"; }
#endif

private:
	Coord3D m_center;
	Real m_rangeSquared;
	const PolygonTrigger* m_area;
	GuardMode m_mode;
};
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

void HordeAIUpdate::retaliate(Object* victim, Object* attacker)
{
	Object* anchor = HordeContain::get(victim) ? victim : HordeContain::getAnchor(victim);
	if (!anchor || !attacker || attacker->getRelationship(anchor) != ENEMIES)
		return;
	AIUpdateInterface* ai = anchor->getAI();
	if (ai && ai->getModuleNameKey() == NAMEKEY("HordeAIUpdate"))
	{
		const AICommandParms parms(AICMD_ATTACK_OBJECT, CMD_FROM_AI);
		auto* hordeAI = static_cast<HordeAIUpdate*>(ai);
		if (hordeAI->isAllowedToRespondToAiCommands(&parms))
			hordeAI->respondToAttack(attacker, NO_MAX_SHOTS_LIMIT, CMD_FROM_AI);
	}
}

void HordeAIUpdate::respondToAttack(Object* attacker, Int maxShots, CommandSourceType source)
{
	if (!attacker || attacker->isEffectivelyDead() || liveTarget(m_attackTarget) ||
		m_order == AICMD_ATTACK_POSITION || (m_order == NO_ORDER && !isIdle()))
		return;
	Object* target = enemyHordeOrUnit(attacker);
	if (target->isEffectivelyDead() || !getObject()->isAbleToAttack())
		return;

	m_attackTarget = target->getID();
	if (m_order == NO_ORDER)
	{
		m_attackSource = source;
		m_maxShots = maxShots;
		m_forceAttack = FALSE;
	}
	m_engaging = FALSE;
	privateIdle(m_attackSource);
	if (RetailTestCommands::tracesCombat())
	{
		std::fprintf(stderr, "TEST horde retaliate %u %u %u\n", TheGameLogic->getFrame(),
			static_cast<unsigned>(getObject()->getID()), static_cast<unsigned>(target->getID()));
		std::fflush(stderr);
	}
}

void HordeAIUpdate::aiDoCommand(const AICommandParms* parms)
{
	if (!isAllowedToRespondToAiCommands(parms))
		return;

	const auto command = parms->m_cmd;
#if RTS_ZEROHOUR
	if (command == AICMD_GUARD_RETALIATE)
	{
		respondToAttack(parms->m_obj, parms->m_intValue, parms->m_cmdSource);
		return;
	}
#endif

	if (command == AICMD_ATTACK_OBJECT || command == AICMD_FORCE_ATTACK_OBJECT)
	{
		if (!parms->m_obj || parms->m_obj->isEffectivelyDead() || parms->m_obj == getObject())
			return;
		if (m_updatingAnchor && (m_attackTarget != INVALID_ID ||
			(m_order != NO_ORDER && m_order != AICMD_ATTACKMOVE_TO_POSITION)))
			return;

		Object* target = m_updatingAnchor ? enemyHordeOrUnit(parms->m_obj) : parms->m_obj;
		if (!m_updatingAnchor || m_order == NO_ORDER)
		{
			m_order = NO_ORDER;
			m_attackSource = parms->m_cmdSource;
			m_maxShots = parms->m_intValue;
			m_forceAttack = command == AICMD_FORCE_ATTACK_OBJECT;
		}
		m_attackTarget = target->getID();
		m_engaging = FALSE;
		privateIdle(m_attackSource);
		return;
	}

	m_order = NO_ORDER;
	m_attackTarget = INVALID_ID;
	m_engaging = FALSE;
	if (isGuardOrder(command) || command == AICMD_ATTACKMOVE_TO_POSITION ||
		command == AICMD_ATTACK_POSITION || command == AICMD_HUNT)
	{
		m_order = command;
		m_orderPosition = parms->m_pos;
		m_guardObject = command == AICMD_GUARD_OBJECT && parms->m_obj ? parms->m_obj->getID() : INVALID_ID;
		m_guardArea = command == AICMD_GUARD_AREA && parms->m_polygon ? parms->m_polygon->getID() : -1;
		m_guardMode = isGuardOrder(command) ? (GuardMode)parms->m_intValue : GUARDMODE_NORMAL;
		m_attackSource = parms->m_cmdSource;
		m_maxShots = isGuardOrder(command) || command == AICMD_HUNT ? NO_MAX_SHOTS_LIMIT : parms->m_intValue;
		m_forceAttack = FALSE;
		privateIdle(m_attackSource);
		return;
	}

	AIUpdateInterface::aiDoCommand(parms);
}

Object* HordeAIUpdate::updateOrder(Object* anchor, Object* target, Bool refresh)
{
	if (m_order == NO_ORDER || m_order == AICMD_ATTACK_POSITION)
		return target;

	Coord3D position = m_orderPosition;
	const Bool guard = isGuardOrder(m_order);
	const PolygonTrigger* area = m_order == AICMD_GUARD_AREA && m_guardArea >= 0
		? PolygonTrigger::getPolygonTriggerByID(m_guardArea) : nullptr;
	const Object* guardObject = m_order == AICMD_GUARD_OBJECT ? liveTarget(m_guardObject) : nullptr;
	if ((m_order == AICMD_GUARD_OBJECT && !guardObject) || (m_order == AICMD_GUARD_AREA && !area))
	{
		m_order = NO_ORDER;
		m_attackTarget = INVALID_ID;
		m_engaging = FALSE;
		privateIdle(m_attackSource);
		return nullptr;
	}
	if (area)
		area->getCenterPoint(&position);
	else if (guardObject)
		position = *guardObject->getPosition();

	const Real vision = guard ? AI::getAdjustedVisionRangeForObject(anchor, AI_VISIONFACTOR_OWNERTYPE | AI_VISIONFACTOR_MOOD) : 0.0f;
	HordeGuardFilter filter(position, vision, area, m_guardMode);
	if (target && guard && m_guardMode == GUARDMODE_GUARD_WITHOUT_PURSUIT && !filter.allow(target))
	{
		m_attackTarget = INVALID_ID;
		m_engaging = FALSE;
		privateIdle(m_attackSource);
		target = nullptr;
	}
	if (target || !refresh)
		return target;

	switch (m_order)
	{
		case AICMD_ATTACKMOVE_TO_POSITION:
			target = getNextMoodTarget(TRUE, FALSE);
			break;
		case AICMD_HUNT:
			target = TheAI->findClosestEnemy(anchor, 9999.9f, AI::CAN_ATTACK, getAttackInfo());
			break;
		default:
		{
			const Real range = area ? area->getRadius() : vision;
			const Coord3D& origin = *anchor->getPosition();
			const Real rangeFromAnchor = range + std::abs(origin.x - position.x) + std::abs(origin.y - position.y);
			target = TheAI->findClosestEnemy(anchor, rangeFromAnchor, AI::CAN_ATTACK | AI::ATTACK_BUILDINGS, getAttackInfo(), &filter);
			break;
		}
	}
	if (target)
	{
		target = enemyHordeOrUnit(target);
		m_attackTarget = target->getID();
		return target;
	}

	if (m_order != AICMD_HUNT)
	{
		if (distanceSquared(*anchor->getPosition(), position) > 16.0f)
		{
			if (isIdle() || distanceSquared(*getGoalPosition(), position) > 16.0f)
				privateMoveToPosition(&position, m_attackSource);
		}
		else if (m_order == AICMD_ATTACKMOVE_TO_POSITION)
			m_order = NO_ORDER;
	}
	return nullptr;
}

void HordeAIUpdate::approach(const Coord3D& position, Real range)
{
	m_engaging = distanceSquared(*getObject()->getPosition(), position) <= range * range;
	if (m_engaging && isMoving())
		privateIdle(m_attackSource);
	else if (!m_engaging && (isIdle() || distanceSquared(*getGoalPosition(), position) > 100.0f))
		privateMoveToPosition(&position, m_attackSource);
}

UpdateSleepTime HordeAIUpdate::update()
{
	m_updatingAnchor = TRUE;
	AIUpdateInterface::update();
	m_updatingAnchor = FALSE;

	Object* horde = getObject();
	const HordeContain* contain = HordeContain::get(horde);
	if (!contain || horde->isEffectivelyDead())
		return UPDATE_SLEEP_NONE;

	Object* target = liveTarget(m_attackTarget);
	if (!target && m_attackTarget != INVALID_ID)
	{
		m_attackTarget = INVALID_ID;
		m_engaging = FALSE;
		privateIdle(m_attackSource);
	}

	const Bool refresh = (TheGameLogic->getFrame() + horde->getID()) % 8 == 0;
	target = updateOrder(horde, target, refresh);
	const Bool attackGround = m_order == AICMD_ATTACK_POSITION;
	const Bool wasEngaging = m_engaging;
	const auto& members = *contain->getContainedItemsList();
	if ((target || attackGround) && !m_engaging && (refresh || isIdle()))
	{
		Real approachRange = 0.0f;
		for (const Object* member : members)
		{
			if (const Weapon* weapon = member->getCurrentWeapon();
				weapon && !member->isEffectivelyDead() && contain->rankCanAttack(member))
			{
				approachRange = std::max(approachRange, weapon->getAttackRange(member));
			}
		}

		const auto* data = contain->getHordeContainModuleData();
		const Real contactDistance = std::max(8.0f, std::max(data->m_meleeAttackLeashDistance, data->m_meleeBehavior.innerRange));
		const Real targetRadius = target ? target->getGeometryInfo().getBoundingCircleRadius() : 0.0f;
		approach(target ? *target->getPosition() : m_orderPosition, std::max(approachRange, contactDistance) + targetRadius);
	}

	const HordeContain* enemy = HordeContain::get(target);
	const auto attackType = m_forceAttack ? ATTACK_NEW_TARGET_FORCED : ATTACK_NEW_TARGET;
	Bool groundAttackInProgress = FALSE;
	for (Object* member : members)
	{
		AIUpdateInterface* ai = member->getAI();
		if (!ai || member->isEffectivelyDead())
			continue;

		if (m_engaging && contain->rankCanAttack(member))
		{
			if (target)
			{
				const Object* current = ai->getGoalObject();
				if (ai->isAttacking() && current && !current->isEffectivelyDead() &&
					(enemy ? current->getContainedBy() == target : current == target))
					continue;
				const Bool attackingAnchor = enemy && current == target && ai->isAttacking();
				if (attackingAnchor)
					ai->aiIdle(CMD_FROM_SCRIPT);
				if ((refresh || !wasEngaging || attackingAnchor) && member->isAbleToAttack())
				{
					CanAttackResult result;
					Object* victim = target;
					if (enemy)
						victim = enemy->findAttackTarget(member, attackType, m_attackSource, (WeaponSlotType)-1, &result);
					else
						result = member->getAbleToAttackSpecificObject(attackType, target, m_attackSource);
					if (result == ATTACKRESULT_POSSIBLE || result == ATTACKRESULT_POSSIBLE_AFTER_MOVING)
					{
						if (m_forceAttack)
							ai->aiForceAttackObject(victim, m_maxShots, m_attackSource);
						else
							ai->aiAttackObject(victim, m_maxShots, m_attackSource);
					}
				}
				continue;
			}
			if (attackGround)
			{
				if (!wasEngaging && member->isAbleToAttack())
				{
					const auto result = member->getAbleToUseWeaponAgainstTarget(ATTACK_NEW_TARGET_FORCED, nullptr,
						&m_orderPosition, m_attackSource);
					if (result == ATTACKRESULT_POSSIBLE || result == ATTACKRESULT_POSSIBLE_AFTER_MOVING)
						ai->aiAttackPosition(&m_orderPosition, m_maxShots, m_attackSource);
				}
				groundAttackInProgress |= ai->isAttacking();
				continue;
			}
		}

		const Coord3D slot = contain->getMemberPosition(member->getID(), isMoving() && !m_engaging);
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

	if (attackGround && m_engaging && !groundAttackInProgress)
	{
		m_order = NO_ORDER;
		m_engaging = FALSE;
	}
	return UPDATE_SLEEP_NONE;
}

void HordeAIUpdate::xferOrder(Xfer* xfer)
{
	xfer->xferUser(&m_order, sizeof(m_order));
	xfer->xferCoord3D(&m_orderPosition);
	xfer->xferObjectID(&m_guardObject);
	xfer->xferInt(&m_guardArea);
	xfer->xferUser(&m_guardMode, sizeof(m_guardMode));
}

void HordeAIUpdate::crc(Xfer* xfer)
{
	AIUpdateInterface::crc(xfer);
	xfer->xferObjectID(&m_attackTarget);
	xfer->xferUser(&m_attackSource, sizeof(m_attackSource));
	xfer->xferInt(&m_maxShots);
	xfer->xferBool(&m_forceAttack);
	xfer->xferBool(&m_engaging);
	xferOrder(xfer);
}

void HordeAIUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 2;
	xfer->xferVersion(&version, 2);
	AIUpdateInterface::xfer(xfer);
	xfer->xferObjectID(&m_attackTarget);
	xfer->xferUser(&m_attackSource, sizeof(m_attackSource));
	xfer->xferInt(&m_maxShots);
	xfer->xferBool(&m_forceAttack);
	xfer->xferBool(&m_engaging);
	if (version >= 2)
		xferOrder(xfer);
}

void HordeAIUpdate::loadPostProcess()
{
	AIUpdateInterface::loadPostProcess();
}
