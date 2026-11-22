#include "PreRTS.h"

#include "GameLogic/Module/HordeAIUpdate.h"

#include "Common/Xfer.h"
#include "Common/AttributeModifierRules.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailSaveBlocks.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/RotwkParseOnlyModules.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Pathfinder/Path.h"
#include "GameLogic/Pathfinder/PathNode.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/RetailTestCommands.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponSet.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
constexpr Real FORMATION_PATHFIND_DISTANCE = 120.0f;
constexpr Real FORMATION_REPATH_DISTANCE = 30.0f;
constexpr Real FORMATION_REFORM_ANGLE = PI / 6.0f;
constexpr Int MAX_ADVANCE_ATTEMPTS = 2;

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

Bool HordeAIUpdate::isReforming(const AIUpdateInterface* ai)
{
	static const NameKeyType hordeKey = NAMEKEY("HordeAIUpdate");
	return RetailFileLayers::enabled() && ai && ai->getModuleNameKey() == hordeKey &&
		static_cast<const HordeAIUpdate*>(ai)->m_reformUntil > TheGameLogic->getFrame();
}

void HordeAIUpdate::crcReformState(Xfer* xfer)
{
	xfer->xferUnsignedInt(&m_reformUntil);
}

void HordeAIUpdate::memberCollided(Object* member, Object* target)
{
	Object* anchor = HordeContain::getAnchor(member);
	if (!RetailFileLayers::enabled() || !anchor || !target || target->isEffectivelyDead() ||
		anchor->getRelationship(target) != ENEMIES)
		return;
	auto* ai = anchor->getAI();
	if (!ai || ai->getModuleNameKey() != NAMEKEY("HordeAIUpdate"))
		return;
	auto* horde = static_cast<HordeAIUpdate*>(ai);
	const Object* targetAnchor = HordeContain::getAnchor(target);
	if (horde->m_attackTarget == target->getID() || (targetAnchor && horde->m_attackTarget == targetAnchor->getID()))
	{
		horde->m_contactTarget = horde->m_attackTarget;
		horde->m_contactUntil = TheGameLogic->getFrame() + 3 * LOGICFRAMES_PER_SECOND;
	}
}

Bool HordeAIUpdate::hasRecentContact() const
{
	return RetailFileLayers::enabled() && m_contactTarget != INVALID_ID && m_contactTarget == m_attackTarget &&
		m_contactUntil > TheGameLogic->getFrame();
}

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
		m_orderPosition = *getObject()->getPosition();
		m_retaliationStartDistanceSquared = distanceSquared(m_orderPosition, *target->getPosition());
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
	if (m_updatingAnchor && m_order != NO_ORDER && command == AICMD_ATTACKMOVE_TO_POSITION)
		return;
	if (!m_updatingAnchor)
		m_reformUntil = 0;
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
			m_plannerOrder = FALSE;
			m_advanceAttempts = 0;
			m_order = m_updatingAnchor ? NO_ORDER : command;
			m_orderPosition = *getObject()->getPosition();
			m_retaliationStartDistanceSquared = 0.0f;
			m_attackSource = parms->m_cmdSource;
			m_maxShots = parms->m_intValue;
			m_forceAttack = command == AICMD_FORCE_ATTACK_OBJECT;
		}
		m_attackTarget = target->getID();
		m_engaging = FALSE;
		privateIdle(m_attackSource);
		return;
	}

	m_plannerOrder = FALSE;
	m_advanceAttempts = 0;
	m_order = NO_ORDER;
	m_attackTarget = INVALID_ID;
	m_retaliationStartDistanceSquared = 0.0f;
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

Bool HordeAIUpdate::issuePlannerOrder(Object* target, const Coord3D& position)
{
	AICommandParms parms(target ? AICMD_ATTACK_OBJECT : AICMD_ATTACKMOVE_TO_POSITION, CMD_FROM_AI);
	parms.m_obj = target;
	parms.m_pos = position;
	parms.m_intValue = NO_MAX_SHOTS_LIMIT;
	if ((target && (target->isEffectivelyDead() || getObject()->getRelationship(target) != ENEMIES)) ||
		!isAllowedToRespondToAiCommands(&parms))
		return FALSE;
	aiDoCommand(&parms);
	m_plannerOrder = TRUE;
	m_plannerOrderFrame = TheGameLogic->getFrame();
	m_blockedPlannerRevision = 0;
	return TRUE;
}

