#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string>
#include <string_view>

#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/RetailTransportTeleportModules.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"

void runRetailPackage24TestCommand(std::string_view action, Object* source, Object* target, const char* argument, UnsignedInt frame)
{
	if (!source)
		return;

	if (action == "pkg24-cast")
	{
		if (!TheSpecialPowerStore || !argument || !*argument)
			return;
		std::string_view spec(argument);
		const size_t at = spec.find('@');
		const size_t gt = spec.find('>');
		const size_t sep = (at != std::string_view::npos) ? at : gt;
		std::string powerName(sep == std::string_view::npos ? spec : spec.substr(0, sep));
		const SpecialPowerTemplate* power = TheSpecialPowerStore->findSpecialPowerTemplate(powerName.c_str());
		if (!power)
			return;
		const CommandButton* btn = TheRetailCommandCatalog ? TheRetailCommandCatalog->findPowerButton(source, power) : nullptr;
		const UnsignedInt options = btn ? btn->getOptions() : 0;
		if (at != std::string_view::npos)
		{
			Coord3D pos = *source->getPosition();
			float x = 0.0f;
			float y = 0.0f;
			if (std::sscanf(argument + at + 1, "%f,%f", &x, &y) == 2)
			{
				pos.x = x;
				pos.y = y;
			}
			GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_LOCATION);
			msg->appendIntegerArgument(power->getID());
			msg->appendLocationArgument(pos);
			msg->appendRealArgument(INVALID_ANGLE);
			msg->appendObjectIDArgument(INVALID_ID);
			msg->appendIntegerArgument(options);
			msg->appendObjectIDArgument(source->getID());
		}
		else if (gt != std::string_view::npos || target)
		{
			Object* resolvedTarget = target;
			if (!resolvedTarget && gt != std::string_view::npos && ThePlayerList)
			{
				char playerName[64] = {};
				char tplName[128] = {};
				Int ordinal = 0;
				if (std::sscanf(argument + gt + 1, "%63[^:]:%127[^:]:%d", playerName, tplName, &ordinal) == 3)
				{
					const NameKeyType playerKey = TheNameKeyGenerator->nameToKey(playerName);
					Int seen = 0;
					for (Object* obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
					{
						if (obj->isEffectivelyDead() || !obj->getControllingPlayer())
							continue;
						if (obj->getControllingPlayer()->getPlayerNameKey() == playerKey &&
							obj->getTemplate()->getName().compareNoCase(tplName) == 0)
						{
							if (seen == ordinal)
							{
								resolvedTarget = obj;
								break;
							}
							++seen;
						}
					}
				}
			}
			if (!resolvedTarget)
				return;
			GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER_AT_OBJECT);
			msg->appendIntegerArgument(power->getID());
			msg->appendObjectIDArgument(resolvedTarget->getID());
			msg->appendIntegerArgument(options);
			msg->appendObjectIDArgument(source->getID());
		}
		else
		{
			GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER);
			msg->appendIntegerArgument(power->getID());
			msg->appendIntegerArgument(options);
			msg->appendObjectIDArgument(source->getID());
		}
		return;
	}

	if (action == "pkg24-state")
	{
		const SpecialPowerTemplate* power = (TheSpecialPowerStore && argument && *argument && std::string_view(argument) != "-")
			? TheSpecialPowerStore->findSpecialPowerTemplate(argument)
			: nullptr;
		const SpecialPowerModuleInterface* sp = power ? source->getSpecialPowerModule(power) : nullptr;
		const SpecialAbilityUpdate* sau = power ? source->findSpecialAbilityUpdate(power->getSpecialPowerType()) : nullptr;
		const ContainModuleInterface* contain = source->getContain();
		const StoreObjectsSpecialPower* store = StoreObjectsSpecialPower::findForCaster(source, nullptr);
		const BodyModuleInterface* body = source->getBodyModule();
		const Player* owner = source->getControllingPlayer();
		const Coord3D& pos = *source->getPosition();
		std::fprintf(stderr,
			"TEST pkg24 state frame=%u object=%u template=%s owner=%s x=%.1f y=%.1f hp=%.1f dead=%d contain=%u stored=%u power=%s ready=%d pct=%.3f active=%d\n",
			frame,
			static_cast<unsigned>(source->getID()),
			source->getTemplate()->getName().str(),
			owner ? TheNameKeyGenerator->keyToName(owner->getPlayerNameKey()).str() : "-",
			pos.x,
			pos.y,
			body ? body->getHealth() : 0.0f,
			source->isEffectivelyDead() ? 1 : 0,
			contain ? contain->getContainCount() : 0u,
			store ? static_cast<unsigned>(store->getStoredObjects().size()) : 0u,
			argument ? argument : "-",
			sp && sp->isReady() ? 1 : 0,
			sp ? sp->getPercentReady() : 0.0f,
			sau && sau->isActive() ? 1 : 0);
		return;
	}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage24TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg24-", [](const Context& ctx) {
		Object* source = ctx.requireObject();
		if (!source)
			return;
		runRetailPackage24TestCommand(ctx.action, source, ctx.target, ctx.argument, ctx.frame);
	});
#endif
}
}
