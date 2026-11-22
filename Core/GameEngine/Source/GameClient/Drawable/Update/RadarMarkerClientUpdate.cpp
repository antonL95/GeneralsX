#include "PreRTS.h"

#include "GameClient/Module/RadarMarkerClientUpdate.h"
#include "Common/INI.h"
#include "Common/RetailFileLayers.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

void RadarMarkerClientUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ClientUpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "MarkerType", INI::parseAsciiString, nullptr, offsetof(RadarMarkerClientUpdateModuleData, markerType) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

RadarMarkerClientUpdate::Markers& RadarMarkerClientUpdate::markers()
{
	static Markers active;
	return active;
}

const RadarMarkerClientUpdate::Markers& RadarMarkerClientUpdate::activeMarkers()
{
	return markers();
}

RadarMarkerClientUpdate::~RadarMarkerClientUpdate()
{
	if (m_active)
		markers().erase(m_entry);
}

void RadarMarkerClientUpdate::clientUpdate()
{
	if (m_active || !RetailFileLayers::enabled() || getMarkerType().isEmpty() ||
		getMarkerType().isNone() || !getMarkerPosition())
		return;

	m_entry = markers().insert(markers().end(), this);
	m_startFrame = TheGameLogic->getFrame();
	m_active = TRUE;
}

const Coord3D* RadarMarkerClientUpdate::getMarkerPosition() const
{
	const Drawable* drawable = getDrawable();
	const Object* object = drawable ? drawable->getObject() : nullptr;
	return object && !object->isEffectivelyDead() ? drawable->getPosition() : nullptr;
}

void RadarMarkerClientUpdate::crc(Xfer* xfer)
{
	ClientUpdateModule::crc(xfer);
}

void RadarMarkerClientUpdate::xfer(Xfer* xfer)
{
	ClientUpdateModule::xfer(xfer);
}

void RadarMarkerClientUpdate::loadPostProcess()
{
	ClientUpdateModule::loadPostProcess();
}
