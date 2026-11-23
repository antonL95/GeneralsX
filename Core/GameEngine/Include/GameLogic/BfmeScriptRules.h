#pragma once

#include <algorithm>
#include <cmath>

namespace BfmeScriptRules
{
enum class WireKind
{
	Legacy,
	UnitReference,
	ModelCondition,
	ObjectType,
	EvaEvent,
	NotificationType,
	Unknown
};

struct WireType
{
	WireKind kind;
	int legacy;
};

constexpr int WIRE_INSERTED = 29;
constexpr int WIRE_LAST_LEGACY = 52;
constexpr int WIRE_UNIT_REFERENCE = 54;
constexpr int WIRE_MODEL_CONDITION = 58;
constexpr int WIRE_OBJECT_TYPE = 61;
constexpr int WIRE_EVA_EVENT = 76;
constexpr int WIRE_NOTIFICATION_TYPE = 77;

constexpr WireType classifyWire(int wire)
{
	if (wire >= 0 && wire < WIRE_INSERTED)
		return { WireKind::Legacy, wire };
	if (wire > WIRE_INSERTED && wire <= WIRE_LAST_LEGACY)
		return { WireKind::Legacy, wire - 1 };
	switch (wire)
	{
		case WIRE_UNIT_REFERENCE: return { WireKind::UnitReference, -1 };
		case WIRE_MODEL_CONDITION: return { WireKind::ModelCondition, -1 };
		case WIRE_OBJECT_TYPE: return { WireKind::ObjectType, -1 };
		case WIRE_EVA_EVENT: return { WireKind::EvaEvent, -1 };
		case WIRE_NOTIFICATION_TYPE: return { WireKind::NotificationType, -1 };
		default: return { WireKind::Unknown, -1 };
	}
}

enum Comparison
{
	LESS_THAN,
	LESS_EQUAL,
	EQUAL,
	GREATER_EQUAL,
	GREATER,
	NOT_EQUAL
};

template<class T>
constexpr bool compare(int comparison, T value, T reference)
{
	switch (comparison)
	{
		case LESS_THAN: return value < reference;
		case LESS_EQUAL: return value <= reference;
		case EQUAL: return value == reference;
		case GREATER_EQUAL: return value >= reference;
		case GREATER: return value > reference;
		case NOT_EQUAL: return value != reference;
		default: return false;
	}
}

constexpr bool skirmishOrMultiplayerIs(bool skirmish, bool multiplayer, bool wanted)
{
	return (skirmish || multiplayer) == wanted;
}

constexpr bool levelCapReached(int rankLevel, int rankLevelLimit, int rankLevelCount)
{
	return rankLevel >= std::min(rankLevelLimit, rankLevelCount);
}

inline bool compareDistance(int comparison, float distanceSquared, float reference)
{
	return compare(comparison, std::sqrt(distanceSquared), reference);
}

constexpr int secondsToFrames(int seconds, int framesPerSecond)
{
	return seconds * framesPerSecond;
}

constexpr float clipDepthMultiplier(float requested)
{
	return requested > 0.0f ? requested : 1.0f;
}

constexpr float RETAIL_FAR_CLIP_DISTANCE = 1800.0f;

constexpr float retailFarClip(float multiplier)
{
	return RETAIL_FAR_CLIP_DISTANCE * clipDepthMultiplier(multiplier);
}

constexpr int militaryCaptionMilliseconds(float seconds)
{
	return seconds > 0.0f ? static_cast<int>(seconds * 1000.0f + 0.5f) : 0;
}
}
