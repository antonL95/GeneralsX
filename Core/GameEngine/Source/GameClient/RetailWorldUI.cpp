#include "PreRTS.h"

#include "GameClient/RetailWorldUI.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

#include "Common/GameUtility.h"
#include "GameClient/SelectionXlat.h"
#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerTemplate.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Display.h"
#include "GameClient/RadiusDecal.h"
#include "GameClient/Drawable.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Image.h"
#include "GameClient/View.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/ExperienceTracker.h"
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
	{
		Int x, y;
		view.getOrigin(&x, &y);
		const Int margin = static_cast<Int>(std::ceil(width));
		if (std::max(a.x, b.x) < x - margin || std::min(a.x, b.x) >= x + view.getWidth() + margin ||
			std::max(a.y, b.y) < y - margin || std::min(a.y, b.y) >= y + view.getHeight() + margin)
			return;
		TheDisplay->drawLine(a.x, a.y, b.x, b.y, width, color);
	}
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

void drawMergedCircles(View& view, const std::vector<Circle>& circles, Color houseColor, Int opacity)
{
	const Color stroke = (houseColor & 0x00FFFFFF) | (opacity << 24);
	const Color glow = (houseColor & 0x00FFFFFF) | ((opacity / 3) << 24);
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
				groundLine(view, first, last, 1.3f, stroke);
				if (step % 4 == 1)
					groundLine(view, first, onCircle(circle, a + (b - a) * 0.55f, 3.0f), 1.0f, stroke);
			}
		}
	}
}

struct DecalMask
{
	std::vector<UnsignedByte> alpha;
	Int width = 0;
	Int height = 0;
};

const DecalMask& getDecalMask(const AsciiString& name)
{
	static std::map<std::string, DecalMask> masks;
	auto [entry, inserted] = masks.try_emplace(name.str());
	if (inserted && !name.isEmpty())
	{
		auto& mask = entry->second;
		TheDisplay->readTextureAlpha(name, mask.alpha, mask.width, mask.height);
	}
	return entry->second;
}

void stampDecalMask(const DecalMask& source, std::vector<UnsignedByte>& combined, Int size,
	Real x, Real y, Real radius)
{
	if (source.alpha.empty())
		return;
	const Int left = std::max(1, static_cast<Int>(std::ceil(x - radius)));
	const Int right = std::min(size - 1, static_cast<Int>(std::floor(x + radius)) + 1);
	const Int top = std::max(1, static_cast<Int>(std::ceil(y - radius)));
	const Int bottom = std::min(size - 1, static_cast<Int>(std::floor(y + radius)) + 1);
	const Real scale = 0.5f / radius;
	for (Int row = top; row < bottom; ++row)
	{
		const Int v = std::min(source.height - 1, static_cast<Int>((0.5f + (row - y) * scale) * source.height));
		for (Int column = left; column < right; ++column)
		{
			const Int u = std::min(source.width - 1, static_cast<Int>((0.5f + (column - x) * scale) * source.width));
			UnsignedByte& alpha = combined[row * size + column];
			alpha = std::max(alpha, source.alpha[v * source.width + u]);
		}
	}
}

Real selectionRadius(const ExperienceLevelStore::SelectionDecal& decal, Real radius)
{
	const Real minimum = std::max(0.0f, decal.minRadius);
	const Real maximum = decal.maxRadius > 0.0f ? std::max(decal.maxRadius, minimum) : std::max(minimum, radius);
	return std::clamp(radius, minimum, maximum);
}

Color selectionTint(const Object& object)
{
	const Player* owner = object.getControllingPlayer();
	return owner ? owner->getPlayerColor() | 0xFF000000 : 0xFFFFFFFF;
}

