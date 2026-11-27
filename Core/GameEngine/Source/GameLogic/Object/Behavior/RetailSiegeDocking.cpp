#include "PreRTS.h"
#include "GameLogic/Module/RetailSiegeDocking.h"

#include <algorithm>
#include <vector>

#include "Common/ActionManager.h"
#include "Common/GameAudio.h"
#include "Common/RetailFileLayers.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "Common/RetailSaveBlocks.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/HordeAIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/RetailGate.h"
#include "GameLogic/Module/RetailSiegeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Pathfinder/PathfindCell.h"
#include "GameLogic/TerrainLogic.h"
#include "WWMath/wwmath.h"

namespace
{
	Int socketType(const Object* carrier)
	{
		return carrier->isKindOf(KINDOF_SIEGE_LADDER) ? 1 : 0;
	}

	Real distanceSquared(const Coord3D& left, const Coord3D& right)
	{
		const Real dx = left.x - right.x;
		const Real dy = left.y - right.y;
		return dx * dx + dy * dy;
	}
}

SiegeDockingBehavior::SiegeDockingBehavior(Thing* thing, const ModuleData* data)
	: BehaviorModule(thing, data)
{
	for (auto& type : m_owners)
		for (auto& owner : type)
			owner = INVALID_ID;
}

SiegeDockingBehavior::~SiegeDockingBehavior() = default;

ObjectID SiegeDockingBehavior::getWallID() const
{
	return getObject()->getID();
}

SiegeDockingBehavior* SiegeDockingBehavior::find(const Object* wall)
{
	if (!RetailFileLayers::enabled() || !wall || !wall->areModulesReady() || wall->isEffectivelyDead())
		return nullptr;
	const NameKeyType name = NAMEKEY("SiegeDockingBehavior");
	for (auto** module = wall->getBehaviorModules(); *module; ++module)
		if ((*module)->getModuleNameKey() == name)
			return static_cast<SiegeDockingBehavior*>(*module);
	return nullptr;
}

Int SiegeDockingBehavior::getSockets(const Object* carrier, Socket (&sockets)[10]) const
{
	Coord3D positions[10];
	Matrix3D transforms[10];
	const Int count = getObject()->getMultiLogicalBonePosition(
		socketType(carrier) ? "SIEGELADDER" : "SIEGETOWER", 10, positions, transforms);
	for (Int slot = 0; slot < count; ++slot)
	{
		const Vector3 direction = transforms[slot].Get_X_Vector();
		sockets[slot] = { positions[slot], WWMath::Atan2Origin(-direction.Y, -direction.X) };
	}
	return count;
}

Bool SiegeDockingBehavior::available(const Object* carrier, Int slot) const
{
	const ObjectID owner = m_owners[socketType(carrier)][slot];
	if (owner == INVALID_ID || owner == carrier->getID())
		return TRUE;
	const Object* current = TheGameLogic->findObjectByID(owner);
	return !current || current->isEffectivelyDead();
}

void SiegeDockingBehavior::reserve(const Object* carrier, Int slot)
{
	m_owners[socketType(carrier)][slot] = carrier->getID();
}

void SiegeDockingBehavior::release(const Object* carrier)
{
	for (ObjectID& owner : m_owners[socketType(carrier)])
		if (owner == carrier->getID())
			owner = INVALID_ID;
}

Object* SiegeDockingBehavior::findDockedCarrierFor(const Object& rider) const
{
	Object* closest = nullptr;
	Real closestDistance = 0;
	for (const auto& type : m_owners)
	{
		for (const ObjectID id : type)
		{
			Object* carrier = TheGameLogic->findObjectByID(id);
			const auto* ai = SiegeAIUpdate::find(carrier);
			if (!ai || !ai->isDockedAt(getObject()) || carrier->isEffectivelyDead())
				continue;
			const auto* contain = SiegeEngineContain::find(carrier);
			if (!contain || !contain->isValidContainerFor(&rider, TRUE))
				continue;
			const Real distance = distanceSquared(*rider.getPosition(), *carrier->getPosition());
			if (!closest || distance < closestDistance ||
				(distance == closestDistance && id < closest->getID()))
			{
				closest = carrier;
				closestDistance = distance;
			}
		}
	}
	return closest;
}

