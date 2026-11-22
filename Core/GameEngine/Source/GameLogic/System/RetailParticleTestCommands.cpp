#include "PreRTS.h"
#include "GameClient/FXList.h"
#include "GameClient/ParticleSys.h"
#include "GameLogic/Object.h"
#include <cstdio>
#include <string_view>

void runRetailParticleTestCommand(std::string_view action, const Object* source, const Object* target, const Coord3D* location,
	const char* argument, UnsignedInt frame)
{
	if (action == "particle-fx")
	{
		const FXList* fx = TheFXListStore->findFXList(argument);
		if (location)
			FXList::doFXPos(fx, location);
		else if (source)
			FXList::doFXObj(fx, source, target);
		std::fprintf(stderr, "TEST particle-fx frame=%u name=%s found=%d\n", frame, argument, fx != nullptr);
		return;
	}

	if (action == "particle-systems")
	{
		UnsignedInt systems = 0;
		UnsignedInt particles = 0;
		UnsignedInt targeted = 0;
		for (ParticleSystem* system : TheParticleSystemManager->getAllParticleSystems())
		{
			const auto* definition = system->getTemplate();
			if (definition && definition->getName() == argument)
			{
				++systems;
				particles += system->getParticleCount();
				targeted += system->getRetailTargetMatrix() != nullptr;
			}
		}
		std::fprintf(stderr, "TEST particle-systems frame=%u name=%s systems=%u particles=%u targeted=%u gpu-total=%u\n",
			frame, argument, systems, particles, targeted, TheParticleSystemManager->getGpuParticleCount());
	}
}
