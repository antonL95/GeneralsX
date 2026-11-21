#include "PreRTS.h"

#include "GameClient/RetailWorldUI.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Common/GameUtility.h"
#include "GameClient/SelectionXlat.h"
#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerTemplate.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "GameClient/Display.h"
#include "GameClient/RadiusDecal.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Image.h"
#include "GameClient/View.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/ExperienceLevelStore.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/TerrainResourceBehavior.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "WWMath/wwmath.h"

namespace
{
struct Circle
{
	Coord3D center;
	Real radius;
};

Bool isVisibleUnit(Drawable* draw)
{
	const Object* object = draw ? draw->getObject() : nullptr;
	return object && !object->isEffectivelyDead() && object->isLogicallyVisible() &&
		!draw->isDrawableEffectivelyHidden() &&
		!draw->getFullyObscuredByShroud() && !object->isKindOf(KINDOF_STRUCTURE);
}

void groundLine(View& view, Coord3D start, Coord3D end, Real width, Color color)
{
	start.z = TheTerrainLogic->getGroundHeight(start.x, start.y) + 1.0f;
	end.z = TheTerrainLogic->getGroundHeight(end.x, end.y) + 1.0f;
	ICoord2D a, b;
	if (view.worldToScreenTriReturn(&start, &a) != View::WTS_INVALID &&
		view.worldToScreenTriReturn(&end, &b) != View::WTS_INVALID &&
		(a.x != b.x || a.y != b.y))
		TheDisplay->drawLine(a.x, a.y, b.x, b.y, width, color);
}

Coord3D onCircle(const Circle& circle, Real angle, Real offset = 0.0f)
{
	return {circle.center.x + (circle.radius + offset) * std::cos(angle),
		circle.center.y + (circle.radius + offset) * std::sin(angle), circle.center.z};
}

Circle unitCircle(const Drawable& draw, Bool hordeMember)
{
	const Real radius = draw.getDrawableGeometryInfo().getMajorRadius();
	return {*draw.getPosition(), hordeMember ? std::max(14.0f, radius + 6.0f) :
		std::max(10.0f, radius + 3.0f)};
}

void drawUnitCircle(View& view, const Drawable& draw, Bool preview)
{
	const Circle circle = unitCircle(draw, FALSE);
	const Color color = preview ? GameMakeColor(244, 219, 116, 95) : GameMakeColor(119, 255, 73, 240);
	constexpr Int segments = 48;
	for (Int index = 0; index < segments; ++index)
	{
		if (!preview && index % 6 == 0)
			continue;
		const Real start = index * (WWMATH_TWO_PI / segments);
		const Real end = (index + 1) * (WWMATH_TWO_PI / segments);
		groundLine(view, onCircle(circle, start), onCircle(circle, end), 1.4f, color);
	}
}

void drawMergedCircles(View& view, const std::vector<Circle>& circles, Int opacity)
{
	const Color green = GameMakeColor(105, 255, 56, opacity);
	const Color glow = GameMakeColor(50, 239, 20, opacity / 3);
	std::vector<Real> cuts;
	for (size_t index = 0; index < circles.size(); ++index)
	{
		const Circle& circle = circles[index];
		cuts.clear();
		cuts.push_back(0.0f);
		cuts.push_back(WWMATH_TWO_PI);
		Bool fullyCovered = FALSE;
		for (size_t otherIndex = 0; otherIndex < circles.size(); ++otherIndex)
		{
			if (index == otherIndex)
				continue;
			const Circle& other = circles[otherIndex];
			const Real dx = other.center.x - circle.center.x;
			const Real dy = other.center.y - circle.center.y;
			const Real distance = std::sqrt(dx * dx + dy * dy);
			if (distance + circle.radius <= other.radius &&
				(distance > 0.0f || circle.radius < other.radius || otherIndex < index))
			{
				fullyCovered = TRUE;
				break;
			}
			if (distance >= circle.radius + other.radius || distance <= std::abs(circle.radius - other.radius))
				continue;
			const Real bearing = std::atan2(dy, dx);
			const Real offset = std::acos(std::clamp((distance * distance + circle.radius * circle.radius -
				other.radius * other.radius) / (2.0f * distance * circle.radius), -1.0f, 1.0f));
			for (Real angle : {bearing - offset, bearing + offset})
				cuts.push_back(angle < 0.0f ? angle + WWMATH_TWO_PI : angle);
		}
		if (fullyCovered)
			continue;

		std::sort(cuts.begin(), cuts.end());
		for (size_t cut = 1; cut < cuts.size(); ++cut)
		{
			const Real start = cuts[cut - 1], end = cuts[cut];
			if (end - start < 0.0001f)
				continue;
			const Coord3D midpoint = onCircle(circle, (start + end) * 0.5f);
			Bool covered = FALSE;
			for (size_t otherIndex = 0; otherIndex < circles.size(); ++otherIndex)
			{
				if (index == otherIndex)
					continue;
				const Circle& other = circles[otherIndex];
				const Real dx = midpoint.x - other.center.x;
				const Real dy = midpoint.y - other.center.y;
				if ((other.radius != circle.radius || other.center.x != circle.center.x ||
					 other.center.y != circle.center.y) && dx * dx + dy * dy < other.radius * other.radius)
				{
					covered = TRUE;
					break;
				}
			}
			if (covered)
				continue;

			const Int segments = std::max(1, static_cast<Int>(std::ceil((end - start) * std::max(9.0f, circle.radius * 0.3f))));
			for (Int step = 0; step < segments; ++step)
			{
				const Real a = start + (end - start) * step / segments;
				const Real b = start + (end - start) * (step + 1) / segments;
				const Coord3D first = onCircle(circle, a);
				const Coord3D last = onCircle(circle, b);
				groundLine(view, first, last, 3.0f, glow);
				groundLine(view, first, last, 1.3f, green);
				if (step % 4 == 1)
					groundLine(view, first, onCircle(circle, a + (b - a) * 0.55f, 3.0f), 1.0f, green);
			}
		}
	}
}

void drawHordeBoundary(View& view, const HordeContain& horde, const ExperienceLevelStore::SelectionDecal* decal)
{
	std::vector<Circle> circles;
	for (const Object* member : *horde.getContainedItemsList())
	{
		Drawable* draw = member->getDrawable();
		if (isVisibleUnit(draw))
			circles.push_back(unitCircle(*draw, TRUE));
	}
	const Int opacity = decal && decal->opacityMax > 0.0f ?
		static_cast<Int>(std::clamp(decal->opacityMax, 0.0f, 1.0f) * 255.0f) : 246;
	drawMergedCircles(view, circles, opacity);
}

Bool drawDragPreview(Drawable* draw, void* userData)
{
	if (isVisibleUnit(draw) && draw->getObject()->isLocallyControlled() &&
		!HordeContain::get(draw->getObject()) && (CanSelectDrawable(draw, FALSE) ||
			HordeContain::getAnchor(draw->getObject())))
		drawUnitCircle(*static_cast<View*>(userData), *draw, TRUE);
	return TRUE;
}

void drawRankStar(Int x, Int y)
{
	const Color white = GameMakeColor(255, 255, 250, 255);
	const Color shadow = GameMakeColor(23, 24, 20, 245);
	TheDisplay->drawLine(x - 3, y, x + 3, y, 3.0f, shadow);
	TheDisplay->drawLine(x, y - 3, x, y + 2, 1.4f, white);
	TheDisplay->drawLine(x - 3, y - 1, x + 3, y - 1, 1.4f, white);
	TheDisplay->drawLine(x - 2, y + 2, x + 2, y - 2, 1.0f, white);
	TheDisplay->drawLine(x + 2, y + 2, x - 2, y - 2, 1.0f, white);
}
}

