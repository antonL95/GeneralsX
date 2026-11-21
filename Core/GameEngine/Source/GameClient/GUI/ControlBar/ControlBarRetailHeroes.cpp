#include "PreRTS.h"

#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "Common/GameCommon.h"
#include "Common/MessageStream.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ThingTemplate.h"

#include "GameClient/ControlBar.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/HotKey.h"
#include "GameClient/Image.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Mouse.h"
#include "GameClient/RetailHudExtras.h"
#include "GameClient/RetailHudSettings.h"
#include "GameClient/UiHudRules.h"
#include "GameClient/UiScale.h"
#include "GameClient/View.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/HeroRoster.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"

namespace
{
constexpr Int MaxHeroes = 16;
constexpr Int HeroesPerRow = 13;
constexpr Real OriginX = 40.0f;
constexpr Real OriginY = 100.0f;
constexpr Real HeroSize = 56.0f;
constexpr Real HeroSpacing = 75.0f;
constexpr Real FirstHeroX = 24.0f;
constexpr Real SecondRowX = 99.0f;
constexpr Real SecondRowY = -70.0f;
constexpr Real HaloSize = 86.0f;
constexpr Real BurstSize = 112.0f;
constexpr Real BuilderX = -15.0f;
constexpr Real BuilderY = 30.0f;
constexpr Real BuilderSize = 32.5f;
constexpr UnsignedInt AttackedFrames = LOGICFRAMES_PER_SECOND * 2;
constexpr UnsignedInt LevelUpMs = 1200;
constexpr UnsignedInt FlickerMs = 400;
constexpr Int ButtonNone = -2;
constexpr Int ButtonBuilder = -1;

struct HeroSlot
{
	const ThingTemplate *hero = nullptr;
	ObjectID id = INVALID_ID;
	Int rosterIndex = -1;
	Bool alive = FALSE;
	Bool reviving = FALSE;
	Bool selected = FALSE;
	Bool attacked = FALSE;
	Int level = 1;
	Real health = 1.0f;
	Real maxHealth = 0.0f;
};

struct HeroMemory
{
	const ThingTemplate *hero = nullptr;
	Int level = 0;
	Bool alive = FALSE;
	UnsignedInt levelUpStart = 0;
	Bool levelUp = FALSE;
};

struct BarState
{
	GameWindow *bar = nullptr;
	std::vector<const ThingTemplate *> order;
	std::vector<HeroSlot> slots;
	std::vector<HeroMemory> memory;
	Bool populated = FALSE;
	Int pressed = ButtonNone;
	UnsignedInt builderTime = 0;
	ObjectID builder = INVALID_ID;
	std::array<DisplayString *, MaxHeroes> levelText = {};
	std::array<Int, MaxHeroes> shownLevel = {};
	GameFont *font = nullptr;
	Real fontScale = 0.0f;
};

BarState s_bar;

Real uiScale()
{
	return static_cast<Real>(UiScale::scale(TheDisplay->getWidth(), TheDisplay->getHeight()));
}

const Image *findImage(const char *name)
{
	return TheMappedImageCollection ? TheMappedImageCollection->findImageByName(name) : nullptr;
}

Int roundPixel(Real value)
{
	return static_cast<Int>(std::floor(value + 0.5f));
}

struct PixelBox
{
	Int x0;
	Int y0;
	Int x1;
	Int y1;
	Int width() const { return x1 - x0; }
	Int height() const { return y1 - y0; }
	Int centerX() const { return (x0 + x1) / 2; }
	Int centerY() const { return (y0 + y1) / 2; }
};

ICoord2D barOrigin(Real scale)
{
	Int x, y;
	s_bar.bar->winGetScreenPosition(&x, &y);
	return ICoord2D{ x + roundPixel(OriginX * scale), y + roundPixel(OriginY * scale) };
}

PixelBox designBox(const ICoord2D &origin, Real scale, Real x, Real y, Real width, Real height)
{
	return PixelBox{ origin.x + roundPixel(x * scale), origin.y + roundPixel(y * scale),
		origin.x + roundPixel((x + width) * scale), origin.y + roundPixel((y + height) * scale) };
}

Coord2D heroOffset(Int index)
{
	if (index < HeroesPerRow)
		return Coord2D{ FirstHeroX + HeroSpacing * index, 0.0f };
	return Coord2D{ SecondRowX + HeroSpacing * (index - HeroesPerRow), SecondRowY };
}

PixelBox heroBox(const ICoord2D &origin, Real scale, Int index, Real dx = 0.0f, Real dy = 0.0f, Real width = HeroSize, Real height = HeroSize)
{
	const Coord2D offset = heroOffset(index);
	return designBox(origin, scale, offset.x + dx, offset.y + dy, width, height);
}

PixelBox centeredHeroBox(const ICoord2D &origin, Real scale, Int index, Real size)
{
	const Real inset = (HeroSize - size) * 0.5f;
	return heroBox(origin, scale, index, inset, inset, size, size);
}

void drawImageBox(const Image *image, const PixelBox &box, Color color = 0xFFFFFFFF, Display::DrawImageMode mode = Display::DRAW_IMAGE_ALPHA)
{
	if (TheDisplay->hasImageTexture(image))
		TheDisplay->drawImage(image, box.x0, box.y0, box.x1, box.y1, color, mode);
}

void fillCircle(const PixelBox &box, Color color)
{
	const Int radius = MIN(box.width(), box.height()) / 2;
	if (radius <= 0)
		return;
	TheDisplay->setCircularClip(box.centerX(), box.centerY(), radius);
	TheDisplay->drawFillRect(box.x0, box.y0, box.width(), box.height(), color);
	TheDisplay->setCircularClip(0, 0, 0);
}

Color withAlpha(Int red, Int green, Int blue, Real alpha)
{
	return GameMakeColor(static_cast<UnsignedByte>(red), static_cast<UnsignedByte>(green), static_cast<UnsignedByte>(blue),
		static_cast<UnsignedByte>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f + 0.5f));
}

