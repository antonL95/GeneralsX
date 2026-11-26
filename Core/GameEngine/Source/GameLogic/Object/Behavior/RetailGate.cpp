#include "PreRTS.h"

#include "GameLogic/Module/RetailGate.h"
#include "GameLogic/Module/RetailSiegeDocking.h"
#include "GameLogic/Module/WallHubBehavior.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Common/AudioEventRTS.h"
#include "Common/GameAudio.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailSaveBlocks.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
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

GateOpenAndCloseBehavior* GateOpenAndCloseBehavior::resolveForControl(const Object* object)
{
	if (!object || !RetailFileLayers::enabled() || !object->areModulesReady())
		return nullptr;
	if (GateOpenAndCloseBehavior* gate = find(object))
		return gate;
	return GateProxyBehavior::resolveGate(object);
}

Bool GateOpenAndCloseBehavior::canPass(const Object* obstacle, const Object* unit, Bool planning)
{
	if (!RetailFileLayers::enabled() || !unit || !unit->getAIUpdateInterface() || unit->isKindOf(KINDOF_PROJECTILE))
		return FALSE;
	if (const auto* gate = find(obstacle))
		return gate->permits(*unit, planning);
	if (DynamicPortalBehaviour::find(obstacle))
	{
		const NameKeyType key = NAMEKEY("DynamicPortalBehaviour");
		for (BehaviorModule** module = obstacle->getBehaviorModules(); *module; ++module)
		{
			if ((*module)->getModuleNameKey() == key && static_cast<const DynamicPortalBehaviour*>(*module)->permits(*unit))
				return TRUE;
		}
	}
	return FALSE;
}

Bool GateOpenAndCloseBehavior::couldConnect(const Object* unit)
{
	if (!unit || !RetailFileLayers::enabled())
		return FALSE;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if ((object->isKindOf(KINDOF_WALL_GATE) || DynamicPortalBehaviour::find(object)) && canPass(object, unit, TRUE))
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

void GateOpenAndCloseBehavior::xferRetailBlocks(Xfer* xfer, Bool present)
{
	RetailSaveBlocks::transferSection(xfer, xfer->getXferMode() == XFER_LOAD, present, {});
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
	XferVersion version = 2;
	xfer->xferVersion(&version, 2);
	UpdateModule::xfer(xfer);
	xfer->xferUnsignedInt(&m_pathFrame);
	xfer->xferUnsignedInt(&m_soundFrame);
	xfer->xferUnsignedInt(&m_endFrame);
	xfer->xferBool(&m_wantsOpen);
	xfer->xferBool(&m_passable);
	xfer->xferBool(&m_moving);
	xfer->xferBool(&m_soundPending);
	xfer->xferBool(&m_geometryReady);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, version >= 2);
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
		!TheGameLogic->isSimulatingSkirmishGame() && !TheGameLogic->isInMultiplayerGame()))
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

void GateProxyBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	BehaviorModuleData::buildFieldParse(p);
}

GateProxyBehavior::GateProxyBehavior(Thing* thing, const ModuleData* data)
	: BehaviorModule(thing, data)
{
}

GateProxyBehavior::~GateProxyBehavior() = default;

GateProxyBehavior* GateProxyBehavior::find(const Object* object)
{
	if (!object || !RetailFileLayers::enabled() || !object->areModulesReady())
		return nullptr;
	static const NameKeyType key = NAMEKEY("GateProxyBehavior");
	for (BehaviorModule** module = object->getBehaviorModules(); *module; ++module)
	{
		if ((*module)->getModuleNameKey() == key)
			return static_cast<GateProxyBehavior*>(*module);
	}
	return nullptr;
}

