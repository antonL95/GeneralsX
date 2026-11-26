#include "PreRTS.h"

#include "GameLogic/Module/GiantBirdAIUpdate.h"

#include <algorithm>

#include "Common/INI.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailSaveBlocks.h"
#include "Common/RotwkDuration.h"
#include "Common/ThingFactory.h"
#include "Common/Xfer.h"
#include "GameClient/FXList.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#if !RTS_GENERALS
#include "GameLogic/Module/CreateObjectDie.h"
#endif
#include "GameLogic/Module/PhysicsUpdate.h"
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
#if !RTS_GENERALS
constexpr Real PULL_UP_DISTANCE = 7.0f;
constexpr Real GRAB_TOSS_SPEED_SCALE = 0.5f;
constexpr Real GRAB_TOSS_VERTICAL_VELOCITY = -6.0f;
#endif

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
	{
		m_followingThrough = FALSE;
		m_pullUpActive = FALSE;
		m_swoopVictim = INVALID_ID;
	}
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
#if !RTS_GENERALS
	const Weapon* weapon = bird.getCurrentWeapon();
	if (weapon && !weapon->getTemplate()->canSwoop())
		return TRUE;
#endif
	const Locomotor* locomotor = ai->getCurLocomotor();
	if (!locomotor || locomotor->getAppearance() != LOCO_GIANT_BIRD)
		return TRUE;
	const Real dz = bird.getPosition()->z - target.z - locomotor->getPreferredAttackHeight();
	return ai->m_swooping && !ai->m_followingThrough && dz <= CONTACT_HEIGHT_TOLERANCE && dz >= -CONTACT_HEIGHT_TOLERANCE;
}

void GiantBirdAIUpdate::trackGrabbedPassenger(Object& bird, const Object& passenger)
{
	if (RetailFileLayers::enabled())
		if (auto* ai = static_cast<GiantBirdAIUpdate*>(bird.findUpdateModule(NAMEKEY("GiantBirdAIUpdate"))))
		{
			ai->m_grabbedPassenger = passenger.getID();
			ai->m_killPassengerOnLanding = passenger.isKindOf(KINDOF_GRAB_AND_DROP);
		}
}

void GiantBirdAIUpdate::releaseGrabbedPassenger()
{
	if (!RetailFileLayers::enabled() || m_grabbedPassenger == INVALID_ID)
		return;
	Object* bird = getObject();
	Object* passenger = TheGameLogic->findObjectByID(m_grabbedPassenger);
	if (!passenger || passenger->getContainedBy() != bird || !bird->getContain())
	{
		m_grabbedPassenger = INVALID_ID;
		m_killPassengerOnLanding = FALSE;
		return;
	}

	const auto* data = getGiantBirdAIUpdateModuleData();
	const Coord3D& pos = *bird->getPosition();
	const Real height = pos.z - TheTerrainLogic->getGroundHeight(pos.x, pos.y);
	const UnsignedInt heldFrames = TheGameLogic->getFrame() - passenger->getContainedByFrame();
	const Bool timeReached = data->m_grabTossTimeTrigger > 0.0f &&
		heldFrames > data->m_grabTossTimeTrigger * LOGICFRAMES_PER_SECOND;
	const Bool heightReached = data->m_grabTossHeightTrigger > 0.0f && height > data->m_grabTossHeightTrigger;
	if (!timeReached && !heightReached)
		return;

	const Bool killOnLanding = m_killPassengerOnLanding;
	m_grabbedPassenger = INVALID_ID;
	m_killPassengerOnLanding = FALSE;
	bird->getContain()->removeFromContain(passenger);
	passenger->setOrientation(bird->getOrientation());
	if (auto* physics = passenger->getPhysics())
	{
#if !RTS_GENERALS
		if (getCurLocomotor())
		{
			Vector3 forward = bird->getTransformMatrix()->Get_X_Vector();
			const Real forwardLength = WWMath::SqrtfOrigin(
				forward.X * forward.X + forward.Y * forward.Y + forward.Z * forward.Z);
			if (forwardLength > 0.0f)
			{
				forward.X /= forwardLength;
				forward.Y /= forwardLength;
				forward.Z /= forwardLength;
			}
			else
			{
				const Coord3D& heading = *bird->getUnitDirectionVector2D();
				forward.Set(heading.x, heading.y, 0.0f);
			}
			constexpr Real framesPerRetailTick =
				static_cast<Real>(LOGICFRAMES_PER_SECOND) / RotwkDuration::FRAMES_PER_SECOND;
			const Real birdSpeed = bird->getPhysics() ? bird->getPhysics()->getVelocityMagnitude() : getCurLocomotorSpeed();
			const Real halfRetailSpeed = birdSpeed * framesPerRetailTick * GRAB_TOSS_SPEED_SCALE;
			const Coord3D launchVelocity = {
				forward.X * halfRetailSpeed,
				forward.Y * halfRetailSpeed,
				GRAB_TOSS_VERTICAL_VELOCITY
			};
			physics->launchFromContainment(launchVelocity);
			FXList::doFXObj(data->m_tossFX, passenger, bird);
			if (killOnLanding)
			{
				bird->scoreTheKill(passenger);
				physics->setKillWhenLanding(TRUE);
			}
		}
#else
		(void)killOnLanding;
		if (const auto* carrierPhysics = bird->getPhysics())
			carrierPhysics->transferVelocityTo(physics);
		physics->setAllowToFall(TRUE);
		FXList::doFXObj(data->m_tossFX, passenger, bird);
#endif
	}
	passenger->setModelConditionState(MODELCONDITION_FREEFALL);
}

