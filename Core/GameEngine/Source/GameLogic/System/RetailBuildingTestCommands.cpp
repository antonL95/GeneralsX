#include "PreRTS.h"

#include <cstdio>

#include "Common/ModelState.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameClient/HotKey.h"
#include "GameClient/InGameUI.h"
#include "GameClient/View.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/CostModifierUpgrade.h"
#include "GameLogic/Module/FoundationAIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Module/TerrainResourceBehavior.h"
#include "GameLogic/Object.h"
#include "GameLogic/RetailTestCommands.h"
#include "GameLogic/TerrainLogic.h"

#if !RTS_GENERALS
#include "GameLogic/Module/AttributeModifierHelper.h"
#endif

bool RetailTestCommands::runBuildingCommand(std::string_view action, Object* object, const char* target,
	const Coord3D* location, unsigned int frame)
{
	if (action == "bparity-finish")
	{
		object->setStatus(OBJECT_STATUS_UNDER_CONSTRUCTION);
		object->setConstructionPercent(100.0f);
		FoundationAIUpdate::advanceConstruction(*object, nullptr);
		return true;
	}
	if (action == "bparity-cancel-recruit")
	{
		auto* production = object->getProductionUpdateInterface();
		auto* money = object->getControllingPlayer()->getMoney();
		const auto* entry = production ? production->firstProduction() : nullptr;
		const auto before = money->countMoney();
		const Bool cancelled = entry && production->cancelUnitCreate(entry->getProductionID());
		std::fprintf(stderr, "BPARITY refund %u cancelled=%d before=%u after=%u\n", frame,
			cancelled, before, money->countMoney());
		return true;
	}
	if (action == "bparity-upgrade")
	{
		const auto* upgrade = TheUpgradeCenter->findUpgrade(target);
		std::fprintf(stderr, "BPARITY upgrade %u %s %s owned=%d\n", frame,
			object->getTemplate()->getName().str(), target, upgrade && object->hasUpgrade(upgrade));
		return true;
	}
	if (action == "bparity-horde-casualty")
	{
		if (const auto* horde = HordeContain::get(object))
		{
			Object* casualty = nullptr;
			for (Object* member : *horde->getContainedItemsList())
				if (!member->isEffectivelyDead() && !member->isKindOf(KINDOF_BANNER))
				{
					casualty = member;
					break;
				}
			if (casualty)
				casualty->kill();
		}
		return true;
	}
	if (action == "bparity-focus")
	{
		if (TheTacticalView)
			TheTacticalView->lookAt(object->getPosition());
		return true;
	}
	if (action == "bparity-preview")
	{
		const ThingTemplate* type = TheThingFactory->findTemplate(target, FALSE);
		if (type && location && TheInGameUI && TheTacticalView && object->getDrawable())
		{
			Coord3D point = *location;
			point.z = TheTerrainLogic->getGroundHeight(point.x, point.y);
			TheTacticalView->lookAt(&point);
			TheInGameUI->placeBuildAvailable(type, object->getDrawable());
			ICoord2D pixel;
			TheTacticalView->worldToScreen(&point, &pixel);
			TheInGameUI->setPlacementStart(&pixel);
			TheInGameUI->setPlacementEnd(&pixel);
		}
		return true;
	}
	if (action == "bparity-commands")
	{
		const CommandSet* set = TheControlBar ? TheControlBar->findCommandSet(object->getCommandSetString()) : nullptr;
		if (set)
		{
			for (Int slot = 0; slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
			{
				const CommandButton* button = set->getCommandButton(slot);
				if (!button)
					continue;
				const AsciiString key = TheHotKeyManager ? TheHotKeyManager->searchHotKey(button->getTextLabel()) : AsciiString();
				std::fprintf(stderr, "BPARITY command %u %s slot=%d button=%s key=%s radial=%d palantir=%d\n", frame,
					object->getTemplate()->getName().str(), slot + 1, button->getName().str(),
					key.isEmpty() ? "-" : key.str(), button->isRadial(), button->isInPalantir());
			}
		}
		return true;
	}
	if (action != "bparity-state")
		return false;

	const Player* player = object->getControllingPlayer();
	if (!player)
		return true;
	const ThingTemplate* type = *target ? TheThingFactory->findTemplate(target, FALSE) : object->getTemplate();
	if (!type)
		return true;
	AsciiString label;
	Real radius = 0.0f;
	Real efficiency = 0.0f;
	Real discount = 0.0f;
	if (type == object->getTemplate())
	{
		TerrainResourceBehavior::getDisplayInfo(*type, *object->getPosition(), *player, object, radius, efficiency);
		CostModifierUpgrade::retailLabel(*object, label, discount);
	}
	Real armor = 0.0f;
	Real production = 1.0f;
#if !RTS_GENERALS
	armor = AttributeModifiers::value(object, AttributeModifierRules::ARMOR);
	production = AttributeModifiers::productionFactor(object);
#endif
	const auto* body = object->getBodyModule();
	const auto* drawable = object->getDrawable();
	std::fprintf(stderr, "BPARITY state %u source=%s type=%s cost=%d frames=%d construction=%.2f health=%.2f/%.2f "
		"radius=%.1f efficiency=%.3f discount=%.3f label=%s armor=%.3f production=%.3f money=%u points=%d"
		" stonework=%d ivory=%d munitions=%d\n",
		frame, object->getTemplate()->getName().str(), type->getName().str(), type->calcCostToBuild(player),
		type->calcTimeToBuild(player), object->getConstructionPercent(), body ? body->getHealth() : 0.0f,
		body ? body->getMaxHealth() : 0.0f, radius, efficiency, discount, label.isEmpty() ? "-" : label.str(),
		armor, production, player->getMoney()->countMoney(), player->getCommandPointsTotal(),
		drawable && drawable->getModelConditionFlags().test(MODELCONDITION_UPGRADE_NUMENOR_STONEWORK),
		drawable && drawable->getModelConditionFlags().test(MODELCONDITION_UPGRADE_IVORY_TOWER),
		drawable && drawable->getModelConditionFlags().test(MODELCONDITION_USER_12));
	return true;
}
