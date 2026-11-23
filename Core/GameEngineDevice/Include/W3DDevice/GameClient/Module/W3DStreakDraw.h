#pragma once

#include "Common/DrawModule.h"
#include "Common/GameType.h"
#include "WWMath/vector3.h"

#include <utility>
#include <vector>

class SegmentedLineClass;
class TextureClass;

class W3DStreakDrawModuleData : public ModuleData
{
public:
	Real m_length = 0.0f;
	Real m_width = 0.0f;
	Bool m_additive = TRUE;
	RGBColor m_color{};
	Int m_numSegments = 1;
	AsciiString m_texture;
	std::vector<std::pair<Int, AsciiString>> m_weatherTextures;

	static void buildFieldParse(MultiIniFieldParse& p);
	const AsciiString& textureFor(Weather weather) const;

private:
	static void parseWeatherTexture(INI* ini, void* instance, void* store, const void* userData);
};

class W3DStreakDraw : public DrawModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(W3DStreakDraw, "W3DStreakDraw")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(W3DStreakDraw, W3DStreakDrawModuleData)

public:
	W3DStreakDraw(Thing* thing, const ModuleData* moduleData);

	virtual void doDrawModule(const Matrix3D* transformMtx) override;
	virtual void releaseShadows() override {}
	virtual void allocateShadows() override {}
	virtual void setShadowsEnabled(Bool) override {}
	virtual void setFullyObscuredByShroud(Bool fullyObscured) override;
	virtual void reactToTransformChange(const Matrix3D*, const Coord3D*, Real) override;
	virtual void reactToGeometryChange() override {}

private:
	void addPoint(const Vector3& location);
	void updateTrail(const Vector3& head);
	void updateLine();

	SegmentedLineClass* m_line = nullptr;
	TextureClass* m_streakTexture = nullptr;
	std::vector<Vector3> m_points;
	std::vector<Real> m_distances;
	Bool m_obscured = FALSE;
};