void GiantBirdAIUpdate::tryGrabDebrisPortion(Object* victim)
{
#if !RTS_GENERALS
	if (!RetailFileLayers::enabled() || m_grabbedPassenger != INVALID_ID || !victim ||
		!victim->isEffectivelyDead() || !victim->isKindOf(KINDOF_MACHINE))
		return;
	Object* bird = getObject();
	ContainModuleInterface* contain = bird->getContain();
	if (!contain || contain->getContainCount() >= contain->getContainMax())
		return;
	static const NameKeyType createDieKey = NAMEKEY("CreateObjectDie");
	AsciiString debrisName;
	if (BehaviorModule** behaviors = victim->getBehaviorModules())
	{
		for (; *behaviors; ++behaviors)
		{
			if ((*behaviors)->getModuleNameKey() == createDieKey)
			{
				const auto* createDie = static_cast<const CreateObjectDie*>(*behaviors);
				if (!createDie->getDebrisPortionOfSelf().isEmpty())
				{
					debrisName = createDie->getDebrisPortionOfSelf();
					break;
				}
			}
		}
	}
	if (debrisName.isEmpty())
		return;
	const ThingTemplate* debrisTemplate = TheThingFactory->findTemplate(debrisName, FALSE);
	if (!debrisTemplate)
		return;
	Object* debris = TheThingFactory->newObject(debrisTemplate, victim->getTeam());
	if (!debris)
		return;
	debris->setTransformMatrix(bird->getTransformMatrix());
	contain->addToContain(debris);
	if (debris->getContainedBy() == bird)
	{
		m_grabbedPassenger = debris->getID();
		m_killPassengerOnLanding = FALSE;
		m_swoopVictim = INVALID_ID;
	}
	else
		TheGameLogic->destroyObject(debris);
#else
	(void)victim;
#endif
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
	if (RetailFileLayers::enabled() && m_followingThrough)
	{
		m_pullUpOrigin = origin;
		m_pullUpActive = TRUE;
	}
}

UpdateSleepTime GiantBirdAIUpdate::update()
{
	const UpdateSleepTime sleep = AIUpdateInterface::update();
	releaseGrabbedPassenger();
	return getObject()->isEffectivelyDead() || !getCurLocomotor() ? sleep : UPDATE_SLEEP_NONE;
}

