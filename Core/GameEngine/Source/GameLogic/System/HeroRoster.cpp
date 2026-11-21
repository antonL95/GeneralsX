#include "PreRTS.h"

#include "GameLogic/HeroRoster.h"

#include <vector>

#include "Common/GameCommon.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/ExperienceLevelStore.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Module/RespawnModules.h"
#include "GameLogic/Object.h"

namespace HeroRoster
{
namespace
{
struct Entry
{
	const ThingTemplate *hero = nullptr;
	HeroRules::State state = HeroRules::State::Recruitable;
	Int level = 1;
	Int startingLevel = 1;
	Real experience = 0.0f;
	UpgradeMaskType upgrades;
};

struct Roster
{
	const PlayerTemplate *source = nullptr;
	Bool built = FALSE;
	std::vector<Entry> entries;
};

Roster s_rosters[MAX_PLAYER_COUNT];

Roster *rosterOf(const Player *player)
{
	if (!player)
		return nullptr;
	const Int index = player->getPlayerIndex();
	if (index < 0 || index >= MAX_PLAYER_COUNT)
		return nullptr;
	Roster &roster = s_rosters[index];
	const PlayerTemplate *source = player->getPlayerTemplate();
	if (roster.built && roster.source == source)
		return &roster;
	roster = Roster();
	roster.built = TRUE;
	roster.source = source;
	if (source && TheThingFactory)
	{
		for (const AsciiString &name : HeroRules::rosterOrder(source->getBuildableRingHeroesMP(), source->getBuildableHeroesMP()))
		{
			Entry entry;
			entry.hero = TheThingFactory->findTemplate(name, FALSE);
			roster.entries.push_back(entry);
		}
	}
	return &roster;
}

Entry *entryOf(const Player *player, Int index)
{
	Roster *roster = rosterOf(player);
	if (!roster || index < 0 || index >= static_cast<Int>(roster->entries.size()))
		return nullptr;
	return &roster->entries[index];
}

struct Census
{
	const ThingTemplate *hero = nullptr;
	Int alive = 0;
	Int queued = 0;
};

void countHero(Object *obj, void *userData)
{
	Census &census = *static_cast<Census *>(userData);
	if (!obj->isEffectivelyDead() && !obj->isDestroyed() && census.hero->isEquivalentTo(obj->getHeroIdentityTemplate()))
		++census.alive;
	ProductionUpdateInterface *production = obj->getProductionUpdateInterface();
	if (production)
		census.queued += production->countUnitTypeInQueue(census.hero);
}

Census takeCensus(const Player *player, const ThingTemplate *hero)
{
	Census census;
	census.hero = hero;
	if (player && hero)
		player->iterateObjects(countHero, &census);
	return census;
}

const RespawnUpdateModuleData *respawnData(const ThingTemplate *hero)
{
	if (!hero)
		return nullptr;
	const ModuleInfo &modules = hero->getBehaviorModuleInfo();
	for (Int i = 0; i < modules.getCount(); ++i)
	{
		if (modules.getNthName(i).compareNoCase("RespawnUpdate") == 0)
			return static_cast<const RespawnUpdateModuleData *>(modules.getNthData(i));
	}
	return nullptr;
}

Bool countsAsRevive(const CommandButton *button)
{
	return button && button->getCommandType() == GUI_COMMAND_REVIVE && !BitIsSet(button->getOptions(), SCRIPT_ONLY);
}

void xferEntry(Xfer *xfer, Entry &entry)
{
	Int state = static_cast<Int>(entry.state);
	xfer->xferInt(&state);
	if (state < static_cast<Int>(HeroRules::State::Recruitable) || state > static_cast<Int>(HeroRules::State::Lost))
		throw XFER_INVALID_PARAMETERS;
	entry.state = static_cast<HeroRules::State>(state);
	xfer->xferInt(&entry.level);
	xfer->xferInt(&entry.startingLevel);
	xfer->xferReal(&entry.experience);
	xfer->xferUpgradeMask(&entry.upgrades);
}
}

void reset(Int playerIndex)
{
	if (playerIndex >= 0 && playerIndex < MAX_PLAYER_COUNT)
		s_rosters[playerIndex] = Roster();
}

void resetAll()
{
	for (Int i = 0; i < MAX_PLAYER_COUNT; ++i)
		reset(i);
}

void xferPlayer(const Player *player, Xfer *xfer)
{
	const Bool loading = xfer->getXferMode() == XFER_LOAD;
	if (loading)
		reset(player->getPlayerIndex());
	Roster *roster = rosterOf(player);
	UnsignedInt count = 0;
	if (!loading && roster)
		for (const Entry &entry : roster->entries)
			if (entry.hero && entry.state != HeroRules::State::Recruitable)
				++count;
	xfer->xferUnsignedInt(&count);
	if (loading)
	{
		for (UnsignedInt i = 0; i < count; ++i)
		{
			AsciiString name;
			Entry loaded;
			xfer->xferAsciiString(&name);
			xferEntry(xfer, loaded);
			loaded.hero = TheThingFactory->findTemplate(name, FALSE);
			if (Entry *entry = entryOf(player, findIndex(player, loaded.hero)))
			{
				loaded.hero = entry->hero;
				*entry = loaded;
			}
		}
	}
	else if (roster)
	{
		for (Entry &entry : roster->entries)
		{
			if (!entry.hero || entry.state == HeroRules::State::Recruitable)
				continue;
			AsciiString name = entry.hero->getName();
			xfer->xferAsciiString(&name);
			xferEntry(xfer, entry);
		}
	}
}

Int count(const Player *player)
{
	const Roster *roster = rosterOf(player);
	return roster ? static_cast<Int>(roster->entries.size()) : 0;
}

const ThingTemplate *heroAt(const Player *player, Int index)
{
	const Entry *entry = entryOf(player, index);
	return entry ? entry->hero : nullptr;
}

Int findIndex(const Player *player, const ThingTemplate *hero)
{
	const Roster *roster = rosterOf(player);
	if (!roster || !hero)
		return -1;
	for (std::size_t i = 0; i < roster->entries.size(); ++i)
	{
		if (roster->entries[i].hero && roster->entries[i].hero->isEquivalentTo(hero))
			return static_cast<Int>(i);
	}
	return -1;
}

HeroRules::State stateAt(const Player *player, Int index)
{
	const Entry *entry = entryOf(player, index);
	return entry ? entry->state : HeroRules::State::Recruitable;
}

Int savedLevelAt(const Player *player, Int index)
{
	const Entry *entry = entryOf(player, index);
	return entry ? entry->level : 1;
}

Bool isAlive(const Player *player, const ThingTemplate *hero)
{
	return takeCensus(player, hero).alive > 0;
}

Bool isQueued(const Player *player, const ThingTemplate *hero)
{
	return takeCensus(player, hero).queued > 0;
}

HeroRules::Price revivePrice(const ThingTemplate *hero, Int level)
{
	const RespawnUpdateModuleData *data = respawnData(hero);
	if (!data)
		return HeroRules::Price();
	return HeroRules::revivePrice(data->m_rules, data->m_entries, level);
}

HeroRules::Price price(const Player *player, const ThingTemplate *hero)
{
	HeroRules::Price result;
	if (!hero)
		return result;
	const Entry *entry = entryOf(player, findIndex(player, hero));
	if (entry && entry->state == HeroRules::State::Revivable && respawnData(hero))
		return revivePrice(hero, entry->level);
	result.cost = hero->calcCostToBuild(player);
	result.frames = static_cast<UnsignedInt>(hero->calcTimeToBuild(player));
	return result;
}

void recordDeath(Object *hero, HeroRules::State outcome)
{
	if (!hero)
		return;
	const Player *player = hero->getControllingPlayer();
	Entry *entry = entryOf(player, findIndex(player, hero->getHeroIdentityTemplate()));
	if (!entry)
		return;
	entry->state = outcome;
	if (outcome != HeroRules::State::Revivable)
		return;
	const ExperienceTracker *tracker = hero->getExperienceTracker();
	entry->level = tracker ? tracker->getRank() : 1;
	entry->startingLevel = tracker ? tracker->getRetailStartingRank() : 1;
	entry->experience = tracker ? tracker->getRetailExperience() : 0.0f;
	entry->upgrades = hero->getObjectCompletedUpgradeMask();
}

Bool onHeroProduced(Object *hero)
{
	if (!hero)
		return FALSE;
	const Player *player = hero->getControllingPlayer();
	Entry *entry = entryOf(player, findIndex(player, hero->getTemplate()));
	if (!entry || entry->state != HeroRules::State::Revivable)
		return FALSE;
	entry->state = HeroRules::State::Recruitable;

	ExperienceLevelStore::applyPendingLevels(hero);
	ExperienceTracker *tracker = hero->getExperienceTracker();
	if (tracker)
	{
		tracker->advanceRetailLevel(entry->experience, FALSE);
		tracker->setMinRetailRank(entry->level);
		tracker->setRetailStartingRank(entry->startingLevel);
	}
	if (TheUpgradeCenter)
	{
		for (const UpgradeTemplate *upgrade = TheUpgradeCenter->firstUpgradeTemplate(); upgrade; upgrade = upgrade->friend_getNext())
		{
			if (upgrade->getUpgradeType() == UPGRADE_TYPE_OBJECT && entry->upgrades.testForAny(upgrade->getUpgradeMask())
				&& !hero->hasUpgrade(upgrade))
				hero->giveUpgrade(upgrade);
		}
	}

	const RespawnUpdateModuleData *data = respawnData(hero->getTemplate());
	UpdateModule *update = hero->findUpdateModule(TheNameKeyGenerator->nameToKey("RespawnUpdate"));
	if (data && update)
		static_cast<RespawnUpdate *>(update)->markRevived(data->m_respawnHealthPercent);
	return TRUE;
}

Int reviveSlotForEntry(const CommandSet *commandSet, Int entry)
{
	if (!commandSet || entry < 0 || entry >= CommandButtonRules::RETAIL_COMMANDS_PER_SET)
		return -1;
	std::vector<bool> counted;
	for (Int i = 0; i <= entry; ++i)
		counted.push_back(countsAsRevive(commandSet->getCommandButton(i)));
	return HeroRules::reviveSlots(counted)[entry];
}

Bool buttonUpgradesMet(const CommandButton *button, const Player *player, const Object *producer)
{
	if (!button || !player)
		return FALSE;
	std::vector<bool> owned;
	for (const UpgradeTemplate *upgrade : button->getNeededUpgrades())
	{
		if (upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER)
			owned.push_back(player->hasUpgradeComplete(upgrade));
		else
			owned.push_back(producer && producer->hasUpgrade(upgrade));
	}
	return HeroRules::neededUpgradesMet(owned, button->isNeededUpgradeAny());
}

Bool isReviveTemplateInSet(const CommandSet *commandSet, const Player *player, const Object *producer, const ThingTemplate *hero)
{
	if (!commandSet || !hero)
		return FALSE;
	Int slot = 0;
	for (Int i = 0; i < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++i)
	{
		const CommandButton *button = commandSet->getCommandButton(i);
		if (!countsAsRevive(button))
			continue;
		const ThingTemplate *candidate = heroAt(player, slot++);
		if (!candidate || !candidate->isEquivalentTo(hero))
			continue;
		if (!BitIsSet(button->getOptions(), NEED_UPGRADE) || buttonUpgradesMet(button, player, producer))
			return TRUE;
	}
	return FALSE;
}

HeroRules::Slot slotFor(const Player *player, const Object *producer, const CommandButton *button, Int heroIndex)
{
	const ThingTemplate *hero = heroAt(player, heroIndex);
	const Bool upgradesMet = !BitIsSet(button->getOptions(), NEED_UPGRADE) || buttonUpgradesMet(button, player, producer);
	const Census census = takeCensus(player, hero);
	return HeroRules::slotFor(hero != nullptr, upgradesMet, stateAt(player, heroIndex), census.alive > 0, census.queued > 0);
}
}