Bool drawMergedSelection(std::vector<Circle>& circles, const ExperienceLevelStore::SelectionDecal& decal,
	Bool horde, Color tint, WorldGroundDecal& projection)
{
	const auto& rank = getDecalMask(decal.texture);
	const auto& outline = getDecalMask(decal.texture2);
	if (circles.empty() || (rank.alpha.empty() && outline.alpha.empty()))
		return FALSE;

	Coord3D center{};
	for (const Circle& circle : circles)
	{
		center.x += circle.center.x;
		center.y += circle.center.y;
		center.z += circle.center.z;
	}
	center.x /= circles.size();
	center.y /= circles.size();
	center.z /= circles.size();
	Real radius = 0.0f;
	constexpr Real stampExtentScale = 1.4f;
	const Real memberMinimum = horde ? decal.minRadius / std::sqrt(static_cast<Real>(circles.size())) : 0.0f;
	for (Circle& circle : circles)
	{
		circle.radius = std::max(circle.radius, memberMinimum);
		radius = std::max(radius, std::max(std::abs(circle.center.x - center.x),
			std::abs(circle.center.y - center.y)) + circle.radius * stampExtentScale);
	}
	if (horde)
		radius = selectionRadius(decal, radius);

	Real opacity = decal.opacityMax;
	if (TheGlobalData->m_useSimpleMergeDecals)
		opacity *= TheGlobalData->m_opacityOfSimpleMergeDecals;
	const Int alpha = static_cast<Int>(std::clamp(opacity, 0.0f, 1.0f) * 255);
	if (TheGlobalData->m_useSimpleHordeDecals)
	{
		projection.set(decal.texture, center, radius * 2.0f, radius * 2.0f, alpha, tint);
		return TRUE;
	}

	constexpr Int size = 128;
	const Real pixelScale = (size - 4) / (radius * 2.0f);
	std::vector<UnsignedByte> mask(size * size);
	for (const Circle& circle : circles)
	{
		const Real x = (circle.center.x - center.x) * pixelScale + (size - 1) * 0.5f;
		const Real y = (center.y - circle.center.y) * pixelScale + (size - 1) * 0.5f;
		const Real stampRadius = circle.radius * stampExtentScale * pixelScale;
		stampDecalMask(rank, mask, size, x, y, stampRadius);
		stampDecalMask(outline, mask, size, x, y, stampRadius);
	}

	std::vector<UnsignedInt> pixels(size * size, 0xffffff);
	const Int edge = TheGlobalData->m_useSimpleMergeDecals ? 2 : 1;
	for (Int y = edge; y < size - edge; ++y)
		for (Int x = edge; x < size - edge; ++x)
		{
			const Int i = y * size + x;
			const auto inside = std::min({mask[i], mask[i - edge], mask[i + edge],
				mask[i - edge * size], mask[i + edge * size]});
			pixels[i] |= static_cast<UnsignedInt>(mask[i] - inside) << 24;
		}
	projection.setComposited(center, size / pixelScale, alpha, tint, std::move(pixels), size);
	return TRUE;
}

Bool drawDragPreview(Drawable* draw, void* userData)
{
	if (isVisibleUnit(draw) && draw->getObject()->isLocallyControlled() &&
		!HordeContain::get(draw->getObject()) && (CanSelectDrawable(draw, FALSE) ||
			HordeContain::getAnchor(draw->getObject())))
		drawUnitCircle(*static_cast<View*>(userData), *draw, TRUE);
	return TRUE;
}

}

void RetailWorldUI::drawUnitStatus(Drawable& drawable)
{
	Object* obj = drawable.getObject();
	if (!obj || obj->isEffectivelyDead() || !obj->isLogicallyVisible() ||
		drawable.isDrawableEffectivelyHidden() || drawable.getFullyObscuredByShroud() ||
		obj->isKindOf(KINDOF_IGNORED_IN_GUI) || obj->isKindOf(KINDOF_EXPANSION_PAD))
		return;

	const Object* anchor = HordeContain::getAnchor(obj);
	const Drawable* group = anchor ? anchor->getDrawable() : &drawable;
	if (!group)
		return;
	const DrawableID hover = TheInGameUI->getMousedOverDrawableID();
	const Bool showBar = TheGlobalData->m_showObjectHealth &&
		(group->isSelected() || drawable.getID() == hover);
	const ExperienceTracker* tracker = (anchor ? anchor : obj)->getExperienceTracker();
	const Int rank = tracker ? tracker->getRank() : 1;
	const Bool showRank = rank > 1 && isVisibleUnit(&drawable);
	if (!showBar && !showRank)
		return;

	Coord3D position = *obj->getPosition();
	const auto& data = obj->getTemplate()->getRotWKObjectData();
	Real top = data.primaryGeometry.usedForHealthBox ?
		data.primaryGeometry.offset.z + obj->getGeometryInfo().getMaxHeightAbovePosition() : 0.0f;
	for (const auto& geometry : data.additionalGeometries)
		if (geometry.details.active && geometry.details.usedForHealthBox)
			top = std::max(top, geometry.details.offset.z + geometry.shape.getMaxHeightAbovePosition());
	position.z += top + data.healthBoxHeightOffset;
	ICoord2D screen;
	if (!TheTacticalView->worldToScreen(&position, &screen))
		return;

	const Real scale = TheDisplay->getHeight() / 720.0f;
	const Bool structure = obj->isKindOf(KINDOF_STRUCTURE);
	const Real relativeWidth = structure ? std::clamp(obj->getGeometryInfo().getMajorRadius() * 3.5f, 55.0f, 180.0f) :
		(anchor ? 27.0f : 34.0f);
	const Int width = std::max(6, static_cast<Int>(relativeWidth * scale * std::clamp(data.healthBoxScale, 0.25f, 4.0f)));
	const Int height = std::max(4, static_cast<Int>(5.0f * scale));
	const BodyModuleInterface* body = obj->getBodyModule();
	if (showBar && body && body->getHealth() > 0.0f && body->getMaxHealth() > 0.0f)
	{
		const Real health = std::clamp(body->getHealth() / body->getMaxHealth(), 0.0f, 1.0f);
		const Bool building = obj->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION);
		const Real fraction = building ? std::clamp(obj->getConstructionPercent() * 0.01f, 0.0f, 1.0f) : health;
		const Int x = screen.x - width / 2;
		const Int y = screen.y - height / 2;
		const Color fill = building ? GameMakeColor(97, 209, 244, 255) :
			health > TheGlobalData->m_unitDamagedThresh ? GameMakeColor(136, 226, 59, 255) :
			health > TheGlobalData->m_unitReallyDamagedThresh ? GameMakeColor(239, 196, 84, 255) : GameMakeColor(220, 76, 36, 255);
		TheDisplay->drawFillRect(x - 1, y - 1, width + 2, height + 2, GameMakeColor(0, 0, 0, 255));
		TheDisplay->drawOpenRect(x, y, width, height, 1.0f, GameMakeColor(199, 178, 131, 255));
		const Int fillWidth = static_cast<Int>((width - 2) * fraction);
		if (fillWidth > 0)
			TheDisplay->drawFillRect(x + 1, y + 1, fillWidth, height - 2, fill);
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
		if (!star)
			return;
		const Int starSize = std::max(1, static_cast<Int>(star->getImageWidth() * scale * 0.5f));
		const Int dotSize = dot ? std::max(1, static_cast<Int>(dot->getImageWidth() * scale * 0.5f)) : 0;
		const Int dots = dot ? std::min(rank, 10) - 2 : 0;
		const Int x = screen.x - (starSize + dots * dotSize) / 2;
		const Int y = screen.y - (showBar ? height / 2 + 1 : 0) - starSize;
		TheDisplay->drawImage(star, x, y, x + starSize, y + starSize);
		for (Int index = 0; index < dots; ++index)
		{
			const Int dotX = x + starSize + index * dotSize;
			const Int dotY = y + (starSize - dotSize) / 2;
			TheDisplay->drawImage(dot, dotX, dotY, dotX + dotSize, dotY + dotSize);
		}
	}
}

