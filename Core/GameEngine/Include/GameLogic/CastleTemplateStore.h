#pragma once

#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
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
	std::int32_t priority = 0;
	std::int32_t phase = 0;
};

using Members = std::vector<Member>;

using Perimeter = std::vector<std::pair<float, float>>;

struct Camp
{
	Members members;
	std::vector<Perimeter> perimeters;
};

inline constexpr std::size_t MAX_BODY_SIZE = 16 * 1024 * 1024;
inline constexpr float MAX_OFFSET = 8192.0f * 10.0f;

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

inline std::optional<Camp> parse(std::span<const std::uint8_t> bytes, unsigned version)
{
	if (version < 1 || version > 5)
		return std::nullopt;

	try
	{
		Detail::Reader reader(bytes);
		reader.skip(4);
		const auto count = reader.count(version >= 4 ? 28 : 20);
		Camp camp;
		camp.members.reserve(count);
		for (std::uint32_t i = 0; i < count; ++i)
		{
			reader.text();
			const auto name = reader.text();
			constexpr float maxAngle = 64.0f;
			if (name.find('\0') != std::string_view::npos)
				return std::nullopt;
			Member member{std::string(name), reader.real(), reader.real(), reader.real(), reader.real()};
			if (std::abs(member.offsetX) > MAX_OFFSET || std::abs(member.offsetY) > MAX_OFFSET ||
				std::abs(member.offsetZ) > MAX_OFFSET || std::abs(member.angle) > maxAngle)
				return std::nullopt;
			if (version >= 4)
			{
				member.priority = std::bit_cast<std::int32_t>(reader.word());
				member.phase = std::bit_cast<std::int32_t>(reader.word());
			}
			camp.members.push_back(std::move(member));
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
				auto& perimeter = camp.perimeters.emplace_back();
				perimeter.reserve(points);
				for (std::uint32_t j = 0; j < points; ++j)
				{
					float x, y;
					if (version == 2)
					{
						x = static_cast<float>(std::bit_cast<std::int32_t>(reader.word()));
						y = static_cast<float>(std::bit_cast<std::int32_t>(reader.word()));
						reader.word();
					}
					else
					{
						x = reader.real();
						y = reader.real();
					}
					if (std::abs(x) > MAX_OFFSET || std::abs(y) > MAX_OFFSET)
						return std::nullopt;
					perimeter.emplace_back(x, y);
				}
			}
		}
		if (reader.remaining() == 0)
			return camp;
	}
	catch (const Detail::Reader::InvalidChunk&)
	{
		return std::nullopt;
	}
	return std::nullopt;
}

const Camp* find(const AsciiString& camp, bool required = true);
void reset();
}