void SiegeDockingBehavior::transferSockets(Xfer* xfer)
{
	RetailSaveBlocks::transferSection(xfer, xfer->getXferMode() == XFER_LOAD, TRUE, {
		{ "siege-dock-owners", [this](Xfer* x) {
			for (auto& type : m_owners)
				for (auto& owner : type)
					x->xferObjectID(&owner);
		}, {} }
	});
}

void SiegeDockingBehavior::crc(Xfer* xfer)
{
	BehaviorModule::crc(xfer);
	if (RetailFileLayers::enabled())
		transferSockets(xfer);
}

void SiegeDockingBehavior::xfer(Xfer* xfer)
{
	BehaviorModule::xfer(xfer);
	if (RetailFileLayers::enabled())
		transferSockets(xfer);
}

void SiegeDockingBehavior::loadPostProcess()
{
	BehaviorModule::loadPostProcess();
}

SiegeAIUpdate::SiegeAIUpdate(Thing* thing, const ModuleData* data)
	: AIUpdateInterface(thing, data)
{
}

SiegeAIUpdate::~SiegeAIUpdate() = default;

Bool SiegeAIUpdate::hasSpecializedAI(const ModuleInfo& modules)
{
	for (size_t index = 0; index < modules.getCount(); ++index)
		if (modules.getNthName(index) == "SiegeAIUpdate")
			return TRUE;
	return FALSE;
}

const AsciiString& SiegeAIUpdate::getRetailLuaEventList() const
{
	const auto& primary = AIUpdateInterface::getRetailLuaEventList();
	if (!primary.isEmpty())
		return primary;
	const auto& modules = getObject()->getTemplate()->getBehaviorModuleInfo();
	for (size_t index = 0; index < modules.getCount(); ++index)
		if (modules.getNthName(index) == "AIUpdateInterface")
			return static_cast<const AIUpdateModuleData*>(modules.getNthData(index))->m_luaEventsList;
	return primary;
}

SiegeAIUpdate* SiegeAIUpdate::find(const Object* carrier)
{
	if (!RetailFileLayers::enabled() || !carrier || !carrier->areModulesReady())
		return nullptr;
	return static_cast<SiegeAIUpdate*>(carrier->findUpdateModule(NAMEKEY("SiegeAIUpdate")));
}

Bool SiegeAIUpdate::getClimbEntryPosition(const Object* carrier, const Object* rider, Coord3D& position)
{
	const auto* siege = find(carrier);
	const auto* portal = DynamicPortalBehaviour::find(carrier);
	if (!siege || !siege->isDocked() || !portal || !rider || !portal->allowsClimber(*rider) ||
		!portal->getClimbEntryPosition(position))
		return FALSE;
	position.z = TheTerrainLogic->getLayerHeight(position.x, position.y, LAYER_GROUND);
	return TRUE;
}

Bool SiegeAIUpdate::isAllowedToAdjustDestination() const
{
	return m_phase != Approaching || !m_skipAdjustPosition;
}

Bool SiegeAIUpdate::chooseSocket(SiegeDockingBehavior& wall, CommandSourceType source)
{
	Object* carrier = getObject();
	SiegeDockingBehavior::Socket sockets[10];
	const Int count = wall.getSockets(carrier, sockets);
	Real closest = 0;
	Int selected = -1;
	for (Int slot = 0; slot < count; ++slot)
	{
		if ((m_rejected & (1 << slot)) || !wall.available(carrier, slot))
			continue;
		auto& socket = sockets[slot];
		socket.position.x -= m_retreatDistance * WWMath::CosTrig(socket.facing);
		socket.position.y -= m_retreatDistance * WWMath::SinTrig(socket.facing);
		socket.position.z = TheTerrainLogic->getLayerHeight(socket.position.x, socket.position.y, LAYER_GROUND);
		const Real distance = distanceSquared(*carrier->getPosition(), socket.position);
		const auto* cell = TheAI->pathfinder()->getCell(LAYER_GROUND, &socket.position);
		if (!cell)
			continue;
		const ObjectID obstruction = cell->getObstacleID();
		if (obstruction != INVALID_ID && obstruction != wall.getWallID() && obstruction != carrier->getID())
			continue;
		if (selected < 0 || distance < closest)
		{
			closest = distance;
			selected = slot;
		}
	}
	if (selected < 0)
		return FALSE;

	m_rejected |= 1 << selected;
	m_dockPosition = sockets[selected].position;
	m_dockFacing = sockets[selected].facing;
	wall.reserve(carrier, selected);
	if (closest > 0.0f)
	{
		AICommandParms move(AICMD_MOVE_TO_POSITION, source);
		move.m_pos = m_dockPosition;
		AIUpdateInterface::aiDoCommand(&move);
	}
	return TRUE;
}