GateOpenAndCloseBehavior* GateProxyBehavior::resolveGate(const Object* object)
{
	if (!find(object) || object->isEffectivelyDead())
		return nullptr;

	GateOpenAndCloseBehavior* best = nullptr;
	ObjectID bestID = INVALID_ID;
	for (Object* candidate = TheGameLogic->getFirstObject(); candidate; candidate = candidate->getNextObject())
	{
		if (candidate == object || candidate->isDestroyed() || candidate->isEffectivelyDead() ||
			candidate->getControllingPlayer() != object->getControllingPlayer())
			continue;
		GateOpenAndCloseBehavior* gate = GateOpenAndCloseBehavior::find(candidate);
		if (!gate)
			continue;
		const AsciiString& proxyName = gate->getProxyName();
		if (proxyName.isEmpty())
			continue;
		if (object->getName() != proxyName)
			continue;
		const ObjectID id = candidate->getID();
		if (!best || id < bestID)
		{
			best = gate;
			bestID = id;
		}
	}
	return best;
}

void GateProxyBehavior::crc(Xfer* xfer)
{
	BehaviorModule::crc(xfer);
}

void GateProxyBehavior::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	BehaviorModule::xfer(xfer);
	if (RetailFileLayers::enabled())
		RetailSaveBlocks::transferSection(xfer, xfer->getXferMode() == XFER_LOAD, TRUE, {});
}

void GateProxyBehavior::loadPostProcess()
{
	BehaviorModule::loadPostProcess();
}

void DynamicPortalBehaviourModuleData::parseWayPoint(INI* ini, void*, void* store, const void*)
{
	WayPoint wp;
	while (const char* token = ini->getNextTokenOrNull())
	{
		if (strnicmp(token, "Index:", 6) == 0)
			wp.index = INI::scanInt(token + 6);
		else if (strnicmp(token, "Type:", 5) == 0)
		{
			const char* value = token + 5;
			if (stricmp(value, "PreClimb") == 0)
				wp.type = 1;
			else if (stricmp(value, "Climb") == 0)
				wp.type = 2;
			else if (stricmp(value, "Walk") == 0)
				wp.type = 3;
		}
	}
	static_cast<std::vector<WayPoint>*>(store)->push_back(wp);
}

void DynamicPortalBehaviourModuleData::parseLink(INI* ini, void*, void* store, const void*)
{
	Link link;
	while (const char* token = ini->getNextTokenOrNull())
	{
		if (strnicmp(token, "From:", 5) == 0)
			link.from = INI::scanInt(token + 5);
		else if (strnicmp(token, "To:", 3) == 0)
			link.to = INI::scanInt(token + 3);
		else if (strnicmp(token, "Via:", 4) == 0)
			link.vias.push_back(INI::scanInt(token + 4));
	}
	static_cast<std::vector<Link>*>(store)->push_back(std::move(link));
}

void DynamicPortalBehaviourModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "GenerateNow", INI::parseBool, nullptr, offsetof(DynamicPortalBehaviourModuleData, m_generateNow) },
		{ "AllowKindOf", KindOfMaskType::parseFromINI, nullptr, offsetof(DynamicPortalBehaviourModuleData, m_allowKindOf) },
		{ "RejectKindOf", KindOfMaskType::parseFromINI, nullptr, offsetof(DynamicPortalBehaviourModuleData, m_rejectKindOf) },
		{ "AllowEnemies", INI::parseBool, nullptr, offsetof(DynamicPortalBehaviourModuleData, m_allowEnemies) },
		{ "BonePrefix", INI::parseAsciiString, nullptr, offsetof(DynamicPortalBehaviourModuleData, m_bonePrefix) },
		{ "NumberOfBones", INI::parseInt, nullptr, offsetof(DynamicPortalBehaviourModuleData, m_numberOfBones) },
		{ "WayPoint", parseWayPoint, nullptr, offsetof(DynamicPortalBehaviourModuleData, m_wayPoints) },
		{ "Link", parseLink, nullptr, offsetof(DynamicPortalBehaviourModuleData, m_links) },
		{ "TriggeredBy", INI::parseAsciiStringVectorAppend, nullptr, offsetof(DynamicPortalBehaviourModuleData, m_triggeredBy) },
		{ "ConflictsWith", INI::parseAsciiStringVectorAppend, nullptr, offsetof(DynamicPortalBehaviourModuleData, m_conflictsWith) },
		{ "CustomAnimAndDuration", SpecialPowerAnimation::parse, nullptr, offsetof(DynamicPortalBehaviourModuleData, m_customAnim) },
		{ "ActivationDelaySeconds", INI::parseReal, nullptr, offsetof(DynamicPortalBehaviourModuleData, m_activationDelaySeconds) },
		{ "AboveWall", INI::parseInt, nullptr, offsetof(DynamicPortalBehaviourModuleData, m_aboveWall) },
		{ "TopAttackPos", INI::parseCoord3D, nullptr, offsetof(DynamicPortalBehaviourModuleData, m_topAttackPos) },
		{ "TopAttackRadius", INI::parseReal, nullptr, offsetof(DynamicPortalBehaviourModuleData, m_topAttackRadius) },
		{ "ObjectFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(DynamicPortalBehaviourModuleData, m_objectFilter) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields, 0);
}

