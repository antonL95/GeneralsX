#include <gtest/gtest.h>

#include "GameLogic/ShroudGeometry.h"

namespace
{

TEST(ShroudGeometryTest, ElevationBonusIsZeroWhenPerFootRateIsZero)
{
	EXPECT_FLOAT_EQ(ShroudGeometry::computeVisionElevationBonus(200.0f, 100.0f, 0.0f, -0.5f, 3.0f), 0.0f);
	EXPECT_FLOAT_EQ(ShroudGeometry::applyVisionElevationBonus(150.0f, 200.0f, 100.0f, 0.0f, -0.5f, 3.0f), 150.0f);
}

TEST(ShroudGeometryTest, ElevationBonusScalesAndClampsToMinAndMax)
{
	EXPECT_NEAR(ShroudGeometry::computeVisionElevationBonus(120.0f, 100.0f, 0.01f, -0.5f, 3.0f), 0.2f, 1e-5f);
	EXPECT_NEAR(ShroudGeometry::applyVisionElevationBonus(100.0f, 120.0f, 100.0f, 0.01f, -0.5f, 3.0f), 120.0f, 1e-4f);
	EXPECT_NEAR(ShroudGeometry::computeVisionElevationBonus(500.0f, 100.0f, 0.02f, -0.5f, 3.0f), 3.0f, 1e-5f);
	EXPECT_NEAR(ShroudGeometry::applyVisionElevationBonus(100.0f, 500.0f, 100.0f, 0.02f, -0.5f, 3.0f), 400.0f, 1e-4f);
	EXPECT_NEAR(ShroudGeometry::computeVisionElevationBonus(0.0f, 100.0f, 0.02f, -0.5f, 3.0f), -0.5f, 1e-5f);
	EXPECT_NEAR(ShroudGeometry::applyVisionElevationBonus(100.0f, 0.0f, 100.0f, 0.02f, -0.5f, 3.0f), 50.0f, 1e-4f);
}

TEST(ShroudGeometryTest, DirectionalShroudDistinguishesFrontSideAndRear)
{
	EXPECT_FALSE(ShroudGeometry::isDirectionalShroud(200.0f, 200.0f, 200.0f));
	EXPECT_TRUE(ShroudGeometry::isDirectionalShroud(200.0f, 100.0f, 50.0f));

	EXPECT_TRUE(ShroudGeometry::isPointInDirectionalShroud(190.0f, 0.0f, 1.0f, 0.0f, 200.0f, 100.0f, 50.0f));
	EXPECT_FALSE(ShroudGeometry::isPointInDirectionalShroud(210.0f, 0.0f, 1.0f, 0.0f, 200.0f, 100.0f, 50.0f));

	EXPECT_TRUE(ShroudGeometry::isPointInDirectionalShroud(0.0f, 95.0f, 1.0f, 0.0f, 200.0f, 100.0f, 50.0f));
	EXPECT_FALSE(ShroudGeometry::isPointInDirectionalShroud(0.0f, 105.0f, 1.0f, 0.0f, 200.0f, 100.0f, 50.0f));

	EXPECT_TRUE(ShroudGeometry::isPointInDirectionalShroud(-45.0f, 0.0f, 1.0f, 0.0f, 200.0f, 100.0f, 50.0f));
	EXPECT_FALSE(ShroudGeometry::isPointInDirectionalShroud(-55.0f, 0.0f, 1.0f, 0.0f, 200.0f, 100.0f, 50.0f));
}

TEST(ShroudGeometryTest, DirectionalShroudRotatesWithFacing)
{
	EXPECT_TRUE(ShroudGeometry::isPointInDirectionalShroud(0.0f, 190.0f, 0.0f, 1.0f, 200.0f, 100.0f, 50.0f));
	EXPECT_FALSE(ShroudGeometry::isPointInDirectionalShroud(0.0f, -60.0f, 0.0f, 1.0f, 200.0f, 100.0f, 50.0f));
	EXPECT_TRUE(ShroudGeometry::isPointInDirectionalShroud(95.0f, 0.0f, 0.0f, 1.0f, 200.0f, 100.0f, 50.0f));
	EXPECT_FALSE(ShroudGeometry::isPointInDirectionalShroud(110.0f, 0.0f, 0.0f, 1.0f, 200.0f, 100.0f, 50.0f));
}

}
