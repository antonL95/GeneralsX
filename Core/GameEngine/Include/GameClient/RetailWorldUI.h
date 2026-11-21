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
void drawTerrainClaims(View& view, const Player& player, const Object* site = nullptr, Real siteRadius = 0.0f);
void drawOrder(View& view, const Coord3D& position, Bool attack, UnsignedInt age);
Bool getOrderPosition(const GameMessage& message, Coord3D& position);
}
