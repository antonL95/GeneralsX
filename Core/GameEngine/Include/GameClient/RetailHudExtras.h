#pragma once

#include "Lib/BaseType.h"

class AsciiString;
class GameWindow;
class INI;
class Image;
class UnicodeString;

namespace RetailHeroBar
{
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
void load();
void parse(INI *ini);
void init(GameWindow *master);
void reset();
void update();
void show(const UnicodeString &message, const Image *portrait, Real seconds);
void showType(const AsciiString &type, const UnicodeString &message, Real seconds = 5.0f);
Bool handleInput(UnsignedInt msg, Int x, Int y);
}
