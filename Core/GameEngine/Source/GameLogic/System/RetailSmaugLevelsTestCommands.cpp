#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/ActionManager.h"
#include "Common/BuildAssistant.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

#include <cstdio>
#include <string>

namespace
{
void build(const RetailTestCommands::Context& ctx)
{
	const ThingTemplate* type = TheThingFactory->findTemplate(ctx.targetName, FALSE);
	if (type && ctx.player && ctx.hasPosition)
	{
		const Coord3D point = ctx.groundLocation();
		Object* object = TheBuildAssistant->buildObjectNow(nullptr, type, &point, 0.0f, ctx.player);
		if (object && object->getAI())
			object->getAI()->setAttitude(ATTITUDE_PASSIVE);
	}
}

void power(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject(true);
	if (!source)
		return;
	const std::string args(ctx.argument);
	const auto separator = args.find(',');
	const std::string name = args.substr(0, separator);
	Object* target = separator == std::string::npos ? nullptr : ctx.findTarget(args.substr(separator + 1), true);
	const CommandButton* button = TheRetailCommandCatalog->findCommandButton(name.c_str());
	const SpecialPowerTemplate* ability = button ? button->getSpecialPowerTemplate() : nullptr;
	const SpecialPowerModuleInterface* module = ability ? source->getSpecialPowerModule(ability) : nullptr;
	const UpgradeTemplate* level3 = TheUpgradeCenter->findUpgrade("Upgrade_ObjectLevel3");
	const UpgradeTemplate* level6 = TheUpgradeCenter->findUpgrade("Upgrade_ObjectLevel6");
	const Bool objectTarget = button && BitIsSet(button->getOptions(), COMMAND_OPTION_NEED_OBJECT_TARGET);
	Coord3D point = ctx.hasPosition ? ctx.groundLocation() : target ? *target->getPosition() : *source->getPosition();
	if (!objectTarget && ability)
	{
		Region3D bounds;
		TheTerrainLogic->getExtent(&bounds);
		Bool found = FALSE;
		for (Real y = bounds.lo.y; y < bounds.hi.y && !found; y += 100.0f)
			for (Real x = bounds.lo.x; x < bounds.hi.x; x += 100.0f)
			{
				point.set(x, y, TheTerrainLogic->getGroundHeight(x, y));
				if (ability->isValidRetailTarget(source, nullptr, &point, CMD_FROM_PLAYER))
				{
					found = TRUE;
					break;
				}
			}
	}
	const Bool valid = ability && (objectTarget && target
		? TheActionManager->canDoSpecialPowerAtObject(source, target, CMD_FROM_PLAYER, ability, button->getOptions())
		: TheActionManager->canDoSpecialPowerAtLocation(source, &point, CMD_FROM_PLAYER, ability, nullptr, button->getOptions()));
	std::fprintf(stderr, "SMAUG-DETAIL frame=%u target=%d has=%d percent=%.2f allowed=%d disabled=%d\n",
		ctx.frame, target != nullptr, ability && source->hasSpecialPower(ability->getSpecialPowerType()),
		module ? module->getPercentReady() : 0.0f, module && module->isRetailCastAllowed(button->getOptions()), source->isDisabled());
	Region3D map;
	TheTerrainLogic->getExtent(&map);
	std::fprintf(stderr, "SMAUG-TARGET frame=%u map=%d mount=%d model=%d container=%d target=%d water=%d\n",
		ctx.frame, map.containsWithin(point.asCoord2D()), button && button->isMountStateAllowed(source),
		button && button->isModelConditionAllowed(source), button && button->isValidContainerForCommand(source),
		ability && ability->isValidRetailTarget(source, objectTarget ? target : nullptr, objectTarget ? nullptr : &point, CMD_FROM_PLAYER),
		TheTerrainLogic->isUnderwater(point.x, point.y));
	std::fprintf(stderr, "SMAUG-LEVELS frame=%u button=%s level3=%d level6=%d ready=%d valid=%d\n",
		ctx.frame, name.c_str(), level3 && source->hasUpgrade(level3), level6 && source->hasUpgrade(level6),
		module && module->isReady(), valid);
	if (ctx.action != "smaug-levels-cast" || !ability || !valid)
		return;
	ctx.selectObject(source);
	GameMessage* message = TheMessageStream->appendMessage(objectTarget ? GameMessage::MSG_DO_SPECIAL_POWER_AT_OBJECT : GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION);
	message->appendIntegerArgument(ability->getID());
	if (objectTarget)
		message->appendObjectIDArgument(target->getID());
	else
	{
		message->appendLocationArgument(point);
		message->appendRealArgument(INVALID_ANGLE);
		message->appendObjectIDArgument(INVALID_ID);
	}
	message->appendIntegerArgument(button->getOptions());
	message->appendObjectIDArgument(INVALID_ID);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailSmaugLevelsTestCommands()
{
#if !RTS_GENERALS
	registerAction("smaug-levels-build", build);
	registerAction("smaug-levels-probe", power);
	registerAction("smaug-levels-cast", power);
#endif
}
}
