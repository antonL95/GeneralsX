#pragma once

#include "Lib/BaseType.h"

class AsciiString;
class GameWindow;
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
void init(GameWindow *master);
void reset();
void update();
void show(const UnicodeString &message, const Image *portrait, Real seconds);
Bool handleInput(UnsignedInt msg, Int x, Int y);
}
