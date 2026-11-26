#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/RetailGate.h"
#include "GameLogic/Module/RetailSiegeDocking.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"

namespace
{
void probeHordeClimb(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	if (!object)
		return;
	const Coord3D& pos = *object->getPosition();
	const auto* ai = object->getAI();
	const auto* horde = HordeContain::get(object);
	if (horde)
	{
		Int alive = 0;
		Int onWall = 0;
		Int sameHorde = 0;
		Int held = 0;
		for (const Object* member : *horde->getContainedItemsList())
		{
			if (!member || member->isEffectivelyDead())
				continue;
			++alive;
			if (member->getLayer() == LAYER_WALL)
				++onWall;
			if (member->getContainedBy() == object)
				++sameHorde;
			if (member->isDisabledByType(DISABLED_HELD))
				++held;
		}
		const Object* enterTarget = ai ? const_cast<AIUpdateInterface*>(ai)->getEnterTarget() : nullptr;
		const Coord3D goal = ai && ai->getGoalPosition() ? *ai->getGoalPosition() : Coord3D{};
		const auto* power = object->findSpecialPowerModuleInterface(SPECIAL_SIEGEDEPLOY);
		std::fprintf(stderr,
			"TEST horde-climb frame=%u object=%s layer=%d wall=%d pos=%.2f,%.2f,%.2f state=%u idle=%d moving=%d "
			"enterTarget=%u goal=%.2f,%.2f,%.2f contained=%u accepted=%d members=%d wallMembers=%d sameHorde=%d held=%d\n",
			ctx.frame, object->getTemplate()->getName().str(),
			object->getLayer(), object->getLayer() == LAYER_WALL,
			pos.x, pos.y, pos.z,
			ai ? static_cast<unsigned>(ai->getCurrentStateID()) : 0u,
			ai && ai->isIdle(), ai && ai->isMoving(),
			enterTarget ? static_cast<unsigned>(enterTarget->getID()) : 0u,
			goal.x, goal.y, goal.z,
			object->getContainedBy() ? static_cast<unsigned>(object->getContainedBy()->getID()) : 0u,
			power && power->wasLastCastAccepted(),
			alive, onWall, sameHorde, held);
		return;
	}
	const auto* siege = SiegeAIUpdate::find(object);
	const auto* portal = DynamicPortalBehaviour::find(object);
	const auto* contain = object->getContain();
	Coord3D entry{};
	Coord3D above{};
	Coord3D release{};
	const Bool hasEntry = portal && portal->getClimbEntryPosition(entry);
	const Bool hasAbove = portal && portal->getAboveWallPosition(above);
	const Bool hasRelease = siege && siege->getReleasePosition(release);
	std::fprintf(stderr,
		"TEST horde-climb frame=%u object=%s deployed=%d occupants=%d pos=%.2f,%.2f,%.2f facing=%.4f "
		"entry=%d(%.2f,%.2f,%.2f) above=%d(%.2f,%.2f,%.2f) release=%d(%.2f,%.2f,%.2f)\n",
		ctx.frame, object->getTemplate()->getName().str(),
		siege && siege->isDocked(),
		contain ? static_cast<Int>(contain->getContainCount()) : -1,
		pos.x, pos.y, pos.z, object->getOrientation(),
		hasEntry, entry.x, entry.y, entry.z,
		hasAbove, above.x, above.y, above.z,
		hasRelease, release.x, release.y, release.z);
}

void issueHordeClimbOrder(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	if (!object || !ctx.target)
		return;
	if (auto* power = object->findSpecialPowerModuleInterface(SPECIAL_SIEGEDEPLOY))
		power->doSpecialPowerAtObject(ctx.target, COMMAND_OPTION_INTERNAL_AUTO);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailHordeClimbTestCommands()
{
#if !RTS_GENERALS
	registerAction("horde-climb-probe", probeHordeClimb);
	registerAction("horde-climb-ai-order", issueHordeClimbOrder);
#endif
}
}