void RetailWorldUI::drawSelection(View& view, const IRegion2D* dragRegion,
	std::map<DrawableID, WorldGroundDecal>& decals)
{
	std::set<DrawableID> active;
	const auto displaySelection = [&](Drawable* draw)
	{
		Object* object = draw->getObject();
		if (!object || object->isEffectivelyDead())
			return;
		Object* anchor = HordeContain::getAnchor(object);
		if (anchor)
		{
			object = anchor;
			draw = anchor->getDrawable();
			if (!draw)
				return;
		}
		const DrawableID id = draw->getID();
		if (active.count(id))
			return;
		const auto* decal = ExperienceLevelStore::findSelectionDecal(*object);
		if (decal && decal->style.compareNoCase("SHADOW_MERGE_DECAL") == 0)
		{
			std::vector<Circle> circles;
			const HordeContain* horde = HordeContain::get(object);
			if (horde)
			{
				for (const Object* member : *horde->getContainedItemsList())
				{
					Drawable* memberDraw = member->getDrawable();
					if (isVisibleUnit(memberDraw))
						circles.push_back(unitCircle(*memberDraw, TRUE));
					if (decal->maxSelectedUnits > 0 && circles.size() >= static_cast<size_t>(decal->maxSelectedUnits))
						break;
				}
			}
			else if (isVisibleUnit(draw))
				circles.push_back(unitCircle(*draw, FALSE));
			if (drawMergedSelection(circles, *decal, horde != nullptr, selectionTint(*object), decals[id]))
				active.insert(id);
		}
		else if (isVisibleUnit(draw) && decal && decal->style.compareNoCase("SHADOW_ALPHA_DECAL") == 0)
		{
			const Real radius = selectionRadius(*decal, unitCircle(*draw, FALSE).radius);
			decals[id].set(decal->texture, *draw->getPosition(), radius * 2.0f, radius * 2.0f,
				static_cast<Int>(std::clamp(decal->opacityMax, 0.0f, 1.0f) * 255), selectionTint(*object));
			active.insert(id);
		}
	};
	for (Drawable* draw : *TheInGameUI->getAllSelectedDrawables())
		displaySelection(draw);
	std::erase_if(decals, [&](const auto& item) { return !active.count(item.first); });
	if (dragRegion)
	{
		IRegion2D region = *dragRegion;
		view.iterateDrawablesInRegion(&region, drawDragPreview, &view);
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
	const Player* owner = source.getControllingPlayer();
	const Color color = bounty || !owner ? GameMakeColor(255, 255, 255, 255) : owner->getPlayerColor() | 0xFF000000;
	TheInGameUI->addFloatingText(text, &position, color);
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
	drawMergedCircles(view, circles, player.getPlayerColor(), 130);
}
