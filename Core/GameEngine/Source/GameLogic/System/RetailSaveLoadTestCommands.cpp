#include "PreRTS.h"

#include <cstdio>
#include <string_view>

#include "Common/Player.h"
#include "Common/LatchRestore.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Pathfinder/PathfindCell.h"
#include "GameLogic/ScriptActions.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/TerrainLogic.h"

void runRetailSaveLoadPartitionTestCommand(std::string_view action, Object* object, const char* argument, UnsignedInt frame)
{
	if (!object)
		return;

	const GeometryInfo& geometry = object->getGeometryInfo();
	if (action == "snapshot-partition-scale")
	{
		Real factor;
		if (std::sscanf(argument, "%f", &factor) == 1 && factor > 0.0f)
			object->setGeometryInfo(GeometryInfo(geometry.getGeomType(), FALSE, geometry.getMaxHeightAbovePosition() * factor,
				geometry.getMajorRadius() * factor, geometry.getMinorRadius() * factor));
		return;
	}

	const PartitionData* data = object->friend_getPartitionData();
	if (!data)
		return;

	Int count = 0;
	std::fprintf(stderr, "TEST save-load partition frame=%u template=%s major=%.2f cells=", frame,
		object->getTemplate()->getName().str(), geometry.getMajorRadius());
	for (Int y = 0; y < ThePartitionManager->getCellCountY(); ++y)
	{
		for (Int x = 0; x < ThePartitionManager->getCellCountX(); ++x)
		{
			for (auto* entry = ThePartitionManager->getCellAt(x, y)->getFirstCoiInCell(); entry; entry = entry->getNextCoi())
			{
				if (entry->getModule() != data)
					continue;
				std::fprintf(stderr, "%d,%d;", x, y);
				++count;
				break;
			}
		}
	}
	std::fprintf(stderr, " count=%d\n", count);
}

void runRetailSaveLoadWallTestCommand(std::string_view action, Player* player, Object* wall,
	const char* name, const Coord3D& position, UnsignedInt frame)
{
	if (action == "snapshot-wall-reinforce")
	{
		Waypoint* waypoint = TheTerrainLogic->getFirstWaypoint();
		Team* team = player ? player->getDefaultTeam() : nullptr;
		if (!waypoint || !team)
			return;
		auto* info = const_cast<TeamTemplateInfo*>(team->getPrototype()->getTemplateInfo());
		LatchRestore<Int> count(info->m_numUnitsInfo, 1);
		LatchRestore<TCreateUnitsInfo> entry(info->m_unitsInfo[0], {1, 1, "GondorCastleWallSegment"});
		LatchRestore<AsciiString> transport(info->m_transportUnitType, AsciiString());
		LatchRestore<AsciiString> origin(info->m_startReinforceWaypoint, AsciiString());
		LatchRestore<Coord3D> location(*const_cast<Coord3D*>(waypoint->getLocation()), position);
		ScriptAction* script = newInstance(ScriptAction)(ScriptAction::CREATE_REINFORCEMENT_TEAM);
		script->getParameter(0)->friend_setString(team->getName());
		script->getParameter(1)->friend_setString(waypoint->getName());
		TheScriptActions->executeAction(script);
		deleteInstance(script);
		return;
	}
	if (action == "snapshot-wall-occupant")
	{
		const Object* unit = TheScriptEngine->getUnitNamed(name);
		std::fprintf(stderr, "TEST save-load wall-occupant frame=%u name=%s layer=%d walkable=%d\n", frame, name,
			unit ? static_cast<Int>(unit->getLayer()) : -1,
			unit && TheAI->pathfinder()->isPointOnWall(unit->getPosition()));
		return;
	}
	if (!wall)
		return;
	if (action == "snapshot-wall-delete")
	{
		wall->setName(name);
		TheScriptEngine->addObjectToCache(wall);
		ScriptAction* script = newInstance(ScriptAction)(ScriptAction::NAMED_DELETE);
		script->getParameter(0)->friend_setString(wall->getName());
		TheScriptActions->executeAction(script);
		deleteInstance(script);
		return;
	}
	const ThingTemplate* type = TheThingFactory->findTemplate("GondorFighter", FALSE);
	if (!type || !TheAI->pathfinder()->isPointOnWall(wall->getPosition()))
		return;
	Object* unit = TheThingFactory->newObjectFromExactTemplate(type, wall->getTeam(), OBJECT_STATUS_MASK_NONE);
	Coord3D deck = *wall->getPosition();
	deck.z = TheTerrainLogic->getLayerHeight(deck.x, deck.y, LAYER_WALL);
	unit->setPosition(&deck);
	unit->setDestinationLayer(LAYER_WALL);
	unit->setLayer(LAYER_WALL);
	unit->setName(name);
	TheScriptEngine->addObjectToCache(unit);
}

void runRetailSaveLoadPathTestCommand(std::string_view action, Object* object, UnsignedInt frame)
{
	if (action == "snapshot-obstacle-owner")
	{
		Bool ownsCell = FALSE;
		if (object)
		{
			const Coord3D& position = *object->getPosition();
			const PathfindCell* cell = TheAI->pathfinder()->getCell(LAYER_GROUND,
				REAL_TO_INT_FLOOR(position.x / PATHFIND_CELL_SIZE_F), REAL_TO_INT_FLOOR(position.y / PATHFIND_CELL_SIZE_F));
			ownsCell = cell && cell->getType() == PathfindCell::CELL_OBSTACLE && cell->getObstacleID() == object->getID();
		}
		std::fprintf(stderr, "TEST save-load obstacle frame=%u template=%s ownsCell=%d\n", frame,
			object ? object->getTemplate()->getName().str() : "-", ownsCell);
		return;
	}
	if (action == "snapshot-register-footprint")
	{
		if (object)
			TheAI->pathfinder()->addObjectToPathfindMap(object);
		return;
	}
	if (action == "snapshot-wall-state")
	{
		const Bool walkable = object && TheAI->pathfinder()->isPointOnWall(object->getPosition());
		std::fprintf(stderr, "TEST save-load wall-state frame=%u template=%s walkable=%d\n", frame,
			object ? object->getTemplate()->getName().str() : "-", walkable);
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
