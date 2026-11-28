#include "W3DDevice/GameClient/W3DBillboardParticles.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "W3DDevice/GameClient/HeightMap.h"
#include "WW3D2/camera.h"
#include "WW3D2/dx8indexbuffer.h"
#include "WW3D2/dx8vertexbuffer.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/rinfo.h"
#include "WW3D2/sortingrenderer.h"
#include "WW3D2/texture.h"
#include "WW3D2/vertmaterial.h"
#include "WW3D2/ww3d.h"
#include "WWMath/matrix3d.h"
#include "WWMath/sphere.h"

W3DBillboardParticles::W3DBillboardParticles() :
	m_vertexData(),
	m_indexData(),
	m_numVertices(0),
	m_numIndices(0),
	m_hasBounds(false),
	m_boundsMin(0.0f, 0.0f, 0.0f),
	m_boundsMax(0.0f, 0.0f, 0.0f),
	m_pointCount(0),
	m_defaultPointSize(0.0f),
	m_defaultPointColor(1.0f, 1.0f, 1.0f),
	m_defaultPointAlpha(1.0f),
	m_defaultPointOrientation(0)
{
}

Bool W3DBillboardParticles::canRenderGroundFaded() const
{
	return TheTerrainRenderObject != nullptr && TheTerrainRenderObject->getMap() != nullptr;
}

void W3DBillboardParticles::setTexture(TextureClass* texture)
{
	m_texture = texture;
}

void W3DBillboardParticles::setShader(ShaderClass shader)
{
	m_shader = shader;
}

void W3DBillboardParticles::setArrays(
	ShareBufferClass<Vector3>* locs,
	ShareBufferClass<Vector4>* diffuse,
	ShareBufferClass<Real>* sizes,
	ShareBufferClass<UnsignedByte>* orientations,
	Int activePointCount)
{
	m_pointLoc = locs;
	m_pointDiffuse = diffuse;
	m_pointSize = sizes;
	m_pointOrientation = orientations;
	m_pointCount = activePointCount >= 0 ? activePointCount : (locs ? locs->Get_Count() : 0);
}

void W3DBillboardParticles::render(RenderInfoClass& rinfo)
{
	if (m_pointCount <= 0 || !m_pointLoc || !canRenderGroundFaded())
		return;

	const Matrix3D& camTransform = rinfo.Camera.Get_Transform();
	const Vector3 camRightVec = camTransform.Get_X_Vector();
	const Vector3 camUpVec = camTransform.Get_Y_Vector();
	const BillboardGroundClip::Vec3 camRight{ camRightVec.X, camRightVec.Y, camRightVec.Z };
	const BillboardGroundClip::Vec3 camUp{ camUpVec.X, camUpVec.Y, camUpVec.Z };

	const Vector3* locs = m_pointLoc->Get_Array();
	const Vector4* diffuse = m_pointDiffuse ? m_pointDiffuse->Get_Array() : nullptr;
	const Real* sizes = m_pointSize ? m_pointSize->Get_Array() : nullptr;
	const UnsignedByte* orientations = m_pointOrientation ? m_pointOrientation->Get_Array() : nullptr;

	m_numVertices = 0;
	m_numIndices = 0;
	m_hasBounds = false;

	for (Int i = 0; i < m_pointCount; ++i)
	{
		const Real size = sizes ? sizes[i] : m_defaultPointSize;
		const Vector4 baseColor = diffuse
			? diffuse[i]
			: Vector4(m_defaultPointColor.X, m_defaultPointColor.Y, m_defaultPointColor.Z, m_defaultPointAlpha);
		if (size <= 0.0f || baseColor.W <= 0.0f)
			continue;

		const UnsignedByte angleIndex = orientations ? orientations[i] : m_defaultPointOrientation;
		const BillboardGroundClip::Vec3 center{ locs[i].X, locs[i].Y, locs[i].Z };

		std::array<BillboardGroundClip::ClipVertex, 4> corners{};
		BillboardGroundClip::buildQuadCorners(center, camRight, camUp, size, angleIndex, corners);

		std::array<Real, 4> groundHeights{};
		for (int j = 0; j < 4; ++j)
		{
			groundHeights[j] = TheTerrainRenderObject->getHeightMapHeight(corners[j].x, corners[j].y, nullptr);
		}

		m_clipVerts.clear();
		m_clipIndices.clear();
		BillboardGroundClip::clipGroundBillboard(corners, groundHeights, m_clipVerts, m_clipIndices);
		if (m_clipVerts.empty() || m_clipIndices.empty())
			continue;

		if (m_numVertices + m_clipVerts.size() > BillboardParticles::MAX_VERTICES ||
		    m_numIndices + m_clipIndices.size() > BillboardParticles::MAX_INDICES)
		{
			flushBatch();
		}

		const UnsignedShort vertexBase = m_numVertices;
		for (const BillboardGroundClip::ClipVertex& cv : m_clipVerts)
		{
			VertexFormatXYZNDUV2& out = m_vertexData[m_numVertices++];
			out.x = cv.x;
			out.y = cv.y;
			out.z = cv.z;
			out.nx = 0.0f;
			out.ny = 0.0f;
			out.nz = 0.0f;
			out.diffuse = DX8Wrapper::Convert_Color_Clamp(
				Vector4(baseColor.X, baseColor.Y, baseColor.Z, baseColor.W * cv.alphaScale));
			out.u1 = cv.u;
			out.v1 = cv.v;
			out.u2 = 0.0f;
			out.v2 = 0.0f;

			if (!m_hasBounds)
			{
				m_boundsMin.Set(cv.x, cv.y, cv.z);
				m_boundsMax = m_boundsMin;
				m_hasBounds = true;
			}
			else
			{
				m_boundsMin.X = std::min(m_boundsMin.X, cv.x);
				m_boundsMin.Y = std::min(m_boundsMin.Y, cv.y);
				m_boundsMin.Z = std::min(m_boundsMin.Z, cv.z);
				m_boundsMax.X = std::max(m_boundsMax.X, cv.x);
				m_boundsMax.Y = std::max(m_boundsMax.Y, cv.y);
				m_boundsMax.Z = std::max(m_boundsMax.Z, cv.z);
			}
		}

		for (const UnsignedShort idx : m_clipIndices)
		{
			m_indexData[m_numIndices++] = static_cast<UnsignedShort>(vertexBase + idx);
		}
	}

	flushBatch();
}

