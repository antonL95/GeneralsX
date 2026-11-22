#include "W3DDevice/GameClient/RetailTerrainShadow.h"

#include "Common/GlobalData.h"
#include "Common/File.h"
#include "Common/FileSystem.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "W3DDevice/GameClient/BaseHeightMap.h"
#include "WW3D2/mesh.h"
#include "WW3D2/meshmdl.h"
#include "WW3D2/scene.h"
#include "WWMath/matrix3d.h"
#include "d3dx8math.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

RetailTerrainShadow *TheRetailTerrainShadow = nullptr;

namespace
{
constexpr auto ShadowDepthFloat = static_cast<D3DFORMAT>(114);

struct ShadowTrace
{
	unsigned sequence = 0;
	unsigned props = 0;
	unsigned trees = 0;
	unsigned treeCutouts = 0;
	unsigned failures = 0;
	uint32_t treeHash = 2166136261u;
};

ShadowTrace shadowTrace;

bool tracingShadows()
{
	static const bool enabled = std::getenv("GENERALSX_TRACE_RETAIL_SHADOWS") != nullptr;
	return enabled;
}

const DWORD DepthShader[] = {
	0xffff0104,
	0x00000040, 0x80070000, 0xb0e40000,
	0x00000005, 0x83010001, 0x80000000, 0xa0000000,
	0x00000005, 0x83010001, 0x80000001, 0xa0000000,
	0x00000005, 0x82010001, 0x80000001, 0xa0550000,
	0x00000013, 0x80020000, 0x80000001,
	0x00000004, 0x80010000, 0x80550000, 0xa0aa0000, 0x80000000,
	0x00000001, 0x80080000, 0xa0000000,
	0x0000ffff
};

const DWORD CutoutDepthShader[] = {
	0xffff0104,
	0x00000040, 0x80070000, 0xb0e40000,
	0x00000042, 0x800f0001, 0xb0e40001,
	0x00000005, 0x83010002, 0x80000000, 0xa0000000,
	0x00000005, 0x83010002, 0x80000002, 0xa0000000,
	0x00000005, 0x82010002, 0x80000002, 0xa0550000,
	0x00000013, 0x80020000, 0x80000002,
	0x00000004, 0x80010000, 0x80550000, 0xa0aa0000, 0x80000000,
	0x00000001, 0x80080000, 0x80ff0001,
	0x0000ffff
};

const DWORD ReceiveShader[] = {
	0xffff0104,
	0x00000040, 0x80070004, 0xb0e40000,
	0x00000042, 0x800f0000, 0xb0e40000,
	0x00000042, 0x800f0001, 0xb0e40001,
	0x00000042, 0x800f0002, 0xb0e40002,
	0x00000042, 0x800f0003, 0xb0e40003,
	0x00000005, 0x80030005, 0x80e40004, 0xa0000002,
	0x00000013, 0x80030005, 0x80e40005,
	0x00000003, 0x80040004, 0x80aa0004, 0xa0550000,
	0x00000004, 0x80010000, 0x80550000, 0xa0000000, 0x80000000,
	0x00000004, 0x80010001, 0x80550001, 0xa0000000, 0x80000001,
	0x00000004, 0x80010002, 0x80550002, 0xa0000000, 0x80000002,
	0x00000004, 0x80010003, 0x80550003, 0xa0000000, 0x80000003,
	0x00000003, 0x80010000, 0x80aa0004, 0x80000000,
	0x00000003, 0x80010001, 0x80aa0004, 0x80000001,
	0x00000003, 0x80010002, 0x80aa0004, 0x80000002,
	0x00000003, 0x80010003, 0x80aa0004, 0x80000003,
	0x0000fffd,
	0x00000058, 0x80010000, 0x80000000, 0xa0aa0000, 0xa0ff0000,
	0x00000058, 0x80010001, 0x80000001, 0xa0aa0000, 0xa0ff0000,
	0x00000058, 0x80010002, 0x80000002, 0xa0aa0000, 0xa0ff0000,
	0x00000058, 0x80010003, 0x80000003, 0xa0aa0000, 0xa0ff0000,
	0x00000012, 0x80010000, 0x80000005, 0x80000001, 0x80000000,
	0x00000012, 0x80010002, 0x80000005, 0x80000003, 0x80000002,
	0x00000012, 0x80010000, 0x80550005, 0x80000002, 0x80000000,
	0x00000004, 0x80070000, 0x80000000, 0xa0e40001, 0xa0ff0001,
	0x0000ffff
};

void setStage(Int stage, D3DTEXTUREADDRESS address, D3DTEXTUREFILTERTYPE filter)
{
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSU, address);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_ADDRESSV, address);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MINFILTER, filter);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MAGFILTER, filter);
	DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_MIPFILTER, D3DTEXF_NONE);
}

