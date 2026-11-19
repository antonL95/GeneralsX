#pragma once

#include "w3dfxmaterial.h"
#include "WWLib/chunkio.h"
#include "w3d_file.h"
#include "Common/INIAudit.h"
#include "assetmgr.h"
#include "meshmatdesc.h"
#include "texture.h"

namespace W3dFx
{
inline bool readBytes(ChunkLoadClass& chunk, std::size_t length, Bytes& bytes)
{
	if (length != chunk.Cur_Chunk_Length())
		return false;

	bytes.resize(length);
	return chunk.Read(bytes.data(), bytes.size()) == length;
}

inline bool readMaterials(ChunkLoadClass& chunk, Mesh& mesh)
{
	bool valid = true;
	while (chunk.Open_Chunk_Bounded()) {
		Material material;
		bool validMaterial = chunk.Cur_Chunk_ID() == W3D_CHUNK_FX_SHADER;
		if (validMaterial) {
			while (chunk.Open_Chunk_Bounded()) {
				Bytes bytes;
				const auto length = chunk.Cur_Chunk_Length();
				bool validChild = false;
				switch (chunk.Cur_Chunk_ID()) {
					case W3D_CHUNK_FX_SHADER_INFO:
						validChild = material.effect.empty() && readBytes(chunk, 37, bytes) && readHeader(bytes, material);
						break;
					case W3D_CHUNK_FX_SHADER_CONSTANT:
						validChild = length <= 65536 && readBytes(chunk, length, bytes) && readProperty(bytes, material);
						break;
				}
				validMaterial &= validChild;
				chunk.Close_Chunk();
			}
			validMaterial &= chunk.Cur_Chunk_Remaining() == 0 && !material.effect.empty();
		}
		chunk.Close_Chunk();
		valid &= validMaterial;
		if (!validMaterial)
			material = {};
		mesh.materials.push_back(std::move(material));
	}
	return valid && chunk.Cur_Chunk_Remaining() == 0 && !mesh.materials.empty();
}

inline bool readPassChunk(ChunkLoadClass& chunk, Pass& pass, std::size_t vertices, std::size_t triangles)
{
	Bytes bytes;
	const auto length = chunk.Cur_Chunk_Length();
	if (chunk.Cur_Chunk_ID() == W3D_CHUNK_FXSHADER_IDS) {
		if (length != 4 && length != triangles * 4)
			return false;
		return pass.ids.empty() && readBytes(chunk, length, bytes) && readIds(bytes, triangles, pass.ids);
	}
	return pass.uv.empty() && readBytes(chunk, vertices * 8, bytes) && readVectors(bytes, vertices, pass.uv);
}

inline bool readTangentChunk(ChunkLoadClass& chunk, std::size_t vertices, std::vector<Float3>& vectors)
{
	Bytes bytes;
	return vectors.empty() && readBytes(chunk, vertices * 12, bytes) && readVectors(bytes, vertices, vectors);
}

inline bool loadProblem(const char* meshName, std::uint32_t chunkId)
{
	if (!INIAudit::isEnabled())
		return false;

	INIAudit::record("w3d-fx", "Invalid shader data in chunk " + std::to_string(chunkId), meshName, 0);
	return true;
}

struct FixedMaterial
{
	ShaderClass shader;
	Float4 ambient{0.4f, 0.4f, 0.4f, 0};
	Float4 diffuse{1, 1, 1, 1};
	Float4 specular{};
	Float4 emissive{};
	float exponent = 0;
	std::string texture;
	bool unlit = false;
	bool flipV = true;
};

inline FixedMaterial fixedMaterial(const Material& fx, const char* meshName, bool autoFog)
{
	FixedMaterial fixed;
	if (fx.effect == "normalmapped.fx") {
		fixed.texture = fx.get<std::string>("DiffuseTexture", "");
		fixed.ambient = fx.get("AmbientColor", fixed.ambient);
		fixed.diffuse = fx.get("DiffuseColor", fixed.diffuse);
		fixed.specular = fx.get("SpecularColor", fixed.specular);
		fixed.exponent = fx.get("SpecularExponent", 0.0f);
		fixed.shader.Set_Alpha_Test(fx.get("AlphaTestEnable", false) ? ShaderClass::ALPHATEST_ENABLE : ShaderClass::ALPHATEST_DISABLE);
		fixed.shader.Set_Secondary_Gradient(ShaderClass::SECONDARY_GRADIENT_ENABLE);
	} else if (fx.effect == "simple.fx") {
		fixed.texture = fx.get<std::string>("Texture_0", "");
		fixed.emissive = fx.get("ColorEmissive", Float4{1, 1, 1, 0});
		fixed.unlit = true;
		fixed.flipV = false;
		fixed.shader.Set_Primary_Gradient(ShaderClass::GRADIENT_DISABLE);
		if (fx.get("AlphaBlendingEnable", false)) {
			fixed.shader.Set_Src_Blend_Func(ShaderClass::SRCBLEND_SRC_ALPHA);
			fixed.shader.Set_Dst_Blend_Func(ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA);
		}
		fixed.shader.Set_Depth_Mask(fx.get("DepthWriteEnable", true) ? ShaderClass::DEPTH_WRITE_ENABLE : ShaderClass::DEPTH_WRITE_DISABLE);
		fixed.shader.Set_Fog_Func(fx.get("FogEnable", false) ? ShaderClass::FOG_ENABLE : ShaderClass::FOG_DISABLE);
	} else if (fx.effect == "default.fx") {
		fixed.ambient = fx.get("k_a", Float4{0.47f, 0.47f, 0.47f, 1});
		fixed.diffuse = fx.get("k_d", Float4{0.47f, 0.47f, 0.47f, 1});
		fixed.specular = fx.get("k_s", Float4{1, 1, 1, 1});
		fixed.exponent = static_cast<float>(fx.get<std::int32_t>("n", 15));
		fixed.shader.Set_Secondary_Gradient(ShaderClass::SECONDARY_GRADIENT_ENABLE);
	} else {
		fixed.diffuse = fx.get("MaterialColorDiffuse", Float4{0.2f, 0.35f, 0.45f, 1});
	}

	if (!fixed.texture.empty() && stricmp(fixed.texture.c_str(), "None") != 0)
		fixed.shader.Set_Texturing(ShaderClass::TEXTURING_ENABLE);
	if (autoFog && fx.effect != "simple.fx")
		fixed.shader.Enable_Fog(meshName);
	return fixed;
}

inline void configureVertexMaterial(const FixedMaterial& fixed, VertexMaterialClass& material)
{
	material.Set_Ambient(fixed.ambient[0], fixed.ambient[1], fixed.ambient[2]);
	material.Set_Diffuse(fixed.diffuse[0], fixed.diffuse[1], fixed.diffuse[2]);
	material.Set_Specular(fixed.specular[0], fixed.specular[1], fixed.specular[2]);
	material.Set_Emissive(fixed.emissive[0], fixed.emissive[1], fixed.emissive[2]);
	material.Set_Shininess(fixed.exponent);
	material.Set_Opacity(fixed.diffuse[3]);
}
}

