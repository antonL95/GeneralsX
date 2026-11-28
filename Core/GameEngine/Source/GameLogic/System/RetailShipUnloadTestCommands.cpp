#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/MessageStream.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/Pathfinder/PathfindCell.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
void selectSingle(Object* obj)
{
	GameMessage* sel = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
	sel->appendBooleanArgument(TRUE);
	sel->appendObjectIDArgument(obj->getID());
}

Int getCellTypeAt(const Coord3D& pos, PathfindLayerEnum layer)
{
	const PathfindCell* pfCell = TheAI->pathfinder()->getCell(layer, &pos);
	return pfCell ? static_cast<Int>(pfCell->getType()) : -1;
}

void logProbe(const RetailTestCommands::Context& ctx, const Object* obj, const char* label)
{
	const Coord3D& pos = *obj->getPosition();
	const Int cellType = getCellTypeAt(pos, obj->getLayer());
	Real waterZ = 0.0f;
	Real terrainZ = 0.0f;
	const Bool uw = TheTerrainLogic->isUnderwater(pos.x, pos.y, &waterZ, &terrainZ);
	const Real dz = uw ? (waterZ - terrainZ) : 0.0f;
	const ContainModuleInterface* contain = obj->getContain();
	const Int containCount = contain ? static_cast<Int>(contain->getContainCount()) : -1;
	const Int containedBy = obj->getContainedBy() ? 1 : 0;

	Int members = 0;
	Int landMembers = 0;
	Int waterMembers = 0;
	if (const HordeContain* horde = HordeContain::get(obj))
	{
		for (const Object* member : *horde->getContainedItemsList())
		{
			if (!member || member->isEffectivelyDead())
				continue;
			++members;
			const Coord3D* mp = member->getPosition();
			const Int mCell = getCellTypeAt(*mp, member->getLayer());
			const Bool mUw = TheTerrainLogic->isUnderwater(mp->x, mp->y);
			if (mUw || mCell == PathfindCell::CELL_WATER || mCell == PathfindCell::CELL_DEEP_WATER)
			{
				++waterMembers;
				std::fprintf(stderr, "TEST ship-unload-member-water frame=%u label=%s pos=%.1f,%.1f cell=%d uw=%d\n",
					ctx.frame, label, mp->x, mp->y, mCell, mUw ? 1 : 0);
			}
			else if (mCell == PathfindCell::CELL_CLEAR)
				++landMembers;
		}
	}

	const AIUpdateInterface* ai = obj->getAI();
	const Coord3D* goal = ai ? ai->getGoalPosition() : nullptr;
	std::fprintf(stderr,
		"TEST ship-unload-probe frame=%u label=%s pos=%.1f,%.1f,%.1f cell=%d uw=%d dz=%.1f contain=%d contained_by=%d members=%d land=%d water=%d sid=%d goal=%.1f,%.1f\n",
		ctx.frame, label, pos.x, pos.y, pos.z, cellType, uw ? 1 : 0, dz,
		containCount, containedBy, members, landMembers, waterMembers,
		ai ? static_cast<Int>(ai->getCurrentStateID()) : -1,
		goal ? goal->x : 0.0f, goal ? goal->y : 0.0f);
	std::fflush(stderr);
}

void handleShipUnload(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;
	const std::string_view action(ctx.action);
	const char* label = ctx.argument && ctx.argument[0] ? ctx.argument : "-";

	if (action == "ship-unload-probe")
	{
		logProbe(ctx, source, label);
		return;
	}

	if (action == "ship-unload-enter" && ctx.target)
	{
		selectSingle(source);
		GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_ENTER);
		msg->appendObjectIDArgument(source->getID());
		msg->appendObjectIDArgument(ctx.target->getID());
		return;
	}

	if (action == "ship-unload-evacuate")
	{
		selectSingle(source);
		TheMessageStream->appendMessage(GameMessage::MSG_EVACUATE);
		const ContainModuleInterface* contain = source->getContain();
		const Int count = contain ? static_cast<Int>(contain->getContainCount()) : -1;
		std::fprintf(stderr, "TEST ship-unload-cmd frame=%u cmd=evacuate label=%s contain=%d\n",
			ctx.frame, label, count);
		std::fflush(stderr);
		return;
	}

	if (action == "ship-unload-exit" && ctx.target)
	{
		const CommandSet* set = TheRetailCommandCatalog->findCommandSet(source->getCommandSetString());
		const CommandButton* button = set ? set->getCommandButton(0) : nullptr;
		if (!button || button->getCommandType() != GUI_COMMAND_EXIT_CONTAINER)
			return;
		selectSingle(source);
		GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_EXIT);
		msg->appendObjectIDArgument(ctx.target->getID());
		const ContainModuleInterface* contain = source->getContain();
		const Int count = contain ? static_cast<Int>(contain->getContainCount()) : -1;
		std::fprintf(stderr, "TEST ship-unload-cmd frame=%u cmd=exit button=%s contain=%d\n",
			ctx.frame, button->getName().str(), count);
		std::fflush(stderr);
		return;
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailShipUnloadTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("ship-unload-", handleShipUnload);
#endif
}
}
