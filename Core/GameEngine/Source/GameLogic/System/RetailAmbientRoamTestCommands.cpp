#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AnimalAIUpdate.h"
#include "GameLogic/Object.h"
#include "WWMath/wwmath.h"

namespace
{
void sampleAmbientRoam(const RetailTestCommands::Context& ctx)
{
	const Object* building = ctx.requireObject(true);
	if (!building)
		return;

	const auto& center = *building->getPosition();
	for (const Object* actor = TheGameLogic->getFirstObject(); actor; actor = actor->getNextObject())
	{
		if (actor->getProducerID() != building->getID() || actor->isEffectivelyDead())
			continue;
		const char* name = actor->getTemplate()->getName().str();
		if (std::string_view(name) != "RivendellTownsman" && std::string_view(name) != "HorseGrazing_SlavedforPasture")
			continue;

		const auto& position = *actor->getPosition();
		const auto* ai = actor->getAI();
		const auto* goal = ai ? ai->getGoalPosition() : nullptr;
		const Real dx = position.x - center.x;
		const Real dy = position.y - center.y;
		std::fprintf(stderr, "TEST ambient-roam %u %s %u %.5f %.5f %.5f %d %d %.5f %.5f\n",
			ctx.frame, name, unsigned(actor->getID()), dx, dy, WWMath::Sqrt(dx * dx + dy * dy),
			dynamic_cast<const AnimalAIUpdate*>(ai) != nullptr, ai && ai->isMoving(),
			goal ? goal->x - center.x : 0.0f, goal ? goal->y - center.y : 0.0f);
	}
	std::fflush(stderr);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailAmbientRoamTestCommands()
{
#if !RTS_GENERALS
	registerAction("ambient-roam-sample", sampleAmbientRoam);
#endif
}
}
