#include "PreRTS.h"
#include "GameLogic/RetailLogicBenchmark.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

#ifndef _WIN32
#include <sys/resource.h>
#endif

namespace RetailLogicBenchmark
{
namespace
{
struct FrameSample
{
	std::uint64_t totalNs = 0;
	std::array<std::uint64_t, static_cast<std::size_t>(Subsystem::Count)> subsystemNs = {};
	std::uint64_t particlesNs = 0;
};

struct ScopeEntry
{
	Subsystem subsystem = Subsystem::Pathfinding;
	std::chrono::steady_clock::time_point start;
	std::uint64_t childNs = 0;
};

constexpr std::size_t kMaxScopeDepth = 16;

struct State
{
	bool inFrame = false;
	std::chrono::steady_clock::time_point runStart;
	std::chrono::steady_clock::time_point frameStart;
	double cpuStartMs = 0.0;
	FrameSample current;
	std::uint64_t pendingParticlesNs = 0;
	std::array<ScopeEntry, kMaxScopeDepth> stack = {};
	std::size_t stackDepth = 0;
	std::vector<FrameSample> samples;
	UnsignedInt peakObjects = 0;
	UnsignedInt peakParticles = 0;
};

State& state()
{
	static State instance;
	return instance;
}

double processCpuMs()
{
#ifndef _WIN32
	struct rusage usage = {};
	if (getrusage(RUSAGE_SELF, &usage) == 0)
	{
		const double userMs = static_cast<double>(usage.ru_utime.tv_sec) * 1000.0 +
			static_cast<double>(usage.ru_utime.tv_usec) / 1000.0;
		const double sysMs = static_cast<double>(usage.ru_stime.tv_sec) * 1000.0 +
			static_cast<double>(usage.ru_stime.tv_usec) / 1000.0;
		return userMs + sysMs;
	}
#endif
	return 0.0;
}

bool readEnvFlag()
{
	const char* value = std::getenv("GENERALSX_LOGIC_BENCHMARK");
	return value != nullptr && value[0] != '\0' && value[0] != '0';
}
}

bool g_enabled = readEnvFlag();
bool g_active = false;

void start(UnsignedInt frame)
{
	if (!g_enabled)
		return;
	State& s = state();
	s.inFrame = false;
	s.stackDepth = 0;
	s.pendingParticlesNs = 0;
	s.current = FrameSample{};
	s.samples.clear();
	s.samples.reserve(4096);
	s.peakObjects = 0;
	s.peakParticles = 0;
	s.runStart = std::chrono::steady_clock::now();
	s.cpuStartMs = processCpuMs();
	g_active = true;
	std::fprintf(stderr, "BENCH_START frame=%u\n", frame);
	std::fflush(stderr);
}

void beginLogicFrame()
{
	State& s = state();
	s.inFrame = true;
	s.stackDepth = 0;
	s.current = FrameSample{};
	s.current.particlesNs = s.pendingParticlesNs;
	s.pendingParticlesNs = 0;
	s.frameStart = std::chrono::steady_clock::now();
}

void endLogicFrame()
{
	State& s = state();
	if (!s.inFrame)
		return;
	const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now() - s.frameStart).count();
	s.current.totalNs = elapsed > 0 ? static_cast<std::uint64_t>(elapsed) : 0;
	s.samples.push_back(s.current);
	s.inFrame = false;
	s.stackDepth = 0;
}

void recordCounts(UnsignedInt objectCount, UnsignedInt particleCount)
{
	State& s = state();
	if (objectCount > s.peakObjects)
		s.peakObjects = objectCount;
	if (particleCount > s.peakParticles)
		s.peakParticles = particleCount;
}

std::size_t pushScope(Subsystem subsystem, std::chrono::steady_clock::time_point start)
{
	State& s = state();
	if (!s.inFrame || s.stackDepth >= kMaxScopeDepth)
		return static_cast<std::size_t>(-1);
	const std::size_t slot = s.stackDepth++;
	s.stack[slot].subsystem = subsystem;
	s.stack[slot].start = start;
	s.stack[slot].childNs = 0;
	return slot;
}