Bool insideCircle(Int px, Int py, const PixelBox &box)
{
	const Real radius = MIN(box.width(), box.height()) * 0.5f;
	const Real dx = px - (box.x0 + box.x1) * 0.5f;
	const Real dy = py - (box.y0 + box.y1) * 0.5f;
	return dx * dx + dy * dy <= radius * radius;
}

Player *localPlayer()
{
	return ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
}

UnicodeString withName(const AsciiString &label, const UnicodeString &name)
{
	UnicodeString format = TheGameText->fetchOrSubstitute(label.str(), L"");
	UnicodeString text;
	Bool replaced = FALSE;
	for (const WideChar *ch = format.str(); ch && *ch; ++ch)
	{
		if (!replaced && ch[0] == L'%' && (ch[1] == L's' || (ch[1] == L'l' && ch[2] == L's')))
		{
			text.concat(name);
			ch += ch[1] == L'l' ? 2 : 1;
			replaced = TRUE;
			continue;
		}
		text.concat(*ch);
	}
	return text;
}

HeroMemory &memoryFor(const ThingTemplate *hero)
{
	for (HeroMemory &memory : s_bar.memory)
		if (memory.hero == hero)
			return memory;
	s_bar.memory.push_back(HeroMemory{ hero });
	return s_bar.memory.back();
}

void notifyHero(const RetailHudSettings::Notification &notification, const ThingTemplate *hero)
{
	if (notification.message.isEmpty() || !hero)
		return;
	RetailNotification::show(withName(notification.message, hero->getDisplayName()), hero->getButtonImage(), notification.timeout);
}

struct Census
{
	const Player *player;
	std::vector<HeroSlot> *slots;
};

