#include "PreRTS.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/GameText.h"
#include "GameClient/Mouse.h"
#include "GameClient/RetailHudExtras.h"
#include "GameClient/RetailHudSettings.h"
#include "GameLogic/ExperienceLevelStore.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"

namespace
{
Bool hasHelpBox(const Object &object)
{
	if (object.isKindOf(KINDOF_LINKED_TO_FLAG))
		return TRUE;
	return object.isLocallyControlled() && (object.isKindOf(KINDOF_STRUCTURE) || object.isKindOf(KINDOF_INFANTRY)
		|| object.isKindOf(KINDOF_CAVALRY) || object.isKindOf(KINDOF_MONSTER) || object.isKindOf(KINDOF_MACHINE));
}

Bool showsCombatStats(const Object &object)
{
	return !object.isKindOf(KINDOF_HORDE) && !object.isKindOf(KINDOF_UNATTACKABLE) && !object.isKindOf(KINDOF_ROCK_VENDOR)
		&& !object.isKindOf(KINDOF_BASE_FOUNDATION)
		&& (!object.isKindOf(KINDOF_DEFENSIVE_WALL) || object.isKindOf(KINDOF_HAS_HEALTH_BAR));
}

void addLine(UnicodeString &body, const UnicodeString &line)
{
	if (line.isEmpty())
		return;
	if (!body.isEmpty())
		body.concat(L"\n");
	body.concat(line);
}

void addValue(UnicodeString &body, const char *label, const WideChar *fallback, Int value)
{
	UnicodeString line;
	line.format(TheGameText->fetchOrSubstitute(label, fallback), value);
	addLine(body, line);
}

Int modifiedDamage(const Object &object, Int authored)
{
	const AttributeModifierHelper *helper = object.getAttributeModifierHelper();
	if (!helper)
		return authored;
	const AttributeModifierRules::Totals &totals = helper->getTotals();
	const Int added = authored + static_cast<Int>(std::floor(totals.value[AttributeModifierRules::DAMAGE_ADD] + 0.5f));
	const Real factor = totals.value[AttributeModifierRules::DAMAGE_MULT];
	return added > 0 ? static_cast<Int>(std::floor(added * factor + 0.5f)) : added;
}

void addRank(UnicodeString &body, const Object &object)
{
	const ExperienceTracker *tracker = object.getExperienceTracker();
	const Int rank = tracker ? tracker->getRank() : 0;
	const Int maximum = rank > 0 ? ExperienceLevelStore::maximumRank(&object) : 0;
	if (maximum <= 1)
		return;
	UnicodeString line;
	line.format(TheGameText->fetchOrSubstitute("TOOLTIP:Rank", L"Level: %d / Maximum Level: %d"), rank, maximum);
	addLine(body, line);
}

void addCombatStats(UnicodeString &body, const Object &object)
{
	const BodyModuleInterface *health = object.getBodyModule();
	if (health && health->getMaxHealth() > 0.0f)
		addValue(body, "TOOLTIP:MaxHealth", L"Health: %d", static_cast<Int>(health->getMaxHealth()));
	const ThingTemplate::RotWKObjectData &data = object.getTemplate()->getRotWKObjectData();
	if (data.displayMeleeDamage >= 0)
		addValue(body, "TOOLTIP:MeleeDamage", L"Melee damage: %d", modifiedDamage(object, data.displayMeleeDamage));
	if (data.displayRangedDamage >= 0)
		addValue(body, "TOOLTIP:RangedDamage", L"Ranged damage: %d", modifiedDamage(object, data.displayRangedDamage));
}

void addUpgrades(UnicodeString &body, const Object &object)
{
	std::vector<const UpgradeTemplate *> upgrades;
	for (UpgradeTemplate *upgrade = TheUpgradeCenter->firstUpgradeTemplate(); upgrade; upgrade = upgrade->friend_getNext())
		if (!upgrade->getTooltipLabel().isEmpty() && TEST_UPGRADE_MASK_ANY(object.getObjectCompletedUpgradeMask(), upgrade->getUpgradeMask()))
			upgrades.push_back(upgrade);
	std::reverse(upgrades.begin(), upgrades.end());
	for (const UpgradeTemplate *upgrade : upgrades)
		addLine(body, TheGameText->fetch(upgrade->getTooltipLabel()));
}
}

namespace RetailWorldHover
{
Bool showHelpBox(const Object &object, const UnicodeString &name, const AsciiString &descriptionLabel)
{
	if (!hasHelpBox(object))
		return FALSE;
	Mouse::RetailTooltip tooltip;
	tooltip.title = name;
	addRank(tooltip.body, object);
	if (showsCombatStats(object))
		addCombatStats(tooltip.body, object);
	addUpgrades(tooltip.body, object);
	const AsciiString &description = descriptionLabel.isEmpty() ? object.getTemplate()->getRotWKObjectData().description : descriptionLabel;
	if (!description.isEmpty())
		addLine(tooltip.body, TheGameText->fetch(description));
	const Real seconds = TheRetailHudSettings ? TheRetailHudSettings->unitHelpTextDelay : RetailHudSettings().unitHelpTextDelay;
	TheMouse->setRetailTooltip(tooltip, REAL_TO_INT(seconds * 1000.0f));
	return TRUE;
}
}