RGBColor terrainLightWithoutSun()
{
	const auto &ambient = TheGlobalData->m_terrainAmbient[0];
	RGBColor fill = ambient;
	RGBColor sun = {};
	for (Int i = 0; i < std::min(TheGlobalData->m_numGlobalLights, Int(MAX_GLOBAL_LIGHTS)); ++i)
	{
		const Real facing = std::clamp(-TheGlobalData->m_terrainLightPos[i].z, 0.0f, 1.0f);
		const auto &diffuse = TheGlobalData->m_terrainDiffuse[i];
		auto &contribution = i == 0 ? sun : fill;
		contribution.red += diffuse.red * facing;
		contribution.green += diffuse.green * facing;
		contribution.blue += diffuse.blue * facing;
	}
	const auto ratio = [](Real fill, Real sun)
	{
		const Real total = fill + sun;
		return total > 0 ? std::clamp(fill / total, 0.0f, 1.0f) : 1.0f;
	};
	return {ratio(fill.red, sun.red), ratio(fill.green, sun.green), ratio(fill.blue, sun.blue)};
}
}

RetailTerrainShadow::RetailTerrainShadow()
{
	DX8Wrapper::AddCleanupHook(this);
	for (const char *filename : {"Data/INI/Default/Environment.ini", "Data/INI/Environment.ini"})
	{
		File *file = TheFileSystem->openFile(filename, File::READ);
		if (!file)
			continue;
		const Int size = file->seek(0, File::END);
		if (size > 0 && size <= 1024 * 1024 && file->seek(0, File::START) == 0)
		{
			std::string content(size, '\0');
			if (file->read(content.data(), size) == size)
			{
				std::istringstream input(content);
				bool shadowBlock = false;
				for (std::string line; std::getline(input, line); )
				{
					std::istringstream row(line);
					std::string name;
					row >> name;
					if (stricmp(name.c_str(), "ShadowMap") == 0)
						shadowBlock = true;
					else if (shadowBlock && stricmp(name.c_str(), "End") == 0)
						break;
					else if (shadowBlock)
					{
						char separator = 0;
						Real value = 0;
						if (row >> separator >> value && separator == '=' && std::isfinite(value))
						{
							if (stricmp(name.c_str(), "MapSize") == 0 && value >= 256 && value <= 4096)
								m_authoredSize = static_cast<Int>(value);
							else if (stricmp(name.c_str(), "MaxViewDistance") == 0 && value > 0.0f)
								m_viewDistance = value;
						}
					}
				}
			}
		}
		file->close();
	}
}

RetailTerrainShadow::~RetailTerrainShadow()
{
	DX8Wrapper::RemoveCleanupHook(this);
	ReleaseResources();
}

