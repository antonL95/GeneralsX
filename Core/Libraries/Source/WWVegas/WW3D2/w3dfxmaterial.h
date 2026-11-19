#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace W3dFx
{
using Bytes = std::vector<std::uint8_t>;
using Float2 = std::array<float, 2>;
using Float3 = std::array<float, 3>;
using Float4 = std::array<float, 4>;
using Property = std::variant<std::string, float, Float2, Float3, Float4, std::int32_t, bool>;

struct Material
{
	std::string effect;
	std::uint8_t technique = 0;
	std::map<std::string, Property, std::less<>> properties;

	template<class T>
	T get(std::string_view name, T fallback) const
	{
		auto property = properties.find(name);
		if (property != properties.end())
		{
			if (const auto* value = std::get_if<T>(&property->second))
				return *value;
		}
		return fallback;
	}
};

struct Pass
{
	std::vector<std::uint32_t> ids;
	std::vector<Float2> uv;
	std::vector<int> vertexMaterials;
};

struct Mesh
{
	std::vector<Material> materials;
	std::vector<Pass> passes;
	std::vector<Float3> tangents;
	std::vector<Float3> bitangents;
};

inline std::uint32_t readWord(std::span<const std::uint8_t> bytes)
{
	return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8) |
		(std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
}

inline bool readString(std::span<const std::uint8_t>& bytes, std::string& result)
{
	if (bytes.size() < 4)
		return false;

	const auto length = readWord(bytes);
	bytes = bytes.subspan(4);
	if (length == 0 || length > bytes.size() || bytes[length - 1] != 0)
		return false;

	const auto value = bytes.first(length - 1);
	if (std::find(value.begin(), value.end(), 0) != value.end())
		return false;

	result.assign(value.begin(), value.end());
	bytes = bytes.subspan(length);
	return true;
}

template<std::size_t N>
std::array<float, N> readVector(std::span<const std::uint8_t> bytes)
{
	std::array<float, N> value;
	for (std::size_t i = 0; i < N; ++i)
		value[i] = std::bit_cast<float>(readWord(bytes.subspan(i * 4)));
	return value;
}

inline bool readHeader(std::span<const std::uint8_t> bytes, Material& material)
{
	if (bytes.size() != 37 || bytes[0] != 1)
		return false;

	const auto name = bytes.subspan(1, 32);
	const auto end = std::find(name.begin(), name.end(), 0);
	if (end == name.begin() || end == name.end())
		return false;

	material.effect.assign(name.begin(), end);
	for (char& c : material.effect)
	{
		if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
	}
	material.technique = bytes[33];
	return true;
}

inline bool readProperty(std::span<const std::uint8_t> bytes, Material& material)
{
	if (bytes.size() < 4)
		return false;

	const auto type = readWord(bytes);
	bytes = bytes.subspan(4);

	std::string name;
	if (!readString(bytes, name) || name.empty())
		return false;

	Property property;
	if (type == 1)
	{
		std::string texture;
		if (!readString(bytes, texture) || !bytes.empty())
			return false;
		property = std::move(texture);
	}
	else if (type >= 2 && type <= 5 && bytes.size() == (type - 1) * 4)
	{
		switch (type)
		{
			case 2: property = readVector<1>(bytes)[0]; break;
			case 3: property = readVector<2>(bytes); break;
			case 4: property = readVector<3>(bytes); break;
			case 5: property = readVector<4>(bytes); break;
		}
	}
	else if (type == 6 && bytes.size() == 4)
	{
		property = std::bit_cast<std::int32_t>(readWord(bytes));
	}
	else if (type == 7 && bytes.size() == 1)
	{
		property = bytes[0] != 0;
	}
	else
	{
		return false;
	}

	return material.properties.emplace(std::move(name), std::move(property)).second;
}

inline bool readIds(std::span<const std::uint8_t> bytes, std::size_t triangleCount, std::vector<std::uint32_t>& ids)
{
	if (bytes.empty() || bytes.size() % 4 != 0 || (bytes.size() != 4 && bytes.size() / 4 != triangleCount))
		return false;

	ids.resize(bytes.size() / 4);
	for (std::size_t i = 0; i < ids.size(); ++i)
		ids[i] = readWord(bytes.subspan(i * 4));
	return true;
}

template<std::size_t N>
bool readVectors(std::span<const std::uint8_t> bytes, std::size_t vertexCount, std::vector<std::array<float, N>>& vectors)
{
	constexpr std::size_t stride = N * 4;
	if (bytes.size() % stride != 0 || bytes.size() / stride != vertexCount)
		return false;

	vectors.resize(vertexCount);
	for (std::size_t i = 0; i < vertexCount; ++i)
		vectors[i] = readVector<N>(bytes.subspan(i * stride));
	return true;
}
}