void collectHero(Object *obj, void *userData)
{
	Census &census = *static_cast<Census *>(userData);
	if (!obj->isKindOf(KINDOF_HERO) || obj->isEffectivelyDead() || obj->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION))
		return;
	for (const HeroSlot &slot : *census.slots)
		if (slot.hero->isEquivalentTo(obj->getTemplate()))
			return;
	HeroSlot slot;
	slot.hero = obj->getTemplate();
	slot.id = obj->getID();
	slot.alive = TRUE;
	slot.level = HeroRules::levelFromVeterancy(static_cast<Int>(obj->getVeterancyLevel()));
	slot.rosterIndex = HeroRoster::findIndex(census.player, obj->getTemplate());
	const BodyModuleInterface *body = obj->getBodyModule();
	if (body && body->getMaxHealth() > 0.0f)
	{
		slot.maxHealth = body->getMaxHealth();
		slot.health = std::clamp(body->getHealth() / body->getMaxHealth(), 0.0f, 1.0f);
		const UnsignedInt *damaged = body->getLastDamageTimestamp();
		const UnsignedInt now = TheGameLogic->getFrame();
		slot.attacked = damaged && *damaged > 0 && now >= *damaged && now - *damaged < AttackedFrames;
	}
	const Drawable *drawable = obj->getDrawable();
	slot.selected = drawable && drawable->isSelected();
	census.slots->push_back(slot);
}

std::vector<HeroSlot> collectHeroes(const Player *player)
{
	std::vector<HeroSlot> slots;
	Census census{ player, &slots };
	player->iterateObjects(collectHero, &census);
	const Int count = HeroRoster::count(player);
	for (Int i = 0; i < count; ++i)
	{
		const ThingTemplate *hero = HeroRoster::heroAt(player, i);
		if (!hero || HeroRoster::stateAt(player, i) != HeroRules::State::Revivable)
			continue;
		Bool present = FALSE;
		for (const HeroSlot &slot : slots)
			present = present || slot.hero->isEquivalentTo(hero);
		if (present)
			continue;
		HeroSlot slot;
		slot.hero = hero;
		slot.rosterIndex = i;
		slot.level = HeroRoster::savedLevelAt(player, i);
		slot.health = 0.0f;
		slot.reviving = HeroRoster::isQueued(player, hero);
		slots.push_back(slot);
	}
	return slots;
}

void trackEvents(const std::vector<HeroSlot> &slots)
{
	const UnsignedInt now = timeGetTime();
	const RetailHudSettings *settings = TheRetailHudSettings;
	for (const HeroSlot &slot : slots)
	{
		HeroMemory &memory = memoryFor(slot.hero);
		const Bool known = memory.level > 0;
		if (s_bar.populated && settings)
		{
			if (slot.alive && !known)
				notifyHero(settings->heroInitialSpawn, slot.hero);
			else if (slot.alive && known && !memory.alive)
				notifyHero(settings->heroRespawn, slot.hero);
			else if (!slot.alive && known && memory.alive)
				notifyHero(settings->heroDeath, slot.hero);
		}
		if (known && slot.alive && memory.alive && slot.level > memory.level)
		{
			memory.levelUp = TRUE;
			memory.levelUpStart = now;
		}
		memory.level = slot.level;
		memory.alive = slot.alive;
	}
	for (HeroMemory &memory : s_bar.memory)
	{
		Bool present = FALSE;
		for (const HeroSlot &slot : slots)
			present = present || slot.hero == memory.hero;
		if (!present && memory.alive && s_bar.populated && settings)
			notifyHero(settings->heroDeath, memory.hero);
		if (!present)
			memory.alive = FALSE;
	}
	s_bar.populated = TRUE;
}

const Image *heroImage(const HeroSlot &slot)
{
	const Image *image = slot.hero->getButtonImage();
	if (!slot.alive && image)
	{
		AsciiString revive;
		revive.format("%s_res", image->getName().str());
		if (const Image *dead = findImage(revive.str()))
			return dead;
	}
	return image;
}

const Image *builderImage()
{
	const Player *player = localPlayer();
	if (!player)
		return nullptr;
	AsciiString name;
	name.format("ResourceBar_%s", player->getSide().str());
	return findImage(name.str());
}