void popScope(std::size_t slot, std::chrono::steady_clock::time_point end)
{
	State& s = state();
	if (!s.inFrame || slot >= s.stackDepth)
		return;
	const ScopeEntry& entry = s.stack[slot];
	const auto raw = std::chrono::duration_cast<std::chrono::nanoseconds>(end - entry.start).count();
	const std::uint64_t elapsed = raw > 0 ? static_cast<std::uint64_t>(raw) : 0;
	const std::uint64_t exclusive = elapsed > entry.childNs ? elapsed - entry.childNs : 0;
	s.current.subsystemNs[static_cast<std::size_t>(entry.subsystem)] += exclusive;
	s.stackDepth = slot;
	if (slot > 0)
		s.stack[slot - 1].childNs += elapsed;
}

void recordParticleNs(std::uint64_t ns)
{
	State& s = state();
	if (s.inFrame)
		s.current.particlesNs += ns;
	else
		s.pendingParticlesNs += ns;
}

void report(UnsignedInt frame, UnsignedInt finalCrc)
{
	if (!g_enabled)
		return;
	State& s = state();
	g_active = false;
	const double wallMs = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - s.runStart).count();
	const double cpuMs = processCpuMs() - s.cpuStartMs;
	const std::size_t count = s.samples.size();
	double logicMeanUs = 0.0;
	double logicP99Us = 0.0;
	double logicMaxUs = 0.0;
	double pathMeanUs = 0.0;
	double aiMeanUs = 0.0;
	double partMeanUs = 0.0;
	double weapMeanUs = 0.0;
	double particleMeanUs = 0.0;
	double otherMeanUs = 0.0;

	if (count > 0)
	{
		std::vector<std::uint64_t> totals;
		totals.reserve(count);
		double sumTotal = 0.0;
		double sumPath = 0.0;
		double sumAi = 0.0;
		double sumPart = 0.0;
		double sumWeap = 0.0;
		double sumParticles = 0.0;
		double sumOther = 0.0;
		for (const FrameSample& sample : s.samples)
		{
			totals.push_back(sample.totalNs);
			const std::uint64_t p = sample.subsystemNs[static_cast<std::size_t>(Subsystem::Pathfinding)];
			const std::uint64_t a = sample.subsystemNs[static_cast<std::size_t>(Subsystem::AI)];
			const std::uint64_t c = sample.subsystemNs[static_cast<std::size_t>(Subsystem::Partition)];
			const std::uint64_t w = sample.subsystemNs[static_cast<std::size_t>(Subsystem::Weapons)];
			const std::uint64_t accounted = p + a + c + w;
			const std::uint64_t other = sample.totalNs > accounted ? sample.totalNs - accounted : 0;
			sumTotal += static_cast<double>(sample.totalNs);
			sumPath += static_cast<double>(p);
			sumAi += static_cast<double>(a);
			sumPart += static_cast<double>(c);
			sumWeap += static_cast<double>(w);
			sumParticles += static_cast<double>(sample.particlesNs);
			sumOther += static_cast<double>(other);
		}
		std::sort(totals.begin(), totals.end());
		const std::size_t p99Index = std::min(count - 1, (count * 99) / 100);
		const double denom = static_cast<double>(count) * 1000.0;
		logicMeanUs = sumTotal / denom;
		logicP99Us = static_cast<double>(totals[p99Index]) / 1000.0;
		logicMaxUs = static_cast<double>(totals.back()) / 1000.0;
		pathMeanUs = sumPath / denom;
		aiMeanUs = sumAi / denom;
		partMeanUs = sumPart / denom;
		weapMeanUs = sumWeap / denom;
		particleMeanUs = sumParticles / denom;
		otherMeanUs = sumOther / denom;
	}

	std::fprintf(
		stderr,
		"BENCH_RESULT frame=%u frames=%zu logic_mean_us=%.1f logic_p99_us=%.1f logic_max_us=%.1f "
		"pathfinding_mean_us=%.1f ai_mean_us=%.1f partition_mean_us=%.1f weapons_mean_us=%.1f "
		"particles_mean_us=%.1f other_mean_us=%.1f peak_objects=%u peak_particles=%u "
		"cpu_ms=%.1f wall_ms=%.1f final_crc=%08X\n",
		frame,
		count,
		logicMeanUs,
		logicP99Us,
		logicMaxUs,
		pathMeanUs,
		aiMeanUs,
		partMeanUs,
		weapMeanUs,
		particleMeanUs,
		otherMeanUs,
		s.peakObjects,
		s.peakParticles,
		cpuMs,
		wallMs,
		finalCrc);
	std::fflush(stderr);
}
}
