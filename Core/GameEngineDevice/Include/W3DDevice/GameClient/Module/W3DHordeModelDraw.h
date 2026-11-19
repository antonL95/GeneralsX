#pragma once

#include "W3DDevice/GameClient/Module/W3DScriptedModelDraw.h"

#include <array>

class W3DHordeModelDrawModuleData : public W3DScriptedModelDrawModuleData
{
public:
	struct LodOption
	{
		Int level = 0;
		Bool allowMultipleModels = FALSE;
		Int maxRandomTextures = 0;
		Int maxRandomAnimations = 0;
		Real maxAnimFrameDelta = 0.0f;
		Int randomStartFramePercent = 0;
	};

	static void buildFieldParse(MultiIniFieldParse& p);

	const std::array<LodOption, 3>& getLodOptions() const { return m_lodOptions; }

private:
	static void parseLodOption(INI*, void*, void*, const void*);

	std::array<LodOption, 3> m_lodOptions{};
};

class W3DHordeModelDraw : public W3DScriptedModelDraw
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(W3DHordeModelDraw, "W3DHordeModelDraw")
	MAKE_STANDARD_MODULE_MACRO(W3DHordeModelDraw)

public:
	W3DHordeModelDraw(Thing* thing, const ModuleData* data);
	static ModuleData* friend_newModuleData(INI* ini);
};