Int hitTest(Int px, Int py)
{
	if (!s_bar.bar || s_bar.bar->winIsHidden())
		return ButtonNone;
	const Real scale = uiScale();
	const ICoord2D origin = barOrigin(scale);
	if (insideCircle(px, py, designBox(origin, scale, BuilderX, BuilderY, BuilderSize, BuilderSize)))
		return ButtonBuilder;
	for (Int i = 0; i < static_cast<Int>(s_bar.slots.size()); ++i)
		if (insideCircle(px, py, heroBox(origin, scale, i)))
			return i;
	return ButtonNone;
}

void selectObject(Object *obj, Bool center)
{
	Drawable *drawable = obj ? obj->getDrawable() : nullptr;
	if (!drawable || !TheInGameUI)
		return;
	TheInGameUI->deselectAllDrawables();
	TheInGameUI->selectDrawable(drawable);
	GameMessage *select = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP);
	select->appendBooleanArgument(TRUE);
	select->appendObjectIDArgument(obj->getID());
	if (center && TheTacticalView)
		TheTacticalView->lookAt(obj->getPosition());
}

struct BuilderCensus
{
	Coord3D center;
	std::vector<UiHudRules::Candidate> candidates;
};

void collectBuilder(Object *obj, void *userData)
{
	if (!obj->isKindOf(KINDOF_DOZER) || obj->isEffectivelyDead() || obj->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION))
		return;
	BuilderCensus &census = *static_cast<BuilderCensus *>(userData);
	const Coord3D *position = obj->getPosition();
	const Real dx = position->x - census.center.x;
	const Real dy = position->y - census.center.y;
	const AIUpdateInterface *ai = obj->getAI();
	census.candidates.push_back(UiHudRules::Candidate{ static_cast<double>(dx * dx + dy * dy),
		static_cast<unsigned int>(obj->getID()), ai && ai->isIdle() });
}

void selectNearestBuilder()
{
	Player *player = localPlayer();
	if (!player || !TheTacticalView || !TheGameLogic)
		return;
	BuilderCensus census;
	census.center = TheTacticalView->getPosition();
	player->iterateObjects(collectBuilder, &census);
	const std::vector<unsigned int> order = UiHudRules::builderOrder(census.candidates);
	const UnsignedInt now = timeGetTime();
	const UnsignedInt timeout = TheRetailHudSettings ? TheRetailHudSettings->selectNearestBuilderCycleTimeOut : 3500;
	const Bool cycling = s_bar.builder != INVALID_ID && now - s_bar.builderTime <= timeout;
	const Int next = UiHudRules::nextBuilder(order, static_cast<unsigned int>(s_bar.builder), cycling);
	if (next < 0)
		return;
	Object *builder = TheGameLogic->findObjectByID(static_cast<ObjectID>(order[next]));
	if (!builder)
		return;
	s_bar.builder = builder->getID();
	s_bar.builderTime = now;
	selectObject(builder, TRUE);
}

void activate(Int button, Bool center)
{
	if (button == ButtonBuilder)
	{
		selectNearestBuilder();
		return;
	}
	if (button < 0 || button >= static_cast<Int>(s_bar.slots.size()) || !s_bar.slots[button].alive || !TheGameLogic)
		return;
	selectObject(TheGameLogic->findObjectByID(s_bar.slots[button].id), center);
}

AsciiString builderHotKey()
{
	AsciiString key = TheHotKeyManager ? TheHotKeyManager->searchHotKey(AsciiString("CONTROLBAR:SelectNearestBuilder")) : AsciiString::TheEmptyString;
	key.toLower();
	return key;
}

UnicodeString stripAccelerator(const UnicodeString &label)
{
	UnicodeString text;
	for (const WideChar *ch = label.str(); ch && *ch; ++ch)
	{
		if (*ch == L'&' && ch[1] && ch[1] != L' ')
			++ch;
		text.concat(*ch);
	}
	return text;
}

