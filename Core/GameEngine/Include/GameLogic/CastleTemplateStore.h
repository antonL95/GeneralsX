#pragma once

#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class AsciiString;

namespace CastleTemplates
{
struct Member
{
	std::string templateName;
	float offsetX;
	float offsetY;
	float offsetZ;
	float angle;
};

using Members = std::vector<Member>;

inline constexpr std::size_t MAX_BODY_SIZE = 16 * 1024 * 1024;

namespace Detail
{
class Reader
{
public:
	explicit Reader(std::span<const std::uint8_t> bytes) : m_remaining(bytes) {}

	std::size_t remaining() const { return m_remaining.size(); }

	void skip(std::size_t size)
	{
		if (size > remaining())
			throw InvalidChunk{};
		m_remaining = m_remaining.subspan(size);
	}

	std::uint32_t word()
	{
		const auto bytes = m_remaining;
		skip(4);
		return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8) |
			(std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
	}

	float real()
	{
		const float value = std::bit_cast<float>(word());
		if (!std::isfinite(value))
			throw InvalidChunk{};
		return value;
	}

	std::string_view text()
	{
		const auto bytes = m_remaining;
		skip(2);
		const std::size_t size = unsigned(bytes[0]) | (unsigned(bytes[1]) << 8);
		skip(size);
		return {reinterpret_cast<const char*>(bytes.data() + 2), size};
	}

	std::uint32_t count(std::size_t minimumBytes)
	{
		const auto value = word();
		if (value > remaining() / minimumBytes)
			throw InvalidChunk{};
		return value;
	}

	struct InvalidChunk {};

private:
	std::span<const std::uint8_t> m_remaining;
};
}

inline std::optional<Members> parse(std::span<const std::uint8_t> bytes, unsigned version)
{
	if (version < 1 || version > 5)
		return std::nullopt;

	try
	{
		Detail::Reader reader(bytes);
		reader.skip(4);
		const auto count = reader.count(version >= 4 ? 28 : 20);
		Members members;
		members.reserve(count);
		for (std::uint32_t i = 0; i < count; ++i)
		{
			reader.text();
			const auto name = reader.text();
			constexpr float maxOffset = 8192.0f * 10.0f;
			constexpr float maxAngle = 64.0f;
			if (name.empty() || name.find('\0') != std::string_view::npos)
				return std::nullopt;
			const Member member{std::string(name), reader.real(), reader.real(), reader.real(), reader.real()};
			if (std::abs(member.offsetX) > maxOffset || std::abs(member.offsetY) > maxOffset ||
				std::abs(member.offsetZ) > maxOffset || std::abs(member.angle) > maxAngle)
				return std::nullopt;
			members.push_back(member);
			if (version >= 4)
				reader.skip(8);
		}
		if (version >= 2)
		{
			const auto polygons = reader.count(version >= 5 ? 6 : 4);
			const std::size_t pointSize = version == 2 ? 12 : 8;
			for (std::uint32_t i = 0; i < polygons; ++i)
			{
				if (version >= 5)
					reader.text();
				const auto points = reader.count(pointSize);
				reader.skip(points * pointSize);
			}
		}
		if (reader.remaining() == 0)
			return members;
	}
	catch (const Detail::Reader::InvalidChunk&)
	{
		return std::nullopt;
	}
	return std::nullopt;
}

const Members* find(const AsciiString& camp);
void reset();
}