void RetailTerrainShadow::ReleaseResources()
{
	m_ready = false;
	m_floatDepth = false;
	m_size = 0;
	if (m_depth) { m_depth->Release(); m_depth = nullptr; }
	if (m_surface) { m_surface->Release(); m_surface = nullptr; }
	if (m_texture) { m_texture->Release(); m_texture = nullptr; }
	if (auto *device = DX8Wrapper::_Get_D3D_Device8())
	{
		if (m_depthShader) device->DeletePixelShader(m_depthShader);
		if (m_cutoutShader) device->DeletePixelShader(m_cutoutShader);
		if (m_receiveShader) device->DeletePixelShader(m_receiveShader);
	}
	m_depthShader = m_cutoutShader = m_receiveShader = 0;
}

void RetailTerrainShadow::ReAcquireResources()
{
	m_ready = false;
}

bool RetailTerrainShadow::prepare(Int size)
{
	if (m_size == size)
		return m_texture && m_surface && m_depth && m_depthShader && m_cutoutShader && m_receiveShader;

	ReleaseResources();
	m_size = size;
	auto *device = DX8Wrapper::_Get_D3D_Device8();
	const auto createDepthMap = [&](D3DFORMAT format) {
		return SUCCEEDED(device->CreateTexture(size, size, 1, D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT, &m_texture));
	};
	m_floatDepth = DX8Wrapper::Get_Effect_Device9() && createDepthMap(ShadowDepthFloat);
	return (m_floatDepth || createDepthMap(D3DFMT_A8R8G8B8)) && SUCCEEDED(m_texture->GetSurfaceLevel(0, &m_surface)) &&
		SUCCEEDED(device->CreateDepthStencilSurface(size, size, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, &m_depth)) &&
		SUCCEEDED(device->CreatePixelShader(DepthShader, &m_depthShader)) &&
		SUCCEEDED(device->CreatePixelShader(CutoutDepthShader, &m_cutoutShader)) &&
		SUCCEEDED(device->CreatePixelShader(ReceiveShader, &m_receiveShader));
}

void RetailTerrainShadow::project(const Vector3 &position, Vector3 &coord) const
{
	const Vector3 delta = position - m_centre;
	coord.Set(0.5f + Vector3::Dot_Product(delta, m_right) / m_viewDistance,
		0.5f - Vector3::Dot_Product(delta, m_up) / m_viewDistance,
		0.5f + Vector3::Dot_Product(delta, m_ray) / m_viewDistance);
}

bool RetailTerrainShadow::contains(const Vector3 &position, Real radius) const
{
	Vector3 p;
	project(position, p);
	const Real margin = 0.5f + radius / m_viewDistance;
	return std::abs(p.X - 0.5f) <= margin && std::abs(p.Y - 0.5f) <= margin &&
		std::abs(p.Z - 0.5f) <= margin;
}