void showTooltipFor(Int button)
{
	Mouse::RetailTooltip tooltip;
	if (button == ButtonBuilder)
	{
		tooltip.title = stripAccelerator(TheGameText->fetchOrSubstitute("CONTROLBAR:SelectNearestBuilder", L""));
		AsciiString key = builderHotKey();
		key.toUpper();
		tooltip.shortcut.translate(key);
		tooltip.body = TheGameText->fetchOrSubstitute("CONTROLBAR:SelectNearestBuilderDescription", L"");
	}
	else
	{
		const HeroSlot &slot = s_bar.slots[button];
		tooltip.title = slot.hero->getDisplayName();
		UnicodeString rank;
		rank.format(TheGameText->fetchOrSubstitute("TOOLTIP:Rank", L"Level: %d / Maximum Level: %d"), slot.level, static_cast<Int>(LEVEL_COUNT));
		tooltip.body = rank;
		if (slot.alive)
		{
			UnicodeString health;
			health.format(TheGameText->fetchOrSubstitute("TOOLTIP:MaxHealth", L"Health: %d"), static_cast<Int>(slot.maxHealth + 0.5f));
			tooltip.body.concat(L"\n");
			tooltip.body.concat(health);
		}
	}
	if (!tooltip.title.isEmpty())
		TheMouse->setRetailTooltip(tooltip);
}

void ensureLevelText(Int index, Int level)
{
	if (!TheDisplayStringManager)
		return;
	if (!s_bar.levelText[index])
		s_bar.levelText[index] = TheDisplayStringManager->newDisplayString();
	DisplayString *text = s_bar.levelText[index];
	if (!text)
		return;
	if (s_bar.font && text->getFont() != s_bar.font)
		text->setFont(s_bar.font);
	if (s_bar.shownLevel[index] != level)
	{
		UnicodeString number;
		number.format(L"%d", level);
		text->setText(number);
		s_bar.shownLevel[index] = level;
	}
}

