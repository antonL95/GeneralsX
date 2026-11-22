#include "W3DDevice/GameClient/RetailEffectInput.h"

#include "Common/RetailFileLayers.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/TerrainTex.h"
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "W3DDevice/GameClient/W3DShroud.h"
#include "WW3D2/assetmgr.h"
#include "WW3D2/dx8renderer.h"
#include "WW3D2/lightenvironment.h"
#include "WW3D2/mesh.h"
#include "WW3D2/meshmdl.h"
#include "WW3D2/vertmaterial.h"
#include "WW3D2/w3dfxmaterial.h"

#include <cstdlib>
#include <memory>
#include <set>

namespace RetailFX
{
namespace
{

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
		initialized.fill(false);
	}
	void ReAcquireResources() override {}

	std::array<std::unique_ptr<NativePass>, 2> passes;
	std::vector<NormalMappedVertex> vertices;
	std::vector<uint32_t> indices;
	std::set<std::string, std::less<>> traced;
	std::array<bool, 2> initialized{};
	std::array<bool, 2> reportedFailure{};
};

std::unique_ptr<NormalMappedRuntime> runtime;

bool fillMesh(MeshModelClass &model, const W3dFx::Mesh *fx, NormalMappedRuntime &renderer)
{
	const Int vertexCount = model.Get_Vertex_Count();
	const Int polygonCount = model.Get_Polygon_Count();
	if (vertexCount <= 0 || polygonCount <= 0 || (fx &&
		(fx->tangents.size() != static_cast<size_t>(vertexCount) || fx->bitangents.size() != static_cast<size_t>(vertexCount))))
		return false;
	const auto *uv = model.Get_UV_Array(0, 0);
	const auto *position = model.Get_Vertex_Array();
	const auto *normal = model.Get_Vertex_Normal_Array();
	const auto *color = model.Get_Color_Array(0, false);
	const auto *polys = model.MeshGeometryClass::Get_Polygon_Array();
	if (!uv || !position || !normal || !polys)
		return false;
	renderer.vertices.clear();
	for (Int i = 0; i < vertexCount; ++i)
	{
		const auto t = fx ? fx->tangents[i] : W3dFx::Float3{};
		const auto b = fx ? fx->bitangents[i] : W3dFx::Float3{};
		renderer.vertices.push_back({
			{position[i].X, position[i].Y, position[i].Z}, {normal[i].X, normal[i].Y, normal[i].Z},
			{t[0], t[1], t[2]}, {b[0], b[1], b[2]}, {uv[i].U, uv[i].V},
			{uv[i].U, uv[i].V}, color ? static_cast<uint32_t>(color[i]) : 0xffffffffu
		});
	}
	renderer.indices.clear();
	for (Int i = 0; i < polygonCount; ++i)
	{
		for (Int corner = 0; corner < 3; ++corner)
		{
			const auto index = static_cast<uint32_t>(polys[i][corner]);
			if (index >= static_cast<uint32_t>(vertexCount))
				return false;
			renderer.indices.push_back(index);
		}
	}
	return true;
}

Register fromVector(const Vector3 &value)
{
	return {value.X, value.Y, value.Z, 0.0f};
}