void RetailWorldUI::drawUnitStatus(Drawable& drawable)
{
	Object* obj = drawable.getObject();
	if (!obj || obj->isEffectivelyDead())
		return;

	const Object* anchor = HordeContain::getAnchor(obj);
	const Drawable* group = anchor ? anchor->getDrawable() : &drawable;
	if (!group)
		return;
	const DrawableID hover = TheInGameUI->getMousedOverDrawableID();
	Drawable* hoveredDrawable = anchor ? TheGameClient->findDrawableByID(hover) : nullptr;
	const Bool hoveredGroup = anchor && hoveredDrawable && hoveredDrawable->getObject() &&
		HordeContain::getAnchor(hoveredDrawable->getObject()) == anchor;
	const Bool showBar = TheGlobalData->m_showObjectHealth &&
		(group->isSelected() || group->getID() == hover || drawable.getID() == hover || hoveredGroup);
	const Int rank = obj->getVeterancyLevel();
	const Bool showRank = rank > 0 && obj->getExperienceTracker() && !obj->isKindOf(KINDOF_STRUCTURE);
	if (!showBar && !showRank)
		return;

	Coord3D position;
	obj->getHealthBoxPosition(position);
	const auto& data = obj->getTemplate()->getRotWKObjectData();
	position.z += data.healthBoxHeightOffset;
	ICoord2D screen;
	if (!TheTacticalView->worldToScreen(&position, &screen))
		return;

	const Real scale = TheDisplay->getHeight() / 720.0f;
	const Bool structure = obj->isKindOf(KINDOF_STRUCTURE);
	const Real relativeWidth = structure ? std::clamp(obj->getGeometryInfo().getMajorRadius() * 2.1f, 55.0f, 150.0f) :
		(anchor ? 24.0f : 28.0f);
	const Int width = std::max(6, static_cast<Int>(relativeWidth * scale * std::clamp(data.healthBoxScale, 0.25f, 4.0f)));
	const Int height = std::max(3, static_cast<Int>((structure ? 5.0f : 4.0f) * scale));
	const BodyModuleInterface* body = obj->getBodyModule();
	if (showBar && body && body->getHealth() > 0.0f && body->getMaxHealth() > 0.0f)
	{
		const Real health = std::clamp(body->getHealth() / body->getMaxHealth(), 0.0f, 1.0f);
		const Int x = screen.x - width / 2;
		const Int y = screen.y - height / 2;
		const Int red = static_cast<Int>(255.0f * std::min(1.0f, (1.0f - health) * 2.0f));
		const Int green = static_cast<Int>(225.0f * std::min(1.0f, health * 2.0f));
		TheDisplay->drawFillRect(x, y, width, height, GameMakeColor(18, 19, 13, 250));
		TheDisplay->drawOpenRect(x, y, width, height, 1.0f, GameMakeColor(36, 34, 27, 255));
		TheDisplay->drawFillRect(x + 1, y + 1, std::max(1, static_cast<Int>((width - 2) * health)),
			height - 2, GameMakeColor(red + (255 - red) / 6, green, 38, 255));
	}
	if (showRank)
	{
		const Player* owner = obj->getControllingPlayer();
		const PlayerTemplate* side = owner ? owner->getPlayerTemplate() : nullptr;
		const Bool evil = side && side->isEvil();
		const Image* star = TheMappedImageCollection ?
			TheMappedImageCollection->findImageByName(evil ? "Evil_Vet" : "Good_Vet") : nullptr;
		const Image* dot = TheMappedImageCollection ?
			TheMappedImageCollection->findImageByName(evil ? "Evil_Vet_Dot" : "Good_Vet_Dot") : nullptr;
		const Int x = screen.x + width / 2 - 4;
		const Int y = screen.y - height - 4;
		if (star)
			TheDisplay->drawImage(star, x - 5, y - 5, x + 5, y + 5);
		else
			drawRankStar(x, y);
		for (Int index = 1; dot && index < std::min(rank, 3); ++index)
			TheDisplay->drawImage(dot, x + 1 + index * 4, y - 2, x + 5 + index * 4, y + 2);
	}
}