void drawHero(const HeroSlot &slot, Int index, const ICoord2D &origin, Real scale, UnsignedInt now, Bool hovered)
{
	const PixelBox portrait = heroBox(origin, scale, index);
	const HeroMemory &memory = memoryFor(slot.hero);

	if (slot.selected)
		drawImageBox(findImage("UiRetailHeroHalo"), centeredHeroBox(origin, scale, index, HaloSize));

	fillCircle(portrait, GameMakeColor(6, 6, 6, 255));
	const Image *image = heroImage(slot);
	TheDisplay->setCircularClip(portrait.centerX(), portrait.centerY(), portrait.width() / 2);
	if (TheDisplay->hasImageTexture(image))
	{
		const Bool deadArt = !slot.alive && image != slot.hero->getButtonImage();
		const Color tint = slot.alive || deadArt ? 0xFFFFFFFF : GameMakeColor(255, 120, 96, 255);
		TheDisplay->drawImage(image, portrait.x0, portrait.y0, portrait.x1, portrait.y1, tint,
			slot.alive || deadArt ? Display::DRAW_IMAGE_ALPHA : Display::DRAW_IMAGE_GRAYSCALE);
	}
	if (slot.attacked)
	{
		const Real flicker = static_cast<Real>(UiHudRules::pulse(now, FlickerMs));
		TheDisplay->drawFillRect(portrait.x0, portrait.y0, portrait.width(), portrait.height(), withAlpha(220, 30, 20, 0.15f + 0.35f * flicker));
	}
	if (hovered)
		TheDisplay->drawFillRect(portrait.x0, portrait.y0, portrait.width(), portrait.height(), GameMakeColor(255, 236, 170, 40));
	TheDisplay->setCircularClip(0, 0, 0);

	if (slot.reviving)
		drawImageBox(findImage("UiRetailHeroRevive"), centeredHeroBox(origin, scale, index, HeroSize));
	if (!slot.alive)
		drawImageBox(findImage("UiRetailHeroFireRing"), centeredHeroBox(origin, scale, index, HaloSize));
	else if (slot.attacked)
		drawImageBox(findImage("UiRetailHeroFireRing"), centeredHeroBox(origin, scale, index, HaloSize),
			withAlpha(255, 255, 255, 0.35f + 0.65f * static_cast<Real>(UiHudRules::pulse(now, FlickerMs))));

	if (memory.levelUp && now - memory.levelUpStart < LevelUpMs)
	{
		const Real t = static_cast<Real>(now - memory.levelUpStart) / LevelUpMs;
		drawImageBox(findImage("UiRetailHeroLevelBurst"), centeredHeroBox(origin, scale, index, BurstSize * (0.6f + 0.4f * t)),
			withAlpha(255, 255, 255, 1.0f - t));
	}

	drawImageBox(findImage("UiRetailHeroTrough"), heroBox(origin, scale, index, -13.15f, 32.85f, 77.0f, 34.0f));
	if (slot.alive && slot.health > 0.0f)
	{
		const char *arc = "UiRetailHeroArcGreen";
		switch (UiHudRules::healthStage(slot.health))
		{
			case UiHudRules::HealthStage::Yellow: arc = "UiRetailHeroArcYellow"; break;
			case UiHudRules::HealthStage::Red: arc = "UiRetailHeroArcRed"; break;
			default: break;
		}
		const Image *arcImage = findImage(arc);
		const PixelBox full = heroBox(origin, scale, index, 9.0f, 45.0f, 57.0f, 12.0f);
		if (TheDisplay->hasImageTexture(arcImage))
		{
			const Int right = full.x0 + roundPixel(full.width() * slot.health);
			Region2D part;
			part.lo.x = 0.0f;
			part.lo.y = 0.0f;
			part.hi.x = slot.health;
			part.hi.y = 1.0f;
			TheDisplay->drawImagePart(arcImage, full.x0, full.y0, right, full.y1, part);
		}
	}
	const PixelBox badge = heroBox(origin, scale, index, -5.69f, 37.86f, 21.08f, 21.08f);
	fillCircle(badge, GameMakeColor(10, 18, 48, 255));
	TheDisplay->setCircularClip(badge.centerX(), badge.centerY(), badge.width() / 2);
	drawImageBox(findImage("UiRetailHeroBadge"), badge);
	TheDisplay->setCircularClip(0, 0, 0);
	ensureLevelText(index, slot.level);
	if (DisplayString *text = s_bar.levelText[index])
	{
		Int width, height;
		text->getSize(&width, &height);
		text->draw(badge.centerX() - width / 2, badge.centerY() - height / 2, GameMakeColor(255, 255, 255, 255), GameMakeColor(0, 0, 0, 255), 1, 1);
	}
}

void drawBuilder(const ICoord2D &origin, Real scale, Bool hovered, Bool pressed)
{
	const PixelBox box = designBox(origin, scale, BuilderX, BuilderY, BuilderSize, BuilderSize);
	fillCircle(box, GameMakeColor(12, 74, 80, 255));
	const Image *icon = builderImage();
	if (TheDisplay->hasImageTexture(icon))
	{
		const Int inset = MAX(1, box.width() / 5) + (pressed ? 1 : 0);
		TheDisplay->drawImage(icon, box.x0 + inset, box.y0 + inset, box.x1 - inset, box.y1 - inset);
	}
	drawImageBox(findImage("UiRetailGlass"), box, hovered ? 0xFFFFFFFF : 0xD0FFFFFF);
}

void drawHeroBar(GameWindow *, WinInstanceData *)
{
	if (!localPlayer())
		return;
	const Real scale = uiScale();
	const ICoord2D origin = barOrigin(scale);
	const ICoord2D mouse = TheMouse->getMouseStatus()->pos;
	const Int hovered = hitTest(mouse.x, mouse.y);
	const UnsignedInt now = timeGetTime();
	if (TheFontLibrary && (!s_bar.font || s_bar.fontScale != scale))
	{
		s_bar.fontScale = scale;
		s_bar.font = TheFontLibrary->getUiFont(AsciiString("Albertus MT"), 12, FALSE, TheDisplay->getWidth(), TheDisplay->getHeight());
	}
	drawBuilder(origin, scale, hovered == ButtonBuilder, s_bar.pressed == ButtonBuilder);
	for (Int i = 0; i < static_cast<Int>(s_bar.slots.size()); ++i)
		drawHero(s_bar.slots[i], i, origin, scale, now, hovered == i);
}