UpdateSleepTime GiantBirdAIUpdate::doLocomotor()
{
	Object* bird = getObject();
	if (bird->isEffectivelyDead() && GiantBirdSlowDeathBehavior::controlsMotion(*bird))
		return UPDATE_SLEEP_FOREVER;

	Locomotor* locomotor = getCurLocomotor();
	if (!locomotor || locomotor->getAppearance() != LOCO_GIANT_BIRD || bird->isEffectivelyDead())
		return AIUpdateInterface::doLocomotor();

#if !RTS_GENERALS
	if (RetailFileLayers::enabled() && getCurLocomotorSetType() == LOCOMOTORSET_SUPERSONIC &&
		bird->findUpdateModule(NAMEKEY("StrafeAreaUpdate")))
		return AIUpdateInterface::doLocomotor();
#endif

	if (m_swooping)
	{
		for (Int slot = PRIMARY_WEAPON; slot < WEAPONSLOT_COUNT; ++slot)
		{
			const Weapon* weapon = bird->getWeaponInWeaponSlot(static_cast<WeaponSlotType>(slot));
			if (weapon && weapon->getLastShotFrame() == TheGameLogic->getFrame())
			{
				Object* shotVictim = m_swoopVictim != INVALID_ID
					? TheGameLogic->findObjectByID(m_swoopVictim)
					: getCurrentVictim();
				if (shotVictim && m_swoopVictim == INVALID_ID)
					m_swoopVictim = shotVictim->getID();
				tryGrabDebrisPortion(shotVictim);
				setSwooping(FALSE);
				startFollowThrough();
				break;
			}
		}
	}
	const Coord3D& at = *bird->getPosition();
	if (m_followingThrough)
	{
		if (m_swoopVictim != INVALID_ID && m_grabbedPassenger == INVALID_ID)
		{
			if (Object* shotVictim = TheGameLogic->findObjectByID(m_swoopVictim))
				tryGrabDebrisPortion(shotVictim);
			else
				m_swoopVictim = INVALID_ID;
		}
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
#if RTS_GENERALS
				locomotor->locoUpdate_moveTowardsPosition(bird, m_followThroughGoal, WWMath::Sqrt(remaining), speed, &blocked);
#else
				m_pullUpActive = m_pullUpActive && planarDistanceSquared(at, m_pullUpOrigin) < PULL_UP_DISTANCE * PULL_UP_DISTANCE;
				locomotor->locoUpdate_moveTowardsPosition(bird, m_followThroughGoal, WWMath::Sqrt(remaining), speed, &blocked, m_pullUpActive);
#endif
				updateAirborneTargetStatus();
				return UPDATE_SLEEP_NONE;
			}
		}
		m_followingThrough = FALSE;
		m_pullUpActive = FALSE;
		m_swoopVictim = INVALID_ID;
	}

	const Object* victim = isAttacking() ? getCurrentVictim() : nullptr;
	if (victim && victim->isEffectivelyDead())
		victim = nullptr;
	const Weapon* weapon = bird->getCurrentWeapon();
	const Coord3D* strike = victim ? victim->getPosition() : getCurrentVictimPos();
	Bool canSwoop = weapon != nullptr;
#if RTS_GENERALS
	const Real range = weapon ? weapon->getAttackRange(bird) : 0.0f;
#else
	canSwoop = canSwoop && weapon->getTemplate()->canSwoop();
	const Real range = weapon ? weapon->getAttackRange(bird, strike) : 0.0f;
#endif
	const Real approach = range + std::max(40.0f, locomotor->getPreferredAttackHeight());
	Bool standoffLimitsEntry = FALSE;
#if !RTS_GENERALS
	Bool withinStandoff = FALSE;
	if (RetailFileLayers::enabled() && strike && canSwoop)
	{
		const Real radius = locomotor->getSwoopStandoffRadius();
		withinStandoff = radius > 0.0f && planarDistanceSquared(at, *strike) <= radius * radius;
		standoffLimitsEntry = radius > 0.0f && !withinStandoff && !m_swooping;
	}
