#include "PreRTS.h"
#include "Common/GameLOD.h"
#include "Common/GlobalData.h"
#include "GameClient/ParticleSys.h"
#include "GameLogic/RetailTestCommands.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <string>

namespace
{
void sampleBudget(const RetailTestCommands::Context& ctx)
{
	std::map<std::pair<Int, std::string>, UnsignedInt> counts;
	for (ParticleSystem* system : TheParticleSystemManager->getAllParticleSystems())
	{
		const auto* definition = system->getTemplate();
		if (definition && system->getParticleCount())
			counts[{system->getPriority(), definition->getName().str()}] += system->getParticleCount();
	}
	for (const auto& [key, count] : counts)
		std::fprintf(stderr, "TEST particle-breakdown frame=%u priority=%d name=%s count=%u\n",
			ctx.frame, key.first, key.second.c_str(), count);
}

void sampleParticles(const RetailTestCommands::Context& ctx)
{
	for (ParticleSystem* system : TheParticleSystemManager->getAllParticleSystems())
	{
		const auto* definition = system->getTemplate();
		if (!definition || definition->getName() != ctx.argument)
			continue;

		UnsignedInt oldest = 0;
		UnsignedInt youngest = system->getParticleCount() ? system->getFirstParticle()->getElapsedFrames() : 0;
		Real peakAlpha = 0.0f;
		Real totalAlpha = 0.0f;
		for (Particle* particle = system->getFirstParticle(); particle; particle = particle->m_systemNext)
		{
			oldest = std::max(oldest, particle->getElapsedFrames());
			youngest = std::min(youngest, particle->getElapsedFrames());
			peakAlpha = std::max(peakAlpha, particle->getAlpha());
			totalAlpha += particle->getAlpha();
		}
		std::fprintf(stderr,
			"TEST particle-stats frame=%u name=%s count=%u youngest=%u oldest=%u peak-alpha=%.4f total-alpha=%.4f\n",
			ctx.frame, ctx.argument, system->getParticleCount(), youngest, oldest, peakAlpha, totalAlpha);
	}
	std::fprintf(stderr, "TEST particle-budget frame=%u count=%u limit=%d skip-priority=%d dynamic-lod=%s\n",
		ctx.frame, TheParticleSystemManager->getParticleCount(), TheGlobalData->m_maxParticleCount,
		TheGameLODManager->getMinDynamicParticleSkipPriority(),
		TheGameLODManager->getDynamicGameLODLevelName(TheGameLODManager->getDynamicLODLevel()));
}

void sampleParticleSystems(const RetailTestCommands::Context& ctx)
{
	for (ParticleSystem* system : TheParticleSystemManager->getAllParticleSystems())
	{
		const auto* definition = system->getTemplate();
		Particle* first = system->getFirstParticle();
		if (!definition || !first)
			continue;

		const auto* color = first->getColor();
		const auto* position = first->getPosition();
		std::fprintf(stderr,
			"TEST particle-live frame=%u id=%u name=%s shader=%d texture=%s count=%u age=%u rgb=%.5f,%.5f,%.5f alpha=%.5f xyz=%.2f,%.2f,%.2f size=%.2f\n",
			ctx.frame, static_cast<UnsignedInt>(system->getSystemID()), definition->getName().str(),
			system->getShaderType(), system->getParticleTypeName().str(), system->getParticleCount(),
			first->getElapsedFrames(), color->red, color->green, color->blue, first->getAlpha(),
			position->x, position->y, position->z, first->getSize());
	}
}
}

namespace RetailTestCommands
{
void registerRetailParticleStatsTestCommands()
{
	registerAction("particle-stats", sampleParticles);
	registerAction("particle-budget-breakdown", sampleBudget);
	registerAction("particle-live", sampleParticleSystems);
}
}