void RetailTerrainShadow::castMesh(MeshClass &mesh, const Matrix3D &transform, Bool tree)
{
	auto *model = mesh.Peek_Model();
	const auto vertices = model->Get_Vertex_Count();
	const auto triangles = model->Get_Polygon_Count();
	if (!vertices || !triangles || vertices > 65535)
		return;
	const bool skin = model->Get_Flag(MeshGeometryClass::SKIN);
	if (skin && (!mesh.Get_Container() || !mesh.Get_Container()->Get_HTree()))
		return;
	const Vector3 *geometry = model->Get_Vertex_Array();
	if (skin)
	{
		m_deformed.resize(vertices);
		mesh.Get_Deformed_Vertices(m_deformed.data());
		geometry = m_deformed.data();
	}
	auto *texture = tree || mesh.Is_Alpha() ? model->Peek_Single_Texture() : nullptr;
	const Vector2 *uv = texture ? model->Get_UV_Array() : nullptr;
	auto *device = DX8Wrapper::_Get_D3D_Device8();
	IDirect3DTexture8 *cutout = uv ? texture->Peek_D3D_Texture() : nullptr;
	m_vertices.resize(vertices);
	for (Int i = 0; i < vertices; ++i)
	{
		Vector3 world = geometry[i];
		if (!skin)
			Matrix3D::Transform_Vector(transform, geometry[i], &world);
		Vector3 p;
		project(world, p);
		m_vertices[i] = {p.X * m_size - 0.5f, p.Y * m_size - 0.5f, p.Z, 1.0f,
			p.Z, 0.0f, uv ? uv[i].X : 0.0f, uv ? uv[i].Y : 0.0f};
	}
	device->SetTexture(1, cutout);
	device->SetPixelShader(cutout ? m_cutoutShader : m_depthShader);
	device->SetRenderState(D3DRS_ALPHATESTENABLE, cutout != nullptr);
	const HRESULT result = device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, vertices, triangles,
		model->Get_Polygon_Array(), sizeof(TriIndex) == 6 ? D3DFMT_INDEX16 : D3DFMT_INDEX32,
		m_vertices.data(), sizeof(CasterVertex));
	if (tracingShadows())
	{
		shadowTrace.failures += FAILED(result);
		if (tree)
		{
			++shadowTrace.trees;
			shadowTrace.treeCutouts += cutout != nullptr;
			for (const auto &vertex : m_vertices)
				for (float value : {vertex.x, vertex.y, vertex.depth})
					shadowTrace.treeHash = (shadowTrace.treeHash ^ std::bit_cast<uint32_t>(value)) * 16777619u;
		}
		else
			++shadowTrace.props;
	}
}

void RetailTerrainShadow::castObject(RenderObjClass &object)
{
	if (object.Is_Hidden() || object.Is_Animation_Hidden())
		return;
	if (object.Class_ID() == RenderObjClass::CLASSID_MESH)
	{
		auto &mesh = static_cast<MeshClass &>(object);
		if (!mesh.Is_Translucent() || mesh.Is_Alpha())
			castMesh(mesh, mesh.Get_Transform());
		return;
	}
	for (Int i = 0; i < object.Get_Num_Sub_Objects(); ++i)
	{
		RenderObjClass *child = object.Get_Sub_Object(i);
		if (!child)
			continue;
		castObject(*child);
		child->Release_Ref();
	}
}