Bool SiegeAIUpdate::chooseNearbyWall(const Object& wall, CommandSourceType source)
{
	if (m_wallSearchDistance <= 0)
		return FALSE;
	const Coord3D center = *wall.getPosition();
	const Real radiusSquared = m_wallSearchDistance * m_wallSearchDistance;
	m_wallSearchDistance = 0;
	std::vector<Object*> candidates;
	for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (obj != &wall && distanceSquared(center, *obj->getPosition()) <= radiusSquared && SiegeDockingBehavior::find(obj))
			candidates.push_back(obj);
	}
	std::sort(candidates.begin(), candidates.end(), [&center](const Object* left, const Object* right) {
		const Real leftDistance = distanceSquared(center, *left->getPosition());
		const Real rightDistance = distanceSquared(center, *right->getPosition());
		return leftDistance == rightDistance ? left->getID() < right->getID() : leftDistance < rightDistance;
	});
	for (Object* candidate : candidates)
	{
		m_rejected = 0;
		if (chooseSocket(*SiegeDockingBehavior::find(candidate), source))
		{
			m_wall = candidate->getID();
			return TRUE;
		}
	}
	return FALSE;
}

Bool SiegeAIUpdate::beginDock(Object* wall, const SiegeDeploySpecialPowerModuleData& data, CommandSourceType source)
{
	auto* docking = SiegeDockingBehavior::find(wall);
	if (!docking || m_phase != Idle || getObject()->isEffectivelyDead())
		return FALSE;
	m_wall = wall->getID();
	m_lowerDelay = data.m_lowerDelay;
	m_raiseDelay = data.m_raiseDelay;
	m_retreatDistance = data.m_extraWallDistance;
	m_wallSearchDistance = data.m_wallSearchDistance;
	m_evacuate = data.m_evacuate;
	m_skipAdjustPosition = data.m_skipAdjustPosition;
	setPhase(Approaching);
	if (!chooseSocket(*docking, source) && !chooseNearbyWall(*wall, source))
	{
		resetDock();
		return FALSE;
	}
	setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
	return TRUE;
}

void SiegeAIUpdate::setPhase(Phase phase)
{
	m_phase = phase;
	Object* carrier = getObject();
	carrier->clearModelConditionState(MODELCONDITION_UNPACKING);
	carrier->clearModelConditionState(MODELCONDITION_PACKING);
	carrier->clearModelConditionState(MODELCONDITION_DEPLOYED);
	carrier->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_DEPLOYED), phase == Docked);
	carrier->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_IMMOBILE),
		phase == Lowering || phase == Docked || phase == Raising);
	if (phase == Lowering)
		carrier->setModelConditionState(MODELCONDITION_UNPACKING);
	else if (phase == Raising)
		carrier->setModelConditionState(MODELCONDITION_PACKING);
	else if (phase == Docked)
		carrier->setModelConditionState(MODELCONDITION_DEPLOYED);
}

void SiegeAIUpdate::resetDock()
{
	if (auto* wall = SiegeDockingBehavior::find(TheGameLogic->findObjectByID(m_wall)))
		wall->release(getObject());
	m_wall = INVALID_ID;
	m_rejected = 0;
	m_deadline = 0;
	m_dockPosition = {};
	m_dockFacing = 0;
	m_evacuate = FALSE;
	m_skipAdjustPosition = FALSE;
	m_lowerDelay = 0;
	m_raiseDelay = 0;
	m_retreatDistance = 0;
	m_wallSearchDistance = 0;
	setPhase(Idle);
}