Object* HordeAIUpdate::updateOrder(Object* anchor, Object* target, Bool refresh)
{
	if (m_order == NO_ORDER || m_order == AICMD_ATTACK_POSITION ||
		m_order == AICMD_ATTACK_OBJECT || m_order == AICMD_FORCE_ATTACK_OBJECT)
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
			UnsignedInt qualifiers = AI::CAN_ATTACK | AI::ATTACK_BUILDINGS;
#if !RTS_GENERALS
			if (!area && !guardObject && distanceSquared(origin, position) <= 16.0f)
				qualifiers |= AI::USE_HORDE_VISION;
#endif
			target = TheAI->findClosestEnemy(anchor, rangeFromAnchor, qualifiers, getAttackInfo(), &filter);
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
				advance(position);
		}
		else if (m_order == AICMD_ATTACKMOVE_TO_POSITION)
			m_order = NO_ORDER;
	}
	return nullptr;
}

void HordeAIUpdate::advance(const Coord3D& position)
{
	if (isWaitingForPath())
		return;
	if (m_order == AICMD_ATTACKMOVE_TO_POSITION || m_plannerOrder)
	{
		const Coord3D& origin = *getObject()->getPosition();
		const UnsignedInt revision = TheAI->pathfinder()->getRetailRouteRevision();
		if (m_advanceRouteRevision != revision ||
			(m_advanceAttempts && distanceSquared(m_advanceOrigin, origin) > sqr(FORMATION_REPATH_DISTANCE) &&
				distanceSquared(origin, position) < distanceSquared(m_advanceOrigin, position)))
			m_advanceAttempts = 0;
		if (m_advanceAttempts >= MAX_ADVANCE_ATTEMPTS)
		{
			if (m_plannerOrder)
				m_blockedPlannerRevision = m_advanceRouteRevision;
			m_order = NO_ORDER;
			m_attackTarget = INVALID_ID;
			m_engaging = FALSE;
			m_plannerOrder = FALSE;
			privateIdle(m_attackSource);
			return;
		}
		if (!m_advanceAttempts)
			m_advanceOrigin = origin;
		m_advanceRouteRevision = revision;
		++m_advanceAttempts;
	}
	privateMoveToPosition(&position, m_attackSource);
}

void HordeAIUpdate::approach(const Coord3D& position, Bool inRange)
{
	m_engaging = inRange;
	if (m_engaging && isMoving())
		privateIdle(m_attackSource);
	else if (!m_engaging && (isIdle() || distanceSquared(*getGoalPosition(), position) > 100.0f))
		advance(position);
}

