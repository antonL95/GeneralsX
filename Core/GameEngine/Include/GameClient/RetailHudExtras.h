#pragma once

#include "Lib/BaseType.h"

class AsciiString;
class GameWindow;
class INI;
class Image;
class ThingTemplate;
class UnicodeString;

namespace RetailHeroBar
{
const Image *reviveImage(const ThingTemplate *hero);
void init(GameWindow *master);
void reset();
void update();
Bool executeHotKey(const AsciiString &key);
Bool handleInput(UnsignedInt msg, Int x, Int y);
Bool showTooltip(Int x, Int y);
}

namespace RetailEdges
{
void init(GameWindow *master);
void reset();
void update();
void drawSideColumnVine(GameWindow *sideBar);
void drawPalantirTop(GameWindow *palantir);
Bool handleInput(UnsignedInt msg, Int x, Int y);
Bool showTooltip(Int x, Int y);
}

namespace RetailNotification
{
enum class HeroEvent { InitialSpawn, Respawn, Death, EarnedAward };

void load();
void parse(INI *ini);
void init(GameWindow *master);
void reset();
void update();
void showType(const AsciiString &type, const UnicodeString &message, Real seconds = 5.0f, const Image *icon = nullptr);
void showHero(HeroEvent event, const ThingTemplate *hero, const UnicodeString *award = nullptr, const Image *awardIcon = nullptr);
Bool handleInput(UnsignedInt msg, Int x, Int y);
}

namespace RetailObjectives
{
void parse(INI *ini);
void reset();
void show();
void close();
}
