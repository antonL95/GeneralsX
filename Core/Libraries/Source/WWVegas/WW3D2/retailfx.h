#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace RetailFX
{

enum class ValueClass : uint32_t { Scalar, Vector, MatrixRows, MatrixColumns, Object, Struct };
enum class ValueType : uint32_t
{
	Void, Bool, Int, Float, String, Texture, Texture1D, Texture2D, Texture3D, TextureCube,
	Sampler, Sampler1D, Sampler2D, Sampler3D, SamplerCube, PixelShader, VertexShader
};
enum class ResourceUsage : uint32_t { Program, Parameter, ArraySelector };

struct State;

struct Value
{
	std::string name;
	std::string semantic;
	ValueClass valueClass{};
	ValueType type{};
	uint32_t rows = 0;
	uint32_t columns = 0;
	uint32_t arraySize = 0;
	std::vector<Value> members;
	std::vector<Value> elements;
	std::vector<uint32_t> words;
	std::optional<uint32_t> object;
	std::vector<State> samplerStates;
};

struct State
{
	uint32_t operation;
	uint32_t index;
	Value value;
	std::optional<ResourceUsage> resourceUsage;
	std::string parameterReference;
	std::vector<uint8_t> resource;
};

struct Parameter
{
	Value value;
	uint32_t flags;
	std::vector<Value> annotations;
};

struct Pass
{
	std::string name;
	std::vector<Value> annotations;
	std::vector<State> states;
};

struct Technique
{
	std::string name;
	std::vector<Value> annotations;
	std::vector<Pass> passes;
};

struct Effect
{
	std::vector<Parameter> parameters;
	std::vector<Technique> techniques;
	std::vector<std::vector<uint8_t>> objects;
};

std::optional<Effect> parse(std::span<const uint8_t> bytes, std::string &error);

}
