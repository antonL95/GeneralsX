#include "w3dfxmaterial.h"

#include <gtest/gtest.h>

using namespace W3dFx;

TEST(W3dFxMaterialTest, ReadsVersionEffectAndTechnique)
{
	Bytes header(37, 0);
	const std::string effect = "NormalMapped.fx";
	std::copy(effect.begin(), effect.end(), header.begin() + 1);
	header[0] = 1;
	header[33] = 2;

	Material material;
	ASSERT_TRUE(readHeader(header, material));
	EXPECT_EQ(material.effect, "normalmapped.fx");
	EXPECT_EQ(material.technique, 2);
	header[0] = 2;
	EXPECT_FALSE(readHeader(header, material));
	header.pop_back();
	EXPECT_FALSE(readHeader(header, material));
}

TEST(W3dFxMaterialTest, ReadsTypedProperties)
{
	Material material;
	ASSERT_TRUE(readProperty(Bytes{1, 0, 0, 0, 2, 0, 0, 0, 'T', 0, 2, 0, 0, 0, 'x', 0}, material));
	ASSERT_TRUE(readProperty(Bytes{2, 0, 0, 0, 2, 0, 0, 0, 'F', 0, 0, 0, 128, 63}, material));
	ASSERT_TRUE(readProperty(Bytes{3, 0, 0, 0, 2, 0, 0, 0, 'V', 0, 0, 0, 128, 63, 0, 0, 0, 64}, material));
	ASSERT_TRUE(readProperty(Bytes{4, 0, 0, 0, 2, 0, 0, 0, 'W', 0, 0, 0, 128, 63, 0, 0, 0, 64, 0, 0, 64, 64}, material));
	ASSERT_TRUE(readProperty(Bytes{5, 0, 0, 0, 2, 0, 0, 0, 'C', 0, 0, 0, 128, 63, 0, 0, 0, 64, 0, 0, 64, 64, 0, 0, 128, 64}, material));
	ASSERT_TRUE(readProperty(Bytes{6, 0, 0, 0, 2, 0, 0, 0, 'I', 0, 255, 255, 255, 255}, material));
	ASSERT_TRUE(readProperty(Bytes{7, 0, 0, 0, 2, 0, 0, 0, 'B', 0, 1}, material));

	EXPECT_EQ(material.get<std::string>("T", ""), "x");
	EXPECT_EQ(material.get<float>("F", 0.0f), 1.0f);
	EXPECT_EQ(material.get("V", Float2{}), (Float2{1, 2}));
	EXPECT_EQ(material.get("W", Float3{}), (Float3{1, 2, 3}));
	EXPECT_EQ(material.get("C", Float4{}), (Float4{1, 2, 3, 4}));
	EXPECT_EQ(material.get<std::int32_t>("I", 0), -1);
	EXPECT_TRUE(material.get("B", false));
	EXPECT_EQ(material.get("F", Float2{8, 9}), (Float2{8, 9}));
}

TEST(W3dFxMaterialTest, RejectsMalformedPropertiesWithoutInstallingThem)
{
	Material material;
	EXPECT_FALSE(readProperty(Bytes{7, 0, 0, 0, 255, 255, 255, 255, 'B', 0, 1}, material));
	EXPECT_FALSE(readProperty(Bytes{8, 0, 0, 0, 2, 0, 0, 0, 'B', 0, 1}, material));
	EXPECT_FALSE(readProperty(Bytes{7, 0, 0, 0, 2, 0, 0, 0, 'B', 'x', 1}, material));
	EXPECT_FALSE(readProperty(Bytes{7, 0, 0, 0, 2, 0, 0, 0, 'B', 0, 1, 0}, material));
	EXPECT_FALSE(readProperty(Bytes{1, 0, 0, 0, 2, 0, 0, 0, 'T', 0, 4, 0, 0, 0, 'x', 0}, material));
	EXPECT_TRUE(material.properties.empty());
}

TEST(W3dFxMaterialTest, ReadsSingleAndPerTriangleIndices)
{
	std::vector<std::uint32_t> ids;
	ASSERT_TRUE(readIds(Bytes{1, 0, 0, 0}, 2, ids));
	EXPECT_EQ(ids, (std::vector<std::uint32_t>{1}));
	ASSERT_TRUE(readIds(Bytes{1, 0, 0, 0, 0, 0, 0, 0}, 2, ids));
	EXPECT_EQ(ids, (std::vector<std::uint32_t>{1, 0}));
	EXPECT_FALSE(readIds(Bytes{1, 0, 0, 0, 0}, 2, ids));
}

TEST(W3dFxMaterialTest, ReadsVertexStreamsWithoutChangingTheStoredCoordinates)
{
	std::vector<Float2> uv;
	std::vector<Float3> tangent;
	ASSERT_TRUE(readVectors(Bytes{0, 0, 128, 63, 0, 0, 0, 64}, 1, uv));
	EXPECT_EQ(uv, (std::vector<Float2>{{1, 2}}));
	ASSERT_TRUE(readVectors(Bytes{0, 0, 128, 63, 0, 0, 0, 64, 0, 0, 64, 64}, 1, tangent));
	EXPECT_EQ(tangent, (std::vector<Float3>{{1, 2, 3}}));
	EXPECT_FALSE(readVectors(Bytes{0, 0, 128, 63, 0, 0, 0, 64}, 1, tangent));
}