#endif
	const Bool swooping = strike && canSwoop && !standoffLimitsEntry && (victim
		? ThePartitionManager->getDistanceSquared(bird, victim, FROM_BOUNDINGSPHERE_2D)
		: ThePartitionManager->getDistanceSquared(bird, strike, FROM_BOUNDINGSPHERE_2D)) <= approach * approach;
	setSwooping(swooping);
	if (swooping)
	{
		m_swoopVictim = victim ? victim->getID() : INVALID_ID;
		const AsciiString& authored = getGiantBirdAIUpdateModuleData()->m_attackLocomotorType;
		const Int set = authored.isEmpty() ? LOCOMOTORSET_PANIC : AIUpdateModuleData::findLocomotorSetType(authored);
		if (set >= LOCOMOTORSET_NORMAL && set < LOCOMOTORSET_COUNT && set != LOCOMOTORSET_NORMAL_UPGRADED)
			chooseLocomotorSet(static_cast<LocomotorSetType>(set));
		if (Locomotor* attackLoco = getCurLocomotor())
		{
			const Real surface = TheTerrainLogic->getGroundHeight(at.x, at.y);
			attackLoco->setPreferredHeight(std::max(1.0f, strike->z + attackLoco->getPreferredAttackHeight() - surface));
			const PhysicsBehavior* physics = bird->getPhysics();
			if (!getPath() && physics)
			{
				const Real launchSpeed = attackLoco->getMaxAcceleration(bird->getBodyModule()->getDamageState());
				Real speed = std::max(physics->getForwardSpeed2D(), launchSpeed);
				const Coord3D& heading = *bird->getUnitDirectionVector2D();
				const Coord3D delta = *strike - at;
				const Real forward = delta.x * heading.x + delta.y * heading.y;
				const Real descent = std::max(0.0f, -physics->getVelocity()->z);
				const Real heightToContact = -delta.z - attackLoco->getPreferredAttackHeight() - CONTACT_HEIGHT_TOLERANCE;
				if (forward > 0.0f && heightToContact > descent)
					speed = std::clamp(forward * (descent / heightToContact), launchSpeed, speed);

				Coord3D guide = *strike;
				if (bird->testStatus(OBJECT_STATUS_IS_FIRING_WEAPON))
				{
					const Real lookAhead = std::max(speed, attackLoco->getCloseEnoughDist());
					const Real extension = std::max(0.0f, lookAhead - forward);
					guide.x += heading.x * extension;
					guide.y += heading.y * extension;
				}
				const Real remaining = WWMath::Sqrt(planarDistanceSquared(at, guide)) +
					std::max(0.0f, getGiantBirdAIUpdateModuleData()->m_followThroughDistance);
				attackLoco->setPhysicsOptions(bird);
				Bool blocked = FALSE;
				attackLoco->locoUpdate_moveTowardsPosition(bird, guide, remaining, speed, &blocked);
				updateAirborneTargetStatus();
				return UPDATE_SLEEP_NONE;
			}
		}
	}
	else
	{
		m_swoopVictim = INVALID_ID;
		chooseLocomotorSet(LOCOMOTORSET_NORMAL);
		if (Locomotor* normalLoco = getCurLocomotor())
		{
			normalLoco->restorePreferredHeightFromTemplate();
#if !RTS_GENERALS
			if (withinStandoff && normalLoco->getSwoopStandoffHeight() > 0.0f)
				normalLoco->setPreferredHeight(normalLoco->getSwoopStandoffHeight());
#endif
		}
	}
	return AIUpdateInterface::doLocomotor();
}

void GiantBirdAIUpdate::xferFlightState(Xfer* xfer)
{
	xfer->xferCoord3D(&m_followThroughGoal);
	xfer->xferBool(&m_followingThrough);
	xfer->xferBool(&m_swooping);
}

void GiantBirdAIUpdate::xferRetailBlocks(Xfer* xfer, Bool present)
{
	RetailSaveBlocks::transferSection(xfer, xfer->getXferMode() == XFER_LOAD, present, {
		{ "grabbed-passenger", [this](Xfer* x) { x->xferObjectID(&m_grabbedPassenger); },
			[this] { m_grabbedPassenger = INVALID_ID; } },
		{ "swoop-pull-up", [this](Xfer* x)
		{
			x->xferCoord3D(&m_pullUpOrigin);
			x->xferBool(&m_pullUpActive);
		}, [this]
		{
			m_pullUpOrigin = {};
			m_pullUpActive = FALSE;
		} },
		{ "swoop-victim", [this](Xfer* x) { x->xferObjectID(&m_swoopVictim); },
			[this] { m_swoopVictim = INVALID_ID; } },
		{ "grab-kill-on-landing", [this](Xfer* x) { x->xferBool(&m_killPassengerOnLanding); },
			[this] { m_killPassengerOnLanding = FALSE; } }
	});
}

void GiantBirdAIUpdate::crc(Xfer* xfer)
{
	AIUpdateInterface::crc(xfer);
	xferFlightState(xfer);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, TRUE);
}

void GiantBirdAIUpdate::xfer(Xfer* xfer)
{
	const XferVersion currentVersion = RetailFileLayers::enabled() ? 2 : 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	AIUpdateInterface::xfer(xfer);
	xferFlightState(xfer);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, version >= 2);
}

void GiantBirdAIUpdate::loadPostProcess()
{
	AIUpdateInterface::loadPostProcess();
}
