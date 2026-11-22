#pragma once

#include "Common/DrawModule.h"
#include "Common/GameType.h"
#include "Common/ModelState.h"

class RenderObjClass;
class W3DAssetManager;

struct W3DFloorDrawModuleData : public ModuleData
{
	AsciiString m_modelName;
	ModelConditionFlags m_hideConditions;
	AsciiString m_weatherTextures[WEATHER_COUNT];
	Bool m_staticModelLODMode = FALSE;
	Bool m_forceToBack = FALSE;
	Bool m_startHidden = FALSE;

	static void buildFieldParse(MultiIniFieldParse& p);
	Bool isHidden(const ModelConditionFlags& conditions) const;
	RenderObjClass* createModel(W3DAssetManager& assets, Real scale, Weather weather) const;
};

class W3DFloorDraw : public DrawModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( W3DFloorDraw, "W3DFloorDraw" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( W3DFloorDraw, W3DFloorDrawModuleData )

public:
	W3DFloorDraw(Thing* thing, const ModuleData* data);
	void doDrawModule(const Matrix3D* transform) override;
	void reactToTransformChange(const Matrix3D*, const Coord3D*, Real) override;
	void reactToGeometryChange() override {}
	void setShadowsEnabled(Bool) override {}
	void releaseShadows() override {}
	void allocateShadows() override {}
	void setFullyObscuredByShroud(Bool obscured) override;
	void setDrawableHidden(Bool hidden) override;
	void showDrawableSubObject(const AsciiString& name, Bool show) override;

private:
	RenderObjClass* m_model = nullptr;
	Bool m_initialized = FALSE;
	Weather m_weather = WEATHER_NORMAL;
	StaticGameLODLevel m_lod;
	Bool m_hiddenBySubObject;
	void updateModel();
	void updateVisibility(Bool hidden, Bool obscured);
	void removeModel();
};
