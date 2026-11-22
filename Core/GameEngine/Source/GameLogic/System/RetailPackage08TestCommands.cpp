#include "PreRTS.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/XferCRC.h"
#include "GameClient/GameClient.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/RetailConstruction.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/Weapon.h"

namespace
{
UnsignedInt objectCRC(Object* object)
{
	XferCRC xfer;
	xfer.open("Package08");
	xfer.xferSnapshot(object);
	const UnsignedInt checksum = xfer.getCRC();
	xfer.close();
	return checksum;
}
}

void runRetailPackage08TestCommand(std::string_view action, Object* source, Object* target, const char* name,
	const Coord3D* position, UnsignedInt frame)
{
	if (action == "pkg08-rebuild" && position)
	{
		const ThingTemplate* building = TheThingFactory->findTemplate(name, FALSE);
		auto* dozer = source->getAI() ? source->getAI()->getDozerAIInterface() : nullptr;
		if (building && dozer)
			dozer->construct(building, position, 0.0f, source->getControllingPlayer(), TRUE);
	}
	else if (action == "pkg08-rebuild-state")
	{
		auto* dozer = source->getAI() ? source->getAI()->getDozerAIInterface() : nullptr;
		const Object* site = dozer ? TheGameLogic->findObjectByID(dozer->getTaskTarget(DOZER_TASK_BUILD)) : nullptr;
		std::fprintf(stderr, "TEST pkg08 rebuild frame=%u active=%d reconstructing=%d\n", frame,
			dozer && dozer->getIsRebuild(), site && site->testStatus(OBJECT_STATUS_RECONSTRUCTING));
	}
	else if (action == "pkg08-queue" && position)
	{
		const ThingTemplate* building = TheThingFactory->findTemplate(name, FALSE);
		if (!building)
			return;
		GameMessage* selection = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
		selection->appendBooleanArgument(TRUE);
		selection->appendObjectIDArgument(source->getID());
		GameMessage* order = TheMessageStream->appendMessage(GameMessage::MSG_DOZER_CONSTRUCT);
		order->appendIntegerArgument(building->getTemplateID());
		order->appendLocationArgument(*position);
		order->appendRealArgument(0.0f);
		order->appendBooleanArgument(TRUE);
	}
	else if (action == "pkg08-queue-state")
	{
		auto* dozer = source->getAI() ? source->getAI()->getDozerAIInterface() : nullptr;
		const ObjectID target = dozer ? dozer->getTaskTarget(DOZER_TASK_BUILD) : INVALID_ID;
		Object* active = TheGameLogic->findObjectByID(target);
		std::fprintf(stderr, "TEST pkg08 builder frame=%u target=%s money=%u\n", frame,
			active ? active->getTemplate()->getName().str() : "-",
			source->getControllingPlayer()->getMoney()->countMoney());
		for (Object* site = TheGameLogic->getFirstObject(); site; site = site->getNextObject())
		{
			if (site->getProducerID() != source->getID() || !site->isKindOf(KINDOF_STRUCTURE))
				continue;
			std::fprintf(stderr, "TEST pkg08 site frame=%u name=%s x=%.0f progress=%.1f paid=%d builder=%d health=%.1f\n",
				frame, site->getTemplate()->getName().str(), site->getPosition()->x,
				site->getConstructionPercent(), site->getConstructionCostPaid(), static_cast<int>(site->getBuilderID()),
				site->getBodyModule()->getHealth());
		}
	}
	else if (action == "pkg08-contact" && target)
	{
		Coord3D result;
		const Bool found = DozerAIUpdate::findGoodBuildOrRepairPosition(source, target, result);
		std::fprintf(stderr, "TEST pkg08 contact frame=%u object=%s found=%d x=%.2f y=%.2f\n",
			frame, source->getTemplate()->getName().str(), found, result.x, result.y);
	}
	else if (action == "pkg08-mount")
	{
		if (std::string_view(name) == "on")
			source->setModelConditionState(MODELCONDITION_RIDER8);
		else
			source->clearModelConditionState(MODELCONDITION_RIDER8);
	}
	else if (action == "pkg08-rider-state")
	{
		const auto* mounted = TheUpgradeCenter->findUpgrade("Upgrade_MountedBodyGuard");
		const auto* dismounted = TheUpgradeCenter->findUpgrade("Upgrade_DismountedBodyGuard");
		std::fprintf(stderr, "TEST pkg08 rider frame=%u mounted=%d dismounted=%d\n",
			frame, mounted && source->hasUpgrade(mounted), dismounted && source->hasUpgrade(dismounted));
	}
	else if (action == "pkg08-base-state")
	{
		Int placed = 0;
		for (Object* building = TheGameLogic->getFirstObject(); building; building = building->getNextObject())
		{
			if (building->getProducerID() != source->getID())
				continue;
			++placed;
			std::fprintf(stderr, "TEST pkg08 placement frame=%u name=%s x=%.2f y=%.2f same-owner=%d\n",
				frame, building->getTemplate()->getName().str(), building->getPosition()->x, building->getPosition()->y,
				building->getControllingPlayer() == source->getControllingPlayer());
		}
		std::fprintf(stderr, "TEST pkg08 base frame=%u placed=%d\n", frame, placed);
	}
	else if (action == "pkg08-night")
	{
		const TimeOfDay time = std::string_view(name) == "on" ? TIME_OF_DAY_NIGHT : TIME_OF_DAY_AFTERNOON;
		if (TheWritableGlobalData->setTimeOfDay(time) && TheGameClient)
			TheGameClient->setTimeOfDay(time);
	}
	else if (action == "pkg08-flame")
		source->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_AFLAME), std::string_view(name) == "on");
	else if (action == "pkg08-windows")
	{
		const auto* behavior = static_cast<const BuildingBehavior*>(source->findUpdateModule(NAMEKEY("BuildingBehavior")));
		std::fprintf(stderr, "TEST pkg08 windows frame=%u state=%d\n", frame, behavior ? behavior->getWindowState() : -1);
		if (behavior)
		{
			std::fprintf(stderr, "TEST pkg08 night-meshes frame=%u count=%zu\n", frame, behavior->getNightWindowCount());
			std::fprintf(stderr, "TEST pkg08 fire-meshes frame=%u count=%zu\n", frame, behavior->getFireMeshCount());
		}
	}
	else if (action == "pkg08-enter" && target && source->getAI())
		source->getAI()->aiEnter(target, CMD_FROM_PLAYER);
	else if (action == "pkg08-chase-order" && target && source->getAI())
		source->getAI()->aiAttackObject(target, NO_MAX_SHOTS_LIMIT, CMD_FROM_AI);
	else if (action == "pkg08-chase" && target && source->getAI())
		std::fprintf(stderr, "TEST pkg08 chase frame=%u allowed=%d\n",
			frame, source->getAI()->stanceAllowsChase(target));
	else if (action == "pkg08-fire-state")
		std::fprintf(stderr, "TEST pkg08 fire frame=%u contained=%d attack=%d\n",
			frame, source->getContainedBy() != nullptr, source->isAbleToAttack());
	else if (action == "pkg08-burn")
	{
		DamageInfo damage;
		damage.in.m_damageType = DAMAGE_FLAME;
		damage.in.m_deathType = DEATH_BURNED;
		damage.in.m_kill = TRUE;
		source->attemptDamage(&damage);
	}
	else if (action == "pkg08-burn-state" && source->getAI())
		std::fprintf(stderr, "TEST pkg08 burn frame=%u name=%s end=%u burning=%d dead=%d x=%.2f y=%.2f\n", frame,
			source->getTemplate()->getName().str(), source->getAI()->getRetailBurningDeathDeadline(),
			source->getLogicModelConditions().test(MODELCONDITION_BURNINGDEATH), source->isEffectivelyDead(),
			source->getPosition()->x, source->getPosition()->y);
	else if (action == "pkg08-cower" && target && source->getAI())
		source->getAI()->fleeFromFear(target, LOGICFRAMES_PER_SECOND, MODELCONDITION_EMOTION_AFRAID);
	else if (action == "pkg08-cower-state")
		std::fprintf(stderr, "TEST pkg08 cower frame=%u active=%d\n", frame,
			source->getLogicModelConditions().test(MODELCONDITION_EMOTION_AFRAID));
	else if (action == "pkg08-priority-define" && target)
	{
		auto* priority = newInstance(ScriptAction)(ScriptAction::SET_ATTACK_PRIORITY_THING);
		priority->setNextAction(nullptr);
		priority->getParameter(0)->friend_setString("AttackPriority_HERO");
		priority->getParameter(1)->friend_setString(target->getTemplate()->getName());
		priority->getParameter(2)->friend_setInt(1000);
		TheScriptEngine->friend_executeAction(priority, source->getTeam());
		deleteInstance(priority);
	}
	else if (action == "pkg08-priority" && target && source->getAI())
	{
		const AttackPriorityInfo* info = source->getAI()->getAttackInfo();
		std::fprintf(stderr, "TEST pkg08 priority frame=%u resolved=%d weight=%d\n",
			frame, info && info != TheScriptEngine->getDefaultAttackInfo(), info ? info->getPriority(target->getTemplate()) : -1);
	}
	else if (action == "pkg08-priority-default" && source->getAI())
		source->getAI()->setAttackInfo(TheScriptEngine->getDefaultAttackInfo());
	else if (action == "pkg08-queue-crc" && position)
	{
		const auto* building = TheThingFactory->findTemplate(name, FALSE);
		auto* dozer = static_cast<DozerAIUpdate*>(source->findUpdateModule(NAMEKEY("DozerAIUpdate")));
		if (!building || !dozer)
			return;
		const UnsignedInt before = objectCRC(source);
		const Object* site = dozer->queueConstruction(building, position, 0.0f, source->getControllingPlayer());
		std::fprintf(stderr, "TEST pkg08 queue-crc frame=%u placed=%d changed=%d\n",
			frame, site != nullptr, objectCRC(source) != before);
	}
}
#endif
