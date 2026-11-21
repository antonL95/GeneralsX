#pragma once

#include "Common/GameCommon.h"
#include "Common/GameType.h"

#include <map>

class Drawable;
class GameMessage;
class Object;
class Player;
class View;
class WorldGroundDecal;

namespace RetailWorldUI
{
void drawUnitStatus(Drawable& drawable);
void showPayout(const Object& source, Int amount, Bool bounty);
void drawSelection(View& view, const IRegion2D* dragRegion,
	std::map<DrawableID, WorldGroundDecal>& decals);
void drawPlacement(View& view, const Drawable& ghost, Bool valid);
void drawTerrainClaims(View& view, const Player& player);
void drawOrder(View& view, const Coord3D& position, Bool attack, UnsignedInt age);
Bool setOrderModel(DrawableID& id, const Coord3D& position, Bool attack, UnsignedInt age);
void clearOrderModel(DrawableID& id);
Bool getOrderPosition(const GameMessage& message, Coord3D& position);
}
