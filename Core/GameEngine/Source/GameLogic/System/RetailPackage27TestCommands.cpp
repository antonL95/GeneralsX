#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/MessageStream.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/RetailGate.h"
#include "GameLogic/Module/RetailSiegeDocking.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

namespace
{
void reportSiegeState(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	if (!object)
		return;
	const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(ctx.argument);
	const auto* module = power ? object->getSpecialPowerModule(power) : nullptr;
	const auto* contain = object->getContain();
	const Coord3D& point = *object->getPosition();
	const auto& conditions = object->getLogicModelConditions();
	const auto* siege = SiegeAIUpdate::find(object);
	const auto* ai = object->getAI();
	const Coord3D goal = siege ? siege->getDockPosition() : Coord3D{};
	if (siege && siege->isDocked())
	{
		const auto* portal = DynamicPortalBehaviour::find(object);
		Coord3D landing{};
		const Bool hasBone = portal && portal->getAboveWallPosition(landing);
		const Coord3D authored = landing;
		const Bool legal = siege->getReleasePosition(landing);
		std::fprintf(stderr, "TEST pkg27 landing frame=%u bone=%d raw=%.2f,%.2f,%.2f legal=%d pos=%.2f,%.2f,%.2f\n",
			ctx.frame, hasBone, authored.x, authored.y, authored.z, legal, landing.x, landing.y, landing.z);
	}
	std::fprintf(stderr, "TEST pkg27 frame=%u object=%s occupants=%d deployed=%d packing=%d unpacking=%d power=%d ready=%d layer=%d pos=%.2f,%.2f,%.2f facing=%.4f approaching=%d idle=%d moving=%d siege=%d cooldown=%d accepted=%d goal=%.2f,%.2f,%.2f wall=%d\n",
		ctx.frame, object->getTemplate()->getName().str(), contain ? static_cast<Int>(contain->getContainCount()) : -1,
		conditions.test(MODELCONDITION_DEPLOYED), conditions.test(MODELCONDITION_PACKING), conditions.test(MODELCONDITION_UNPACKING),
		module != nullptr, module && module->isReady() && module->isRetailCastAllowed(),
		object->getLayer(), point.x, point.y, point.z, object->getOrientation(),
		siege && siege->isApproaching(), ai && ai->isIdle(), ai && ai->isMoving(), siege != nullptr,
		module && module->isReady(), module && module->wasLastCastAccepted(),
		goal.x, goal.y, goal.z, object->getLayer() == LAYER_WALL);
}

void issueOrder(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	if (!object || !ctx.target)
		return;
	const std::string_view action(ctx.action);
	if (action == "pkg27-enter")
	{
		auto* selection = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
		selection->appendBooleanArgument(TRUE);
		selection->appendObjectIDArgument(object->getID());
		auto* message = TheMessageStream->appendMessage(GameMessage::MSG_ENTER);
		message->appendObjectIDArgument(object->getID());
		message->appendObjectIDArgument(ctx.target->getID());
	}
	else if (action == "pkg27-script-horde" || action == "pkg27-script-man")
	{
		const SpecialPowerType type = action == "pkg27-script-horde" ? SPECIAL_SIEGEDEPLOY : SPECIAL_MAN_THE_WALLS;
		if (auto* power = object->findSpecialPowerModuleInterface(type))
			power->doSpecialPowerAtObject(ctx.target, COMMAND_FIRED_BY_SCRIPT);
	}
	else if (auto* ai = object->getAI())
	{
		const CommandSourceType source = action == "pkg27-ai-dock" ? CMD_FROM_AI : CMD_FROM_SCRIPT;
		ai->aiAttackObject(ctx.target, NO_MAX_SHOTS_LIMIT, source);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage27TestCommands()
{
#if !RTS_GENERALS
	registerAction("pkg27-state", reportSiegeState);
	registerAction("pkg27-enter", issueOrder);
	registerAction("pkg27-ai-dock", issueOrder);
	registerAction("pkg27-script-dock", issueOrder);
	registerAction("pkg27-script-horde", issueOrder);
	registerAction("pkg27-script-man", issueOrder);
#endif
}
}
