#include "PreRTS.h"

#include "GameLogic/Module/RunOffMapBehavior.h"

#include "Common/RetailFileLayers.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

void RunOffMapBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] = {
		{ "RunOffMapWaypointName", INI::parseAsciiString, nullptr, offsetof(RunOffMapBehaviorModuleData, m_waypoint) },
		{ "RequiresSpecificTrigger", INI::parseBool, nullptr, offsetof(RunOffMapBehaviorModuleData, m_requiresSpecificTrigger) },
		{ "DieOnMap", INI::parseBool, nullptr, offsetof(RunOffMapBehaviorModuleData, m_dieOnMap) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

RunOffMapBehavior::RunOffMapBehavior(Thing* thing, const ModuleData* data)
	: UpdateModule(thing, data),
	  m_phase(getRunOffMapBehaviorModuleData()->m_requiresSpecificTrigger ? Phase::Waiting : Phase::Pending)
{
}

RunOffMapBehavior::~RunOffMapBehavior() = default;

RunOffMapBehavior* RunOffMapBehavior::find(const Object* object)
{
	static const NameKeyType key = NAMEKEY("RunOffMapBehavior");
	return static_cast<RunOffMapBehavior*>(object->findUpdateModule(key));
}

void RunOffMapBehavior::trigger()
{
	if (m_phase != Phase::Waiting)
		return;
	m_phase = Phase::Pending;
	setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
}

UpdateSleepTime RunOffMapBehavior::update()
{
	if (!RetailFileLayers::enabled() || m_phase == Phase::Waiting || m_phase == Phase::Finished)
		return UPDATE_SLEEP_FOREVER;
	Object* self = getObject();
	auto* ai = self->getAIUpdateInterface();
	if (!ai || !self->isMobile() || self->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;
	const auto* data = getRunOffMapBehaviorModuleData();
	if (m_phase == Phase::Pending)
	{
		const Waypoint* waypoint = data->m_waypoint.isEmpty() ? nullptr : TheTerrainLogic->getWaypointByName(data->m_waypoint);
		Coord3D destination = waypoint ? *waypoint->getLocation() : *self->getPosition();
		if (!waypoint)
		{
			const Vector3 direction = self->getTransformMatrix()->Get_X_Vector();
			destination.x += direction.X * 500.0f;
			destination.y += direction.Y * 500.0f;
			destination.z = TheTerrainLogic->getGroundHeight(destination.x, destination.y);
		}
		ai->aiMoveToPosition(&destination, CMD_FROM_SCRIPT);
		m_phase = Phase::Running;
		return UPDATE_SLEEP_NONE;
	}
	if (!self->isOffMap() && !ai->isIdle())
		return UPDATE_SLEEP_NONE;
	m_phase = Phase::Finished;
	if (data->m_dieOnMap && !self->isOffMap())
		self->kill();
	else
		TheGameLogic->destroyObject(self);
	return UPDATE_SLEEP_FOREVER;
}

void RunOffMapBehavior::crc(Xfer* xfer) { UpdateModule::crc(xfer); }

void RunOffMapBehavior::crcRetailState(Xfer* xfer)
{
	xfer->xferUser(&m_phase, sizeof(m_phase));
	UnsignedInt wakeFrame = friend_getNextCallFrame();
	xfer->xferUnsignedInt(&wakeFrame);
}

void RunOffMapBehavior::xfer(Xfer* xfer)
{
	const XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	UpdateModule::xfer(xfer);
	xfer->xferUser(&m_phase, sizeof(m_phase));
}

void RunOffMapBehavior::loadPostProcess() { UpdateModule::loadPostProcess(); }