Bool SiegeAIUpdate::stopDock()
{
	if (!isUsingDock())
		return FALSE;
	if (m_phase == Approaching)
	{
		resetDock();
		AIUpdateInterface::aiIdle(CMD_FROM_AI);
	}
	else
	{
		setPhase(Raising);
		m_deadline = TheGameLogic->getFrame() + m_raiseDelay;
	}
	return TRUE;
}

void SiegeAIUpdate::onDelete()
{
	if (m_phase != Idle)
		resetDock();
	AIUpdateInterface::onDelete();
}

void SiegeAIUpdate::aiDoCommand(const AICommandParms* parms)
{
	if (RetailFileLayers::enabled() && m_phase == Idle && parms->m_cmd == AICMD_ATTACK_OBJECT &&
		parms->m_cmdSource != CMD_FROM_PLAYER && SiegeDockingBehavior::find(parms->m_obj))
	{
		if (auto* power = getObject()->findSpecialPowerModuleInterface(SPECIAL_SIEGEDEPLOY))
		{
			power->doSpecialPowerAtObject(parms->m_obj, COMMAND_OPTION_INTERNAL_AUTO |
				(parms->m_cmdSource == CMD_FROM_SCRIPT ? COMMAND_FIRED_BY_SCRIPT : 0));
			if (power->wasLastCastAccepted())
				return;
		}
	}
	if (m_phase != Idle && m_phase != Approaching && parms->m_cmdSource == CMD_FROM_AI)
	{
		switch (parms->m_cmd)
		{
			case AICMD_MOVE_TO_OBJECT:
			case AICMD_MOVE_TO_POSITION:
			case AICMD_MOVE_AWAY_FROM_UNIT:
			case AICMD_ATTACKMOVE_TO_POSITION:
				return;
			default:
				break;
		}
	}
	if (m_phase != Idle && parms->m_cmdSource != CMD_FROM_AI)
	{
		switch (parms->m_cmd)
		{
			case AICMD_IDLE:
			case AICMD_MOVE_TO_OBJECT:
			case AICMD_MOVE_TO_POSITION:
			case AICMD_ATTACKMOVE_TO_POSITION:
				if (m_phase != Approaching)
				{
					stopDock();
					return;
				}
				resetDock();
				break;
			default:
				break;
		}
	}
	AIUpdateInterface::aiDoCommand(parms);
}

Bool SiegeAIUpdate::getReleasePosition(Coord3D& position) const
{
	if (!isDocked())
		return FALSE;
	const auto* portal = DynamicPortalBehaviour::find(getObject());
	if (!portal || !portal->getAboveWallPosition(position))
		return FALSE;
	auto* pathfinder = TheAI->pathfinder();
	if (!pathfinder->isPointOnWall(&position))
	{
		const Coord3D start = position;
		const Real maxOffset = getObject()->getGeometryInfo().getMinorRadius();
		const Real dx = WWMath::CosTrig(m_dockFacing);
		const Real dy = WWMath::SinTrig(m_dockFacing);
		for (Real offset = PATHFIND_CELL_SIZE; offset <= maxOffset; offset += PATHFIND_CELL_SIZE)
		{
			position.x = start.x + offset * dx;
			position.y = start.y + offset * dy;
			if (pathfinder->isPointOnWall(&position))
				break;
			position.x = start.x - offset * dx;
			position.y = start.y - offset * dy;
			if (pathfinder->isPointOnWall(&position))
				break;
		}
		if (!pathfinder->isPointOnWall(&position))
			return FALSE;
	}
	position.z = TheTerrainLogic->getLayerHeight(position.x, position.y, LAYER_WALL);
	return TRUE;
}

Bool SiegeAIUpdate::isDockedAt(const Object* wall) const
{
	return isDocked() && wall && wall->getID() == m_wall;
}

