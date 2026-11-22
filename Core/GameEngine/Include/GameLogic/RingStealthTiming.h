#pragma once

#include <cstdint>

namespace RingStealthTiming
{

struct Config
{
	uint32_t delayOn = 0;
	uint32_t delayOff = 0;
	uint32_t animOn = 0;
	uint32_t animOff = 0;
	uint32_t delayAfterRemoving = 0;
	bool hasOnFX = false;
	bool hasOffFX = false;
};

struct State
{
	uint32_t toggleFrame = 0;
	uint32_t animEndFrame = 0;
	uint32_t blockedUntilFrame = 0;
	bool toggleOn = false;
	bool animOn = false;
};

inline bool hasRingConfig(const Config& config)
{
	return config.animOn != 0 || config.delayAfterRemoving != 0;
}

inline bool canToggle(const Config& config, const State& state, uint32_t now, bool currentlyWorn)
{
	if (!hasRingConfig(config))
		return false;
	if (state.toggleFrame || state.animEndFrame)
		return false;
	if (!currentlyWorn && now < state.blockedUntilFrame)
		return false;
	return true;
}

inline void startTransition(const Config& config, State& state, uint32_t now, bool turnOn)
{
	const uint32_t delay = turnOn ? config.delayOn : config.delayOff;
	const uint32_t anim = turnOn ? config.animOn : config.animOff;
	state.toggleOn = turnOn;
	state.toggleFrame = now + (delay > 0 ? delay : 1u);
	state.animOn = turnOn;
	state.animEndFrame = anim > 0 ? now + anim : 0u;
}

}
