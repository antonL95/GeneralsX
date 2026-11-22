#include "PreRTS.h"

#include "GameLogic/Module/GiantBirdAIUpdate.h"

#include <algorithm>

#include "Common/INI.h"
#include "Common/RetailFileLayers.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/RetailSlowDeath.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"
#include "WWMath/wwmath.h"

namespace
{
constexpr Real CONTACT_HEIGHT_TOLERANCE = 12.0f;
constexpr Real FOLLOW_THROUGH_ARRIVAL_DISTANCE = 6.0f;

Real planarDistanceSquared(const Coord3D& a, const Coord3D& b)
{
	const Real dx = a.x - b.x;
	const Real dy = a.y - b.y;
	return dx * dx + dy * dy;
}
}

void GiantBirdAIUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	AIUpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "FollowThroughDistance", INI::parseReal, nullptr, offsetof(GiantBirdAIUpdateModuleData, m_followThroughDistance) },
		{ "FollowThroughCheckStep", INI::parseReal, nullptr, offsetof(GiantBirdAIUpdateModuleData, m_followThroughCheckStep) },
		{ "FollowThroughGradient", INI::parseReal, nullptr, offsetof(GiantBirdAIUpdateModuleData, m_followThroughGradient) },
		{ "GrabTossTimeTrigger", INI::parseReal, nullptr, offsetof(GiantBirdAIUpdateModuleData, m_grabTossTimeTrigger) },
		{ "GrabTossHeightTrigger", INI::parseReal, nullptr, offsetof(GiantBirdAIUpdateModuleData, m_grabTossHeightTrigger) },
		{ "TossFX", INI::parseFXList, nullptr, offsetof(GiantBirdAIUpdateModuleData, m_tossFX) },
		{ "SpecialContactPoints", INI::parseAsciiString, nullptr, offsetof(GiantBirdAIUpdateModuleData, m_specialContactPoints) },
		{ "AttackLocomotorType", INI::parseAsciiString, nullptr, offsetof(GiantBirdAIUpdateModuleData, m_attackLocomotorType) },
		{ "ReturnForAmmoLocomotorType", INI::parseAsciiString, nullptr, offsetof(GiantBirdAIUpdateModuleData, m_returnForAmmoLocomotorType) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields, 0);
}

GiantBirdAIUpdate::GiantBirdAIUpdate(Thing* thing, const ModuleData* data) : AIUpdateInterface(thing, data)
{
}

GiantBirdAIUpdate::~GiantBirdAIUpdate() = default;

void GiantBirdAIUpdate::aiDoCommand(const AICommandParms* parms)
{
	if (parms->m_cmdSource != CMD_FROM_AI)
		m_followingThrough = FALSE;
	AIUpdateInterface::aiDoCommand(parms);
}

void GiantBirdAIUpdate::setSwooping(Bool swooping)
{
	if (m_swooping == swooping)
		return;
	m_swooping = swooping;
	if (swooping)
		getObject()->setModelConditionState(MODELCONDITION_SWOOPING);
	else
		getObject()->clearModelConditionState(MODELCONDITION_SWOOPING);
}

Bool GiantBirdAIUpdate::canStrike(const Object& bird, const Coord3D& target)
{
	if (!RetailFileLayers::enabled())
		return TRUE;
	const auto* ai = static_cast<const GiantBirdAIUpdate*>(bird.findUpdateModule(NAMEKEY("GiantBirdAIUpdate")));
	if (!ai)
		return TRUE;
	const Locomotor* locomotor = ai->getCurLocomotor();
	if (!locomotor || locomotor->getAppearance() != LOCO_GIANT_BIRD)
		return TRUE;
	const Real dz = bird.getPosition()->z - target.z - locomotor->getPreferredAttackHeight();
	return !ai->m_followingThrough && dz <= CONTACT_HEIGHT_TOLERANCE && dz >= -CONTACT_HEIGHT_TOLERANCE;
}

void GiantBirdAIUpdate::startFollowThrough()
{
	const auto* data = getGiantBirdAIUpdateModuleData();
	if (data->m_followThroughDistance <= 0.0f)
		return;

	const Coord3D& origin = *getObject()->getPosition();
	const Coord3D& direction = *getObject()->getUnitDirectionVector2D();
	Region3D extent;
	TheTerrainLogic->getExtent(&extent);
	m_followThroughGoal = origin;
	const Real step = std::max(1.0f, data->m_followThroughCheckStep);
	const Int checks = static_cast<Int>(std::clamp(data->m_followThroughDistance / step, 1.0f, 64.0f));
	const Real spacing = data->m_followThroughDistance / static_cast<Real>(checks);
	const Real ground = TheTerrainLogic->getGroundHeight(origin.x, origin.y);
	for (Int i = 1; i <= checks; ++i)
	{
		const Real distance = spacing * static_cast<Real>(i);
		Coord3D goal = { origin.x + direction.x * distance, origin.y + direction.y * distance, origin.z };
		if (goal.x <= extent.lo.x || goal.y <= extent.lo.y || goal.x >= extent.hi.x || goal.y >= extent.hi.y)
			break;
		if (data->m_followThroughGradient > 0.0f && TheTerrainLogic->getGroundHeight(goal.x, goal.y) >
			ground + distance * data->m_followThroughGradient)
			break;
		m_followThroughGoal = goal;
	}
	m_followingThrough = planarDistanceSquared(origin, m_followThroughGoal) >
		FOLLOW_THROUGH_ARRIVAL_DISTANCE * FOLLOW_THROUGH_ARRIVAL_DISTANCE;
}