void RetailTerrainShadow::render(SceneClass &scene, const Vector3 &centre, Int size)
{
	m_ready = false;
	if (m_authoredSize)
		size = std::min(size, m_authoredSize);
	const auto &sun = TheGlobalData->m_terrainObjectsLighting[TheGlobalData->m_timeOfDay][0].lightPos;
	if (sun.z >= -0.01f || !prepare(std::clamp(size, 256, 4096)))
		return;

	m_centre = centre;
	m_ray.Set(sun.x, sun.y, sun.z);
	m_ray.Normalize();
	Vector3::Cross_Product(Vector3(0, 0, 1), m_ray, &m_right);
	if (m_right.Length2() < 1e-6f)
		m_right.Set(1, 0, 0);
	m_right.Normalize();
	Vector3::Cross_Product(m_ray, m_right, &m_up);

	auto *device = DX8Wrapper::_Get_D3D_Device8();
	IDirect3DSurface8 *target = nullptr;
	IDirect3DSurface8 *depth = nullptr;
	D3DVIEWPORT8 viewport;
	device->GetRenderTarget(&target);
	device->GetDepthStencilSurface(&depth);
	device->GetViewport(&viewport);
	DWORD coordIndex = 0, texTransform = 0, zBias = 0;
	if (tracingShadows())
	{
		const unsigned sequence = shadowTrace.sequence + 1;
		shadowTrace = {};
		shadowTrace.sequence = sequence;
		device->GetTextureStageState(1, D3DTSS_TEXCOORDINDEX, &coordIndex);
		device->GetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, &texTransform);
		device->GetRenderState(D3DRS_ZBIAS, &zBias);
	}
	const HRESULT targetResult = target && depth ? device->SetRenderTarget(m_surface, m_depth) : E_FAIL;
	HRESULT clearResult = E_FAIL;
	if (SUCCEEDED(targetResult))
	{
		const D3DVIEWPORT8 shadowViewport{0, 0, DWORD(m_size), DWORD(m_size), 0, 1};
		device->SetViewport(&shadowViewport);
		clearResult = device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xffff0000, 1.0f, 0);
		device->SetRenderState(D3DRS_ZENABLE, TRUE);
		device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
		device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
		device->SetRenderState(D3DRS_ALPHAREF, 96);
		device->SetRenderState(D3DRS_FOGENABLE, FALSE);
		device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
		device->SetRenderState(D3DRS_COLORWRITEENABLE, 15);
		device->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX2);
		const float encode[4] = {1.0f, 255.0f / 256.0f, m_floatDepth ? 0.0f : -1.0f / 255.0f, 0.0f};
		device->SetPixelShaderConstant(0, encode, 1);
		setStage(1, D3DTADDRESS_WRAP, D3DTEXF_LINEAR);
		for (DWORD stage = 0; stage < 2; ++stage)
		{
			device->SetTextureStageState(stage, D3DTSS_TEXCOORDINDEX, stage);
			device->SetTextureStageState(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		}

		SceneIterator *it = scene.Create_Iterator();
		for (it->First(); !it->Is_Done(); it->Next())
		{
			auto *object = it->Current_Item();
			if (object->Class_ID() != RenderObjClass::CLASSID_HLOD && object->Class_ID() != RenderObjClass::CLASSID_MESH)
				continue;
			const auto *info = static_cast<const DrawableInfo *>(object->Get_User_Data());
			if (!info || !info->m_drawable || !info->m_drawable->getTemplate() ||
				!info->m_drawable->getTemplate()->getShadowType())
				continue;
			const auto &sphere = object->Get_Bounding_Sphere();
			if (contains(sphere.Center, sphere.Radius))
				castObject(*object);
		}
		scene.Destroy_Iterator(it);
		if (TheTerrainRenderObject)
			TheTerrainRenderObject->castRetailTreeShadows(*this);
		device->SetTexture(1, nullptr);
		device->SetPixelShader(0);
		device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		device->SetRenderTarget(target, depth);
		device->SetViewport(&viewport);
		DX8Wrapper::Invalidate_Cached_Render_States();
		ShaderClass::Invalidate();
		m_ready = true;
	}
	if (tracingShadows())
		std::fprintf(stderr,
			"[Retail Shadow] pass=%u game=%u center=(%.3f,%.3f,%.3f) size=%d float=%d target=%08x clear=%08x props=%u trees=%u cutouts=%u hash=%08x failures=%u coord=%u texTransform=%u zBias=%u\n",
			shadowTrace.sequence, TheGameLogic->getFrame(), centre.X, centre.Y, centre.Z, m_size, m_floatDepth,
			static_cast<unsigned>(targetResult), static_cast<unsigned>(clearResult), shadowTrace.props, shadowTrace.trees,
			shadowTrace.treeCutouts, shadowTrace.treeHash, shadowTrace.failures, static_cast<unsigned>(coordIndex),
			static_cast<unsigned>(texTransform), static_cast<unsigned>(zBias));
	if (target) target->Release();
	if (depth) depth->Release();
}

void RetailTerrainShadow::traceReceiver(bool compiled, bool authored, bool legacy) const
{
	if (tracingShadows())
		std::fprintf(stderr, "[Retail Shadow] receiver=%u game=%u compiled=%d authored=%d legacy=%d ready=%d\n",
			shadowTrace.sequence, TheGameLogic->getFrame(), compiled, authored, legacy, m_ready);
}

