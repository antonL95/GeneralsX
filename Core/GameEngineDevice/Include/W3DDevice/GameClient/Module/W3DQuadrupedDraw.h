#pragma once

#include "W3DDevice/GameClient/Module/W3DScriptedModelDraw.h"

class W3DQuadrupedDrawModuleData : public W3DScriptedModelDrawModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

private:
	AsciiString m_leftFrontFootBone;
	AsciiString m_rightFrontFootBone;
	AsciiString m_leftRearFootBone;
	AsciiString m_rightRearFootBone;
};

class W3DQuadrupedDraw : public W3DScriptedModelDraw
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(W3DQuadrupedDraw, "W3DQuadrupedDraw")
	MAKE_STANDARD_MODULE_MACRO(W3DQuadrupedDraw)

public:
	W3DQuadrupedDraw(Thing* thing, const ModuleData* data);
	static ModuleData* friend_newModuleData(INI* ini);
};
