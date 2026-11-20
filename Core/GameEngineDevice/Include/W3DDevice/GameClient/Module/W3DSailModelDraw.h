#pragma once

#include "W3DDevice/GameClient/Module/W3DScriptedModelDraw.h"

class W3DSailModelDrawModuleData : public W3DScriptedModelDrawModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

private:
	Int m_maxRotationDegrees = 0;
	Int m_blowingThresholdDegrees = 0;
	Real m_aboutDamping = 0;
};

class W3DSailModelDraw : public W3DScriptedModelDraw
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(W3DSailModelDraw, "W3DSailModelDraw")
	MAKE_STANDARD_MODULE_MACRO(W3DSailModelDraw)

public:
	W3DSailModelDraw(Thing* thing, const ModuleData* data);
	static ModuleData* friend_newModuleData(INI* ini);
};