void RetailTerrainShadow::worldToShadowMatrix(D3DMATRIX &matrix) const
{
	const Vector3 right = m_right * (1.0f / m_viewDistance);
	const Vector3 up = m_up * (-1.0f / m_viewDistance);
	const Vector3 ray = m_ray * (1.0f / m_viewDistance);
	matrix = D3DXMATRIX(right.X, up.X, ray.X, 0,
		right.Y, up.Y, ray.Y, 0, right.Z, up.Z, ray.Z, 0,
		0.5f - Vector3::Dot_Product(m_centre, right),
		0.5f - Vector3::Dot_Product(m_centre, up),
		0.5f - Vector3::Dot_Product(m_centre, ray), 1);
}

bool RetailTerrainShadow::getAuthoredShadow(void *&texture, D3DMATRIX &worldToShadow, float &texelSize) const
{
	if (!m_ready || !m_floatDepth || ShaderClass::Is_Backface_Culling_Inverted())
		return false;
	texture = DX8Wrapper::Get_Effect_Texture9(m_texture);
	if (!texture)
		return false;
	worldToShadowMatrix(worldToShadow);
	texelSize = 1.0f / m_size;
	return true;
}

bool RetailTerrainShadow::beginTerrainPass()
{
	if (!m_ready || ShaderClass::Is_Backface_Culling_Inverted())
		return false;

	Matrix4x4 view;
	DX8Wrapper::_Get_DX8_Transform(D3DTS_VIEW, view);
	D3DXMATRIX inverse;
	if (!D3DXMatrixInverse(&inverse, nullptr, reinterpret_cast<const D3DXMATRIX *>(&view)))
		return false;

	D3DXMATRIX projection;
	worldToShadowMatrix(projection);
	const float offset = 0.5f / m_size;
	auto *device = DX8Wrapper::_Get_D3D_Device8();
	DX8Wrapper::Apply_Render_State_Changes();
	for (Int pass = 0; pass < 4; ++pass)
	{
		projection._41 += (pass & 1) ? offset : -offset;
		projection._42 += (pass & 2) ? offset : -offset;
		const D3DXMATRIX transform = inverse * projection;
		device->SetTransform(static_cast<D3DTRANSFORMSTATETYPE>(D3DTS_TEXTURE0 + pass), &transform);
		device->SetTexture(pass, m_texture);
		DX8Wrapper::Set_DX8_Texture_Stage_State(pass, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
		DX8Wrapper::Set_DX8_Texture_Stage_State(pass, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT3);
		setStage(pass, D3DTADDRESS_CLAMP, D3DTEXF_POINT);
		projection._41 -= (pass & 1) ? offset : -offset;
		projection._42 -= (pass & 2) ? offset : -offset;
	}

	const RGBColor light = terrainLightWithoutSun();
	const float constants[3][4] = {
		{1.0f / 255.0f, 1.5f / m_viewDistance, 1.0f, 0.0f},
		{light.red - 1.0f, light.green - 1.0f, light.blue - 1.0f, 1.0f},
		{float(m_size), 0.0f, 0.0f, 0.0f}
	};
	device->SetPixelShaderConstant(0, constants, 3);
	DX8Wrapper::Set_Pixel_Shader(m_receiveShader);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZENABLE, TRUE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC, D3DCMP_EQUAL);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHABLENDENABLE, TRUE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_DESTBLEND, D3DBLEND_ZERO);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ALPHATESTENABLE, FALSE);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_FOGENABLE, FALSE);
	return true;
}

void RetailTerrainShadow::endTerrainPass()
{
	DX8Wrapper::Set_Pixel_Shader(0);
	for (Int stage = 0; stage < 4; ++stage)
	{
		DX8Wrapper::_Get_D3D_Device8()->SetTexture(stage, nullptr);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXCOORDINDEX, stage);
		DX8Wrapper::Set_DX8_Texture_Stage_State(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	}
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	DX8Wrapper::Set_DX8_Render_State(D3DRS_ZWRITEENABLE, TRUE);
	DX8Wrapper::Invalidate_Cached_Render_States();
	ShaderClass::Invalidate();
}
