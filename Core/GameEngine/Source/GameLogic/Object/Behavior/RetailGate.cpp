#include "PreRTS.h"

#include "GameLogic/Module/RetailGate.h"

#include <algorithm>

#include "Common/AudioEventRTS.h"
#include "Common/GameAudio.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "WWMath/wwmath.h"

void GateOpenAndCloseBehaviorModuleData::parseGeometryList(INI* ini, void*, void* store, const void*)
{
	auto& names = *static_cast<std::vector<AsciiString>*>(store);
	names.clear();
	while (const char* token = ini->getNextTokenOrNull())
		names.emplace_back(token);
}

void GateOpenAndCloseBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "ResetTimeInMilliseconds", INI::parseDurationUnsignedInt, nullptr, offsetof(GateOpenAndCloseBehaviorModuleData, m_resetTime) },
		{ "OpenByDefault", INI::parseBool, nullptr, offsetof(GateOpenAndCloseBehaviorModuleData, m_openByDefault) },
		{ "PercentOpenForPathing", INI::parseUnsignedInt, nullptr, offsetof(GateOpenAndCloseBehaviorModuleData, m_pathingPercent) },
		{ "SoundOpeningGateLoop", INI::parseAsciiString, nullptr, offsetof(GateOpenAndCloseBehaviorModuleData, m_openingSound) },
		{ "SoundClosingGateLoop", INI::parseAsciiString, nullptr, offsetof(GateOpenAndCloseBehaviorModuleData, m_closingSound) },
		{ "SoundFinishedOpeningGate", INI::parseAsciiString, nullptr, offsetof(GateOpenAndCloseBehaviorModuleData, m_openedSound) },
		{ "SoundFinishedClosingGate", INI::parseAsciiString, nullptr, offsetof(GateOpenAndCloseBehaviorModuleData, m_closedSound) },
		{ "TimeBeforePlayingOpenSound", INI::parseDurationUnsignedInt, nullptr, offsetof(GateOpenAndCloseBehaviorModuleData, m_openSoundTime) },
		{ "TimeBeforePlayingClosedSound", INI::parseDurationUnsignedInt, nullptr, offsetof(GateOpenAndCloseBehaviorModuleData, m_closeSoundTime) },
		{ "RepelCollidingUnits", INI::parseBool, nullptr, offsetof(GateOpenAndCloseBehaviorModuleData, m_repelUnits) },
		{ "Proxy", INI::parseAsciiString, nullptr, offsetof(GateOpenAndCloseBehaviorModuleData, m_proxy) },
		{ "GeometryForClosed", parseGeometryList, nullptr, offsetof(GateOpenAndCloseBehaviorModuleData, m_closedGeometry) },
		{ "GeometryForOpen", parseGeometryList, nullptr, offsetof(GateOpenAndCloseBehaviorModuleData, m_openGeometry) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields, 0);
}

GateOpenAndCloseBehavior::GateOpenAndCloseBehavior(Thing* thing, const ModuleData* data)
	: UpdateModule(thing, data)
{
	m_wantsOpen = getGateOpenAndCloseBehaviorModuleData()->m_openByDefault;
	m_passable = m_wantsOpen;
}

GateOpenAndCloseBehavior::~GateOpenAndCloseBehavior() = default;

GateOpenAndCloseBehavior* GateOpenAndCloseBehavior::find(const Object* object)
{
	if (!object || !RetailFileLayers::enabled() || !object->areModulesReady())
		return nullptr;
	return static_cast<GateOpenAndCloseBehavior*>(object->findUpdateModule(NAMEKEY("GateOpenAndCloseBehavior")));
}

Bool GateOpenAndCloseBehavior::canPass(const Object* obstacle, const Object* unit, Bool planning)
{
	if (!RetailFileLayers::enabled() || !unit || !unit->getAIUpdateInterface() || unit->isKindOf(KINDOF_PROJECTILE))
		return FALSE;
	const auto* gate = find(obstacle);
	return gate && gate->permits(*unit, planning);
}

Bool GateOpenAndCloseBehavior::couldConnect(const Object* unit)
{
	if (!unit || !RetailFileLayers::enabled())
		return FALSE;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->isKindOf(KINDOF_WALL_GATE) && canPass(object, unit, TRUE))
			return TRUE;
	}
	return FALSE;
}