void W3DBillboardParticles::flushBatch()
{
	if (m_numIndices > 0 && m_numVertices > 0)
	{
		m_shader.Set_Primary_Gradient(ShaderClass::GRADIENT_MODULATE);
		m_shader.Set_Texturing(m_texture ? ShaderClass::TEXTURING_ENABLE : ShaderClass::TEXTURING_DISABLE);
		m_shader.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);

		DX8Wrapper::Set_World_Identity();
		RefCountPtr<VertexMaterialClass> material;
		material.Assign_No_Add_Ref(VertexMaterialClass::Get_Preset(VertexMaterialClass::PRELIT_DIFFUSE));
		DX8Wrapper::Set_Material(material.Peek());
		DX8Wrapper::Set_Shader(m_shader);
		DX8Wrapper::Set_Texture(0, m_texture.Peek());

		const Bool sort =
			m_shader.Get_Dst_Blend_Func() != ShaderClass::DSTBLEND_ZERO &&
			m_shader.Get_Alpha_Test() == ShaderClass::ALPHATEST_DISABLE &&
			WW3D::Is_Sorting_Enabled();

		const UnsignedInt vbType =
			sort ? BUFFER_TYPE_DYNAMIC_SORTING : BUFFER_TYPE_DYNAMIC_DX8;

		DynamicVBAccessClass vertexAccess(vbType, dynamic_fvf_type, m_numVertices);
		{
			DynamicVBAccessClass::WriteLockClass vertexLock(&vertexAccess);
			memcpy(vertexLock.Get_Formatted_Vertex_Array(),
			       m_vertexData.data(),
			       m_numVertices * sizeof(VertexFormatXYZNDUV2));
		}

		DynamicIBAccessClass indexAccess(vbType, m_numIndices);
		{
			DynamicIBAccessClass::WriteLockClass indexLock(&indexAccess);
			memcpy(indexLock.Get_Index_Array(),
			       m_indexData.data(),
			       m_numIndices * sizeof(UnsignedShort));
		}

		DX8Wrapper::Set_Index_Buffer(indexAccess, 0);
		DX8Wrapper::Set_Vertex_Buffer(vertexAccess);

		if (sort)
		{
			const Vector3 center = (m_boundsMin + m_boundsMax) * 0.5f;
			const Real radius = std::max(0.01f, (m_boundsMax - m_boundsMin).Length() * 0.5f);
			SortingRendererClass::Insert_Triangles(
				SphereClass(center, radius),
				0,
				m_numIndices / 3,
				0,
				m_numVertices);
		}
		else
		{
			DX8Wrapper::Draw_Triangles(0, m_numIndices / 3, 0, m_numVertices);
		}
	}

	m_numVertices = 0;
	m_numIndices = 0;
	m_hasBounds = false;
}
