#pragma once

#include "Common/DrawModule.h"
#include "Common/GameType.h"

class W3DDynamicLight;

class W3DLightDrawModuleData : public ModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse& p);

	RGBColor m_ambient{};
	RGBColor m_diffuse{};
	RGBColor m_specular{};
	Real m_radius = 0.0f;
	Real m_intensity = 1.0f;
	Real m_flickerAmplitude = 0.0f;
	Real m_flickerFrequency = 0.0f;
	Real m_pulseAmplitude = 0.0f;
	Real m_pulseFrequency = 0.0f;
	AsciiString m_attachBone;
};

class W3DLightDraw : public DrawModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(W3DLightDraw, "W3DLightDraw")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(W3DLightDraw, W3DLightDrawModuleData)

public:
	W3DLightDraw(Thing* thing, const ModuleData* moduleData);

	void doDrawModule(const Matrix3D* transformMtx) override;
	void releaseShadows() override {}
	void allocateShadows() override {}
	void setShadowsEnabled(Bool) override {}
	void setDrawableHidden(Bool hidden) override;
	void setFullyObscuredByShroud(Bool obscured) override;
	void reactToTransformChange(const Matrix3D*, const Coord3D*, Real) override {}
	void reactToGeometryChange() override {}

private:
	void releaseLight();

	W3DDynamicLight* m_light = nullptr;
	Real m_pulsePhase = 0.0f;
	Real m_flickerStart = 0.0f;
	Real m_flickerTarget = 0.0f;
	Real m_flickerStartTime = -1.0f;
	Bool m_obscured = FALSE;
};
