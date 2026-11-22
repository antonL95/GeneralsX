#pragma once

#include "W3DDevice/GameClient/Module/W3DFloorDraw.h"

class RenderInfoClass;

struct W3DBuffDrawModuleData : W3DFloorDrawModuleData
{
	Bool m_preDraw = FALSE;

	static void buildFieldParse(MultiIniFieldParse& parse);
};

class W3DBuffDraw final : public W3DFloorDraw
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(W3DBuffDraw, "W3DBuffDraw")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(W3DBuffDraw, W3DBuffDrawModuleData)

public:
	W3DBuffDraw(Thing* thing, const ModuleData* data) : W3DFloorDraw(thing, data) {}
	static void render(RenderInfoClass& info, Int viewer, Bool preDraw);
	static void reset();
};