void RetailWorldUI::drawSelection(View& view, const IRegion2D* dragRegion,
	std::map<DrawableID, WorldGroundDecal>& decals)
{
	for (auto item = decals.begin(); item != decals.end();)
	{
		Drawable* drawable = TheGameClient->findDrawableByID(item->first);
		if (!isVisibleUnit(drawable) || !drawable->isSelected())
			item = decals.erase(item);
		else
			++item;
	}
	for (Drawable* draw : *TheInGameUI->getAllSelectedDrawables())
	{
		const Object* object = draw->getObject();
		if (!object || object->isEffectivelyDead())
			continue;
		const auto* decal = ExperienceLevelStore::findSelectionDecal(*object);
		if (const HordeContain* horde = HordeContain::get(object))
			drawHordeBoundary(view, *horde, decal);
		else if (isVisibleUnit(draw))
		{
			if (decal && decal->style.compareNoCase("SHADOW_ALPHA_DECAL") == 0)
			{
				const Real radius = unitCircle(*draw, FALSE).radius;
				const Int opacity = static_cast<Int>(std::clamp(decal->opacityMax, 0.0f, 1.0f) * 255.0f);
				decals[draw->getID()].set(decal->texture, *draw->getPosition(), radius * 2.0f, radius * 2.0f, opacity,
					GameMakeColor(115, 255, 80, 255));
			}
			else
				drawUnitCircle(view, *draw, FALSE);
		}
	}
	if (dragRegion)
	{
		IRegion2D region = *dragRegion;
		view.iterateDrawablesInRegion(&region, drawDragPreview, &view);
	}
}

