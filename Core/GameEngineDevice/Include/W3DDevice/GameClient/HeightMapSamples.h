#pragma once

#include <cstdint>
#include <span>

namespace HeightMapSamples
{
	inline constexpr unsigned WideVersion = 5;

	inline constexpr unsigned bytesPerSample(unsigned version)
	{
		return version >= WideVersion ? 2 : 1;
	}

	inline constexpr float scale(unsigned bytesPerSample, float legacyScale)
	{
		return bytesPerSample == 2 ? legacyScale / 16.0f : legacyScale;
	}

	inline constexpr float toWorldHeight(float sample, float scale)
	{
		return sample * scale;
	}

	inline void decode(std::span<const std::uint8_t> bytes, std::span<std::uint16_t> samples, unsigned width)
	{
		for (std::size_t i = 0; i < samples.size(); ++i)
		{
			samples[i] = bytes[i * width];
			if (width == 2)
				samples[i] |= static_cast<std::uint16_t>(bytes[i * width + 1]) << 8;
		}
	}

	inline void encode(std::span<const std::uint16_t> samples, std::span<std::uint8_t> bytes, unsigned width)
	{
		for (std::size_t i = 0; i < samples.size(); ++i)
		{
			bytes[i * width] = static_cast<std::uint8_t>(samples[i]);
			if (width == 2)
				bytes[i * width + 1] = static_cast<std::uint8_t>(samples[i] >> 8);
		}
	}
}
