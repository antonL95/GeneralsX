#include <gtest/gtest.h>

#include "GameClient/BillboardGroundClip.h"

#include <array>
#include <vector>

namespace
{
	using BillboardGroundClip::ClipVertex;
	using BillboardGroundClip::GroundClearanceBias;
	using BillboardGroundClip::Vec3;

	std::array<ClipVertex, 4> makeVerticalQuad(float centerZ, float size)
	{
		std::array<ClipVertex, 4> corners{};
		BillboardGroundClip::buildQuadCorners(
			Vec3{ 100.0f, 100.0f, centerZ },
			Vec3{ 1.0f, 0.0f, 0.0f },
			Vec3{ 0.0f, 0.0f, 1.0f },
			size,
			0,
			corners);
		return corners;
	}
}

TEST(BillboardGroundClipTest, CompletelyUndergroundEmitsNothing)
{
	std::array<ClipVertex, 4> corners = makeVerticalQuad(40.0f, 10.0f);
	const std::array<float, 4> ground = { 56.0f, 56.0f, 56.0f, 56.0f };
	std::vector<ClipVertex> verts;
	std::vector<uint16_t> indices;

	BillboardGroundClip::clipGroundBillboard(corners, ground, verts, indices);

	EXPECT_TRUE(verts.empty());
	EXPECT_TRUE(indices.empty());
}

TEST(BillboardGroundClipTest, HighAboveGroundRemainsSingleFullAlphaQuad)
{
	std::array<ClipVertex, 4> corners = makeVerticalQuad(120.0f, 40.0f);
	const std::array<float, 4> ground = { 56.0f, 56.0f, 56.0f, 56.0f };
	std::vector<ClipVertex> verts;
	std::vector<uint16_t> indices;

	BillboardGroundClip::clipGroundBillboard(corners, ground, verts, indices);

	ASSERT_EQ(verts.size(), 4u);
	ASSERT_EQ(indices.size(), 6u);
	for (const ClipVertex &v : verts)
	{
		EXPECT_FLOAT_EQ(v.alphaScale, 1.0f);
	}
}

TEST(BillboardGroundClipTest, TerrainIntersectingQuadClipsUndergroundAndRampsAlphaToZero)
{
	std::array<ClipVertex, 4> corners = makeVerticalQuad(60.0f, 75.0f);
	const std::array<float, 4> ground = { 56.0f, 56.0f, 56.0f, 56.0f };
	std::vector<ClipVertex> verts;
	std::vector<uint16_t> indices;

	BillboardGroundClip::clipGroundBillboard(corners, ground, verts, indices);

	ASSERT_FALSE(verts.empty());
	ASSERT_FALSE(indices.empty());

	bool foundZeroAtGround = false;
	bool foundFullAtTop = false;
	for (const ClipVertex &v : verts)
	{
		EXPECT_GE(v.z, 56.0f + GroundClearanceBias - 1.0e-4f);
		EXPECT_GE(v.u, -1.0e-5f);
		EXPECT_LE(v.u, 1.0f + 1.0e-5f);
		EXPECT_GE(v.v, -1.0e-5f);
		EXPECT_LE(v.v, 1.0f + 1.0e-5f);
		if (v.z <= 56.0f + GroundClearanceBias + 1.0e-4f)
		{
			EXPECT_NEAR(v.alphaScale, 0.0f, 1.0e-4f);
			foundZeroAtGround = true;
		}
		if (v.z >= 90.0f)
		{
			EXPECT_FLOAT_EQ(v.alphaScale, 1.0f);
			foundFullAtTop = true;
		}
	}
	EXPECT_TRUE(foundZeroAtGround);
	EXPECT_TRUE(foundFullAtTop);
}

TEST(BillboardGroundClipTest, RotatedQuadClipsCleanlyAcrossTwoBands)
{
	std::array<ClipVertex, 4> corners{};
	BillboardGroundClip::buildQuadCorners(
		Vec3{ 200.0f, 300.0f, 62.0f },
		Vec3{ 1.0f, 0.0f, 0.0f },
		Vec3{ 0.0f, 0.6f, 0.8f },
		70.0f,
		32,
		corners);
	const std::array<float, 4> ground = { 56.0f, 56.0f, 56.0f, 56.0f };
	std::vector<ClipVertex> verts;
	std::vector<uint16_t> indices;

	BillboardGroundClip::clipGroundBillboard(corners, ground, verts, indices);

	ASSERT_GT(verts.size(), 4u);
	ASSERT_EQ(indices.size() % 3u, 0u);
	for (uint16_t idx : indices)
	{
		EXPECT_LT(idx, verts.size());
	}
	for (const ClipVertex &v : verts)
	{
		EXPECT_GE(v.z, 56.0f + GroundClearanceBias - 1.0e-4f);
		EXPECT_GE(v.alphaScale, 0.0f);
		EXPECT_LE(v.alphaScale, 1.0f);
	}
}
