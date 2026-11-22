#pragma once

namespace ShroudGeometry
{

inline float computeVisionElevationBonus(
	float objectZ,
	float averageLocalGroundHeight,
	float bonusPercentPerFoot,
	float minBonusPercent,
	float maxBonusPercent)
{
	if (bonusPercentPerFoot <= 0.0f)
		return 0.0f;
	float bonus = (objectZ - averageLocalGroundHeight) * bonusPercentPerFoot;
	if (bonus > maxBonusPercent)
		bonus = maxBonusPercent;
	if (bonus < minBonusPercent)
		bonus = minBonusPercent;
	return bonus;
}

inline float applyVisionElevationBonus(
	float baseRange,
	float objectZ,
	float averageLocalGroundHeight,
	float bonusPercentPerFoot,
	float minBonusPercent,
	float maxBonusPercent)
{
	if (baseRange <= 0.0f || bonusPercentPerFoot <= 0.0f)
		return baseRange;
	const float bonus = computeVisionElevationBonus(
		objectZ,
		averageLocalGroundHeight,
		bonusPercentPerFoot,
		minBonusPercent,
		maxBonusPercent);
	const float adjusted = baseRange * (1.0f + bonus);
	return adjusted > 0.0f ? adjusted : 0.0f;
}

inline bool isDirectionalShroud(float frontRange, float sideRange, float rearRange)
{
	return frontRange > 0.0f && (sideRange != frontRange || rearRange != frontRange);
}

inline bool isPointInDirectionalShroud(
	float dx,
	float dy,
	float cosAngle,
	float sinAngle,
	float frontRange,
	float sideRange,
	float rearRange)
{
	const float localFwd = dx * cosAngle + dy * sinAngle;
	const float localSide = -dx * sinAngle + dy * cosAngle;
	const float rLong = localFwd >= 0.0f ? frontRange : rearRange;
	if (rLong <= 0.0f || sideRange <= 0.0f)
		return false;
	const float nx = localFwd / rLong;
	const float ny = localSide / sideRange;
	return (nx * nx + ny * ny) <= 1.0f;
}

}