WindowMsgHandledType heroBarInput(GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData)
{
	return RetailHeroBar::handleInput(msg, mData1 & 0xFFFF, mData1 >> 16) ? MSG_HANDLED : MSG_IGNORED;
}

void heroBarTooltip(GameWindow *, WinInstanceData *, UnsignedInt mouse)
{
	RetailHeroBar::showTooltip(mouse & 0xFFFF, mouse >> 16);
}
}

namespace RetailHeroBar
{
void init(GameWindow *master)
{
	s_bar.bar = TheWindowManager->winGetWindowFromId(master, NAMEKEY("ControlBar.wnd:HeroBar"));
	if (!s_bar.bar)
		return;
	s_bar.bar->winSetDrawFunc(drawHeroBar);
	s_bar.bar->winSetInputFunc(heroBarInput);
	s_bar.bar->winSetTooltipFunc(heroBarTooltip);
}

void reset()
{
	s_bar.order.clear();
	s_bar.slots.clear();
	s_bar.memory.clear();
	s_bar.populated = FALSE;
	s_bar.pressed = ButtonNone;
	s_bar.builder = INVALID_ID;
	s_bar.builderTime = 0;
	s_bar.shownLevel.fill(0);
	s_bar.font = nullptr;
}

void update()
{
	if (!s_bar.bar)
		return;
	const Player *player = localPlayer();
	if (!player || !TheGameLogic || !player->isPlayerActive())
	{
		s_bar.slots.clear();
		return;
	}
	std::vector<HeroSlot> present = collectHeroes(player);
	trackEvents(present);
	std::vector<const ThingTemplate *> keys;
	for (const HeroSlot &slot : present)
		keys.push_back(slot.hero);
	UiHudRules::reflow(s_bar.order, keys);
	if (s_bar.order.size() > static_cast<std::size_t>(MaxHeroes))
		s_bar.order.resize(MaxHeroes);
	s_bar.slots.clear();
	for (const ThingTemplate *hero : s_bar.order)
		for (const HeroSlot &slot : present)
			if (slot.hero == hero)
				s_bar.slots.push_back(slot);
}

Bool handleInput(UnsignedInt msg, Int x, Int y)
{
	const Int button = hitTest(x, y);
	switch (msg)
	{
		case GWM_LEFT_DOWN:
			s_bar.pressed = button;
			return button != ButtonNone;
		case GWM_LEFT_UP:
		{
			const Int pressed = s_bar.pressed;
			s_bar.pressed = ButtonNone;
			if (button != ButtonNone && button == pressed)
				activate(button, FALSE);
			return button != ButtonNone || pressed != ButtonNone;
		}
		case GWM_LEFT_DOUBLE_CLICK:
			if (button != ButtonNone)
				activate(button, TRUE);
			return button != ButtonNone;
		case GWM_LEFT_DRAG:
		case GWM_RIGHT_DOWN:
		case GWM_RIGHT_UP:
		case GWM_MOUSE_POS:
			return button != ButtonNone;
		default:
			return FALSE;
	}
}

Bool showTooltip(Int x, Int y)
{
	const Int button = hitTest(x, y);
	if (button == ButtonNone)
		return FALSE;
	showTooltipFor(button);
	return TRUE;
}

Bool executeHotKey(const AsciiString &key)
{
	if (!s_bar.bar || s_bar.bar->winIsHidden() || key.isEmpty() || key.compareNoCase(builderHotKey()) != 0)
		return FALSE;
	selectNearestBuilder();
	return TRUE;
}
}
