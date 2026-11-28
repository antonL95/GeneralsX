#include "W3DDevice/GameClient/RetailEffectInput.h"

#include "Common/RetailFileLayers.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameClient/DrawableInfo.h"
#include "GameClient/Snow.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/RetailTerrainShadow.h"
#include "W3DDevice/GameClient/TerrainTex.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/W3DShroud.h"
#include "WW3D2/assetmgr.h"
#include "WW3D2/dx8fvf.h"
#include "WW3D2/dx8indexbuffer.h"
#include "WW3D2/dx8polygonrenderer.h"
#include "WW3D2/dx8renderer.h"
#include "WW3D2/dx8vertexbuffer.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/htree.h"
#include "WW3D2/lightenvironment.h"
#include "WW3D2/mesh.h"
#include "WW3D2/meshmdl.h"
#include "WW3D2/shader.h"
#include "WW3D2/sortingrenderer.h"
#include "WW3D2/vertmaterial.h"
#include "WW3D2/w3dbfmechunks.h"
#include "WW3D2/w3dfxmaterial.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <set>
#include <tuple>

namespace RetailFX
{

struct PackedMesh
{
	struct Source
	{
		const W3dFx::Mesh *fx = nullptr;
		const Vector3 *position = nullptr;
		const Vector3 *normal = nullptr;
		const Vector2 *uv = nullptr;
		const unsigned *color = nullptr;
		const TriIndex *polygons = nullptr;
		Int vertexCount = 0;
		Int polygonCount = 0;
		unsigned pass = 0;
		unsigned material = 0;

		bool operator==(const Source &) const = default;
	} source;
	std::vector<NormalMappedVertex> vertices;
	std::vector<uint32_t> indices;
};

namespace
{

constexpr auto normalMappedEffect = "shaders/compiled/normalmapped.fxo";

struct TextureRelease
{
	void operator()(TextureClass *texture) const { texture->Release_Ref(); }
};

class NormalMappedRuntime : public DX8_CleanupHook
{
public:
	NormalMappedRuntime() { DX8Wrapper::AddCleanupHook(this); }
	~NormalMappedRuntime() { DX8Wrapper::RemoveCleanupHook(this); }
	void ReleaseResources() override
	{
		for (auto &pass : passes)
			pass.reset();
		for (auto &pass : shadowed)
			pass.reset();
		composite.reset();
		initialized.fill(false);
		shadowsInitialized = false;
	}
	void ReAcquireResources() override {}