void RetailWorldUI::drawPlacement(View& view, const Drawable& ghost, Bool valid)
{
	const Circle circle = {*ghost.getPosition(), ghost.getDrawableGeometryInfo().getMajorRadius() + 5.0f};
	const Color color = valid ? GameMakeColor(136, 255, 76, 245) : GameMakeColor(255, 66, 55, 245);
	for (Int i = 0; i < 4; ++i)
	{
		const Real angle = ghost.getOrientation() + (i + 0.5f) * WWMATH_PI * 0.5f;
		const Coord3D point = onCircle(circle, angle, -3.0f);
		groundLine(view, onCircle(circle, angle - 0.15f, 5.0f), point, 2.0f, color);
		groundLine(view, onCircle(circle, angle + 0.15f, 5.0f), point, 2.0f, color);
	}
}

void RetailWorldUI::drawOrder(View& view, const Coord3D& position, Bool attack, UnsignedInt age)
{
	const Int alpha = 255 - static_cast<Int>(std::min(age, 40u)) * 4;
	if (attack)
	{
		ICoord2D screen;
		if (!view.worldToScreen(&position, &screen))
			return;
		const Color red = GameMakeColor(255, 65, 70, alpha);
		for (Int x : {-1, 1})
			for (Int y : {-1, 1})
				TheDisplay->drawLine(screen.x + x * 12, screen.y + y * 12,
					screen.x + x * 6, screen.y + y * 6, 2.5f, red);
		return;
	}
	Circle circle = {position, 14.0f};
	if (age < 12)
	{
		const Color color = GameMakeColor(142, 255, 66, alpha);
		for (Int i = 0; i < 4; ++i)
		{
			const Real angle = (i + 0.5f) * (WWMATH_PI * 0.5f);
			const Coord3D tip = onCircle(circle, angle, -6.0f);
			groundLine(view, onCircle(circle, angle - 0.16f), tip, 2.0f, color);
			groundLine(view, onCircle(circle, angle + 0.16f), tip, 2.0f, color);
		}
		return;
	}
	const Color gold = GameMakeColor(255, 215, 88, alpha);
	for (Int i = 0; i < 32; ++i)
	{
		const Real start = i * (WWMATH_TWO_PI / 32);
		const Real end = (i + 1) * (WWMATH_TWO_PI / 32);
		groundLine(view, onCircle(circle, start), onCircle(circle, end), 1.0f, gold);
		if (i % 2 == 0)
		{
			const Real next = (i + 2) * (WWMATH_TWO_PI / 32);
			groundLine(view, onCircle(circle, start, i % 4 == 0 ? -3.0f : -10.0f),
				onCircle(circle, next, i % 4 == 0 ? -10.0f : -3.0f), 1.4f, gold);
		}
	}
}