bool drawRetailMesh(MeshClass &mesh, TextureClass *diffuse, TextureClass *secondary,
	VertexMaterialClass *vertexMaterial, const ShaderClass &shader, const Matrix4x4 &world)
{
	if (!RetailFileLayers::enabled() || !TheTerrainRenderObject || !diffuse ||
		ShaderClass::Is_Backface_Culling_Inverted())
		return false;
	auto &model = *mesh.Peek_Model();
	const auto *fx = model.Peek_Fx_Materials();
	const bool normalMapped = fx && !fx->materials.empty();
	const auto *lights = mesh.Get_Lighting_Environment();
	auto *shroud = TheTerrainRenderObject->getShroud();
	if (model.Get_Pass_Count() != 1 || model.Get_Flag(MeshGeometryClass::SKIN) || !lights ||
		!shroud || shroud->getTextureWidth() <= 0 || shroud->getTextureHeight() <= 0 ||
		shroud->getCellWidth() <= 0.0f || shroud->getCellHeight() <= 0.0f)
		return false;
	if (normalMapped && (fx->materials.size() != 1 || fx->materials[0].effect != "normalmapped.fx"))
		return false;
	if (!normalMapped && (secondary || !vertexMaterial || vertexMaterial->Peek_Mapper() ||
		vertexMaterial->Get_Opacity() != 1.0f || shader.Get_Depth_Compare() != ShaderClass::PASS_LEQUAL ||
		shader.Get_Depth_Mask() != ShaderClass::DEPTH_WRITE_ENABLE ||
		shader.Get_Src_Blend_Func() != ShaderClass::SRCBLEND_ONE ||
		shader.Get_Dst_Blend_Func() != ShaderClass::DSTBLEND_ZERO))
		return false;
	static const W3dFx::Material noFx{};
	const auto &material = normalMapped ? fx->materials[0] : noFx;
	const auto normalName = material.get<std::string>("NormalMap", {});
	const auto diffuseColor = material.get<W3dFx::Float4>("DiffuseColor", {1.0f, 1.0f, 1.0f, 1.0f});
	if (normalMapped && (normalName.empty() || normalName == "None" || diffuseColor[3] != 1.0f ||
		material.get<bool>("AlphaBlendEnable", false)))
		return false;

	auto *device = DX8Wrapper::Get_Effect_Device9();
	if (!device)
		return false;
	if (!runtime)
		runtime = std::make_unique<NormalMappedRuntime>();
	const auto slot = normalMapped ? 0u : 1u;
	const char *label = normalMapped ? "NormalMapped Default_M" : "DefaultW3D Default";
	auto &pass = runtime->passes[slot];
	if (!runtime->initialized[slot])
	{
		runtime->initialized[slot] = true;
		if (const auto effect = loadEffectFile(normalMapped ? "shaders/compiled/normalmapped.fxo" : "shaders/compiled/defaultw3d.fxo"))
			pass = normalMapped ? NativePass::makeNormalMapped(device, *effect, DX8Wrapper::Create_Effect_Shader9)
				: NativePass::makeDefaultMesh(device, *effect, DX8Wrapper::Create_Effect_Shader9);
		if (!pass)
			std::fprintf(stderr, "[RetailFX] Could not load %s\n", label);
	}
	if (!pass)
		return false;

	const std::unique_ptr<TextureClass, TextureRelease> normalTexture(
		normalMapped ? WW3DAssetManager::Get_Instance()->Get_Texture(normalName.c_str()) : nullptr);
	auto *normal = nativeTexture(normalTexture.get());
	auto *base = nativeTexture(diffuse);
	auto *mask = nativeTexture(shroud->getShroudTexture());
	const auto cloud = normalMapped ? nullptr : nativeTexture(TheTerrainRenderObject->peekCloudTexture());
	if ((normalMapped && !normal) || !base || !mask || !fillMesh(model, normalMapped ? fx : nullptr, *runtime))
		return false;

	const float scaleU = 1.0f / (shroud->getCellWidth() * static_cast<float>(shroud->getTextureWidth()));
	const float scaleV = 1.0f / (shroud->getCellHeight() * static_cast<float>(shroud->getTextureHeight()));
	const Register shroudUV{scaleU, scaleV,
		(shroud->getCellWidth() - shroud->getDrawOriginX()) * scaleU,
		(shroud->getCellHeight() - shroud->getDrawOriginY()) * scaleV};

	auto *device8 = DX8Wrapper::_Get_D3D_Device8();
	D3DXMATRIX view, projection, inverseView;
	if (FAILED(device8->GetTransform(D3DTS_VIEW, &view)) ||
		FAILED(device8->GetTransform(D3DTS_PROJECTION, &projection)) ||
		!D3DXMatrixInverse(&inverseView, nullptr, &view))
		return false;
	const auto fog = fogBindings(device8);
	Register ambient{}, colorDiffuse{}, specular{}, emissive{};
	if (!normalMapped)
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
	std::array<Register, 6> directional{};
	std::array<Register, 3> point{{{}, {}, {1.0f, 0.0f, 0.0f, 0.0f}}};
	unsigned numDirectional = 0;
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
			directional[numDirectional * 2] = fromVector(lights->Get_Light_Diffuse(i));
			directional[numDirectional * 2 + 1] = fromVector(lights->Get_Light_Direction(i));
			++numDirectional;
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
		else if (name == "Fog")
			std::copy_n(fog.begin(), std::min(fog.size(), floats.size()), floats.begin());
		else if (name == "DirectionalLight")
			std::copy_n(directional.begin(), std::min(directional.size(), floats.size()), floats.begin());
		else if (name == "PointLight")
			std::copy_n(point.begin(), std::min(point.size(), floats.size()), floats.begin());
		else if (name == "AmbientLightColor")
			floats[0] = fromVector(lights->Get_Equivalent_Ambient());
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
		else if (name == "Shininess" && !normalMapped)
			floats[0][0] = vertexMaterial->Get_Shininess();
		else if (name == "Opacity" || name == "OpacityOverride")
			floats[0][0] = mesh.Get_Alpha_Override();
		else if (name == "HouseColorEnable")
			floats[0][0] = 0.0f;
		else if (name == "Cloud")
			floats[0] = W3DShaderManager::getRetailCloudUV();
		else if (const auto property = material.properties.find(name); property != material.properties.end())
		{
			if (const auto *scalar = std::get_if<float>(&property->second))
				floats[0][0] = *scalar;
			else if (const auto *v = std::get_if<W3dFx::Float4>(&property->second))
				floats[0] = *v;
			else if (const auto *v = std::get_if<W3dFx::Float3>(&property->second))
				std::copy(v->begin(), v->end(), floats[0].begin());
		}
	};
	const auto getTexture = [&](std::string_view name) -> void * {
		if (name == "NormalMap") return normal;
		if (name == "DiffuseTexture" || name == "Texture_0") return base;
		if (name == "ShroudTexture") return mask;
		if (name == "CloudTexture") return cloud;
		return nullptr;
	};
	const bool alphaTest = normalMapped ? material.get<bool>("AlphaTestEnable", false) :
		shader.Get_Alpha_Test() != ShaderClass::ALPHATEST_DISABLE;
	const bool drawn = pass->drawNormalMapped(device, runtime->vertices, runtime->indices, setBinding, getTexture,
		alphaTest, shader.Get_Depth_Mask() == ShaderClass::DEPTH_WRITE_ENABLE,
		model.Get_Flag(MeshGeometryClass::TWO_SIDED) || shader.Get_Cull_Mode() == ShaderClass::CULL_MODE_DISABLE);
	if (drawn && std::getenv("GENERALSX_TRACE_RETAIL_EFFECTS") && runtime->traced.insert(mesh.Get_Name()).second)
		std::fprintf(stderr, "[RetailFX] Drew %s (%s)\n", label, mesh.Get_Name());
	if (!drawn && !runtime->reportedFailure[slot])
	{
		std::fprintf(stderr, "[RetailFX] Could not bind or draw %s\n", label);
		runtime->reportedFailure[slot] = true;
	}
	return drawn;
}

}

void installRetailMeshDraw()
{
	DX8TextureCategoryClass::Set_Effect_Draw(drawRetailMesh);
}

void removeRetailMeshDraw()
{
	DX8TextureCategoryClass::Set_Effect_Draw(nullptr);
	runtime.reset();
}

}