UpdateSleepTime SiegeAIUpdate::update()
{
	const UpdateSleepTime sleep = AIUpdateInterface::update();
	if (!RetailFileLayers::enabled() || m_phase == Idle)
		return sleep;
	Object* carrier = getObject();
	Object* target = TheGameLogic->findObjectByID(m_wall);
	auto* wall = SiegeDockingBehavior::find(target);
	if (carrier->isEffectivelyDead() || !wall)
	{
		resetDock();
		return sleep;
	}

	const UnsignedInt frame = TheGameLogic->getFrame();
	if (m_phase == Approaching && (isIdle() || ThePartitionManager->isColliding(carrier, target)))
	{
		const Real facingError = normalizeAngle(m_dockFacing - carrier->getOrientation());
		const Real facingTolerance = 0.15f;
		const Bool aligned = WWMath::Fabs(facingError) < facingTolerance;
		const auto* portal = DynamicPortalBehaviour::find(carrier);
		if (aligned && portal && portal->canDockOnWall())
		{
			AIUpdateInterface::aiIdle(CMD_FROM_AI);
			setPhase(Lowering);
			m_deadline = frame + m_lowerDelay;
		}
		else if (isIdle() && !aligned)
		{
			Coord3D lookAt = *carrier->getPosition();
			lookAt.x += WWMath::CosTrig(m_dockFacing);
			lookAt.y += WWMath::SinTrig(m_dockFacing);
			AIUpdateInterface::aiFacePosition(&lookAt, CMD_FROM_AI);
		}
		else if (isIdle())
		{
			wall->release(carrier);
			if (!chooseSocket(*wall, CMD_FROM_AI) && !chooseNearbyWall(*target, CMD_FROM_AI))
				resetDock();
		}
	}
	if (m_phase == Lowering && frame >= m_deadline)
		setPhase(Docked);
	if (isDocked() && m_evacuate)
		if (auto* contain = SiegeEngineContain::find(carrier))
			contain->releasePassengersOnWall();
	if (m_phase == Raising && frame >= m_deadline)
	{
		Coord3D retreat = *carrier->getPosition();
		retreat.x -= m_retreatDistance * WWMath::CosTrig(m_dockFacing);
		retreat.y -= m_retreatDistance * WWMath::SinTrig(m_dockFacing);
		const Bool retreats = m_retreatDistance > 0;
		resetDock();
		if (retreats)
			AIUpdateInterface::aiMoveToPosition(&retreat, CMD_FROM_AI);
	}
	return m_phase == Idle ? sleep : UPDATE_SLEEP_NONE;
}

void SiegeAIUpdate::transferDock(Xfer* xfer)
{
	RetailSaveBlocks::transferSection(xfer, xfer->getXferMode() == XFER_LOAD, TRUE, {
		{ "siege-ai-dock", [this](Xfer* x) {
			x->xferUser(&m_phase, sizeof(m_phase));
			x->xferObjectID(&m_wall);
			x->xferInt(&m_rejected);
			x->xferCoord3D(&m_dockPosition);
			x->xferReal(&m_dockFacing);
			x->xferUnsignedInt(&m_deadline);
			x->xferUnsignedInt(&m_lowerDelay);
			x->xferUnsignedInt(&m_raiseDelay);
			x->xferReal(&m_retreatDistance);
			x->xferReal(&m_wallSearchDistance);
			x->xferBool(&m_evacuate);
			x->xferBool(&m_skipAdjustPosition);
		}, {} }
	});
}

void SiegeAIUpdate::crc(Xfer* xfer)
{
	AIUpdateInterface::crc(xfer);
	if (RetailFileLayers::enabled())
		transferDock(xfer);
}

void SiegeAIUpdate::xfer(Xfer* xfer)
{
	AIUpdateInterface::xfer(xfer);
	if (RetailFileLayers::enabled())
		transferDock(xfer);
}

void SiegeAIUpdate::loadPostProcess()
{
	AIUpdateInterface::loadPostProcess();
}

void SiegeDeploySpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& fields)
{
	SpecialPowerModuleData::buildFieldParse(fields);
	static const FieldParse table[] = {
		{ "LowerDelay", INI::parseDurationUnsignedInt, nullptr, offsetof(SiegeDeploySpecialPowerModuleData, m_lowerDelay) },
		{ "RaiseDelay", INI::parseDurationUnsignedInt, nullptr, offsetof(SiegeDeploySpecialPowerModuleData, m_raiseDelay) },
		{ "EvacuatePassengersOnDeploy", INI::parseBool, nullptr, offsetof(SiegeDeploySpecialPowerModuleData, m_evacuate) },
		{ "SkipAdjustPosition", INI::parseBool, nullptr, offsetof(SiegeDeploySpecialPowerModuleData, m_skipAdjustPosition) },
		{ "WallSearchDistance", INI::parseReal, nullptr, offsetof(SiegeDeploySpecialPowerModuleData, m_wallSearchDistance) },
		{ "ExtraWallDistance", INI::parseReal, nullptr, offsetof(SiegeDeploySpecialPowerModuleData, m_extraWallDistance) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	fields.add(table);
}

SiegeDeploySpecialPower::SiegeDeploySpecialPower(Thing* thing, const ModuleData* data)
	: SpecialPowerModule(thing, data)
{
}

SiegeDeploySpecialPower::~SiegeDeploySpecialPower() = default;

Bool SiegeDeploySpecialPower::isValidPowerTarget(const Object* target) const
{
	const auto* wall = SiegeDockingBehavior::find(target);
	if (!wall)
		return FALSE;
	SiegeDockingBehavior::Socket sockets[10];
	const Int count = wall->getSockets(getObject(), sockets);
	for (Int slot = 0; slot < count; ++slot)
		if (wall->available(getObject(), slot))
			return TRUE;
	return count > 0 && getSiegeDeploySpecialPowerModuleData()->m_wallSearchDistance > 0;
}

Bool SiegeDeploySpecialPower::isRetailCastAllowed(UnsignedInt options) const
{
	const auto* ai = SiegeAIUpdate::find(getObject());
	return ai && ai->canBeginDock() && !getObject()->isDisabled() &&
		SpecialPowerModule::isRetailCastAllowed(options);
}

Real SiegeDeploySpecialPower::getPercentReady() const
{
	return isRetailCastAllowed(COMMAND_OPTION_INTERNAL_AUTO) ? SpecialPowerModule::getPercentReady() : 0.0f;
}

void SiegeDeploySpecialPower::doSpecialPowerAtObject(Object* target, UnsignedInt options)
{
	m_lastCastAccepted = FALSE;
	auto* ai = SiegeAIUpdate::find(getObject());
	if (!ai || !isReady() || !isRetailCastAllowed(options) || !isValidPowerTarget(target) ||
		!CommandButton::isRetailPowerObjectTargetAllowed(getObject(), getSpecialPowerTemplate(), target))
		return;
	const auto* data = getSiegeDeploySpecialPowerModuleData();
	const CommandSourceType source = (options & COMMAND_FIRED_BY_SCRIPT) ? CMD_FROM_SCRIPT :
		(options & COMMAND_OPTION_INTERNAL_AUTO) ? CMD_FROM_AI : CMD_FROM_PLAYER;
	m_lastCastAccepted = ai->beginDock(target, *data, source);
	if (m_lastCastAccepted)
	{
		AudioEventRTS event(getInitiateSound());
		event.setObjectID(getObject()->getID());
		TheAudio->addAudioEvent(&event);
		markSpecialPowerTriggered(target->getPosition());
	}
}

void StopSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& fields)
{
	SpecialPowerModuleData::buildFieldParse(fields);
	static const FieldParse table[] = {
		{ "StopPowerTemplate", INI::parseAsciiString, nullptr, offsetof(StopSpecialPowerModuleData, m_stoppedPower) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	fields.add(table);
}

StopSpecialPower::StopSpecialPower(Thing* thing, const ModuleData* data)
	: SpecialPowerModule(thing, data)
{
}

StopSpecialPower::~StopSpecialPower() = default;

Bool StopSpecialPower::isRetailCastAllowed(UnsignedInt options) const
{
	const auto* ai = SiegeAIUpdate::find(getObject());
	const auto& stoppedPower = getStopSpecialPowerModuleData()->m_stoppedPower;
	const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(stoppedPower);
	return ai && ai->isUsingDock() && power &&
		getObject()->findSpecialPowerModuleInterface(power->getSpecialPowerType()) &&
		SpecialPowerModule::isRetailCastAllowed(options);
}

Real StopSpecialPower::getPercentReady() const
{
	return isRetailCastAllowed(COMMAND_OPTION_INTERNAL_AUTO) ? SpecialPowerModule::getPercentReady() : 0.0f;
}

void StopSpecialPower::doSpecialPower(UnsignedInt options)
{
	m_lastCastAccepted = FALSE;
	if (!isReady() || !isRetailCastAllowed(options))
		return;
	m_lastCastAccepted = SiegeAIUpdate::find(getObject())->stopDock();
	if (m_lastCastAccepted)
		markSpecialPowerTriggered(nullptr);
}

void SiegeDeploySpecialPower::crc(Xfer* xfer)
{
	SpecialPowerModule::crc(xfer);
}

void SiegeDeploySpecialPower::xfer(Xfer* xfer)
{
	SpecialPowerModule::xfer(xfer);
}

void SiegeDeploySpecialPower::loadPostProcess()
{
	SpecialPowerModule::loadPostProcess();
}

void StopSpecialPower::crc(Xfer* xfer)
{
	SpecialPowerModule::crc(xfer);
}

void StopSpecialPower::xfer(Xfer* xfer)
{
	SpecialPowerModule::xfer(xfer);
}

void StopSpecialPower::loadPostProcess()
{
	SpecialPowerModule::loadPostProcess();
}

void SiegeDeployHordeSpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& fields)
{
	SpecialPowerModuleData::buildFieldParse(fields);
	static const FieldParse table[] = {
		{ "HordeDeploy", INI::parseBool, nullptr, offsetof(SiegeDeployHordeSpecialPowerModuleData, m_hordeDeploy) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	fields.add(table);
}

SiegeDeployHordeSpecialPower::SiegeDeployHordeSpecialPower(Thing* thing, const ModuleData* data)
	: SpecialPowerModule(thing, data)
{
}

SiegeDeployHordeSpecialPower::~SiegeDeployHordeSpecialPower() = default;

Object* SiegeDeployHordeSpecialPower::findCarrier(const Object* target) const
{
	const auto* wall = SiegeDockingBehavior::find(target);
	return wall ? wall->findDockedCarrierFor(*getObject()) : nullptr;
}

Bool SiegeDeployHordeSpecialPower::isValidPowerTarget(const Object* target) const
{
	return getSiegeDeployHordeSpecialPowerModuleData()->m_hordeDeploy ?
		findMemberCarrier(target) != nullptr : findCarrier(target) != nullptr;
}

Object* SiegeDeployHordeSpecialPower::findMemberCarrier(const Object* target) const
{
	const auto* horde = HordeContain::get(getObject());
	if (!horde)
		return nullptr;
	for (Object* member : *horde->getContainedItemsList())
	{
		if (!member || member->isEffectivelyDead() || !member->isKindOf(KINDOF_SIEGE_TOWER))
			continue;
		const auto* power = member->findSpecialPowerModuleInterface(getSpecialPowerTemplate()->getSpecialPowerType());
		return power && power->isReady() && power->isValidPowerTarget(target) ? member : nullptr;
	}
	return nullptr;
}

void SiegeDeployHordeSpecialPower::doSpecialPowerAtObject(Object* target, UnsignedInt options)
{
	m_lastCastAccepted = FALSE;
	if (!RetailFileLayers::enabled() || !isReady() || !isRetailCastAllowed(options) || getObject()->isDisabled() ||
		!CommandButton::isRetailPowerObjectTargetAllowed(getObject(), getSpecialPowerTemplate(), target))
		return;
	if (getSiegeDeployHordeSpecialPowerModuleData()->m_hordeDeploy)
	{
		Object* member = findMemberCarrier(target);
		if (!member)
			return;
		const auto type = getSpecialPowerTemplate()->getSpecialPowerType();
		auto* power = member->findSpecialPowerModuleInterface(type);
		if (!power->isRetailCastAllowed(options) ||
			!CommandButton::isRetailPowerObjectTargetAllowed(member, power->getSpecialPowerTemplate(), target))
			return;
		auto* horde = static_cast<HordeContain*>(getObject()->getContain());
		horde->removeFromContain(member);
		if (auto* ai = member->getAI())
			ai->aiIdle(CMD_FROM_AI);
		power->doSpecialPowerAtObject(target, options);
		m_lastCastAccepted = power->wasLastCastAccepted();
		if (!m_lastCastAccepted)
		{
			horde->addToContain(member);
			return;
		}
		auto* tracker = member->getExperienceTracker();
		if (tracker->getExperienceSink() == getObject()->getID())
			tracker->setExperienceSink(INVALID_ID);
		markSpecialPowerTriggered(target->getPosition());
#if !RTS_GENERALS
		std::vector<Object*> results;
		if (!horde->splitHorde(&results))
			return;
		for (Object* result : results)
		{
			if (!dynamic_cast<SiegeDeployHordeSpecialPower*>(result->findSpecialPowerModuleInterface(type)))
				continue;
			if (auto* ai = dynamic_cast<HordeAIUpdate*>(result->getAI()))
				ai->startSiegeDeployment(member->getID(), target->getID());
		}
#endif
		return;
	}
	Object* carrier = findCarrier(target);
	auto* ai = getObject()->getAI();
	if (!carrier || !ai)
		return;
	const CommandSourceType source = (options & COMMAND_FIRED_BY_SCRIPT) ? CMD_FROM_SCRIPT :
		(options & COMMAND_OPTION_INTERNAL_AUTO) ? CMD_FROM_AI : CMD_FROM_PLAYER;
	if (!getObject()->isMobile() || !TheActionManager->canEnterObject(getObject(), carrier, source, CHECK_CAPACITY))
		return;
	ai->aiEnter(carrier, source);
	m_lastCastAccepted = TRUE;
	markSpecialPowerTriggered(target->getPosition());
}

void SiegeDeployHordeSpecialPower::crc(Xfer* xfer)
{
	SpecialPowerModule::crc(xfer);
}

void SiegeDeployHordeSpecialPower::xfer(Xfer* xfer)
{
	SpecialPowerModule::xfer(xfer);
}

void SiegeDeployHordeSpecialPower::loadPostProcess()
{
	SpecialPowerModule::loadPostProcess();
}

ManTheWallsSpecialPower::ManTheWallsSpecialPower(Thing* thing, const ModuleData* data)
	: SpecialPowerModule(thing, data)
{
}

ManTheWallsSpecialPower::~ManTheWallsSpecialPower() = default;

Bool ManTheWallsSpecialPower::isValidPowerTarget(const Object* target) const
{
	if (!RetailFileLayers::enabled() || !target || target->isEffectivelyDead() || target->getContainedBy())
		return FALSE;
	const auto* contain = getObject()->getContain();
	return contain && contain->isGarrisonable() && contain->isValidContainerFor(target, TRUE);
}

void ManTheWallsSpecialPower::doSpecialPowerAtObject(Object* target, UnsignedInt options)
{
	m_lastCastAccepted = FALSE;
	if (!isReady() || !isRetailCastAllowed(options) || getObject()->isDisabled() || !isValidPowerTarget(target))
		return;
	getObject()->getContain()->addToContain(target);
	m_lastCastAccepted = target->getContainedBy() == getObject();
	if (m_lastCastAccepted)
		markSpecialPowerTriggered(target->getPosition());
}

void ManTheWallsSpecialPower::crc(Xfer* xfer)
{
	SpecialPowerModule::crc(xfer);
}

void ManTheWallsSpecialPower::xfer(Xfer* xfer)
{
	SpecialPowerModule::xfer(xfer);
}

void ManTheWallsSpecialPower::loadPostProcess()
{
	SpecialPowerModule::loadPostProcess();
}
