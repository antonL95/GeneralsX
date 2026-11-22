#include "PreRTS.h"

#if !RTS_GENERALS
#include "GameLogic/Module/FloodUpdate.h"

#include <algorithm>
#include <limits>

#include "Common/INI.h"
#include "Common/INIAudit.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailSaveBlocks.h"
#include "Common/RotwkDuration.h"
#include "Common/ThingFactory.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "WWMath/wwmath.h"

void FloodUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "AngleOfFlow", INI::parseAngleReal, nullptr, offsetof(FloodUpdateModuleData, m_angle) },
		{ "DirectionIsRelative", INI::parseBool, nullptr, offsetof(FloodUpdateModuleData, m_relative) },
		{ "FloodMember", parseMember, nullptr, offsetof(FloodUpdateModuleData, m_members) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

void FloodUpdateModuleData::parseMember(INI* ini, void*, void* store, const void*)
{
	static const FieldParse fields[] =
	{
		{ "MemberTemplateName", INI::parseAsciiString, nullptr, offsetof(Member, templateName) },
		{ "ControlPointOffsetOne", INI::parseCoord3D, nullptr, offsetof(Member, offsets) },
		{ "ControlPointOffsetTwo", INI::parseCoord3D, nullptr, offsetof(Member, offsets) + sizeof(Coord3D) },
		{ "ControlPointOffsetThree", INI::parseCoord3D, nullptr, offsetof(Member, offsets) + 2 * sizeof(Coord3D) },
		{ "ControlPointOffsetFour", INI::parseCoord3D, nullptr, offsetof(Member, offsets) + 3 * sizeof(Coord3D) },
		{ "MemberSpeed", INI::parseReal, nullptr, offsetof(Member, speed) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	Member member;
	ini->initFromINI(&member, fields);
	if (member.templateName.isEmpty() || !(member.speed > 0.0f))
	{
		if (INIAudit::isEnabled())
			INIAudit::record("field-error", "FloodMember requires a template and positive speed", ini->getFilename().str(), ini->getLineNum());
		else
			DEBUG_CRASH(("FloodMember requires a template and positive speed"));
		return;
	}
	static_cast<std::vector<Member>*>(store)->push_back(std::move(member));
}

FloodUpdate::~FloodUpdate() = default;

Coord3D FloodUpdate::Route::point(UnsignedInt sample) const
{
	Coord3D position;
	curve.evaluateBezSegmentAtT(static_cast<Real>(sample) / (samples - 1), &position);
	return position;
}

void FloodUpdate::buildRoutes()
{
	const auto& members = getFloodUpdateModuleData()->m_members;
	const Real cosine = WWMath::Cos(m_heading);
	const Real sine = WWMath::Sin(m_heading);
	m_routes.clear();
	m_routes.reserve(members.size());
	for (const auto& member : members)
	{
		Coord3D points[4];
		for (std::size_t i = 0; i < member.offsets.size(); ++i)
		{
			const auto& offset = member.offsets[i];
			points[i] = { m_origin.x + cosine * offset.x - sine * offset.y,
				m_origin.y + sine * offset.x + cosine * offset.y, 0.0f };
		}
		BezierSegment curve(points);
		const Real samples = WWMath::Ceil(curve.getApproximateLength(1.0f) / member.speed);
		const auto count = samples >= 0.0f && samples < static_cast<Real>(std::numeric_limits<UnsignedInt>::max())
			? static_cast<UnsignedInt>(std::max(2.0f, samples)) : 0;
		m_routes.push_back({curve, count});
	}
}

void FloodUpdate::spawnMembers()
{
	Object* controller = getObject();
	const auto* data = getFloodUpdateModuleData();
	m_origin = *controller->getPosition();
	m_heading = data->m_angle + (data->m_relative ? controller->getOrientation() : 0.0f);
	buildRoutes();

	m_memberIDs.assign(m_routes.size(), INVALID_ID);
	for (std::size_t i = 0; i < m_routes.size(); ++i)
	{
		const Route& route = m_routes[i];
		if (route.samples == 0)
			continue;
		const auto& name = data->m_members[i].templateName;
		const auto* memberTemplate = TheThingFactory->findTemplate(name);
		if (!memberTemplate)
		{
			DEBUG_CRASH(("FloodUpdate has an unknown member template '%s'", name.str()));
			continue;
		}
		Object* member = TheThingFactory->newObject(memberTemplate, controller->getTeam());
		Coord3D position = route.point(0);
		const Coord3D next = route.point(1);
		position.z = TheTerrainLogic->getGroundHeight(position.x, position.y);
		member->setPosition(&position);
		member->setOrientation(WWMath::Atan2(next.y - position.y, next.x - position.x));
		m_memberIDs[i] = member->getID();
	}
}

UpdateSleepTime FloodUpdate::update()
{
	if (!RetailFileLayers::enabled())
		return UPDATE_SLEEP_FOREVER;
	if (m_sample == 0)
		spawnMembers();

	++m_sample;
	Bool moving = FALSE;
	for (std::size_t i = 0; i < m_memberIDs.size(); ++i)
	{
		Object* member = TheGameLogic->findObjectByID(m_memberIDs[i]);
		if (!member || member->isEffectivelyDead())
			continue;

		const Route& route = m_routes[i];
		if (m_sample >= route.samples)
		{
			member->kill();
			m_memberIDs[i] = INVALID_ID;
			continue;
		}

		Coord3D position = route.point(m_sample);
		position.z = TheTerrainLogic->getGroundHeight(position.x, position.y);
		member->setPosition(&position);
		if (m_sample + 1 < route.samples)
		{
			const Coord3D next = route.point(m_sample + 1);
			if (next.x != position.x || next.y != position.y)
				member->setOrientation(WWMath::Atan2(next.y - position.y, next.x - position.x));
		}
		moving = TRUE;
	}

	return moving ? UPDATE_SLEEP(RotwkDuration::engineFrames(1, LOGICFRAMES_PER_SECOND)) : UPDATE_SLEEP_FOREVER;
}

void FloodUpdate::xferRetailBlocks(Xfer* xfer)
{
	RetailSaveBlocks::transferSection(xfer, xfer->getXferMode() == XFER_LOAD, true, {
		{"flood-paths", [this](Xfer* block) {
			block->xferCoord3D(&m_origin);
			block->xferReal(&m_heading);
			block->xferUnsignedInt(&m_sample);
			block->xferSTLObjectIDVector(&m_memberIDs);
		}, [this] {
			m_origin = {};
			m_heading = 0.0f;
			m_sample = 0;
			m_memberIDs.clear();
		}}
	});
}

void FloodUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	xferRetailBlocks(xfer);
}

void FloodUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
	xferRetailBlocks(xfer);
}

void FloodUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
	if (m_sample != 0)
	{
		buildRoutes();
		m_memberIDs.resize(m_routes.size(), INVALID_ID);
	}
}
#endif
