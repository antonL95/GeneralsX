#include "W3DDevice/GameClient/HeightMapSamples.h"

#include <array>
#include <bit>
#include <cstdint>
#include <gtest/gtest.h>

TEST(HeightMapSamples, LegacyV4ChunkPreservesEveryCell)
{
	std::array<std::uint8_t, 16 * 16> payload{};
	std::array<std::uint16_t, 16 * 16> samples{};
	for (std::size_t cell = 0; cell < payload.size(); ++cell)
		payload[cell] = static_cast<std::uint8_t>(cell);

	constexpr unsigned version = 4;
	const auto bytesPerSample = HeightMapSamples::bytesPerSample(version);
	ASSERT_EQ(bytesPerSample, 1u);
	HeightMapSamples::decode(payload, samples, bytesPerSample);

	std::array<std::uint8_t, payload.size()> encoded{};
	HeightMapSamples::encode(samples, encoded, bytesPerSample);
	EXPECT_EQ(encoded, payload);
	for (std::size_t cell = 0; cell < payload.size(); ++cell)
	{
		EXPECT_EQ(samples[cell], payload[cell]);
		const float legacy = payload[cell] * (10.0f / 16.0f);
		const float world = HeightMapSamples::toWorldHeight(samples[cell],
			HeightMapSamples::scale(bytesPerSample, 10.0f / 16.0f));
		EXPECT_EQ(std::bit_cast<std::uint32_t>(legacy), std::bit_cast<std::uint32_t>(world));
	}
}

TEST(HeightMapSamples, BfmeV5PreservesWideAndFractionalSamples)
{
	const std::array<std::uint8_t, 8> payload{0x00, 0x00, 0x01, 0x00, 0x00, 0xc8, 0xff, 0xff};
	std::array<std::uint16_t, 4> samples{};
	const auto bytesPerSample = HeightMapSamples::bytesPerSample(5);
	ASSERT_EQ(bytesPerSample, 2u);
	HeightMapSamples::decode(payload, samples, bytesPerSample);
	EXPECT_EQ(samples, (std::array<std::uint16_t, 4>{0, 1, 51200, 65535}));
	EXPECT_EQ(HeightMapSamples::toWorldHeight(samples[1], HeightMapSamples::scale(bytesPerSample, 0.625f)), 0.0390625f);
	EXPECT_EQ(HeightMapSamples::toWorldHeight(samples[2], HeightMapSamples::scale(bytesPerSample, 0.625f)), 2000.0f);

	std::array<std::uint8_t, payload.size()> encoded{};
	HeightMapSamples::encode(samples, encoded, bytesPerSample);
	EXPECT_EQ(encoded, payload);
}