Bool GateOpenAndCloseBehavior::permits(const Object& unit, Bool planning) const
{
	const Object* gate = getObject();
	if (gate->isEffectivelyDead() || gate->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
		return FALSE;
	const auto* portal = static_cast<const FakePathfindPortalBehaviour*>(
		gate->findUpdateModule(NAMEKEY("FakePathfindPortalBehaviour")));
	if (portal ? !portal->permits(unit) : gate->getControllingPlayer() != unit.getControllingPlayer())
		return FALSE;
	return m_passable || (planning && (m_wantsOpen || gate->findUpdateModule(NAMEKEY("AIGateUpdate"))));
}

void GateOpenAndCloseBehavior::playSound(const AsciiString& name) const
{
	if (!TheAudio || name.isEmpty())
		return;
	AudioEventRTS event(name.str());
	event.setObjectID(getObject()->getID());
	TheAudio->addAudioEvent(&event);
}

void GateOpenAndCloseBehavior::applyModelState()
{
	Object* gate = getObject();
	gate->clearAndSetModelConditionFlags(MAKE_MODELCONDITION_MASK4(MODELCONDITION_DOOR_1_OPENING,
		MODELCONDITION_DOOR_1_CLOSING, MODELCONDITION_DOOR_1_WAITING_OPEN, MODELCONDITION_DOOR_1_WAITING_TO_CLOSE),
		MAKE_MODELCONDITION_MASK(m_wantsOpen ? MODELCONDITION_DOOR_1_OPENING : MODELCONDITION_DOOR_1_CLOSING));
}

void GateOpenAndCloseBehavior::request(Bool open)
{
	if (open == m_wantsOpen || getObject()->isEffectivelyDead() ||
		getObject()->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
		return;

	const auto* data = getGateOpenAndCloseBehaviorModuleData();
	const UnsignedInt frame = TheGameLogic->getFrame();
	m_wantsOpen = open;
	m_moving = TRUE;
	m_soundPending = TRUE;
	m_endFrame = frame + std::max(1u, data->m_resetTime);
	const Real fraction = std::min(200u, data->m_pathingPercent) * 0.01f;
	const Real delay = open ? fraction : std::max(0.0f, 1.0f - fraction);
	m_pathFrame = frame + static_cast<UnsignedInt>(data->m_resetTime * delay);
	m_soundFrame = frame + (open ? data->m_openSoundTime : data->m_closeSoundTime);
	applyModelState();
	playSound(open ? data->m_openingSound : data->m_closingSound);
	setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
}

void GateOpenAndCloseBehavior::configureGeometry()
{
	Object* gate = getObject();
	const auto& geometry = gate->getTemplate()->getRotWKObjectData();
	if (geometry.additionalGeometries.empty())
		return;

	const auto* data = getGateOpenAndCloseBehaviorModuleData();
	Real halfX = gate->getGeometryInfo().getMajorRadius();
	Real halfY = gate->getGeometryInfo().getMinorRadius();
	Real height = gate->getGeometryInfo().getMaxHeightAbovePosition();
	for (const auto& part : geometry.additionalGeometries)
	{
		const auto sameName = [&part](const AsciiString& name) { return part.details.name.compareNoCase(name) == 0; };
		const Bool closed = data->m_closedGeometry.empty() ? part.details.name.compareNoCase("Closed") == 0 :
			std::any_of(data->m_closedGeometry.begin(), data->m_closedGeometry.end(), sameName);
		const Bool openOnly = data->m_openGeometry.empty() ? part.details.name.compareNoCase("OpenLeft") == 0 ||
			part.details.name.compareNoCase("OpenRight") == 0 :
			std::any_of(data->m_openGeometry.begin(), data->m_openGeometry.end(), sameName);
		if (!closed && (!part.details.active || openOnly))
			continue;
		halfX = std::max(halfX, WWMath::Fabs(part.details.offset.x) + part.shape.getMajorRadius());
		halfY = std::max(halfY, WWMath::Fabs(part.details.offset.y) + part.shape.getMinorRadius());
		height = std::max(height, part.details.offset.z + part.shape.getMaxHeightAbovePosition());
	}
	TheAI->pathfinder()->removeObjectFromPathfindMap(gate);
	gate->setGeometryInfo(GeometryInfo(GEOMETRY_BOX, FALSE, height, halfX, halfY));
	TheAI->pathfinder()->addObjectToPathfindMap(gate);
}

UpdateSleepTime GateOpenAndCloseBehavior::update()
{
	if (getObject()->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;
	if (!m_geometryReady)
	{
		configureGeometry();
		m_geometryReady = TRUE;
		applyModelState();
	}
	if (getObject()->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
		return UPDATE_SLEEP(LOGICFRAMES_PER_SECOND / 4);
	const UnsignedInt frame = TheGameLogic->getFrame();
	const auto* data = getGateOpenAndCloseBehaviorModuleData();
	if (frame >= m_pathFrame && m_passable != m_wantsOpen)
	{
		m_passable = m_wantsOpen;
		if (m_passable && RetailFileLayers::enabled())
			TheAI->pathfinder()->noteRetailRouteOpening();
	}
	if (m_soundPending && frame >= m_soundFrame)
	{
		playSound(m_wantsOpen ? data->m_openedSound : data->m_closedSound);
		m_soundPending = FALSE;
	}
	if (m_moving && frame >= m_endFrame)
		m_moving = FALSE;
	return m_moving || m_soundPending || m_passable != m_wantsOpen ? UPDATE_SLEEP_NONE : UPDATE_SLEEP_FOREVER;
}

void GateOpenAndCloseBehavior::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	xfer->xferBool(&m_wantsOpen);
	xfer->xferBool(&m_passable);
	xfer->xferUnsignedInt(&m_pathFrame);
}

void GateOpenAndCloseBehavior::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
	xfer->xferUnsignedInt(&m_pathFrame);
	xfer->xferUnsignedInt(&m_soundFrame);
	xfer->xferUnsignedInt(&m_endFrame);
	xfer->xferBool(&m_wantsOpen);
	xfer->xferBool(&m_passable);
	xfer->xferBool(&m_moving);
	xfer->xferBool(&m_soundPending);
	xfer->xferBool(&m_geometryReady);
}

void GateOpenAndCloseBehavior::loadPostProcess()
{
	UpdateModule::loadPostProcess();
	applyModelState();
}

void AIGateUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "TriggerWidthX", INI::parseReal, nullptr, offsetof(AIGateUpdateModuleData, m_triggerWidthX) },
		{ "TriggerWidthY", INI::parseReal, nullptr, offsetof(AIGateUpdateModuleData, m_triggerWidthY) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields, 0);
}

