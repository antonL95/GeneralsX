#include "w3dbfmechunks.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

TEST(W3dBfmeChunks, InfluenceUsesFourLittleEndianWords)
{
	const uint8_t bytes[] = {3, 0, 9, 1, 75, 0, 25, 0};
	const auto influence = W3dBfme::ReadInfluence(bytes);
	EXPECT_EQ(influence.Bone0, 3);
	EXPECT_EQ(influence.Bone1, 265);
	EXPECT_EQ(influence.Weight0, 75);
	EXPECT_EQ(influence.Weight1, 25);
}

TEST(W3dBfmeChunks, SecondStreamRequiresOneVectorPerVertex)
{
	const uint8_t bytes[] = {0, 0, 128, 63, 0, 0, 0, 192, 0, 0, 0, 63};
	float components[3] = {};
	const auto read = [&](unsigned index, float x, float y, float z) {
		EXPECT_EQ(index, 0u);
		components[0] = x;
		components[1] = y;
		components[2] = z;
	};
	EXPECT_FALSE(W3dBfme::ReadVectorStream(bytes, 2, read));
	ASSERT_TRUE(W3dBfme::ReadVectorStream(bytes, 1, read));
	EXPECT_FLOAT_EQ(components[0], 1.0f);
	EXPECT_FLOAT_EQ(components[1], -2.0f);
	EXPECT_FLOAT_EQ(components[2], 0.5f);
}

TEST(W3dBfmeChunks, MotionWidensFramesAndHonorsPadding)
{
	const uint8_t bytes[] = {
		0, 0, 1, 2, 3, 0, 7, 0,
		0, 0, 8, 0, 2, 129, 255, 255,
		0, 0, 128, 63, 0, 0, 0, 64, 0, 0, 64, 64
	};
	const float filters[256] = {};
	W3dBfme::Motion motion;
	ASSERT_EQ(W3dBfme::DecodeMotion(bytes, 259, filters, motion), W3dBfme::MotionResult::Decoded);
	EXPECT_EQ(motion.Pivot, 7);
	EXPECT_EQ(motion.Type, 2);
	EXPECT_EQ(motion.Packets, (std::vector<uint32_t>{0, 0x3f800000, 8, 0x40000000, 0x80000102, 0x40400000}));
	EXPECT_EQ(W3dBfme::DecodeMotion(std::span(bytes).first(sizeof(bytes) - 1), 259, filters, motion),
		W3dBfme::MotionResult::Invalid);
	EXPECT_EQ(W3dBfme::DecodeMotion(bytes, 258, filters, motion), W3dBfme::MotionResult::Invalid);
}

TEST(W3dBfmeChunks, MotionHoldsDelayedFirstKeyFromFrameZero)
{
	const uint8_t bytes[] = {0, 0, 1, 1, 1, 0, 2, 0, 3, 0, 0, 0, 0, 0, 128, 63};
	const float filters[256] = {};
	W3dBfme::Motion motion;
	ASSERT_EQ(W3dBfme::DecodeMotion(bytes, 4, filters, motion), W3dBfme::MotionResult::Decoded);
	EXPECT_EQ(motion.Packets, (std::vector<uint32_t>{0, 0x3f800000, 0x80000003, 0x3f800000}));
}

TEST(W3dBfmeChunks, UnknownScalarIsSkipped)
{
	const uint8_t bytes[] = {0, 0, 1, 15, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 63};
	const float filters[256] = {};
	W3dBfme::Motion motion;
	EXPECT_EQ(W3dBfme::DecodeMotion(bytes, 1, filters, motion), W3dBfme::MotionResult::UnknownScalar);
	EXPECT_TRUE(motion.Packets.empty());
}

TEST(W3dBfmeChunks, BoromirIdleMatchesInventoryVectors)
{
	const char* directory = std::getenv("GENERALSX_MOTION_FIXTURES");
	if (!directory)
		GTEST_SKIP() << "Set GENERALSX_MOTION_FIXTURES to the generated fixture directory";

	std::ifstream payloads(std::string(directory) + "/guboromir_idlb.hex");
	std::ifstream vectors(std::string(directory) + "/guboromir_idlb.tsv");
	ASSERT_TRUE(payloads);
	ASSERT_TRUE(vectors);

	float filters[256] = {};
	for (int index = 0; index < 16; ++index)
		filters[index] = std::pow(10.0f, index - 8);
	for (int index = 16; index < 256; ++index)
		filters[index] = 1.0f - std::sin((90.0f * ((index - 16) / 240.0f)) * (3.14159265358979323846f / 180.0f));

	std::vector<W3dBfme::Motion> channels(76);
	unsigned channel = 0;
	std::string hex;
	unsigned channelCount = 0;
	while (payloads >> channel >> hex) {
		ASSERT_LT(channel, channels.size());
		ASSERT_EQ(hex.size() % 2, 0u);
		std::vector<uint8_t> bytes;
		for (size_t offset = 0; offset < hex.size(); offset += 2)
			bytes.push_back(static_cast<uint8_t>(std::stoul(hex.substr(offset, 2), nullptr, 16)));
		ASSERT_EQ(W3dBfme::DecodeMotion(bytes, 294, filters, channels[channel]), W3dBfme::MotionResult::Decoded);
		++channelCount;
	}

	std::string line;
	std::getline(vectors, line);
	unsigned checked = 0;
	while (std::getline(vectors, line)) {
		std::istringstream row(line);
		std::string name, hierarchy, type, delta;
		unsigned pivot, frame;
		ASSERT_TRUE(row >> hierarchy >> name >> channel >> pivot >> type >> delta >> frame);
		ASSERT_LT(channel, channels.size());
		const auto& motion = channels[channel];
		EXPECT_EQ(motion.Pivot, pivot);
		const size_t start = frame * (motion.VectorLength + 1);
		ASSERT_LT(start + motion.VectorLength, motion.Packets.size());
		EXPECT_EQ(motion.Packets[start], frame);
		for (unsigned component = 1; component <= motion.VectorLength; ++component) {
			float expected;
			ASSERT_TRUE(row >> expected);
			EXPECT_NEAR(std::bit_cast<float>(motion.Packets[start + component]), expected,
				0.00004f + std::abs(expected) * 0.000006f) << channel << ' ' << frame << ' ' << component;
		}
		++checked;
	}
	EXPECT_EQ(channelCount, 75u);
	EXPECT_EQ(checked, 22050u);
}
