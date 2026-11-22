#pragma once

#include <array>
#include <vector>

#include "Common/BezierSegment.h"
#include "GameLogic/Module/UpdateModule.h"

struct FloodUpdateModuleData : public UpdateModuleData
{
	struct Member
	{
		AsciiString templateName;
		std::array<Coord3D, 4> offsets{};
		Real speed = 0.0f;
	};

	std::vector<Member> m_members;
	Real m_angle = 0.0f;
	Bool m_relative = FALSE;

	static void buildFieldParse(MultiIniFieldParse& p);

private:
	static void parseMember(INI* ini, void*, void* store, const void*);
};

class FloodUpdate final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(FloodUpdate, "FloodUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(FloodUpdate, FloodUpdateModuleData)

public:
	FloodUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data) {}
	UpdateSleepTime update() override;

private:
	struct Route
	{
		BezierSegment curve;
		UnsignedInt samples = 0;

		Coord3D point(UnsignedInt sample) const;
	};

	void buildRoutes();
	void spawnMembers();
	void xferRetailBlocks(Xfer* xfer);

	Coord3D m_origin{};
	Real m_heading = 0.0f;
	UnsignedInt m_sample = 0;
	std::vector<ObjectID> m_memberIDs;
	std::vector<Route> m_routes;
};