bool MeshModelClass::read_fx_pass_chunk(ChunkLoadClass & cload, MeshLoadContextClass * context)
{
	if (context->CurPass >= MeshMatDescClass::MAX_PASSES)
		return false;

	auto & passes = context->Get_Fx().passes;
	if (passes.size() <= static_cast<std::size_t>(context->CurPass))
		passes.resize(context->CurPass + 1);
	return W3dFx::readPassChunk(cload, passes[context->CurPass], VertexCount, PolyCount);
}

bool MeshModelClass::install_fx_materials(MeshLoadContextClass * context)
{
	if (!context->Fx)
		return true;

	auto & fx = *context->Fx;
	const bool autoFog = WW3DAssetManager::Get_Instance()->Get_Activate_Fog_On_Load();
	for (std::size_t pass = 0; pass < fx.passes.size(); ++pass) {
		auto & data = fx.passes[pass];
		if (data.ids.empty()) {
			if (!data.uv.empty() && !W3dFx::loadProblem(Get_Name(), W3D_CHUNK_STAGE_TEXCOORDS))
				return false;
			continue;
		}

		const auto invalidId = [&fx](std::uint32_t id) {
			return id >= fx.materials.size() || fx.materials[id].effect.empty();
		};
		if (std::any_of(data.ids.begin(), data.ids.end(), invalidId)) {
			if (!W3dFx::loadProblem(Get_Name(), W3D_CHUNK_FXSHADER_IDS))
				return false;
			continue;
		}

		if (static_cast<int>(pass) >= Get_Pass_Count())
			Set_Pass_Count(pass + 1);

		const auto first = W3dFx::fixedMaterial(fx.materials[data.ids.front()], Get_Name(), autoFog);
		if (!data.uv.empty()) {
			auto * uv = context->Get_Temporary_UV_Array(VertexCount);
			for (int i = 0; i < VertexCount; ++i)
				uv[i].Set(data.uv[i][0], first.flipV ? 1.0f - data.uv[i][1] : data.uv[i][1]);
			DefMatDesc->Install_UV_Array(pass, 0, uv, VertexCount);
		}

		struct Binding
		{
			ShaderClass shader;
			TextureClass * texture = nullptr;
		};
		std::vector<Binding> bindings(fx.materials.size());
		data.vertexMaterials.assign(fx.materials.size(), -1);
		for (const auto id : data.ids) {
			if (data.vertexMaterials[id] >= 0)
				continue;

			auto fixed = id == data.ids.front() ? first : W3dFx::fixedMaterial(fx.materials[id], Get_Name(), autoFog);
			auto * material = NEW_REF(VertexMaterialClass, ());
			W3dFx::configureVertexMaterial(fixed, *material);
			data.vertexMaterials[id] = context->Add_Vertex_Material(material);
			context->FxVertexMaterials.emplace_back(material, pass, fixed.unlit);
			material->Release_Ref();

			auto & binding = bindings[id];
			binding.shader = fixed.shader;
			if (fixed.shader.Get_Texturing() == ShaderClass::TEXTURING_ENABLE) {
				binding.texture = WW3DAssetManager::Get_Instance()->Get_Texture(fixed.texture.c_str());
				if (binding.texture) {
					context->Add_Texture(binding.texture);
					binding.texture->Release_Ref();
				}
			}
			if (pass == 0 && SortLevel == SORT_LEVEL_NONE &&
					binding.shader.Get_Dst_Blend_Func() != ShaderClass::DSTBLEND_ZERO)
				Set_Flag(SORT, true);
		}

		const auto firstId = data.ids.front();
		DefMatDesc->Set_Single_Material(context->Peek_Vertex_Material(data.vertexMaterials[firstId]), pass);
		if (data.ids.size() == 1) {
			DefMatDesc->Set_Single_Shader(bindings[firstId].shader, pass);
			DefMatDesc->Set_Single_Texture(bindings[firstId].texture, pass, 0);
		} else {
			for (int triangle = 0; triangle < PolyCount; ++triangle) {
				const auto & binding = bindings[data.ids[triangle]];
				DefMatDesc->Set_Shader(triangle, binding.shader, pass);
				DefMatDesc->Set_Texture(triangle, binding.texture, pass, 0);
			}
		}
	}

	FxMaterials = context->Fx;
	return true;
}

VertexMaterialClass * MeshModelClass::Peek_Polygon_Material(unsigned polygon, unsigned pass)
{
	if (pass < FxMaterials->passes.size()) {
		const auto & data = FxMaterials->passes[pass];
		if (data.ids.size() > 1 && !data.vertexMaterials.empty())
			return MatInfo->Peek_Vertex_Material(data.vertexMaterials[data.ids[polygon]]);
	}
	return Peek_Material(Get_Polygon_Array()[polygon][0], pass);
}
