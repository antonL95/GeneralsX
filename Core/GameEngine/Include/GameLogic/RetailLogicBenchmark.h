#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

#include "Lib/BaseType.h"

namespace RetailLogicBenchmark
{
enum class Subsystem : std::uint8_t
{
	Pathfinding = 0,
	AI = 1,
	Partition = 2,
	Weapons = 3,
	Count = 4
};

extern bool g_enabled;
extern bool g_active;

inline bool enabled()
{
	return g_enabled;
}

inline bool active()
{
	return g_active;
}

void start(UnsignedInt frame);
void report(UnsignedInt frame, UnsignedInt finalCrc);

void beginLogicFrame();
void endLogicFrame();
void recordCounts(UnsignedInt objectCount, UnsignedInt particleCount);

std::size_t pushScope(Subsystem subsystem, std::chrono::steady_clock::time_point start);
void popScope(std::size_t slot, std::chrono::steady_clock::time_point end);
void recordParticleNs(std::uint64_t ns);

class FrameScope
{
public:
	FrameScope()
		: m_active(active())
	{
		if (m_active)
			beginLogicFrame();
	}

	~FrameScope()
	{
		if (m_active)
			endLogicFrame();
	}

private:
	bool m_active;
};

class Scope
{
public:
	explicit Scope(Subsystem subsystem)
		: m_slot(static_cast<std::size_t>(-1))
	{
		if (active())
			m_slot = pushScope(subsystem, std::chrono::steady_clock::now());
	}

	~Scope()
	{
		if (m_slot != static_cast<std::size_t>(-1))
			popScope(m_slot, std::chrono::steady_clock::now());
	}

private:
	std::size_t m_slot;
};

class ParticleScope
{
public:
	ParticleScope()
		: m_active(active())
	{
		if (m_active)
			m_start = std::chrono::steady_clock::now();
	}

	~ParticleScope()
	{
		if (m_active)
		{
			const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now() - m_start).count();
			if (elapsed > 0)
				recordParticleNs(static_cast<std::uint64_t>(elapsed));
		}
	}

private:
	bool m_active;
	std::chrono::steady_clock::time_point m_start;
};
}
