#pragma once

#include <limits>

namespace RotwkLocomotorRules
{
inline constexpr float FULL_TURN = 6.28318530718f;
inline constexpr int NO_FORMATION_PRIORITY = 0;
inline constexpr int DEFAULT_FORMATION_PRIORITY = 7;

struct Authored
{
	float speedDamagedFraction = -1.0f;
	float minSpeedFraction = 0.0f;
	float minTurnSpeedFraction = -1.0f;
	float liftFraction = -1.0f;
	float liftDamagedFraction = -1.0f;
	float burningDeathRadius = 0.0f;
	float chargeSpeedFraction = 1.0f;
	float backingUpSpeedFraction = 0.75f;
	float backingUpDistanceMin = 0.0f;
	float backingUpDistanceMax = 0.0f;
	float backingUpAngle = 0.5f;
	float walkDistance = 0.0f;
	float accelDecelTrigger = -1.0f;
	float turnThreshold = FULL_TURN / 24.0f;
	float turnThresholdHighSpeed = FULL_TURN / 2.0f;
	float maxOverlappedHeight = std::numeric_limits<float>::max();
	float riverModifier = 1.0f;
	bool scalesWalls = false;
	bool useTerrainSmoothing = false;
	bool enableHighSpeedTurnModelConditions = true;
	bool burningDeathIsCavalry = false;
	bool chargeAvailable = false;
	bool chargeIgnoresCondition = false;
	bool backingUpStopWhenTurning = false;
	bool crewPowered = false;
	int formationPriority = DEFAULT_FORMATION_PRIORITY;
	unsigned int accelerationFrames = 0;
	unsigned int brakingFrames = 0;
	unsigned int turnFrames = 0;
	unsigned int turnFramesDamaged = 0;
};

struct Resolved
{
	float maxSpeed;
	float maxSpeedDamaged;
	float minSpeed;
	float minTurnSpeed;
	float acceleration;
	float accelerationDamaged;
	float braking;
	float lift;
	float liftDamaged;
	float turnRate;
	float turnRateDamaged;
};

inline float rateToReach(float speed, unsigned int frames)
{
	return frames > 0 ? speed / static_cast<float>(frames) : speed;
}

inline Resolved resolve(const Authored& authored, float speed, const Resolved& fallback)
{
	Resolved resolved = fallback;
	resolved.maxSpeed = speed;
	resolved.maxSpeedDamaged = authored.speedDamagedFraction >= 0.0f ? speed * authored.speedDamagedFraction : speed;
	resolved.minSpeed = speed * authored.minSpeedFraction;
	if (authored.minTurnSpeedFraction >= 0.0f)
		resolved.minTurnSpeed = speed * authored.minTurnSpeedFraction;
	resolved.acceleration = rateToReach(speed, authored.accelerationFrames);
	resolved.accelerationDamaged = rateToReach(resolved.maxSpeedDamaged, authored.accelerationFrames);
	if (authored.brakingFrames > 0)
		resolved.braking = speed / static_cast<float>(authored.brakingFrames);
	if (authored.liftFraction >= 0.0f)
		resolved.lift = speed * authored.liftFraction;
	if (authored.liftDamagedFraction >= 0.0f)
		resolved.liftDamaged = speed * authored.liftDamagedFraction;
	else if (authored.liftFraction >= 0.0f)
		resolved.liftDamaged = resolved.lift;
	if (authored.turnFrames > 0)
		resolved.turnRate = FULL_TURN / static_cast<float>(authored.turnFrames);
	resolved.turnRateDamaged = authored.turnFramesDamaged > 0 ? FULL_TURN / static_cast<float>(authored.turnFramesDamaged) : resolved.turnRate;
	return resolved;
}
}