UpdateSleepTime GiantBirdAIUpdate::update()
{
	const Bool wasSwooping = m_swooping;
	const UpdateSleepTime sleep = AIUpdateInterface::update();
	Object* bird = getObject();
	if (bird->isEffectivelyDead() || !getCurLocomotor())
		return sleep;

	if (wasSwooping)
	{
		for (Int slot = PRIMARY_WEAPON; slot < WEAPONSLOT_COUNT; ++slot)
		{
			const Weapon* weapon = bird->getWeaponInWeaponSlot(static_cast<WeaponSlotType>(slot));
			if (weapon && weapon->getLastShotFrame() == TheGameLogic->getFrame())
			{
				setSwooping(FALSE);
				startFollowThrough();
				break;
			}
		}
	}
	return UPDATE_SLEEP_NONE;
}

UpdateSleepTime GiantBirdAIUpdate::doLocomotor()
{
	Object* bird = getObject();
	if (bird->isEffectivelyDead() && GiantBirdSlowDeathBehavior::controlsMotion(*bird))
		return UPDATE_SLEEP_FOREVER;

	Locomotor* locomotor = getCurLocomotor();
	if (!locomotor || locomotor->getAppearance() != LOCO_GIANT_BIRD || bird->isEffectivelyDead())
		return AIUpdateInterface::doLocomotor();

	const Coord3D& at = *bird->getPosition();
	if (m_followingThrough)
	{
		const Real remaining = planarDistanceSquared(at, m_followThroughGoal);
		if (remaining > FOLLOW_THROUGH_ARRIVAL_DISTANCE * FOLLOW_THROUGH_ARRIVAL_DISTANCE)
		{
			chooseLocomotorSet(LOCOMOTORSET_NORMAL);
			locomotor = getCurLocomotor();
			if (locomotor)
			{
				locomotor->restorePreferredHeightFromTemplate();
				locomotor->setPhysicsOptions(bird);
				const Real speed = getCurLocomotorSpeed();
				Bool blocked = FALSE;
				locomotor->locoUpdate_moveTowardsPosition(bird, m_followThroughGoal, WWMath::Sqrt(remaining), speed, &blocked);
				updateAirborneTargetStatus();
				return UPDATE_SLEEP_NONE;
			}
		}
		m_followingThrough = FALSE;
	}

	const Object* victim = isAttacking() ? getCurrentVictim() : nullptr;
	if (victim && victim->isEffectivelyDead())
		victim = nullptr;
	const Weapon* weapon = bird->getCurrentWeapon();
	const Coord3D* strike = victim ? victim->getPosition() : getCurrentVictimPos();
	const Real range = weapon ? weapon->getAttackRange(bird) : 0.0f;
	const Real approach = range + std::max(40.0f, locomotor->getPreferredAttackHeight());
	const Bool swooping = strike && weapon && (victim
		? ThePartitionManager->getDistanceSquared(bird, victim, FROM_BOUNDINGSPHERE_2D)
		: ThePartitionManager->getDistanceSquared(bird, strike, FROM_BOUNDINGSPHERE_2D)) <= approach * approach;
	setSwooping(swooping);
	if (swooping)
	{
		const AsciiString& authored = getGiantBirdAIUpdateModuleData()->m_attackLocomotorType;
		const Int set = authored.isEmpty() ? LOCOMOTORSET_PANIC : AIUpdateModuleData::findLocomotorSetType(authored);
		if (set >= LOCOMOTORSET_NORMAL && set < LOCOMOTORSET_COUNT && set != LOCOMOTORSET_NORMAL_UPGRADED)
			chooseLocomotorSet(static_cast<LocomotorSetType>(set));
		if (Locomotor* attackLoco = getCurLocomotor())
		{
			const Real surface = TheTerrainLogic->getGroundHeight(at.x, at.y);
			attackLoco->setPreferredHeight(std::max(1.0f, strike->z + attackLoco->getPreferredAttackHeight() - surface));
		}
	}
	else
	{
		chooseLocomotorSet(LOCOMOTORSET_NORMAL);
		if (Locomotor* normalLoco = getCurLocomotor())
			normalLoco->restorePreferredHeightFromTemplate();
	}
	return AIUpdateInterface::doLocomotor();
}

void GiantBirdAIUpdate::xferFlightState(Xfer* xfer)
{
	xfer->xferCoord3D(&m_followThroughGoal);
	xfer->xferBool(&m_followingThrough);
	xfer->xferBool(&m_swooping);
}

void GiantBirdAIUpdate::crc(Xfer* xfer)
{
	AIUpdateInterface::crc(xfer);
	xferFlightState(xfer);
}

void GiantBirdAIUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	AIUpdateInterface::xfer(xfer);
	xferFlightState(xfer);
}

void GiantBirdAIUpdate::loadPostProcess()
{
	AIUpdateInterface::loadPostProcess();
}
