#include "PreRTS.h"
#include "GameClient/FXList.h"
#include "GameClient/ParticleSys.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include <cstdio>
#include <string_view>

void runRetailParticleTestCommand(std::string_view action, const Object* source, const Object* target, const Coord3D* location,
	const char* argument, UnsignedInt frame)
{
	if (action == "particle-water-fx")
	{
		Region3D bounds;
		TheTerrainLogic->getExtent(&bounds);
		for (Real y = bounds.lo.y + 5.0f; y < bounds.hi.y; y += 10.0f)
		{
			for (Real x = bounds.lo.x + 5.0f; x < bounds.hi.x; x += 10.0f)
			{
				if (!TheTerrainLogic->isUnderwater(x, y))
					continue;
				const Coord3D water = {x, y, TheTerrainLogic->getGroundHeight(x, y)};
				FXList::doFXPos(TheFXListStore->findFXList(argument), &water);
				std::fprintf(stderr, "TEST particle-water-fx frame=%u x=%.1f y=%.1f\n", frame, x, y);
				return;
			}
		}
		return;
	}

	if (action == "particle-fx")
	{
		const FXList* fx = TheFXListStore->findFXList(argument);
		if (location)
			FXList::doFXPos(fx, location, source ? source->getTransformMatrix() : nullptr, 0.0f,
				target ? target->getPosition() : nullptr, 0.0f, source, target);
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
				if (const Coord3D* endpoint = system->getRetailTargetEndpoint())
					std::fprintf(stderr, "TEST particle-endpoint frame=%u name=%s x=%.3f y=%.3f z=%.3f\n",
						frame, argument, endpoint->x, endpoint->y, endpoint->z);
			}
		}
		std::fprintf(stderr, "TEST particle-systems frame=%u name=%s systems=%u particles=%u targeted=%u gpu-total=%u\n",
			frame, argument, systems, particles, targeted, TheParticleSystemManager->getGpuParticleCount());
	}
}