Bool RetailWorldUI::getOrderPosition(const GameMessage& message, Coord3D& position)
{
	if (message.getArgumentCount() == 0)
		return FALSE;
	switch (message.getArgumentDataType(0))
	{
		case ARGUMENTDATATYPE_LOCATION:
			position = message.getArgument(0)->location;
			return TRUE;
		case ARGUMENTDATATYPE_OBJECTID:
		{
			const Object* target = TheGameLogic->findObjectByID(message.getArgument(0)->objectID);
			if (!target)
				return FALSE;
			position = *target->getPosition();
			position.z += std::max(3.0f, target->getGeometryInfo().getMaxHeightAbovePosition());
			return TRUE;
		}
		default:
			return FALSE;
	}
}

void RetailWorldUI::showPayout(const Object& source, Int amount, Bool bounty)
{
	if (!TheInGameUI || amount <= 0 || !source.isLogicallyVisible())
		return;

	Coord3D position = *source.getPosition();
	position.z += source.getGeometryInfo().getMaxHeightAbovePosition();
	UnicodeString text;
	text.format(L"+%d", amount);
	const Color color = bounty ? GameMakeColor(255, 255, 255, 255) : GameMakeColor(131, 255, 94, 255);
	TheInGameUI->addFloatingText(text, &position, color);
}

Bool RetailWorldUI::setOrderModel(DrawableID& id, const Coord3D& position, Bool attack, UnsignedInt age)
{
	const Real terrainHeight = TheTerrainLogic->getGroundHeight(position.x, position.y);
	if (attack && position.z > terrainHeight + 2.0f)
	{
		clearOrderModel(id);
		return FALSE;
	}

	Drawable* drawable = TheGameClient->findDrawableByID(id);
	if (!drawable)
	{
		const ThingTemplate* type = TheThingFactory->findTemplate(attack ? "AttackHint" : "MoveHint", FALSE);
		if (!type || !type->isKindOf(KINDOF_DRAWABLE_ONLY))
			return FALSE;
		drawable = TheThingFactory->newDrawable(type, DRAWABLE_STATUS_NO_STATE_PARTICLES);
		if (!drawable)
			return FALSE;
		id = drawable->getID();
	}
	Coord3D ground = position;
	ground.z = terrainHeight + 1.0f;
	drawable->setPosition(&ground);
	drawable->setDrawableOpacity(1.0f - static_cast<Real>(age) * 0.012f);
	return TRUE;
}

void RetailWorldUI::clearOrderModel(DrawableID& id)
{
	if (id != INVALID_DRAWABLE_ID)
	{
		if (Drawable* drawable = TheGameClient->findDrawableByID(id))
			TheGameClient->destroyDrawable(drawable);
		id = INVALID_DRAWABLE_ID;
	}
}

void RetailWorldUI::drawTerrainClaims(View& view, const Player& player)
{
	std::vector<Circle> circles;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (object->getControllingPlayer() != &player || !object->isKindOf(KINDOF_STRUCTURE))
			continue;
		const Real radius = TerrainResourceBehavior::getActiveClaimRadius(*object);
		if (radius > 0.0f)
			circles.push_back({*object->getPosition(), radius});
	}
	drawMergedCircles(view, circles, 130);
}
