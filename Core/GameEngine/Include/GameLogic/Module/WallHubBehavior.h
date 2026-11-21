#pragma once

#include "Common/BuildAssistant.h"
#include "GameLogic/Module/UpdateModule.h"

class WallHubBehaviorModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	UnsignedInt m_option = 0;
	Real m_maxDistance = 0.0f;
	Int m_staggerFactor = 0;
	Real m_builderRadius = 0.0f;
	std::vector<AsciiString> m_segments;
	AsciiString m_defaultSegment;
	AsciiString m_hubCap;
	AsciiString m_cliffCap;
	AsciiString m_borderCap;
	AsciiString m_shoreCap;
	AsciiString m_elevatedSegment;

private:
	static void parseOption(INI* ini, void*, void* store, const void*);
	static void parseSegment(INI* ini, void*, void* store, const void*);
};

class WallHubBehavior final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(WallHubBehavior, "WallHubBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(WallHubBehavior, WallHubBehaviorModuleData)

public:
	struct Piece
	{
		const ThingTemplate* type;
		Coord3D position;
		Real facing;
		Int cost;
	};

	struct Span
	{
		std::vector<Piece> pieces;
		UnsignedInt cost = 0;
		LegalBuildCode legal = LBC_OK;
		Bool affordable = FALSE;
	};

	WallHubBehavior(Thing* thing, const ModuleData* data);
	static WallHubBehavior* find(const Object* object, UnsignedInt options);
	Span plan(const ThingTemplate* requested, const Coord3D& target) const;
	Bool construct(const ThingTemplate* requested, const Coord3D& target, Player* player) const;
	static Bool canCancelSpan(const Object* hub);
	static Bool cancelSpan(const Object* hub, Player* player);
	UpdateSleepTime update() override { return UPDATE_SLEEP_FOREVER; }

private:
	Span plan(const ThingTemplate* requested, const Coord3D& target, UnsignedInt overlapChecks) const;
};

class GettingBuiltBehaviorModuleData : public UpdateModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	AsciiString m_worker;
	AsciiString m_buildLoop;
	AsciiString m_repairLoop;
	AsciiString m_rebuildLoop;
	UnsignedInt m_spawnTimer = 0;
	Real m_rebuildTime = 0.0f;
	Bool m_timerWithoutWorker = FALSE;
};

class GettingBuiltBehavior final : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(GettingBuiltBehavior, "GettingBuiltBehavior")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(GettingBuiltBehavior, GettingBuiltBehaviorModuleData)

public:
	GettingBuiltBehavior(Thing* thing, const ModuleData* data);
	static const GettingBuiltBehavior* find(const Object& structure);
	static Object* start(const ThingTemplate& type, Object& builder, const Coord3D& position,
		Real facing, UnsignedInt delay, Int cost, ObjectID span);
	static UnsignedInt constructionRefund(const Object& structure, const Player& player);
	ObjectID getSpan() const { return m_span; }
	UpdateSleepTime update() override;

private:
	UnsignedInt m_startFrame = 0;
	Bool m_running = FALSE;
	Int m_paidCost = -1;
	ObjectID m_span = INVALID_ID;
};
