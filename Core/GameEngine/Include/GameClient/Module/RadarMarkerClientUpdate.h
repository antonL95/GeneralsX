#pragma once

#include <list>

#include "Common/ClientUpdateModule.h"

class RadarMarkerClientUpdateModuleData : public ClientUpdateModuleData
{
public:
	AsciiString markerType;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class RadarMarkerClientUpdate final : public ClientUpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(RadarMarkerClientUpdate, "RadarMarkerClientUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(RadarMarkerClientUpdate, RadarMarkerClientUpdateModuleData)

public:
	RadarMarkerClientUpdate(Thing* thing, const ModuleData* data) : ClientUpdateModule(thing, data) {}
	void clientUpdate() override;

	using Markers = std::list<const RadarMarkerClientUpdate*>;
	static const Markers& activeMarkers();

	const AsciiString& getMarkerType() const { return getRadarMarkerClientUpdateModuleData()->markerType; }
	const Coord3D* getMarkerPosition() const;
	UnsignedInt getStartFrame() const { return m_startFrame; }

private:
	static Markers& markers();

	Markers::iterator m_entry;
	UnsignedInt m_startFrame = 0;
	Bool m_active = FALSE;
};