AIGateUpdate::AIGateUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data) {}
AIGateUpdate::~AIGateUpdate() = default;

UpdateSleepTime AIGateUpdate::update()
{
	Object* object = getObject();
	GateOpenAndCloseBehavior* gate = GateOpenAndCloseBehavior::find(object);
	if (!gate || object->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;
	if (object->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
		return UPDATE_SLEEP(LOGICFRAMES_PER_SECOND / 4);

	const auto* data = getAIGateUpdateModuleData();
	const Real halfX = data->m_triggerWidthX * 0.5f;
	const Real halfY = data->m_triggerWidthY * 0.5f;
	const Coord3D* center = object->getPosition();
	const Real cosAngle = WWMath::CosTrig(object->getOrientation());
	const Real sinAngle = WWMath::SinTrig(object->getOrientation());
	const Real radius = WWMath::SqrtOrigin(halfX * halfX + halfY * halfY);
	Bool friendly = FALSE;
	Bool enemy = FALSE;
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(center, radius, FROM_CENTER_2D);
	for (Object* unit = iter->first(); unit; unit = iter->next())
	{
		if (!unit->getAIUpdateInterface() || unit->isEffectivelyDead() || unit->isKindOf(KINDOF_STRUCTURE) ||
			unit->isSignificantlyAboveTerrain() || unit->getContainedBy())
			continue;
		const Coord3D* pos = unit->getPosition();
		const Real dx = pos->x - center->x;
		const Real dy = pos->y - center->y;
		if (WWMath::Fabs(dx * cosAngle + dy * sinAngle) > halfX ||
			WWMath::Fabs(-dx * sinAngle + dy * cosAngle) > halfY)
			continue;
		enemy |= object->getRelationship(unit) == ENEMIES;
		friendly |= GateOpenAndCloseBehavior::canPass(object, unit, TRUE);
	}
	deleteInstance(iter);
	if (enemy || friendly)
		gate->request(friendly && !enemy);
	return UPDATE_SLEEP(LOGICFRAMES_PER_SECOND / 4);
}

void AIGateUpdate::crc(Xfer* xfer) { UpdateModule::crc(xfer); }

void AIGateUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
}

void AIGateUpdate::loadPostProcess() { UpdateModule::loadPostProcess(); }

void FakePathfindPortalBehaviourModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "AllowEnemies", INI::parseBool, nullptr, offsetof(FakePathfindPortalBehaviourModuleData, m_allowEnemies) },
		{ "AllowNonSkirmishAIUnits", INI::parseBool, nullptr, offsetof(FakePathfindPortalBehaviourModuleData, m_allowNonSkirmishAI) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields, 0);
}

FakePathfindPortalBehaviour::FakePathfindPortalBehaviour(Thing* thing, const ModuleData* data)
	: UpdateModule(thing, data)
{
	setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
}

FakePathfindPortalBehaviour::~FakePathfindPortalBehaviour() = default;

Bool FakePathfindPortalBehaviour::permits(const Object& unit) const
{
	const Player* player = unit.getControllingPlayer();
	const auto* data = getFakePathfindPortalBehaviourModuleData();
	if (!player || (!data->m_allowNonSkirmishAI && player->getPlayerType() == PLAYER_COMPUTER &&
		!TheGameLogic->isInSkirmishGame() && !TheGameLogic->isInMultiplayerGame()))
		return FALSE;
	if (player == getObject()->getControllingPlayer())
		return TRUE;
	return data->m_allowEnemies && getObject()->getRelationship(&unit) == ENEMIES;
}

void FakePathfindPortalBehaviour::crc(Xfer* xfer) { UpdateModule::crc(xfer); }

void FakePathfindPortalBehaviour::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
}

void FakePathfindPortalBehaviour::loadPostProcess() { UpdateModule::loadPostProcess(); }
