#pragma once

#include "Common/HeroRules.h"

class CommandButton;
class CommandSet;
class Object;
class Player;
class ThingTemplate;
class Xfer;

namespace HeroRoster
{
void reset(Int playerIndex);
void resetAll();

Int count(const Player *player);
const ThingTemplate *heroAt(const Player *player, Int index);
Int findIndex(const Player *player, const ThingTemplate *hero);
HeroRules::State stateAt(const Player *player, Int index);
Int savedLevelAt(const Player *player, Int index);

Bool isAlive(const Player *player, const ThingTemplate *hero);
Bool isQueued(const Player *player, const ThingTemplate *hero);
HeroRules::Price price(const Player *player, const ThingTemplate *hero);
HeroRules::Price revivePrice(const ThingTemplate *hero, Int level);

void recordDeath(Object *hero, HeroRules::State outcome);
Bool onHeroProduced(Object *hero);
void xferPlayer(const Player *player, Xfer *xfer);

Int reviveSlotForEntry(const CommandSet *commandSet, Int entry);
Bool buttonUpgradesMet(const CommandButton *button, const Player *player, const Object *producer);
Bool isReviveTemplateInSet(const CommandSet *commandSet, const Player *player, const Object *producer, const ThingTemplate *hero);
HeroRules::Slot slotFor(const Player *player, const Object *producer, const CommandButton *button, Int heroIndex);
}
