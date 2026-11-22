#pragma once

#include "Common/DrawModule.h"
#include "Common/GameCommon.h"
#include "GameClient/Shadow.h"

#include <vector>

class W3DTornadoDrawModuleData : public ModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	struct Decal
	{
		AsciiString texture;
		ShadowType style = SHADOW_ALPHA_DECAL;
		Real opacityMin = 1.0f;
		Real opacityMax = 1.0f;
		UnsignedInt opacityThrobTime = LOGICFRAMES_PER_SECOND;
		Real maxRadius = 0.0f;
		Real rotationsPerMinute = 0.0f;
		Real spiralAcceleration = 0.0f;
		Color color = 0;
		Bool ownerOnly = TRUE;
	};

	Decal m_decal;
	UnsignedInt m_decalCount = 0;
	Real m_decalMaxRadius = 200.0f;

private:
	static void parseDecal(INI* ini, void* instance, void* store, const void* userData);
};

class W3DTornadoDraw : public DrawModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(W3DTornadoDraw, "W3DTornadoDraw")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(W3DTornadoDraw, W3DTornadoDrawModuleData)

public:
	W3DTornadoDraw(Thing* thing, const ModuleData* moduleData);

	void doDrawModule(const Matrix3D* transformMtx) override;
	void releaseShadows() override;
	void allocateShadows() override {}
	void setShadowsEnabled(Bool enabled) override;
	void setDrawableHidden(Bool hidden) override;
	void setFullyObscuredByShroud(Bool obscured) override;
	void reactToTransformChange(const Matrix3D*, const Coord3D*, Real) override {}
	void reactToGeometryChange() override {}

private:
	struct Layer
	{
		Shadow* shadow = nullptr;
		Real spiralPosition = 0.0f;
		Real angle = 0.0f;
	};
	std::vector<Layer> m_layers;
	UnsignedInt m_lastFrame = 0;
	Bool m_obscured = FALSE;
	Bool m_shadowsEnabled = TRUE;
};
