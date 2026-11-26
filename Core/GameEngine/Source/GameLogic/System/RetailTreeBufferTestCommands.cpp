#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cmath>
#include <cstdio>
#include <vector>

#include "Common/MapObject.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameClient/CommandXlat.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/FlammableUpdate.h"
#include "GameLogic/Module/GrabPassengerSpecialPower.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
struct TreeCandidate
{
	Coord3D pos;
	Real scale;
	Bool grabbable;
	Bool toppleable;
	Bool flammable;
};

struct ScenarioTrees
{
	Bool valid = FALSE;
	TreeCandidate fire;
	TreeCandidate crush;
	TreeCandidate grabNear;
	TreeCandidate grabFar;
};

Real dist2DSqr(const Coord3D& a, const Coord3D& b)
{
	const Real dx = a.x - b.x;
	const Real dy = a.y - b.y;
	return dx * dx + dy * dy;
}

Bool templateCanTopple(const ThingTemplate& thing)
{
	const ModuleInfo& modules = thing.getDrawModuleInfo();
	for (Int i = 0; i < modules.getCount(); ++i)
	{
		const ModuleData* module = modules.getNthData(i);
		if (module && module->canToppleMapTree())
			return TRUE;
	}
	return FALSE;
}

Bool templateHasFlammable(const ThingTemplate& thing)
{
	const ModuleInfo& modules = thing.getBehaviorModuleInfo();
	for (Int i = 0; i < modules.getCount(); ++i)
	{
		if (modules.getNthName(i) == "FlammableUpdate")
			return TRUE;
	}
	return FALSE;
}

ScenarioTrees pickScenarioTrees()
{
	std::vector<TreeCandidate> candidates;
	for (MapObject* map = MapObject::getFirstMapObject(); map; map = map->getNext())
	{
		const ThingTemplate* thing = map->getThingTemplate();
		if (!thing || !thing->isKindOf(KINDOF_TREE))
			continue;
		Coord3D pos = *map->getLocation();
		if (pos.x < 1800.0f || pos.y > 1100.0f)
			continue;
		pos.z += TheTerrainLogic->getGroundHeight(pos.x, pos.y);
		Bool hasScale = FALSE;
		const Real mapScale = map->getProperties()->getReal(NAMEKEY("objectPrototypeScale"), &hasScale);
		const Real scale = thing->getAssetScale() * (hasScale ? mapScale : 1.0f);
		if (!std::isfinite(scale) || scale <= 0.0f)
			continue;
		candidates.push_back({
			pos,
			scale,
			thing->getRotWKObjectData().isGrabbable && thing->isKindOf(KINDOF_CLUB),
			templateCanTopple(*thing),
			templateHasFlammable(*thing)
		});
	}

	ScenarioTrees result;
	Bool foundPair = FALSE;
	Real bestPairDist = 0.0f;
	for (size_t i = 0; i < candidates.size(); ++i)
	{
		if (!candidates[i].grabbable)
			continue;
		for (size_t j = i + 1; j < candidates.size(); ++j)
		{
			if (!candidates[j].grabbable)
				continue;
			const Real d2 = dist2DSqr(candidates[i].pos, candidates[j].pos);
			if (d2 < 100.0f || d2 > 2500.0f)
				continue;
			if (!foundPair || d2 < bestPairDist)
			{
				foundPair = TRUE;
				bestPairDist = d2;
				result.grabNear = candidates[i];
				result.grabFar = candidates[j];
			}
		}
	}
	if (!foundPair)
		return result;

	Bool foundCrush = FALSE;
	for (const auto& candidate : candidates)
	{
		if (!candidate.toppleable)
			continue;
		if (dist2DSqr(candidate.pos, result.grabNear.pos) < 22500.0f ||
			dist2DSqr(candidate.pos, result.grabFar.pos) < 22500.0f)
			continue;
		result.crush = candidate;
		foundCrush = TRUE;
		break;
	}
	if (!foundCrush)
		return result;

	for (const auto& candidate : candidates)
	{
		if (!candidate.flammable)
			continue;
		if (dist2DSqr(candidate.pos, result.grabNear.pos) < 22500.0f ||
			dist2DSqr(candidate.pos, result.grabFar.pos) < 22500.0f ||
			dist2DSqr(candidate.pos, result.crush.pos) < 22500.0f)
			continue;
		result.fire = candidate;
		result.valid = TRUE;
		break;
	}
	if (!result.valid)
	{
		for (MapObject* map = MapObject::getFirstMapObject(); map; map = map->getNext())
		{
			const ThingTemplate* thing = map->getThingTemplate();
			if (!thing || !thing->isKindOf(KINDOF_TREE) || !templateHasFlammable(*thing))
				continue;
			Coord3D pos = *map->getLocation();
			if (pos.x < 1800.0f)
				continue;
			pos.z += TheTerrainLogic->getGroundHeight(pos.x, pos.y);
			if (dist2DSqr(pos, result.grabNear.pos) < 22500.0f ||
				dist2DSqr(pos, result.grabFar.pos) < 22500.0f ||
				dist2DSqr(pos, result.crush.pos) < 22500.0f)
				continue;
			result.fire = {pos, 1.0f, FALSE, FALSE, TRUE};
			result.valid = TRUE;
			break;
		}
	}
	return result;
}