DynamicPortalBehaviour::DynamicPortalBehaviour(Thing* thing, const ModuleData* data)
	: UpdateModule(thing, data)
{
}

DynamicPortalBehaviour::~DynamicPortalBehaviour() = default;

DynamicPortalBehaviour* DynamicPortalBehaviour::find(const Object* object)
{
	if (!object || !RetailFileLayers::enabled() || !object->areModulesReady())
		return nullptr;
	return static_cast<DynamicPortalBehaviour*>(object->findUpdateModule(NAMEKEY("DynamicPortalBehaviour")));
}

Bool DynamicPortalBehaviour::shouldActivate() const
{
	const Object* self = getObject();
	if (self->isEffectivelyDead() || self->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
		return FALSE;
	const auto* data = getDynamicPortalBehaviourModuleData();
	const Player* owner = self->getControllingPlayer();
	const auto hasUpgradeNamed = [self, owner](const AsciiString& name) -> Bool
	{
		const UpgradeTemplate* tmpl = TheUpgradeCenter ? TheUpgradeCenter->findUpgrade(name) : nullptr;
		if (!tmpl)
			return FALSE;
		return (self->hasUpgrade(tmpl) || (owner && owner->hasUpgradeComplete(tmpl))) ? TRUE : FALSE;
	};
	for (const AsciiString& conflict : data->m_conflictsWith)
	{
		if (hasUpgradeNamed(conflict))
			return FALSE;
	}
	if (!data->m_triggeredBy.empty())
	{
		for (const AsciiString& trigger : data->m_triggeredBy)
		{
			if (hasUpgradeNamed(trigger))
				return TRUE;
		}
		return FALSE;
	}
	if (const auto* siege = SiegeAIUpdate::find(self))
		return siege->isDocked();
	return data->m_generateNow || data->m_customAnim.m_condition >= 0;
}

UpdateSleepTime DynamicPortalBehaviour::update()
{
	if (!RetailFileLayers::enabled())
		return UPDATE_SLEEP_FOREVER;
	Object* self = getObject();
	if (self->isEffectivelyDead())
	{
		m_active = FALSE;
		m_activationScheduled = FALSE;
		return UPDATE_SLEEP_FOREVER;
	}
	const auto* data = getDynamicPortalBehaviourModuleData();
	const Bool changesWithUpgrades = !data->m_triggeredBy.empty() || !data->m_conflictsWith.empty() ||
		SiegeAIUpdate::find(self) != nullptr;
	if (!shouldActivate())
	{
		if (m_activationScheduled && data->m_customAnim.m_condition >= 0)
			self->clearModelConditionState(static_cast<ModelConditionFlagType>(data->m_customAnim.m_condition));
		m_active = FALSE;
		m_activationScheduled = FALSE;
		return UPDATE_SLEEP_NONE;
	}
	if (m_active)
		return changesWithUpgrades ? UPDATE_SLEEP_NONE : UPDATE_SLEEP_FOREVER;
	const UnsignedInt frame = TheGameLogic->getFrame();
	if (!m_activationScheduled)
	{
		const Real seconds = std::isfinite(data->m_activationDelaySeconds) && data->m_activationDelaySeconds > 0.0f
			? data->m_activationDelaySeconds : 0.0f;
		const double targetFrame = frame + static_cast<double>(seconds) * LOGICFRAMES_PER_SECOND;
		m_activationFrame = static_cast<UnsignedInt>(std::min(targetFrame, static_cast<double>(UPDATE_SLEEP_FOREVER)));
		m_activationScheduled = TRUE;
		if (data->m_customAnim.m_condition >= 0)
			self->setModelConditionState(static_cast<ModelConditionFlagType>(data->m_customAnim.m_condition));
	}
	if (frame < m_activationFrame)
		return changesWithUpgrades ? UPDATE_SLEEP_NONE : UPDATE_SLEEP(m_activationFrame - frame);

	m_active = TRUE;
	TheAI->pathfinder()->noteRetailRouteOpening();
	return changesWithUpgrades ? UPDATE_SLEEP_NONE : UPDATE_SLEEP_FOREVER;
}

Bool DynamicPortalBehaviour::permits(const Object& unit) const
{
	if (!m_active || !RetailFileLayers::enabled())
		return FALSE;
	const Object* self = getObject();
	if (const auto* upgrade = WallUpgradeUpdate::find(self); upgrade && upgrade->isWaitingForWall())
		return FALSE;
	if (self->isEffectivelyDead() || self->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
		return FALSE;
	const Player* unitPlayer = unit.getControllingPlayer();
	if (!unitPlayer)
		return FALSE;
	const auto* data = getDynamicPortalBehaviourModuleData();
	const Relationship rel = self->getRelationship(&unit);
	if (unitPlayer != self->getControllingPlayer() && rel != ALLIES)
	{
		if (!data->m_allowEnemies || rel != ENEMIES)
			return FALSE;
	}
	if (data->m_allowKindOf.any() && !unit.isKindOfMulti(data->m_allowKindOf, KINDOFMASK_NONE))
		return FALSE;
	if (data->m_rejectKindOf.any() && unit.isKindOfMulti(data->m_rejectKindOf, KINDOFMASK_NONE))
		return FALSE;
	const auto& filter = data->m_objectFilter;
	const Bool hasFilter = filter.m_rules != 0 || filter.m_includeKinds.any() || filter.m_excludeKinds.any() ||
		!filter.m_includeObjects.empty() || !filter.m_excludeObjects.empty();
	if (hasFilter && !filter.matchesObject(self, &unit))
		return FALSE;
	return TRUE;
}

Bool DynamicPortalBehaviour::getAboveWallPosition(Coord3D& position) const
{
	const auto* data = getDynamicPortalBehaviourModuleData();
	if (data->m_aboveWall < 0 || data->m_aboveWall >= data->m_numberOfBones || data->m_bonePrefix.isEmpty())
		return FALSE;
	std::vector<Coord3D> points(data->m_aboveWall + 1);
	const Int count = getObject()->getMultiLogicalBonePosition(data->m_bonePrefix.str(),
		static_cast<Int>(points.size()), points.data(), nullptr);
	if (count <= data->m_aboveWall)
		return FALSE;
	position = points[data->m_aboveWall];
	return TRUE;
}

Bool DynamicPortalBehaviour::allowsClimber(const Object& unit) const
{
	const auto* data = getDynamicPortalBehaviourModuleData();
	return data->m_numberOfBones > 0 && data->m_objectFilter.matchesObject(getObject(), &unit);
}

Bool DynamicPortalBehaviour::getClimbEntryPosition(Coord3D& position) const
{
	const auto* data = getDynamicPortalBehaviourModuleData();
	return data->m_numberOfBones > 0 && !data->m_bonePrefix.isEmpty() &&
		getObject()->getMultiLogicalBonePosition(data->m_bonePrefix.str(), 1, &position, nullptr) == 1;
}

Bool DynamicPortalBehaviour::getClimbWaypoints(std::vector<Coord3D>& positions) const
{
	positions.clear();
	const auto* data = getDynamicPortalBehaviourModuleData();
	if (data->m_numberOfBones <= 0 || data->m_bonePrefix.isEmpty())
		return FALSE;
	std::vector<Coord3D> bones(static_cast<size_t>(data->m_numberOfBones));
	const Int found = getObject()->getMultiLogicalBonePosition(data->m_bonePrefix.str(),
		data->m_numberOfBones, bones.data(), nullptr);
	if (found <= 0)
		return FALSE;
	const auto appendWaypoint = [&](Int wpIndex)
	{
		if (wpIndex < 0 || static_cast<size_t>(wpIndex) >= data->m_wayPoints.size())
			return;
		const Int boneIndex = data->m_wayPoints[static_cast<size_t>(wpIndex)].index;
		if (boneIndex < 0 || boneIndex >= found)
			return;
		const Coord3D& pt = bones[static_cast<size_t>(boneIndex)];
		if (!positions.empty())
		{
			const Coord3D& prev = positions.back();
			if (prev.x == pt.x && prev.y == pt.y && prev.z == pt.z)
				return;
		}
		positions.push_back(pt);
	};
	for (const auto& link : data->m_links)
	{
		if (link.to != data->m_aboveWall)
			continue;
		appendWaypoint(link.from);
		for (Int via : link.vias)
			appendWaypoint(via);
		appendWaypoint(link.to);
		if (positions.size() >= 2)
			return TRUE;
		positions.clear();
	}
	Coord3D entry;
	Coord3D above;
	if (getClimbEntryPosition(entry) && getAboveWallPosition(above))
	{
		positions.push_back(entry);
		positions.push_back(above);
		return TRUE;
	}
	return FALSE;
}

Bool DynamicPortalBehaviour::getTopAttackPosition(Coord3D& position) const
{
	const Object* self = getObject();
	const auto* data = getDynamicPortalBehaviourModuleData();
	if (!self || data->m_topAttackRadius <= 0.0f)
		return FALSE;
	const Coord3D unitX = {1.0f, 0.0f, 0.0f};
	Coord3D worldX;
	self->convertBonePosToWorldPos(&unitX, nullptr, &worldX, nullptr);
	const Coord3D* origin = self->getPosition();
	const Real dx = worldX.x - origin->x;
	const Real dy = worldX.y - origin->y;
	const Real maxAxis = std::max(std::fabs(dx), std::fabs(dy));
	Real factor = 1.0f;
	if (maxAxis > 0.0f)
	{
		const Real inv = 1.0f / maxAxis;
		const Real sx = dx * inv;
		const Real sy = dy * inv;
		factor = WWMath::Sqrt(sx * sx + sy * sy);
	}
	const Coord3D localPos = {data->m_topAttackPos.x * factor, data->m_topAttackPos.y * factor, data->m_topAttackPos.z};
	self->convertBonePosToWorldPos(&localPos, nullptr, &position, nullptr);
	return TRUE;
}

Bool DynamicPortalBehaviour::isWithinTopAttackRange(const Object& attacker) const
{
	const Object* self = getObject();
	const auto* siege = SiegeAIUpdate::find(self);
	if (!m_active || !siege || !siege->isDocked())
		return FALSE;
	Coord3D topPos;
	if (!getTopAttackPosition(topPos))
		return FALSE;
	const Coord3D* attackerPos = attacker.getPosition();
	if (std::fabs(topPos.z - attackerPos->z) >= 20.0f)
		return FALSE;
	const Real radius = attacker.getGeometryInfo().getMajorRadius() +
		getDynamicPortalBehaviourModuleData()->m_topAttackRadius;
	return std::fabs(topPos.x - attackerPos->x) < radius &&
		std::fabs(topPos.y - attackerPos->y) < radius;
}

void DynamicPortalBehaviour::xferRetailBlocks(Xfer* xfer, Bool present)
{
	RetailSaveBlocks::transferSection(xfer, xfer->getXferMode() == XFER_LOAD, present,
	{
		{ "dynamic-portal", [this](Xfer* x)
		{
			x->xferBool(&m_active);
			x->xferBool(&m_activationScheduled);
			x->xferUnsignedInt(&m_activationFrame);
		}, [this]
		{
			m_active = FALSE;
			m_activationScheduled = FALSE;
			m_activationFrame = 0;
		} }
	});
}

void DynamicPortalBehaviour::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, TRUE);
}

void DynamicPortalBehaviour::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, version >= 1);
}

void DynamicPortalBehaviour::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
