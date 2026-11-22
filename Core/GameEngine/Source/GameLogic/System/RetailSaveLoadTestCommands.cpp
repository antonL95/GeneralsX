#include "PreRTS.h"

#include <cstdio>
#include <string_view>

#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"

void runRetailSaveLoadPathTestCommand(std::string_view action, Object* object, UnsignedInt frame)
{
	if (action == "snapshot-register-footprint")
	{
		if (object)
			TheAI->pathfinder()->addObjectToPathfindMap(object);
		return;
	}
	AIUpdateInterface* ai = object ? object->getAIUpdateInterface() : nullptr;
	if (action == "snapshot-path-queue")
	{
		Bool queued = FALSE;
		if (ai)
		{
			Coord3D goal = *object->getPosition();
			goal.x += 80.0f;
			ai->requestPath(&goal, TRUE);
			if (ai->isWaitingForPath())
				queued = TheAI->pathfinder()->queueForPath(object->getID());
		}
		std::fprintf(stderr, "TEST save-load path-queue frame=%u queued=%d\n", frame, queued);
		return;
	}
	std::fprintf(stderr, "TEST save-load path-state frame=%u waiting=%d\n", frame,
		ai ? static_cast<Int>(ai->isWaitingForPath()) : -1);
}

void runRetailSaveLoadBookTestCommand(Player* player, const char* templateName)
{
	Object* book = player ? player->getRetailSpellBook() : nullptr;
	const ThingTemplate* type = TheThingFactory->findTemplate(templateName, FALSE);
	if (!book || !type)
	{
		std::fprintf(stderr, "TEST save-load spellbook repair fixture missing object or template\n");
		return;
	}
	const ObjectID bookID = book->getID();
	Object* unit = TheThingFactory->newObjectFromExactTemplate(type, player->getDefaultTeam(), OBJECT_STATUS_MASK_NONE);
	const ObjectID unitID = unit->getID();
	const Bool lookup = bookID != unitID && TheGameLogic->findObjectByID(bookID) == book &&
		TheGameLogic->findObjectByID(unitID) == unit;
	std::fprintf(stderr, "TEST save-load repaired spellbook id=%u unit=%u lookup=%d\n",
		static_cast<UnsignedInt>(bookID), static_cast<UnsignedInt>(unitID), lookup);
}
