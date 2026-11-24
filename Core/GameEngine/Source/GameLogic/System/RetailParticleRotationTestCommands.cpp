#include "PreRTS.h"
#include "GameClient/ParticleSys.h"
#include "GameLogic/RetailTestCommands.h"
#include <cstdio>

namespace
{
void sampleRotation(const RetailTestCommands::Context& ctx)
{
	for (ParticleSystem *system : TheParticleSystemManager->getAllParticleSystems())
	{
		const auto *definition = system->getTemplate();
		Particle *particle = system->getFirstParticle();
		if (definition && particle && definition->getName() == ctx.argument)
			std::fprintf(stderr, "TEST particle-rotation frame=%u name=%s xy=%.5f z=%.5f\n",
				ctx.frame, ctx.argument, particle->getAngleXY(), particle->getAngle());
	}
}
}

namespace RetailTestCommands
{
void registerRetailParticleRotationTestCommands()
{
	registerAction("particle-rotation", sampleRotation);
}
}
