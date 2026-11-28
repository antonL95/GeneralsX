#pragma once

#include <array>
#include <vector>

#include "GameClient/BillboardGroundClip.h"
#include "Lib/BaseType.h"
#include "WW3D2/dx8fvf.h"
#include "WW3D2/shader.h"
#include "WWLib/sharebuf.h"
#include "WWMath/vector3.h"
#include "WWMath/vector4.h"

namespace BillboardParticles
{
	constexpr UnsignedShort MAX_VERTICES = 32768;
	constexpr UnsignedShort MAX_INDICES = 65535;
}

class RenderInfoClass;
class TextureClass;

class W3DBillboardParticles
{
public:
	W3DBillboardParticles();

	Bool canRenderGroundFaded() const;
	void setTexture(TextureClass* texture);
	void setShader(ShaderClass shader);
	void setArrays(ShareBufferClass<Vector3>* locs,
	               ShareBufferClass<Vector4>* diffuse = nullptr,
	               ShareBufferClass<Real>* sizes = nullptr,
	               ShareBufferClass<UnsignedByte>* orientations = nullptr,
	               Int activePointCount = -1);
	void render(RenderInfoClass& rinfo);

private:
	void flushBatch();

	std::array<VertexFormatXYZNDUV2, BillboardParticles::MAX_VERTICES> m_vertexData;
	std::array<UnsignedShort, BillboardParticles::MAX_INDICES> m_indexData;
	std::vector<BillboardGroundClip::ClipVertex> m_clipVerts;
	std::vector<UnsignedShort> m_clipIndices;

	UnsignedShort m_numVertices;
	UnsignedShort m_numIndices;
	Bool m_hasBounds;
	Vector3 m_boundsMin;
	Vector3 m_boundsMax;

	RefCountPtr<TextureClass> m_texture;
	ShaderClass m_shader;

	RefCountPtr<ShareBufferClass<Vector3>> m_pointLoc;
	RefCountPtr<ShareBufferClass<Vector4>> m_pointDiffuse;
	RefCountPtr<ShareBufferClass<Real>> m_pointSize;
	RefCountPtr<ShareBufferClass<UnsignedByte>> m_pointOrientation;
	Int m_pointCount;

	Real m_defaultPointSize;
	Vector3 m_defaultPointColor;
	Real m_defaultPointAlpha;
	UnsignedByte m_defaultPointOrientation;
};