	std::array<std::unique_ptr<NativePass>, 2> passes;
	std::array<std::unique_ptr<NativePass>, 4> shadowed;
	std::unique_ptr<NativePass> composite;
	std::set<std::tuple<std::string, unsigned, unsigned, bool, bool>> traced;
	std::vector<NormalMappedVertex> posed;
	std::vector<uint32_t> sortedIndices;
	std::vector<Vector3> positions;
	std::vector<Vector3> normals;
	std::vector<Matrix3D> inverseBindPose;
	std::vector<Matrix3D> bindPose;
	std::array<bool, 2> initialized{};
	std::array<bool, 2> reportedFailure{};
	bool shadowsInitialized = false;
};

std::unique_ptr<NormalMappedRuntime> runtime;

bool fillMesh(MeshModelClass &model, const W3dFx::Mesh *fx, PackedMesh &geometry,
	DX8PolygonRendererClass &renderer, unsigned material)
{
	const Int vertexCount = model.Get_Vertex_Count();
	const Int polygonCount = model.Get_Polygon_Count();
	if (vertexCount <= 0 || polygonCount <= 0 || (fx &&
		(fx->tangents.size() != static_cast<size_t>(vertexCount) || fx->bitangents.size() != static_cast<size_t>(vertexCount))))
		return false;
	const unsigned pass = renderer.Get_Pass();
	const auto *uv = model.Get_UV_Array(pass, 0);
	const auto *position = model.Get_Vertex_Array();
	const auto *normal = model.Get_Vertex_Normal_Array();
	const auto *color = model.Get_Color_Array(0, false);
	const auto *polys = model.MeshGeometryClass::Get_Polygon_Array();
	if (!uv || !position || !normal || !polys)
		return false;
	const PackedMesh::Source source{fx, position, normal, uv, color, polys, vertexCount, polygonCount, pass, material};
	if (source == geometry.source)
		return true;
	geometry.source = {};
	geometry.vertices.clear();
	geometry.vertices.reserve(vertexCount);
	for (Int i = 0; i < vertexCount; ++i)
	{
		const auto t = fx ? fx->tangents[i] : W3dFx::Float3{};
		const auto b = fx ? fx->bitangents[i] : W3dFx::Float3{};
		geometry.vertices.push_back({
			{position[i].X, position[i].Y, position[i].Z}, {normal[i].X, normal[i].Y, normal[i].Z},
			{t[0], t[1], t[2]}, {b[0], b[1], b[2]}, {uv[i].U, uv[i].V},
			{uv[i].U, uv[i].V}, color ? static_cast<uint32_t>(color[i]) : 0xffffffffu
		});
	}
	geometry.indices.clear();
	geometry.indices.reserve(renderer.Get_Index_Count());
	const auto *ids = fx ? &fx->passes[pass].ids : nullptr;
	for (Int i = 0; i < polygonCount; ++i)
	{
		if (ids && ids->size() > 1 && (*ids)[i] != material)
			continue;
		for (Int corner = 0; corner < 3; ++corner)
		{
			const auto index = static_cast<uint32_t>(polys[i][corner]);
			if (index >= static_cast<uint32_t>(vertexCount))
				return false;
			geometry.indices.push_back(index);
		}
	}
	if (geometry.indices.size() != renderer.Get_Index_Count())
		return false;
	geometry.source = source;
	return true;
}

std::optional<unsigned> selectMaterial(MeshModelClass &model, const W3dFx::Mesh &fx, unsigned pass,
	const VertexMaterialClass *vertexMaterial)
{
	if (pass >= fx.passes.size() || !vertexMaterial)
		return {};
	const auto &data = fx.passes[pass];
	if (data.ids.size() == 1 && !data.vertexMaterials.empty())
		return data.ids.front();
	if (data.ids.size() != static_cast<size_t>(model.Get_Polygon_Count()) || data.vertexMaterials.empty())
		return {};
	const auto crc = vertexMaterial->Get_CRC();
	for (unsigned polygon = 0; polygon < data.ids.size(); ++polygon)
	{
		const auto *polygonMaterial = model.Peek_Polygon_Material(polygon, pass);
		if (polygonMaterial && polygonMaterial->Get_CRC() == crc)
			return data.ids[polygon];
	}
	return {};
}

std::span<const NormalMappedVertex> poseMesh(MeshClass &mesh, const PackedMesh &geometry)
{
	auto &model = *mesh.Peek_Model();
	if (!model.Get_Flag(MeshGeometryClass::SKIN))
		return geometry.vertices;

	const auto *container = mesh.Get_Container();
	const auto *tree = container ? container->Get_HTree() : nullptr;
	const auto *links = model.Get_Vertex_Bone_Links();
	const auto count = geometry.vertices.size();
	if (!tree || !links || std::any_of(links, links + count, [tree](auto bone) { return bone >= tree->Num_Pivots(); }))
		return {};

	runtime->posed = geometry.vertices;
	runtime->positions.resize(count);
	runtime->normals.resize(count);
	mesh.Get_Deformed_Vertices(runtime->positions.data(), runtime->normals.data());
	const auto *influences = geometry.source.fx ? model.Peek_Second_Skin_Influences() : nullptr;
	auto &bindPose = runtime->bindPose;
	auto &inverseBindPose = runtime->inverseBindPose;
	if (influences)
	{
		bindPose.resize(tree->Num_Pivots());
		inverseBindPose.resize(tree->Num_Pivots());
		const Matrix3D identity(true);
		for (Int pivot = 0; pivot < tree->Num_Pivots(); ++pivot)
		{
			tree->Simple_Evaluate_Pivot(pivot, identity, &bindPose[pivot]);
			bindPose[pivot].Get_Orthogonal_Inverse(inverseBindPose[pivot]);
		}
	}
	for (size_t i = 0; i < count; ++i)
	{
		const auto bone = links[i];
		const auto &transform = tree->Get_Transform(bone);
		auto &vertex = runtime->posed[i];
		const auto rotateBasis = [&](const float (&axis)[3], float (&result)[3]) {
			const Vector3 local(axis[0], axis[1], axis[2]);
			auto direction = transform.Rotate_Vector(local);
			if (influences && influences[i].Weight1 && influences[i].Bone1 < tree->Num_Pivots())
			{
				const auto &influence = influences[i];
				const auto second = influence.Bone1;
				const auto bindDirection = bindPose[bone].Rotate_Vector(local);
				const auto secondLocal = inverseBindPose[second].Rotate_Vector(bindDirection);
				direction = direction * (influence.Weight0 / 100.0f) +
					tree->Get_Transform(second).Rotate_Vector(secondLocal) * (influence.Weight1 / 100.0f);
				direction.Normalize();
			}
			result[0] = direction.X;
			result[1] = direction.Y;
			result[2] = direction.Z;
		};
		const auto &position = runtime->positions[i];
		const auto &normal = runtime->normals[i];
		vertex.position[0] = position.X;
		vertex.position[1] = position.Y;
		vertex.position[2] = position.Z;
		vertex.normal[0] = normal.X;
		vertex.normal[1] = normal.Y;
		vertex.normal[2] = normal.Z;
		if (geometry.source.fx)
		{
			rotateBasis(geometry.vertices[i].tangent, vertex.tangent);
			rotateBasis(geometry.vertices[i].bitangent, vertex.bitangent);
		}
	}
	return runtime->posed;
}

Register fromVector(const Vector3 &value)
{
	return {value.X, value.Y, value.Z, 0.0f};
}

bool isSkyboxMesh(MeshClass &mesh)
{
	RenderObjClass *root = &mesh;
	while (root->Get_Container())
		root = root->Get_Container();
	void *userData = root->Get_User_Data();
	if (!userData || *static_cast<const int *>(userData) == RenderObjClass::USER_DATA_MATERIAL_OVERRIDE)
		return false;
	const auto *info = static_cast<const DrawableInfo *>(userData);
	const Drawable *draw = info->m_drawable;
	if (!draw)
		return false;
	if (draw->isKindOf(KINDOF_SKYBOX))
		return true;
	const ThingTemplate *tmpl = draw->getTemplate();
	if (!tmpl)
		return false;
	for (const AsciiString &tag : tmpl->getRotWKObjectData().browser)
	{
		if (tag.compareNoCase("SKYBOXES") == 0)
			return true;
	}
	return false;
}

BlendFactor toBlendFactor(ShaderClass::SrcBlendFuncType src)
{
	switch (src)
	{
		case ShaderClass::SRCBLEND_ZERO: return BlendFactor::Zero;
		case ShaderClass::SRCBLEND_ONE: return BlendFactor::One;
		case ShaderClass::SRCBLEND_SRC_ALPHA: return BlendFactor::SrcAlpha;
		case ShaderClass::SRCBLEND_ONE_MINUS_SRC_ALPHA: return BlendFactor::InvSrcAlpha;
		default: return BlendFactor::One;
	}
}

BlendFactor toBlendFactor(ShaderClass::DstBlendFuncType dst)
{
	switch (dst)
	{
		case ShaderClass::DSTBLEND_ZERO: return BlendFactor::Zero;
		case ShaderClass::DSTBLEND_ONE: return BlendFactor::One;
		case ShaderClass::DSTBLEND_SRC_COLOR: return BlendFactor::SrcColor;
		case ShaderClass::DSTBLEND_ONE_MINUS_SRC_COLOR: return BlendFactor::InvSrcColor;
		case ShaderClass::DSTBLEND_SRC_ALPHA: return BlendFactor::SrcAlpha;
		case ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA: return BlendFactor::InvSrcAlpha;
		default: return BlendFactor::Zero;
	}
}

NativePass *ensureRetailPass(void *device, bool normalMapped, const char *label)
{
	if (!runtime)
		runtime = std::make_unique<NormalMappedRuntime>();
	const auto slot = normalMapped ? 0u : 1u;
	auto &pass = runtime->passes[slot];
	if (!runtime->initialized[slot])
	{
		runtime->initialized[slot] = true;
		if (const auto effect = loadEffectFile(normalMapped ? normalMappedEffect : "shaders/compiled/defaultw3d.fxo"))
			pass = normalMapped ? NativePass::makeNormalMapped(device, *effect, DX8Wrapper::Create_Effect_Shader9)
				: NativePass::makeDefaultMesh(device, *effect, DX8Wrapper::Create_Effect_Shader9);
		if (!pass)
			std::fprintf(stderr, "[RetailFX] Could not load %s\n", label);
	}
	return pass.get();
}

void writeSortedLight(float (&dst)[4], const Register &src)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = src[3];
}

Register readSortedLight(const float (&src)[4])
{
	return {src[0], src[1], src[2], src[3]};
}

bool drawEffectPass(
	void *device,
	bool normalMapped,
	const W3dFx::Material *material,
	unsigned meshPass,
	unsigned materialId,
	const char *meshName,
	TextureClass *diffuse,
	TextureClass *normalTexturePtr,
	VertexMaterialClass *vertexMaterial,
	const Matrix4x4 &world,
	std::span<const Register, 4> fog,
	std::span<const Register, 6> directional,
	std::span<const Register, 3> point,
	const Register &ambientLight,
	bool havePoint,
	float vertexOpacity,
	float overrideOpacity,
	bool alphaTest,
	uint8_t alphaRef,
	bool alphaBlend,
	bool depthWrite,
	bool twoSided,
	BlendFactor srcBlend,
	BlendFactor dstBlend,
	std::span<const NormalMappedVertex> vertices,
	std::span<const uint32_t> indices,
	bool sorted,
	const Matrix4x4 *viewOverride = nullptr)
{
	const char *label = normalMapped ? "NormalMapped Default_M" : "DefaultW3D Default";
	NativePass *pass = ensureRetailPass(device, normalMapped, label);
	if (!pass)
		return false;

	auto *shroud = TheTerrainRenderObject->getShroud();
	if (!shroud || shroud->getTextureWidth() <= 0 || shroud->getTextureHeight() <= 0 ||
		shroud->getCellWidth() <= 0.0f || shroud->getCellHeight() <= 0.0f)
		return false;

	auto *normal = nativeTexture(normalTexturePtr);
	auto *base = nativeTexture(diffuse);
	auto *mask = nativeTexture(shroud->getShroudTexture());
	D3DXMATRIX worldToShadow;
	float shadowTexel = 0.0f;
	void *shadow = nullptr;
	const bool useShadows = normalMapped && !alphaBlend && material && material->technique == 2 && TheRetailTerrainShadow &&
		TheRetailTerrainShadow->getAuthoredShadow(shadow, worldToShadow, shadowTexel);
	const auto cloud = !normalMapped || useShadows ? nativeTexture(TheTerrainRenderObject->peekCloudTexture()) : nullptr;
	if ((normalMapped && !normal) || !base || !mask)
		return false;

	const float scaleU = 1.0f / (shroud->getCellWidth() * static_cast<float>(shroud->getTextureWidth()));
	const float scaleV = 1.0f / (shroud->getCellHeight() * static_cast<float>(shroud->getTextureHeight()));
	const Register shroudUV{scaleU, scaleV,
		(shroud->getCellWidth() - shroud->getDrawOriginX()) * scaleU,
		(shroud->getCellHeight() - shroud->getDrawOriginY()) * scaleV};

	auto *device8 = DX8Wrapper::_Get_D3D_Device8();
	D3DXMATRIX view, projection, inverseView;
	if (viewOverride)
		std::copy_n(&(*viewOverride)[0][0], 16, &view.m[0][0]);
	else if (FAILED(device8->GetTransform(D3DTS_VIEW, &view)))
		return false;
	if (FAILED(device8->GetTransform(D3DTS_PROJECTION, &projection)) ||
		!D3DXMatrixInverse(&inverseView, nullptr, &view))
		return false;

	Register ambient{}, colorDiffuse{}, specular{}, emissive{};
	if (!normalMapped && vertexMaterial)
	{
		Vector3 color;
		vertexMaterial->Get_Ambient(&color);
		ambient = fromVector(color);
		vertexMaterial->Get_Diffuse(&color);
		colorDiffuse = fromVector(color);
		vertexMaterial->Get_Specular(&color);
		specular = fromVector(color);
		vertexMaterial->Get_Emissive(&color);
		emissive = fromVector(color);
	}
	const auto diffuseColor = material ? material->get<W3dFx::Float4>("DiffuseColor", {1.0f, 1.0f, 1.0f, 1.0f})
		: W3dFx::Float4{1.0f, 1.0f, 1.0f, 1.0f};

	if (useShadows && !runtime->shadowsInitialized)
	{
		runtime->shadowsInitialized = true;
		if (const auto effect = loadEffectFile(normalMappedEffect))
		{
			runtime->composite = NativePass::makeNormalMappedComposite(device, *effect, DX8Wrapper::Create_Effect_Shader9);
			if (runtime->composite)
				for (unsigned index = 0; index < runtime->shadowed.size(); ++index)
					runtime->shadowed[index] = NativePass::makeShadowedNormalMapped(device, *effect,
						DX8Wrapper::Create_Effect_Shader9, (index & 1u) != 0, (index & 2u) != 0);
		}
	}

	const auto setBinding = [&](const Binding &binding, std::span<Register> floats, std::span<int>) {
		if (floats.empty())
			return;
		const auto &name = binding.name;
		if (name == "World")
		{
			for (size_t i = 0; i < std::min(floats.size(), size_t{4}); ++i)
				floats[i] = {world[i][0], world[i][1], world[i][2], world[i][3]};
		}
		else if (name == "View")
			writeMatrix(floats, view);
		else if (name == "Projection")
			writeMatrix(floats, projection);
		else if (name == "ViewI")
			writeMatrix(floats, inverseView);
		else if (name == "Shroud")
			floats[0] = shroudUV;
		else if (name == "ShadowInfo" && useShadows)
		{
			writeMatrix(floats, worldToShadow);
			if (floats.size() > 4)
				floats[4] = {0.0f, 0.0f, shadowTexel, shadowTexel};
		}
		else if (name == "Fog")
			std::copy_n(fog.begin(), std::min(fog.size(), floats.size()), floats.begin());
		else if (name == "DirectionalLight")
			std::copy_n(directional.begin(), std::min(directional.size(), floats.size()), floats.begin());
		else if (name == "PointLight")
			std::copy_n(point.begin(), std::min(point.size(), floats.size()), floats.begin());
		else if (name == "AmbientLightColor")
			floats[0] = ambientLight;
		else if (name == "DiffuseColor")
			floats[0] = diffuseColor;
		else if (name == "ColorAmbient")
			floats[0] = ambient;
		else if (name == "ColorDiffuse")
			floats[0] = colorDiffuse;
		else if (name == "ColorSpecular")
			floats[0] = specular;
		else if (name == "ColorEmissive")
			floats[0] = emissive;
		else if (name == "Shininess" && !normalMapped && vertexMaterial)
			floats[0][0] = vertexMaterial->Get_Shininess();
		else if (name == "Opacity")
			floats[0][0] = normalMapped ? overrideOpacity : vertexOpacity;
		else if (name == "OpacityOverride")
			floats[0][0] = overrideOpacity;
		else if (name == "HouseColorEnable")
			floats[0][0] = 0.0f;
		else if (name == "Cloud")
			floats[0] = W3DShaderManager::getRetailCloudUV();
		else if (material)
		{
			if (const auto property = material->properties.find(name); property != material->properties.end())
			{
				if (const auto *scalar = std::get_if<float>(&property->second))
					floats[0][0] = *scalar;
				else if (const auto *v = std::get_if<W3dFx::Float4>(&property->second))
					floats[0] = *v;
				else if (const auto *v = std::get_if<W3dFx::Float3>(&property->second))
					std::copy(v->begin(), v->end(), floats[0].begin());
			}
		}
	};
	const auto getTexture = [&](std::string_view name) -> void * {
		if (name == "NormalMap") return normal;
		if (name == "DiffuseTexture" || name == "Texture_0") return base;
		if (name == "ShroudTexture") return mask;
		if (name == "CloudTexture") return cloud;
		if (name == "ShadowMap") return shadow;
		return nullptr;
	};
	const auto draw = [&](NativePass &effectPass) {
		return effectPass.drawNormalMapped(device, vertices, indices, setBinding, getTexture,
			alphaTest, alphaBlend, depthWrite, twoSided, srcBlend, dstBlend, alphaRef);
	};
	const auto variant = (havePoint ? 1u : 0u) | (fog[0][0] != 0.0f ? 2u : 0u);
	const auto &shadowedPass = runtime->shadowed[variant];
	const bool drewShadows = useShadows && shadowedPass && runtime->composite &&
		draw(*shadowedPass) && draw(*runtime->composite);
	const bool drawn = drewShadows || draw(*pass);
	if (drewShadows)
		label = "NormalMapped Default with authored shadows";
	if (drawn && std::getenv("GENERALSX_TRACE_RETAIL_EFFECTS") &&
		runtime->traced.emplace(meshName ? meshName : "", meshPass, materialId, alphaBlend, sorted).second)
	{
		std::fprintf(stderr, "[RetailFX] Drew %s (%s)\n", label, meshName ? meshName : "");
		if (normalMapped && (materialId > 0 || alphaBlend))
			std::fprintf(stderr, "[RetailFX] NormalMapped batch (%s) pass=%u material=%u triangles=%zu blend=%d opacity=%.3f\n",
				meshName ? meshName : "", meshPass, materialId, indices.size() / 3, alphaBlend, overrideOpacity);
		else if (!normalMapped && sorted)
			std::fprintf(stderr, "[RetailFX] DefaultW3D sorted (%s) triangles=%zu opacity=%.3f override=%.3f\n",
				meshName ? meshName : "", indices.size() / 3, vertexOpacity, overrideOpacity);
	}
	const auto slot = normalMapped ? 0u : 1u;
	if (!drawn && !runtime->reportedFailure[slot])
	{
		std::fprintf(stderr, "[RetailFX] Could not bind or draw %s\n", label);
		runtime->reportedFailure[slot] = true;
	}
	return drawn;
}

bool drawSortedRetailMesh(const RenderStateStruct &state, const SortedRetailEffectData &effect,
	const VertexFormatXYZNDUV2 *sortedVerts, unsigned vertexCount,
	const unsigned short *sortedIndices, unsigned polygonCount)
{
	if (!RetailFileLayers::enabled() || !TheTerrainRenderObject || !sortedVerts || !sortedIndices ||
		vertexCount == 0 || polygonCount == 0 || !state.material || !state.Textures[0])
		return false;
	auto *device = DX8Wrapper::Get_Effect_Device9();
	if (!device)
		return false;
	if (!runtime)
		runtime = std::make_unique<NormalMappedRuntime>();

	runtime->posed.resize(vertexCount);
	for (unsigned i = 0; i < vertexCount; ++i)
	{
		const auto &src = sortedVerts[i];
		auto &dst = runtime->posed[i];
		dst.position[0] = src.x;
		dst.position[1] = src.y;
		dst.position[2] = src.z;
		dst.normal[0] = src.nx;
		dst.normal[1] = src.ny;
		dst.normal[2] = src.nz;
		dst.tangent[0] = 0.0f;
		dst.tangent[1] = 0.0f;
		dst.tangent[2] = 0.0f;
		dst.bitangent[0] = 0.0f;
		dst.bitangent[1] = 0.0f;
		dst.bitangent[2] = 0.0f;
		dst.uv[0] = src.u1;
		dst.uv[1] = src.v1;
		dst.uv1[0] = src.u2;
		dst.uv1[1] = src.v2;
		dst.color = src.diffuse;
	}
	const unsigned indexCount = polygonCount * 3;
	runtime->sortedIndices.resize(indexCount);
	for (unsigned i = 0; i < indexCount; ++i)
		runtime->sortedIndices[i] = sortedIndices[i];

	std::array<Register, 4> fog;
	for (size_t i = 0; i < fog.size(); ++i)
		fog[i] = readSortedLight(effect.fog[i]);
	std::array<Register, 6> directional;
	for (size_t i = 0; i < directional.size(); ++i)
		directional[i] = readSortedLight(effect.directional[i]);
	std::array<Register, 3> point;
	for (size_t i = 0; i < point.size(); ++i)
		point[i] = readSortedLight(effect.point[i]);
	const Register ambientLight = readSortedLight(effect.ambientLight);

	const Matrix4x4 world = state.world.Transpose();
	return drawEffectPass(
		device,
		false,
		nullptr,
		0,
		0,
		effect.meshName,
		static_cast<TextureClass *>(state.Textures[0]),
		nullptr,
		state.material,
		world,
		fog,
		directional,
		point,
		ambientLight,
		effect.hasPointLight,
		effect.vertexOpacity,
		effect.overrideOpacity,
		effect.alphaTest,
		effect.alphaRef,
		effect.alphaBlend,
		effect.depthWrite,
		effect.twoSided,
		static_cast<BlendFactor>(effect.srcBlend),
		static_cast<BlendFactor>(effect.dstBlend),
		runtime->posed,
		runtime->sortedIndices,
		true,
		&state.view);
}

bool drawRetailMesh(MeshClass &mesh, TextureClass *diffuse, TextureClass *secondary,
	VertexMaterialClass *vertexMaterial, const ShaderClass &shader, const Matrix4x4 &world, DX8PolygonRendererClass &renderer)
{
	if (!RetailFileLayers::enabled() || !TheTerrainRenderObject || !diffuse ||
		ShaderClass::Is_Backface_Culling_Inverted() || isSkyboxMesh(mesh))
		return false;
	auto &model = *mesh.Peek_Model();
	const auto *fx = model.Peek_Fx_Materials();
	const bool normalMapped = fx && !fx->materials.empty();
	const auto *lights = mesh.Get_Lighting_Environment();
	auto *shroud = TheTerrainRenderObject->getShroud();
	if (!lights ||
		!shroud || shroud->getTextureWidth() <= 0 || shroud->getTextureHeight() <= 0 ||
		shroud->getCellWidth() <= 0.0f || shroud->getCellHeight() <= 0.0f)
		return false;
	const unsigned meshPass = renderer.Get_Pass();
	const auto materialId = normalMapped ? selectMaterial(model, *fx, meshPass, vertexMaterial) : std::optional(0u);
	if (!materialId || (normalMapped && (*materialId >= fx->materials.size() ||
		fx->materials[*materialId].effect != "normalmapped.fx")))
		return false;
	if (!normalMapped && (model.Get_Pass_Count() != 1 || meshPass != 0 ||
		secondary || !vertexMaterial || vertexMaterial->Peek_Mapper() ||
		shader.Get_Depth_Compare() != ShaderClass::PASS_LEQUAL))
		return false;
	if (normalMapped && mesh.Get_Animation_Opacity() == 1.0f &&
		model.Get_Flag(MeshGeometryClass::SORT) && WW3D::Is_Sorting_Enabled())
		return false;

	static const W3dFx::Material noFx{};
	const auto &material = normalMapped ? fx->materials[*materialId] : noFx;
	const auto normalName = material.get<std::string>("NormalMap", {});
	const auto diffuseColor = material.get<W3dFx::Float4>("DiffuseColor", {1.0f, 1.0f, 1.0f, 1.0f});
	if (normalMapped && (normalName.empty() || stricmp(normalName.c_str(), "None") == 0))
		return false;
	const float vertexOpacity = vertexMaterial ? vertexMaterial->Get_Opacity() : 1.0f;
	const float opacity = mesh.Get_Animation_Opacity() * mesh.Get_Alpha_Override();
	const bool shaderBlend = !normalMapped && (shader.Get_Src_Blend_Func() != ShaderClass::SRCBLEND_ONE ||
		shader.Get_Dst_Blend_Func() != ShaderClass::DSTBLEND_ZERO);
	const bool alphaBlend = normalMapped
		? (opacity < 1.0f || diffuseColor[3] < 1.0f || material.get<bool>("AlphaBlendEnable", false))
		: (shaderBlend || vertexOpacity < 1.0f || opacity < 1.0f);
	const bool useSorting = !normalMapped && WW3D::Is_Sorting_Enabled() &&
		model.Get_Sort_Level() == SORT_LEVEL_NONE &&
		(model.Get_Flag(MeshGeometryClass::SORT) || (!mesh.Is_Additive() && alphaBlend));

	auto *device = DX8Wrapper::Get_Effect_Device9();
	if (!device)
		return false;
	if (!ensureRetailPass(device, normalMapped, normalMapped ? "NormalMapped Default_M" : "DefaultW3D Default"))
		return false;

	const std::unique_ptr<TextureClass, TextureRelease> normalTexture(
		normalMapped ? WW3DAssetManager::Get_Instance()->Get_Texture(normalName.c_str()) : nullptr);
	if (normalMapped && !nativeTexture(normalTexture.get()))
		return false;

	PackedMesh temporary;
	auto &stored = renderer.Effect_Mesh();
	const bool cacheable = mesh.Class_ID() == RenderObjClass::CLASSID_MESH;
	if (cacheable && !stored)
		stored = std::make_shared<PackedMesh>();
	auto &geometry = cacheable ? *stored : temporary;
	if (!fillMesh(model, normalMapped ? fx : nullptr, geometry, renderer, *materialId))
		return false;
	auto *device8 = DX8Wrapper::_Get_D3D_Device8();
	const auto fog = fogBindings(device8);
	RenderStateStruct previous;
	if (useSorting)
	{
		if (!shaderBlend && opacity <= 0.0f)
			return true;
		if (geometry.vertices.size() >= 65536u || geometry.indices.size() >= 65536u)
			return false;
		DX8Wrapper::Get_Render_State(previous);
		SortingRendererClass::Ensure_Dynamic_Sorting_Capacity(
			static_cast<unsigned>(geometry.vertices.size()), static_cast<unsigned>(geometry.indices.size()));
	}
	const auto vertices = poseMesh(mesh, geometry);
	if (vertices.empty())
	{
		if (useSorting)
			DX8Wrapper::Set_Render_State(previous);
		return false;
	}

	std::array<Register, 6> directional{};
	std::array<Register, 3> point{{{}, {}, {1.0f, 0.0f, 0.0f, 0.0f}}};
	unsigned numDirectional = 0;
	const RGBColor weatherScale = TheSnowManager ? TheSnowManager->getRetailCloudyLightScale() : RGBColor{1.0f, 1.0f, 1.0f};
	bool havePoint = false;
	for (Int i = 0; i < lights->Get_Light_Count(); ++i)
	{
		if (lights->isPointLight(i))
		{
			if (!havePoint)
			{
				point[0] = fromVector(lights->getPointDiffuse(i));
				point[1] = fromVector(lights->getPointCenter(i));
				point[2][0] = std::max(lights->getPointOrad(i), 1.0f);
				havePoint = true;
			}
		}
		else if (numDirectional < directional.size() / 2)
		{
			auto diffuseLight = fromVector(lights->Get_Light_Diffuse(i));
			diffuseLight[0] *= weatherScale.red;
			diffuseLight[1] *= weatherScale.green;
			diffuseLight[2] *= weatherScale.blue;
			directional[numDirectional * 2] = diffuseLight;
			directional[numDirectional * 2 + 1] = fromVector(lights->Get_Light_Direction(i));
			++numDirectional;
		}
	}
	auto ambientLight = fromVector(lights->Get_Equivalent_Ambient());
	ambientLight[0] *= weatherScale.red;
	ambientLight[1] *= weatherScale.green;
	ambientLight[2] *= weatherScale.blue;

	const bool alphaTest = normalMapped ? material.get<bool>("AlphaTestEnable", false) :
		shader.Get_Alpha_Test() != ShaderClass::ALPHATEST_DISABLE;
	const uint8_t alphaRef = static_cast<uint8_t>(std::clamp(static_cast<float>(0x60) * opacity, 0.0f, 255.0f));
	const BlendFactor srcBlend = shaderBlend ? toBlendFactor(shader.Get_Src_Blend_Func()) : BlendFactor::SrcAlpha;
	const BlendFactor dstBlend = shaderBlend ? toBlendFactor(shader.Get_Dst_Blend_Func()) : BlendFactor::InvSrcAlpha;
	const bool depthWrite = normalMapped
		? (shader.Get_Depth_Mask() == ShaderClass::DEPTH_WRITE_ENABLE && !alphaBlend)
		: ((!alphaBlend || shaderBlend) && shader.Get_Depth_Mask() == ShaderClass::DEPTH_WRITE_ENABLE);
	const bool twoSided = model.Get_Flag(MeshGeometryClass::TWO_SIDED) ||
		shader.Get_Cull_Mode() == ShaderClass::CULL_MODE_DISABLE;

	if (useSorting)
	{
		DynamicVBAccessClass vb(BUFFER_TYPE_DYNAMIC_SORTING, dynamic_fvf_type, static_cast<unsigned short>(vertices.size()));
		{
			DynamicVBAccessClass::WriteLockClass lock(&vb);
			VertexFormatXYZNDUV2 *dst = lock.Get_Formatted_Vertex_Array();
			for (size_t i = 0; i < vertices.size(); ++i)
			{
				const auto &src = vertices[i];
				dst[i].x = src.position[0];
				dst[i].y = src.position[1];
				dst[i].z = src.position[2];
				dst[i].nx = src.normal[0];
				dst[i].ny = src.normal[1];
				dst[i].nz = src.normal[2];
				dst[i].diffuse = src.color;
				dst[i].u1 = src.uv[0];
				dst[i].v1 = src.uv[1];
				dst[i].u2 = src.uv1[0];
				dst[i].v2 = src.uv1[1];
			}
		}
		DynamicIBAccessClass ib(BUFFER_TYPE_DYNAMIC_SORTING, static_cast<unsigned short>(geometry.indices.size()));
		{
			DynamicIBAccessClass::WriteLockClass lock(&ib);
			unsigned short *dst = lock.Get_Index_Array();
			for (size_t i = 0; i < geometry.indices.size(); ++i)
				dst[i] = static_cast<unsigned short>(geometry.indices[i]);
		}

		ShaderClass effectiveShader = shader;
		if (!shaderBlend && alphaBlend)
		{
			effectiveShader.Set_Src_Blend_Func(ShaderClass::SRCBLEND_SRC_ALPHA);
			effectiveShader.Set_Dst_Blend_Func(ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA);
			effectiveShader.Set_Depth_Mask(ShaderClass::DEPTH_WRITE_DISABLE);
		}
		DX8Wrapper::Set_Shader(effectiveShader);
		DX8Wrapper::Set_Material(vertexMaterial);
		DX8Wrapper::Set_Texture(0, diffuse);
		DX8Wrapper::Set_Texture(1, secondary);
		DX8Wrapper::Set_Transform(D3DTS_WORLD, world);
		DX8Wrapper::Set_Vertex_Buffer(vb);
		DX8Wrapper::Set_Index_Buffer(ib, 0);

		SortedRetailEffectData effectData;
		effectData.twoSided = twoSided;
		effectData.alphaTest = alphaTest;
		effectData.alphaBlend = alphaBlend;
		effectData.depthWrite = depthWrite;
		effectData.alphaRef = alphaRef;
		effectData.srcBlend = static_cast<unsigned char>(srcBlend);
		effectData.dstBlend = static_cast<unsigned char>(dstBlend);
		effectData.vertexOpacity = vertexOpacity;
		effectData.overrideOpacity = opacity;
		effectData.hasPointLight = havePoint;
		for (size_t i = 0; i < fog.size(); ++i)
			writeSortedLight(effectData.fog[i], fog[i]);
		for (size_t i = 0; i < directional.size(); ++i)
			writeSortedLight(effectData.directional[i], directional[i]);
		for (size_t i = 0; i < point.size(); ++i)
			writeSortedLight(effectData.point[i], point[i]);
		writeSortedLight(effectData.ambientLight, ambientLight);
		std::snprintf(effectData.meshName, sizeof(effectData.meshName), "%s", mesh.Get_Name());

		SortingRendererClass::Insert_Triangles_With_Effect(
			mesh.Get_Bounding_Sphere(),
			0,
			static_cast<unsigned short>(geometry.indices.size() / 3),
			0,
			static_cast<unsigned short>(vertices.size()),
			effectData);

		DX8Wrapper::Set_Render_State(previous);
		return true;
	}

	return drawEffectPass(
		device,
		normalMapped,
		&material,
		meshPass,
		*materialId,
		mesh.Get_Name(),
		diffuse,
		normalTexture.get(),
		vertexMaterial,
		world,
		fog,
		directional,
		point,
		ambientLight,
		havePoint,
		vertexOpacity,
		opacity,
		alphaTest,
		alphaRef,
		alphaBlend,
		depthWrite,
		twoSided,
		srcBlend,
		dstBlend,
		vertices,
		geometry.indices,
		false);
}

}

void installRetailMeshDraw()
{
	DX8TextureCategoryClass::Set_Effect_Draw(drawRetailMesh);
	SortingRendererClass::Set_Effect_Draw(drawSortedRetailMesh);
}

void removeRetailMeshDraw()
{
	DX8TextureCategoryClass::Set_Effect_Draw(nullptr);
	SortingRendererClass::Set_Effect_Draw(nullptr);
	runtime.reset();
}

}