UpdateSleepTime HordeAIUpdate::update()
{
	Object* horde = getObject();
	Object* target = liveTarget(m_attackTarget);
	if (m_plannerOrder && target && horde->getRelationship(target) != ENEMIES)
	{
		if (m_order == AICMD_ATTACK_OBJECT)
		{
			m_order = NO_ORDER;
			m_plannerOrder = FALSE;
		}
		m_attackTarget = INVALID_ID;
		m_engaging = FALSE;
		privateIdle(m_attackSource);
		target = nullptr;
	}
#if RTS_ZEROHOUR
	const HordeAIUpdateModuleData* moveData = getHordeAIUpdateModuleData();
	const Bool comboEnabled = RetailFileLayers::enabled() && moveData->m_comboLocomotorSet.compareNoCase("SET_COMBO") == 0;
	const Real comboAttackDistance = moveData->m_comboLocoAttackDistance;
	const auto useComboForTarget = [&](const Object* enemy) {
		return !enemy || horde->getRelationship(enemy) != ENEMIES ||
			distanceSquared(*horde->getPosition(), *enemy->getPosition()) > sqr(comboAttackDistance);
	};
	if (comboEnabled)
		setRetailComboLocomotor(useComboForTarget(target));
#endif
	const Real facingBefore = horde->getOrientation();
	Path* path = getPath();
	const PathNode* end = path ? path->getLastNode() : nullptr;
	const Locomotor* locomotor = getCurLocomotor();
	const Bool reachingOrderEnd = m_order == AICMD_ATTACKMOVE_TO_POSITION && m_attackTarget == INVALID_ID &&
		end && locomotor && distanceSquared(*getGoalPosition(), m_orderPosition) <= 16.0f &&
		distanceSquared(*horde->getPosition(), *end->getPosition()) < sqr(locomotor->getCloseEnoughDist());
	m_updatingAnchor = TRUE;
	AIUpdateInterface::update();
	m_updatingAnchor = FALSE;
	if (reachingOrderEnd && isIdle() && m_attackTarget == INVALID_ID)
	{
		if (m_plannerOrder && distanceSquared(*horde->getPosition(), m_orderPosition) >
			sqr(std::max(FORMATION_REPATH_DISTANCE, horde->getGeometryInfo().getBoundingCircleRadius())))
			m_blockedPlannerRevision = m_advanceRouteRevision;
		m_order = NO_ORDER;
	}

	const HordeContain* contain = HordeContain::get(horde);
	if (!contain || horde->isEffectivelyDead() || (RetailFileLayers::enabled() && horde->getContainedBy() != nullptr))
		return UPDATE_SLEEP_NONE;
	const Real turn = stdAngleDiff(horde->getOrientation(), facingBefore);
	const Bool cavalry = RetailFileLayers::enabled() && horde->isKindOf(KINDOF_CAVALRY);
	if (cavalry)
	{
		m_cavalryTurn = turn;
#if RTS_ZEROHOUR
		if (locomotor && fabs(turn) > locomotor->getMaxTurnWithoutReform())
		{
			const_cast<HordeContain*>(contain)->reassignSlotsByPosition();
			m_cavalryTurn = 0.0f;
		}
#endif
	}
	else if (fabs(turn) > FORMATION_REFORM_ANGLE)
	{
		const_cast<HordeContain*>(contain)->reassignSlotsByPosition();
#if RTS_ZEROHOUR
		if (RetailFileLayers::enabled() && isMoving() && locomotor && locomotor->getAppearance() == LOCO_HORDE &&
			locomotor->getWaitForFormation() && fabs(turn) > locomotor->getMaxTurnWithoutReform() &&
			horde->isKindOf(KINDOF_INFANTRY) && !horde->isKindOf(KINDOF_CAVALRY) &&
			!isReforming(this))
		{
			const Real turnRate = locomotor->getMaxTurnRate(horde->getBodyModule()->getDamageState());
			if (turnRate > 0.0f)
				m_reformUntil = TheGameLogic->getFrame() +
					static_cast<UnsignedInt>(WWMath::Ceil(fabs(turn) / turnRate)) + contain->getFormationReformTravelFrames();
		}
#endif
	}
#if RTS_ZEROHOUR
	if (m_reformUntil && contain->getFormationReformTravelFrames() == 0)
		m_reformUntil = 0;
#endif
	if (m_reformUntil && (!isMoving() || m_reformUntil <= TheGameLogic->getFrame()))
		m_reformUntil = 0;

	target = liveTarget(m_attackTarget);
	if (!target && m_attackTarget != INVALID_ID)
	{
		m_attackTarget = INVALID_ID;
		if (m_order == AICMD_ATTACK_OBJECT || m_order == AICMD_FORCE_ATTACK_OBJECT)
			m_order = NO_ORDER;
		m_retaliationStartDistanceSquared = 0.0f;
		m_engaging = FALSE;
		privateIdle(m_attackSource);
	}

	const Bool refresh = (TheGameLogic->getFrame() + horde->getID()) % 8 == 0;
	target = updateOrder(horde, target, refresh);
	if (m_order == NO_ORDER)
		m_plannerOrder = FALSE;
	const auto* stances = StancesBehavior::find(horde);
	const Bool autonomous = stances && m_order == NO_ORDER && m_attackSource == CMD_FROM_AI;
	const auto* formation = contain->getActiveFormation();
	const auto* melee = &formation->m_meleeBehavior;
#if !RTS_GENERALS
	if (stances && RetailFileLayers::enabled())
	{
		const Int stance = formation->m_isPorcupineFormation ? AttributeModifierRules::STANCE_PORCUPINE : stances->getStance();
		if (const auto* rule = stances->getMeleeBehavior(stance))
			melee = rule;
	}
#endif
	const Bool hold = autonomous && (StancesBehavior::holdsGround(stances->getStance()) ||
		melee->name.compareNoCase("HoldGround") == 0);
	const Bool defend = autonomous && stances->getStance() == AttributeModifierRules::STANCE_BATTLE;
	if (target && defend)
	{
		const Real authored = getHordeAIUpdateModuleData()->m_stopChaseDistance;
		const Real meleeLeash = contain->getActiveFormation()->m_meleeAttackLeashDistance;
		const Real radius = authored > 0.0f ? authored : (meleeLeash > 0.0f ? meleeLeash : horde->getVisionRange());
		const Real retaliationFloor = authored > 0.0f ? 0.0f : m_retaliationStartDistanceSquared;
		const Real pursuitLimitSquared = std::max(radius * radius, retaliationFloor);
		if (radius > 0.0f && !hasRecentContact() && distanceSquared(m_orderPosition, *target->getPosition()) > pursuitLimitSquared)
		{
			const Coord3D origin = m_orderPosition;
			m_attackTarget = INVALID_ID;
			m_retaliationStartDistanceSquared = 0.0f;
			m_engaging = FALSE;
			if (distanceSquared(*horde->getPosition(), origin) > 16.0f)
				privateMoveToPosition(&origin, CMD_FROM_AI);
			else if (isMoving())
				privateIdle(CMD_FROM_AI);
			target = nullptr;
		}
	}
	const Bool attackGround = m_order == AICMD_ATTACK_POSITION;
	const Bool wasEngaging = m_engaging;
	const auto& members = *contain->getContainedItemsList();
	if (target && hold)
	{
		m_engaging = TRUE;
		if (isMoving())
			privateIdle(m_attackSource);
	}
	else if ((target || attackGround) && (!m_engaging || autonomous) && (refresh || isIdle()))
	{
		Real approachRange = 0.0f;
		Bool rankedAdvance = FALSE;
		Bool rankReached = FALSE;
		const Coord3D& destination = target ? *target->getPosition() : m_orderPosition;
		for (const Object* member : members)
		{
			if (const Weapon* weapon = member->getCurrentWeapon();
				weapon && !member->isEffectivelyDead() && contain->rankCanAttack(member))
			{
				approachRange = std::max(approachRange, weapon->getAttackRange(member));
				if (RetailFileLayers::enabled() && contain->rankStopsAdvance(member))
				{
					rankedAdvance = TRUE;
					rankReached |= target ? weapon->isWithinAttackRange(member, target) :
						weapon->isWithinAttackRange(member, &destination);
				}
			}
		}

		const Real innerRange = melee->innerRange > 0.0f ? melee->innerRange : formation->m_meleeBehavior.innerRange;
		const Real contactDistance = std::max(8.0f, std::max(formation->m_meleeAttackLeashDistance, innerRange));
		const Real targetRadius = target ? target->getGeometryInfo().getBoundingCircleRadius() : 0.0f;
		const Real range = std::max(approachRange, contactDistance) + targetRadius;
		approach(destination, rankedAdvance ? rankReached :
			distanceSquared(*horde->getPosition(), destination) <= range * range);
		if (target && m_attackTarget == INVALID_ID)
			target = nullptr;
	}

	const HordeContain* enemy = HordeContain::get(target);
	const auto attackType = m_forceAttack ? ATTACK_NEW_TARGET_FORCED : ATTACK_NEW_TARGET;
	Bool groundAttackInProgress = FALSE;
#if RTS_ZEROHOUR
	const Bool comboPursuit = comboEnabled && useComboForTarget(target);
	const Real anchorRadius = horde->getGeometryInfo().getBoundingCircleRadius();
#endif
	for (Object* member : members)
	{
		AIUpdateInterface* ai = member->getAI();
		if (!ai || member->isEffectivelyDead())
			continue;
#if RTS_ZEROHOUR
		if (comboEnabled)
		{
			const Real memberLimit = 3.0f * anchorRadius + member->getGeometryInfo().getBoundingCircleRadius();
			const Bool following = distanceSquared(*horde->getPosition(), *member->getPosition()) < sqr(memberLimit);
			ai->setRetailComboLocomotor(target ? comboPursuit : following);
		}
#endif

		const auto backing = RetailFileLayers::enabled() ? const_cast<HordeContain*>(contain)->updateBacking(member)
			: HordeContain::BackingPhase::Available;
		if (backing == HordeContain::BackingPhase::Backing)
		{
			groundAttackInProgress |= contain->rankCanAttack(member);
			continue;
		}
		const Bool recovered = backing == HordeContain::BackingPhase::Recovered;
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
				if ((refresh || !wasEngaging || attackingAnchor || recovered) && member->isAbleToAttack())
				{
					CanAttackResult result;
					Object* victim = target;
					if (enemy)
						victim = enemy->findAttackTarget(member, attackType, m_attackSource, (WeaponSlotType)-1, &result);
					else
						result = member->getAbleToAttackSpecificObject(attackType, target, m_attackSource);
					Bool mayApproach = !hold;
#if !RTS_GENERALS
					if (!mayApproach && !formation->m_isPorcupineFormation && victim)
						mayApproach = ai->isWithinHoldGroundCloseRange(victim);
#endif
					if (result == ATTACKRESULT_POSSIBLE || (mayApproach && result == ATTACKRESULT_POSSIBLE_AFTER_MOVING))
					{
						if (m_forceAttack)
							ai->aiForceAttackObject(victim, m_maxShots, m_attackSource);
#if RTS_ZEROHOUR
						else if (m_plannerOrder && m_order == AICMD_ATTACK_OBJECT)
							ai->issueRetailPlannerAttack(victim);
#endif
						else
							ai->aiAttackObject(victim, m_maxShots, m_attackSource);
					}
				}
				continue;
			}
			if (attackGround)
			{
				if ((!wasEngaging || recovered) && member->isAbleToAttack())
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

		HordeContain::FormationGoal goal;
		if (!contain->getFormationGoal(member, goal))
			continue;
		if ((!cavalry && goal.error > FORMATION_PATHFIND_DISTANCE) || !HordeContain::canSteerToFormationGoal(member, goal))
		{
			if (ai->isAttacking() || (refresh && (ai->isIdle() ||
				distanceSquared(*ai->getGoalPosition(), goal.slot) > FORMATION_REPATH_DISTANCE * FORMATION_REPATH_DISTANCE)))
				ai->aiMoveToPositionEvenIfSleeping(&goal.slot, CMD_FROM_SCRIPT);
		}
		else if (!ai->isIdle())
			ai->aiIdle(CMD_FROM_SCRIPT);
		else if (goal.error > 1.0f || goal.hordeMoving)
			ai->wakeForFormation();
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
	xfer->xferReal(&m_retaliationStartDistanceSquared);
	xfer->xferBool(&m_plannerOrder);
	xfer->xferInt(&m_advanceAttempts);
	xfer->xferCoord3D(&m_advanceOrigin);
	xfer->xferUnsignedInt(&m_advanceRouteRevision);
	xfer->xferUnsignedInt(&m_plannerOrderFrame);
	xfer->xferUnsignedInt(&m_blockedPlannerRevision);
	if (RetailFileLayers::enabled())
	{
		xfer->xferObjectID(&m_contactTarget);
		xfer->xferUnsignedInt(&m_contactUntil);
		if (getObject()->isKindOf(KINDOF_CAVALRY))
		{
			xfer->xferReal(&m_cavalryTurn);
			xfer->xferReal(&m_cavalryTurnSinceReform);
		}
		crcReformState(xfer);
		xferRetailBlocks(xfer, TRUE);
	}
}

void HordeAIUpdate::xfer(Xfer* xfer)
{
	const XferVersion currentVersion = RetailFileLayers::enabled() ? 9 : 5;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	AIUpdateInterface::xfer(xfer);
	xfer->xferObjectID(&m_attackTarget);
	xfer->xferUser(&m_attackSource, sizeof(m_attackSource));
	xfer->xferInt(&m_maxShots);
	xfer->xferBool(&m_forceAttack);
	xfer->xferBool(&m_engaging);
	if (version >= 2)
		xferOrder(xfer);
	if (version >= 3)
		xfer->xferReal(&m_retaliationStartDistanceSquared);
	if (version >= 4)
	{
		xfer->xferBool(&m_plannerOrder);
		xfer->xferInt(&m_advanceAttempts);
		xfer->xferCoord3D(&m_advanceOrigin);
	}
	if (version >= 5)
	{
		xfer->xferUnsignedInt(&m_advanceRouteRevision);
		xfer->xferUnsignedInt(&m_plannerOrderFrame);
		xfer->xferUnsignedInt(&m_blockedPlannerRevision);
	}
	if (version >= 6)
	{
		xfer->xferObjectID(&m_contactTarget);
		xfer->xferUnsignedInt(&m_contactUntil);
	}
	if (version >= 7 && getObject()->isKindOf(KINDOF_CAVALRY))
	{
		xfer->xferReal(&m_cavalryTurn);
		xfer->xferReal(&m_cavalryTurnSinceReform);
	}
	if (version >= 8)
		xfer->xferUnsignedInt(&m_reformUntil);
	else if (xfer->getXferMode() == XFER_LOAD)
		m_reformUntil = 0;
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, version >= 9);
}

void HordeAIUpdate::xferRetailBlocks(Xfer* xfer, Bool present)
{
	RetailSaveBlocks::transferSection<Xfer>(xfer, xfer->getXferMode() == XFER_LOAD, present, {});
}

void HordeAIUpdate::loadPostProcess()
{
	AIUpdateInterface::loadPostProcess();
}
