#include "PreRTS.h"
#include "Common/GlobalData.h"
#include "GameClient/FXList.h"
#include "GameClient/ParticleSys.h"
#include "GameLogic/Object.h"
#include "GameLogic/RetailTestCommands.h"
#include "GameLogic/TerrainLogic.h"
#include <cstdio>
#include <string>
#include <string_view>

namespace
{
void runRetailParticleTestCommand(std::string_view action, const Object* source, const Object* target, const Coord3D* location,
	const char* argument, UnsignedInt frame)
{
	if (action == "particle-weather")
	{
		TheWritableGlobalData->m_weather = std::string_view(argument) == "snow" ? WEATHER_SNOWY : WEATHER_NORMAL;
		std::fprintf(stderr, "TEST particle-weather frame=%u weather=%d\n", frame, TheGlobalData->m_weather);
		return;
	}

	if (action == "particle-bone-sample")
	{
		for (ParticleSystem* system : TheParticleSystemManager->getAllParticleSystems())
		{
			const auto* definition = system->getTemplate();
			if (!definition || definition->getName() != argument)
				continue;
			Particle* particle = system->getFirstParticle();
			if (!particle)
				continue;
			while (particle->m_systemNext)
				particle = particle->m_systemNext;
			const Coord3D& position = particle->usesGpuDraw() ? particle->getGpuSpawnPosition() : *particle->getPosition();
			std::fprintf(stderr, "TEST particle-bone frame=%u name=%s attached=%u x=%.3f y=%.3f z=%.3f\n",
				frame, argument, static_cast<UnsignedInt>(system->getAttachedObject()), position.x, position.y, position.z);
		}
		return;
	}

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

void dispatchParticleCommand(const RetailTestCommands::Context& ctx)
{
	const Coord3D location = ctx.hasPosition ? ctx.groundLocation() : Coord3D{};
	const std::string_view argument(ctx.argument);
	const auto separator = argument.find(',');
	const Object* target = separator != std::string_view::npos ? ctx.findTarget(argument.substr(separator + 1), false) : nullptr;
	const std::string name(argument.substr(0, separator));
	const Object* source = std::string_view(ctx.targetName) != "-" ? ctx.source : nullptr;
	runRetailParticleTestCommand(ctx.action, source, target, ctx.hasPosition ? &location : nullptr, name.c_str(), ctx.frame);
}
}

namespace RetailTestCommands
{
void registerRetailParticleTestCommands()
{
	registerAction("particle-fx", dispatchParticleCommand);
	registerAction("particle-systems", dispatchParticleCommand);
	registerAction("particle-water-fx", dispatchParticleCommand);
	registerAction("particle-weather", dispatchParticleCommand);
	registerAction("particle-bone-sample", dispatchParticleCommand);
#if !RTS_GENERALS
	registerAction("particle-condition", [](const Context& ctx) {
		if (Object* object = ctx.source)
		{
			if (std::string_view(ctx.argument) == "ring")
				object->setModelConditionState(MODELCONDITION_ONE_RING);
			else if (std::string_view(ctx.argument) == "hero")
				object->setModelConditionState(MODELCONDITION_CREATE_A_HERO_02);
		}
	});
#endif
}
}