Bool hasMaterializedBurningTreeNear(const Coord3D& pos)
{
	for (const Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
	{
		if (!obj->isKindOf(KINDOF_TREE))
			continue;
		if (obj->getTemplate()->getName() == "TreeBufferFireSource")
			continue;
		if (dist2DSqr(*obj->getPosition(), pos) > 25.0f)
			continue;
		if (obj->testStatus(OBJECT_STATUS_AFLAME) || obj->testStatus(OBJECT_STATUS_BURNED))
			return TRUE;
	}
	return FALSE;
}

void handleTreeBuffer(const RetailTestCommands::Context& ctx)
{
	const ScenarioTrees trees = pickScenarioTrees();
	if (!trees.valid)
	{
		std::fprintf(stderr, "TEST tree-buffer invalid frame=%u\n", ctx.frame);
		return;
	}

	Player* player = ctx.findPlayer();
	if (!player)
		return;

	if (ctx.action == "tree-buffer-setup")
	{
		const ThingTemplate* crushType = TheThingFactory->findTemplate("TreeBufferCrusher", FALSE);
		const ThingTemplate* grabType = TheThingFactory->findTemplate("TreeBufferGrabber", FALSE);
		if (!crushType || !grabType)
			return;

		Coord3D crushStart{trees.crush.pos.x - 45.0f, trees.crush.pos.y, 0.0f};
		crushStart.z = TheTerrainLogic->getGroundHeight(crushStart.x, crushStart.y);
		Coord3D crushEnd{trees.crush.pos.x + 45.0f, trees.crush.pos.y, 0.0f};
		crushEnd.z = TheTerrainLogic->getGroundHeight(crushEnd.x, crushEnd.y);
		if (Object* crusher = TheThingFactory->newObject(crushType, player->getDefaultTeam()))
		{
			crusher->setPosition(&crushStart);
			if (AIUpdateInterface* ai = crusher->getAIUpdateInterface())
				ai->aiMoveToPosition(&crushEnd, CMD_FROM_PLAYER);
		}

		Coord3D grabStart{trees.grabNear.pos.x - 40.0f, trees.grabNear.pos.y, 0.0f};
		grabStart.z = TheTerrainLogic->getGroundHeight(grabStart.x, grabStart.y);
		if (Object* grabber = TheThingFactory->newObject(grabType, player->getDefaultTeam()))
		{
			grabber->setPosition(&grabStart);
		}
		return;
	}

	if (ctx.action == "tree-buffer-ignite")
	{
		const ThingTemplate* fireType = TheThingFactory->findTemplate("TreeBufferFireSource", FALSE);
		if (!fireType)
			return;
		Coord3D firePos{trees.fire.pos.x + 15.0f, trees.fire.pos.y, 0.0f};
		firePos.z = TheTerrainLogic->getGroundHeight(firePos.x, firePos.y);
		if (Object* fireObj = TheThingFactory->newObject(fireType, player->getDefaultTeam()))
		{
			fireObj->setPosition(&firePos);
			if (auto* flammable = static_cast<FlammableUpdate*>(fireObj->findUpdateModule(NAMEKEY("FlammableUpdate"))))
				flammable->tryToIgnite();
		}
		return;
	}

	if (ctx.action == "tree-buffer-grab")
	{
		Object* grabber = ctx.requireObject();
		const auto* button = TheRetailCommandCatalog->findCommandButton("Command_SpecialAbilityTrollGrabTree");
		if (!grabber || !button)
			return;

		Coord3D clickPos{
			trees.grabNear.pos.x + 0.25f * (trees.grabFar.pos.x - trees.grabNear.pos.x),
			trees.grabNear.pos.y + 0.25f * (trees.grabFar.pos.y - trees.grabNear.pos.y),
			0.0f
		};
		clickPos.z = TheTerrainLogic->getGroundHeight(clickPos.x, clickPos.y);
		ctx.selectObject(grabber);
		TheInGameUI->setGUICommand(button);
		CommandTranslator translator;
		const auto result = translator.evaluateContextCommand(nullptr, &clickPos, CommandTranslator::DO_COMMAND);
		std::fprintf(stderr, "TEST tree-buffer grab-order frame=%u sent=%d\n", ctx.frame,
			result == GameMessage::MSG_DO_SPECIAL_POWER_AT_OBJECT);
		return;
	}

	if (ctx.action == "tree-buffer-origin")
	{
		Object* probe = ctx.requireObject();
		if (!probe)
			return;
		const SpecialPowerTemplate* power = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityGrabPassenger");
		auto* ability = power ? probe->findSpecialAbilityUpdate(power->getSpecialPowerType()) : nullptr;
		Bool accepted = FALSE;
		Bool hasPos = FALSE;
		if (ability && power)
		{
			const Coord3D origin{0.0f, 0.0f, 0.0f};
			accepted = ability->initiateIntentToDoSpecialPower(power, nullptr, &origin, nullptr, 0);
			hasPos = ability->hasAbilityTargetPos();
		}
		std::fprintf(stderr, "TEST tree-buffer origin frame=%u accepted=%d has-pos=%d\n", ctx.frame, accepted, hasPos);
		return;
	}

	if (ctx.action == "tree-buffer-state")
	{
		const Bool fireStanding = TheTerrainLogic->isNearMapTree(trees.fire.pos, 1.0f);
		const Bool fireBurning = hasMaterializedBurningTreeNear(trees.fire.pos);
		const Bool crushToppled = TheTerrainLogic->isMapTreeToppled(trees.crush.pos);
		const Bool grabNearStanding = TheTerrainLogic->isNearMapTree(trees.grabNear.pos, 1.0f);
		const Bool grabFarStanding = TheTerrainLogic->isNearMapTree(trees.grabFar.pos, 1.0f);
		Object* grabber = ctx.findObject();
		const auto* contain = grabber ? grabber->getContain() : nullptr;
		const auto* items = contain ? contain->getContainedItemsList() : nullptr;
		const Object* held = items && !items->empty() ? items->front() : nullptr;
		const Bool heldTree = held && held->isKindOf(KINDOF_TREE);
		const Bool scaleMatch = heldTree && held->getDrawable() &&
			std::fabs(held->getDrawable()->getInstanceScale() - trees.grabNear.scale) < 0.01f;
		std::fprintf(stderr,
			"TEST tree-buffer state frame=%u fire-standing=%d fire-burning=%d crush-toppled=%d grab-near=%d grab-far=%d held=%d scale=%d\n",
			ctx.frame, fireStanding, fireBurning, crushToppled, grabNearStanding, grabFarStanding, heldTree, scaleMatch);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailTreeBufferTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("tree-buffer-", handleTreeBuffer);
#endif
}
}
