#include "PreRTS.h"

#include <array>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "Common/AudioEventRTS.h"
#include "Common/GameAudio.h"
#include "Common/GameCommon.h"
#include "Common/INI.h"
#include "Lib/RGBColor.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"

#include "GameClient/ControlBar.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/HotKey.h"
#include "GameClient/Image.h"
#include "GameClient/Mouse.h"
#include "GameClient/RetailHudExtras.h"
#include "GameClient/UiHudRules.h"
#include "GameClient/UiScale.h"

#include "GameLogic/GameLogic.h"

namespace
{
constexpr Int SpellSlots = 11;
constexpr Real SpellButtonX = 12.0f;
constexpr Real SpellButtonY = 462.0f;
constexpr Real SpellButtonSize = 42.0f;
constexpr Real SpellSpacing = 46.0f;
constexpr Real SpellCapOffset = 34.0f;

struct VinePiece
{
	const char *image;
	Real y;
	Real width;
	Real height;
};

constexpr VinePiece SpellVine[SpellSlots] = {
	{ "UiRetailSpellVine1", 448.0f, 46.0f, 117.0f },
	{ "UiRetailSpellVine2", 387.0f, 57.0f, 74.0f },
	{ "UiRetailSpellVine3", 350.0f, 29.0f, 62.0f },
	{ "UiRetailSpellVine4", 315.0f, 24.0f, 61.0f },
	{ "UiRetailSpellVine5", 259.0f, 32.0f, 65.0f },
	{ "UiRetailSpellVine2", 203.0f, 57.0f, 74.0f },
	{ "UiRetailSpellVine3", 166.0f, 29.0f, 62.0f },
	{ "UiRetailSpellVine4", 130.0f, 24.0f, 61.0f },
	{ "UiRetailSpellVine5", 80.0f, 32.0f, 65.0f },
	{ "UiRetailSpellVine2", 34.0f, 57.0f, 74.0f },
	{ "UiRetailSpellVineTop", 0.0f, 33.0f, 82.0f },
};
constexpr VinePiece SpellVineCap = { "UiRetailSpellVineTop", 0.0f, 33.0f, 82.0f };

constexpr Real PalantirDesignWidth = 470.0f;
constexpr Real PowerDiscX = 102.9f;
constexpr Real PowerDiscY = 60.0f;
constexpr Real PowerDiscSize = 50.2f;
constexpr Real PowerTextX = 128.5f;
constexpr Real PowerTextY = 85.3f;
constexpr Int PowerTextSize = 26;

constexpr Real ClockCharSpacing = 8.0f;
constexpr Real ClockTextY = 19.0f;

constexpr Real NoteTopY = 15.0f;
constexpr Real NoteArtX = 9.3f;
constexpr Real NoteArtWidth = 540.0f;
constexpr Real NoteMiddleY = 52.0f;
constexpr Real NoteMiddleHeight = 83.0f;
constexpr Real NoteBottomY = 137.0f;
constexpr Real NoteLeftWidth = 45.3f;
constexpr Real NoteRightWidth = 40.0f;
constexpr Real NoteBodyX = 45.3f;
constexpr Real NoteBodyY = 42.0f;
constexpr Real NoteBodyWidth = 482.7f;
constexpr Real NoteBodyHeight = 100.0f;
constexpr Real NoteTextX = 56.0f;
constexpr Real NoteTextY = 52.0f;
constexpr Real NoteTextWidth = 456.0f;
constexpr Real NoteTextHeight = 83.0f;
constexpr Real NoteIconSize = 83.0f;
constexpr Real NoteCloseX = 500.0f;
constexpr Real NoteCloseY = 35.0f;
constexpr Real NoteCloseWidth = 48.0f;
constexpr Real NoteCloseHeight = 15.0f;
constexpr UnsignedInt NoteOpenMs = 200;
constexpr UnsignedInt NoteFadeMs = 500;

struct SpellEntry
{
	const CommandButton *command = nullptr;
};

struct EdgeState
{
	GameWindow *spellBook = nullptr;
	GameWindow *sideBar = nullptr;
	GameWindow *clock = nullptr;
	std::vector<SpellEntry> spells;
	Int pressed = -1;
	std::array<DisplayString *, 8> clockChars = {};
	std::wstring clockShown;
	DisplayString *powerText = nullptr;
	Int powerShown = -1;
	GameFont *clockFont = nullptr;
	GameFont *powerFont = nullptr;
	Real fontScale = 0.0f;
};

struct NoteType
{
	AsciiString title;
	AsciiString icon;
};

struct NoteSettings
{
	AsciiString font = "Albertus MT";
	Int size = 16;
	Bool bold = FALSE;
	RGBColor color = {1.0f, 0.8f, 0.0f};
	AsciiString audio = "Gui_DaveBox";
	std::map<AsciiString, NoteType> types;
};

NoteSettings s_noteSettings;

struct NoteState
{
	GameWindow *window = nullptr;
	DisplayString *title = nullptr;
	DisplayString *text = nullptr;
	const Image *portrait = nullptr;
	UnsignedInt start = 0;
	UnsignedInt holdMs = 0;
	Bool open = FALSE;
	Bool pressed = FALSE;
};

EdgeState s_edges;
NoteState s_note;

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

struct Box
{
	Int x0;
	Int y0;
	Int x1;
	Int y1;
	Int width() const { return x1 - x0; }
	Int height() const { return y1 - y0; }
	Bool contains(Int x, Int y) const { return x >= x0 && x < x1 && y >= y0 && y < y1; }
};

Box windowBox(const GameWindow *window, Real scale, Real x, Real y, Real width, Real height)
{
	Int left, top;
	const_cast<GameWindow *>(window)->winGetScreenPosition(&left, &top);
	return Box{ left + roundPixel(x * scale), top + roundPixel(y * scale), left + roundPixel((x + width) * scale), top + roundPixel((y + height) * scale) };
}

void drawBox(const Image *image, const Box &box, Color color = 0xFFFFFFFF)
{
	if (TheDisplay->hasImageTexture(image))
		TheDisplay->drawImage(image, box.x0, box.y0, box.x1, box.y1, color);
}

Color alphaColor(Int red, Int green, Int blue, Real alpha)
{
	return GameMakeColor(static_cast<UnsignedByte>(red), static_cast<UnsignedByte>(green), static_cast<UnsignedByte>(blue),
		static_cast<UnsignedByte>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f + 0.5f));
}

Player *localPlayer()
{
	return ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
}

DisplayString *ensureString(DisplayString *&text, GameFont *font)
{
	if (!text && TheDisplayStringManager)
		text = TheDisplayStringManager->newDisplayString();
	if (text && font && text->getFont() != font)
		text->setFont(font);
	return text;
}

void refreshFonts(Real scale)
{
	if (!TheFontLibrary || s_edges.fontScale == scale)
		return;
	s_edges.fontScale = scale;
	s_edges.clockFont = TheFontLibrary->getUiFont(AsciiString("Albertus MT"), 14, FALSE, TheDisplay->getWidth(), TheDisplay->getHeight());
	s_edges.powerFont = TheFontLibrary->getUiFont(AsciiString("Albertus MT"), PowerTextSize, FALSE, TheDisplay->getWidth(), TheDisplay->getHeight());
	s_edges.clockShown.clear();
	s_edges.powerShown = -1;
}

Bool hasSciences(const Player *player, const SpecialPowerTemplate *power)
{
	return player && power->isUnlockedFor(player);
}

const CommandSet *spellBookCommandSet(const Player *player)
{
	const PlayerTemplate *side = player ? player->getPlayerTemplate() : nullptr;
	if (!side || !TheThingFactory || !TheControlBar)
		return nullptr;
	const ThingTemplate *book = TheThingFactory->findTemplate(side->getSpellBook(TRUE), FALSE);
	return book ? TheControlBar->findCommandSet(book->friend_getCommandSetString()) : nullptr;
}

void collectSpells()
{
	s_edges.spells.clear();
	const Player *player = localPlayer();
	const CommandSet *set = spellBookCommandSet(player);
	for (Int i = 0; set && i < CommandButtonRules::RETAIL_COMMANDS_PER_SET && static_cast<Int>(s_edges.spells.size()) < SpellSlots; ++i)
	{
		const CommandButton *command = set->getCommandButton(i);
		const SpecialPowerTemplate *power = command ? command->getSpecialPowerTemplate() : nullptr;
		if (power && hasSciences(player, power))
			s_edges.spells.push_back(SpellEntry{ command });
	}
}

Box spellButton(Real scale, Int slot)
{
	return windowBox(s_edges.spellBook, scale, SpellButtonX, SpellButtonY - SpellSpacing * slot, SpellButtonSize, SpellButtonSize);
}

Int spellAt(Int x, Int y)
{
	if (!s_edges.spellBook || s_edges.spellBook->winIsHidden())
		return -1;
	const Real scale = uiScale();
	for (Int i = 0; i < static_cast<Int>(s_edges.spells.size()); ++i)
	{
		const Box box = spellButton(scale, i);
		const Real radius = box.width() * 0.5f;
		const Real dx = x - (box.x0 + radius);
		const Real dy = y - (box.y0 + radius);
		if (dx * dx + dy * dy <= radius * radius)
			return i;
	}
	return -1;
}

void drawRoundButton(const Box &box, const Image *image, Bool hovered)
{
	const Int radius = box.width() / 2;
	TheDisplay->setCircularClip(box.x0 + radius, box.y0 + radius, radius);
	TheDisplay->drawFillRect(box.x0, box.y0, box.width(), box.height(), GameMakeColor(6, 6, 6, 255));
	if (TheDisplay->hasImageTexture(image))
		TheDisplay->drawImage(image, box.x0, box.y0, box.x1, box.y1);
	if (hovered)
		TheDisplay->drawFillRect(box.x0, box.y0, box.width(), box.height(), GameMakeColor(255, 236, 170, 48));
	TheDisplay->setCircularClip(0, 0, 0);
	drawBox(findImage("UiRetailGlass"), box, hovered ? 0xFFFFFFFF : 0xD0FFFFFF);
}

void drawSpellBook(GameWindow *window, WinInstanceData *)
{
	const Int count = static_cast<Int>(s_edges.spells.size());
	if (count == 0)
		return;
	const Real scale = uiScale();
	for (Int i = 0; i < count; ++i)
	{
		const VinePiece &piece = SpellVine[i];
		drawBox(findImage(piece.image), windowBox(window, scale, 0.0f, piece.y, piece.width, piece.height));
	}
	if (count < SpellSlots)
	{
		const Real capY = SpellVine[count - 1].y - SpellCapOffset;
		drawBox(findImage(SpellVineCap.image), windowBox(window, scale, 0.0f, capY, SpellVineCap.width, SpellVineCap.height));
	}
	const ICoord2D mouse = TheMouse->getMouseStatus()->pos;
	const Int hovered = spellAt(mouse.x, mouse.y);
	for (Int i = 0; i < count; ++i)
		drawRoundButton(spellButton(scale, i), s_edges.spells[i].command->getButtonImage(), hovered == i);
}

WindowMsgHandledType spellBookInput(GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData)
{
	return RetailEdges::handleInput(msg, mData1 & 0xFFFF, mData1 >> 16) ? MSG_HANDLED : MSG_IGNORED;
}

void spellBookTooltip(GameWindow *, WinInstanceData *, UnsignedInt mouse)
{
	RetailEdges::showTooltip(mouse & 0xFFFF, mouse >> 16);
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

void drawSideVine(GameWindow *window, WinInstanceData *)
{
	RetailEdges::drawSideColumnVine(window);
}

void drawImageFlipped(const Image *image, const Box &box)
{
	if (!TheDisplay->hasImageTexture(image))
		return;
	Region2D part;
	part.lo.x = 0.0f;
	part.hi.x = 1.0f;
	part.lo.y = 1.0f;
	part.hi.y = 0.0f;
	TheDisplay->drawImagePart(image, box.x0, box.y0, box.x1, box.y1, part);
}

void drawOutlinedText(DisplayString *text, Int centerX, Int centerY, Color color)
{
	if (!text || text->getTextLength() == 0)
		return;
	Int width, height;
	text->getSize(&width, &height);
	const Int x = centerX - width / 2;
	const Int y = centerY - height / 2;
	const Color black = GameMakeColor(0, 0, 0, 255);
	for (Int dy = -1; dy <= 1; ++dy)
		for (Int dx = -1; dx <= 1; ++dx)
			if (dx != 0 || dy != 0)
				text->draw(x + dx, y + dy, black, black, 0, 0);
	text->draw(x, y, color, black, 0, 0);
}

void drawClock(GameWindow *window, WinInstanceData *)
{
	if (!TheGameLogic)
		return;
	const Real scale = uiScale();
	refreshFonts(scale);
	drawBox(findImage("UiRetailClock"), windowBox(window, scale, 0.0f, 0.0f, 119.0f, 39.0f));
	const std::wstring time = UiHudRules::clockText(TheGameLogic->getFrame() / LOGICFRAMES_PER_SECOND);
	const Int count = MIN(static_cast<Int>(time.size()), static_cast<Int>(s_edges.clockChars.size()));
	const Bool changed = time != s_edges.clockShown;
	s_edges.clockShown = time;
	Int left, top, width, height;
	window->winGetScreenPosition(&left, &top);
	window->winGetSize(&width, &height);
	const Real center = left + width * 0.5f;
	for (Int i = 0; i < count; ++i)
	{
		DisplayString *glyph = ensureString(s_edges.clockChars[i], s_edges.clockFont);
		if (!glyph)
			continue;
		if (changed)
		{
			UnicodeString text;
			text.concat(time[i]);
			glyph->setText(text);
		}
		const Real x = center + (i - (count - 1) * 0.5f) * ClockCharSpacing * scale;
		Int glyphWidth, glyphHeight;
		glyph->getSize(&glyphWidth, &glyphHeight);
		glyph->draw(roundPixel(x) - glyphWidth / 2, top + roundPixel(ClockTextY * scale) - glyphHeight / 2,
			GameMakeColor(255, 255, 255, 255), GameMakeColor(0, 0, 0, 255), 1, 1);
	}
}

Box noteBox(Real scale, Real x, Real y, Real width, Real height)
{
	return windowBox(s_note.window, scale, x, y, width, height);
}

Real noteAlpha(UnsignedInt now)
{
	const UnsignedInt elapsed = now - s_note.start;
	const Real opening = elapsed < NoteOpenMs ? static_cast<Real>(elapsed) / NoteOpenMs : 1.0f;
	return opening * static_cast<Real>(UiHudRules::fadeAlpha(elapsed, s_note.holdMs, NoteFadeMs));
}

void drawCloseGlyph(const Box &pill, Real alpha)
{
	const Color gold = alphaColor(241, 187, 1, alpha);
	const Real size = pill.height() * 0.6f;
	const Real cx = (pill.x0 + pill.x1) * 0.5f;
	const Real cy = (pill.y0 + pill.y1) * 0.5f;
	const Real width = MAX(1.0f, pill.height() * 0.15f);
	TheDisplay->drawLine(roundPixel(cx - size * 0.5f), roundPixel(cy - size * 0.5f), roundPixel(cx + size * 0.5f), roundPixel(cy + size * 0.5f), width, gold);
	TheDisplay->drawLine(roundPixel(cx - size * 0.5f), roundPixel(cy + size * 0.5f), roundPixel(cx + size * 0.5f), roundPixel(cy - size * 0.5f), width, gold);
}

void drawNotification(GameWindow *, WinInstanceData *)
{
	if (!s_note.open)
		return;
	const UnsignedInt now = timeGetTime();
	const Real alpha = noteAlpha(now);
	if (alpha <= 0.0f)
		return;
	const Real scale = uiScale();
	const Color art = alphaColor(255, 255, 255, alpha);
	const Box body = noteBox(scale, NoteBodyX, NoteBodyY, NoteBodyWidth, NoteBodyHeight);
	TheDisplay->drawFillRect(body.x0, body.y0, body.width(), body.height(), alphaColor(0, 0, 0, 0.75f * alpha));
	drawBox(findImage("UiRetailHelpTop"), noteBox(scale, NoteArtX, NoteTopY, NoteArtWidth, NoteMiddleY - NoteTopY), art);
	drawBox(findImage("UiRetailHelpLeft"), noteBox(scale, NoteArtX, NoteMiddleY, NoteLeftWidth, NoteMiddleHeight), art);
	drawBox(findImage("UiRetailHelpRight"), noteBox(scale, NoteArtX + NoteArtWidth - NoteRightWidth, NoteMiddleY, NoteRightWidth, NoteMiddleHeight), art);
	drawBox(findImage("UiRetailHelpBottom"), noteBox(scale, NoteArtX, NoteBottomY, NoteArtWidth, 27.0f), art);

	const Box icon = noteBox(scale, 0.0f, 0.0f, NoteIconSize, NoteIconSize);
	const Box portrait = noteBox(scale, 6.7f, 9.0f, 64.0f, 64.0f);
	if (TheDisplay->hasImageTexture(s_note.portrait))
	{
		const Int radius = portrait.width() / 2;
		TheDisplay->setCircularClip(portrait.x0 + radius, portrait.y0 + radius, radius);
		TheDisplay->drawImage(s_note.portrait, portrait.x0, portrait.y0, portrait.x1, portrait.y1, art);
		TheDisplay->setCircularClip(0, 0, 0);
	}
	drawBox(findImage("UiRetailNotifyIconFrame"), icon, art);

	const ICoord2D mouse = TheMouse->getMouseStatus()->pos;
	const Box pill = noteBox(scale, NoteCloseX, NoteCloseY, NoteCloseWidth, NoteCloseHeight);
	const char *pillImage = s_note.pressed ? "UiRetailNotifyCloseDown" : pill.contains(mouse.x, mouse.y) ? "UiRetailNotifyCloseOver" : "UiRetailNotifyCloseUp";
	drawBox(findImage(pillImage), pill, art);
	drawCloseGlyph(pill, alpha);

	if (s_note.text && s_note.text->getTextLength() > 0)
	{
		const Box area = noteBox(scale, NoteTextX, NoteTextY, NoteTextWidth, NoteTextHeight);
		Int width, height;
		s_note.text->getSize(&width, &height);
		const RGBColor &rgb = s_noteSettings.color;
		const Color color = alphaColor(static_cast<Int>(rgb.red * 255), static_cast<Int>(rgb.green * 255), static_cast<Int>(rgb.blue * 255), alpha);
		const Color shadow = alphaColor(0, 0, 0, alpha);
		Int top = area.y0 + (area.height() - height) / 2;
		if (s_note.title && s_note.title->getTextLength() > 0)
		{
			Int titleWidth, titleHeight;
			s_note.title->getSize(&titleWidth, &titleHeight);
			top -= titleHeight / 2;
			s_note.title->draw(area.x0 + (area.width() - titleWidth) / 2, top, color, shadow, 1, 1);
			top += titleHeight;
		}
		s_note.text->draw(area.x0 + (area.width() - width) / 2, top, color, shadow, 1, 1);
	}
}

WindowMsgHandledType notificationInput(GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData)
{
	return RetailNotification::handleInput(msg, mData1 & 0xFFFF, mData1 >> 16) ? MSG_HANDLED : MSG_IGNORED;
}
}

namespace RetailEdges
{
void init(GameWindow *master)
{
	s_edges.spellBook = TheWindowManager->winGetWindowFromId(master, NAMEKEY("ControlBar.wnd:SpellBook"));
	s_edges.sideBar = TheWindowManager->winGetWindowFromId(master, NAMEKEY("ControlBar.wnd:SideBar"));
	s_edges.clock = TheWindowManager->winGetWindowFromId(master, NAMEKEY("ControlBar.wnd:GameClock"));
	if (s_edges.spellBook)
	{
		s_edges.spellBook->winSetDrawFunc(drawSpellBook);
		s_edges.spellBook->winSetInputFunc(spellBookInput);
		s_edges.spellBook->winSetTooltipFunc(spellBookTooltip);
	}
	if (s_edges.sideBar)
		s_edges.sideBar->winSetDrawFunc(drawSideVine);
	if (s_edges.clock)
		s_edges.clock->winSetDrawFunc(drawClock);
}

void reset()
{
	s_edges.spells.clear();
	s_edges.pressed = -1;
	s_edges.clockShown.clear();
	s_edges.powerShown = -1;
	s_edges.fontScale = 0.0f;
	if (s_edges.clock)
		s_edges.clock->winHide(TRUE);
}

void update()
{
	const Player *player = localPlayer();
	collectSpells();
	if (s_edges.clock)
		s_edges.clock->winHide(!player || !TheGameLogic || !TheGameLogic->isInGame());
}

void drawSideColumnVine(GameWindow *sideBar)
{
	std::vector<Box> frames;
	for (Int i = 1; i <= 12; ++i)
	{
		AsciiString name;
		name.format("ControlBar.wnd:SideButton%02d", i);
		GameWindow *button = TheWindowManager->winGetWindowFromId(sideBar, TheNameKeyGenerator->nameToKey(name));
		if (!button || button->winIsHidden())
			continue;
		Int x, y, width, height;
		button->winGetScreenPosition(&x, &y);
		button->winGetSize(&width, &height);
		const Int pad = MAX(1, width / 21);
		frames.push_back(Box{ x - pad, y - pad, x + width + pad, y + height + pad });
	}
	if (frames.empty())
		return;
	const Image *link = findImage("UiRetailSideLink");
	const Image *curl = findImage("UiRetailSideTopCurl");
	const Real unit = frames.front().width() / 46.0f;
	for (std::size_t i = 0; i + 1 < frames.size(); ++i)
	{
		const Box &frame = frames[i];
		const Int top = frame.y0 + roundPixel(23.0f * unit);
		drawBox(link, Box{ frame.x0, top, frame.x1, top + frame.height() });
	}
	const Box &first = frames.front();
	drawImageFlipped(curl, Box{ first.x0, first.y0 - roundPixel(40.0f * unit), first.x1, first.y0 + roundPixel(23.0f * unit) });
	const Box &last = frames.back();
	drawBox(curl, Box{ last.x0, last.y0 + roundPixel(22.0f * unit), last.x1, last.y0 + roundPixel(85.0f * unit) });
}

void drawPalantirTop(GameWindow *palantir)
{
	const Player *player = localPlayer();
	if (!player || !palantir)
		return;
	Int x, y, width, height;
	palantir->winGetScreenPosition(&x, &y);
	palantir->winGetSize(&width, &height);
	const Real scale = width / PalantirDesignWidth;
	refreshFonts(uiScale());
	drawBox(findImage("UiRetailPowerDisc"), Box{ x + roundPixel(PowerDiscX * scale), y + roundPixel(PowerDiscY * scale),
		x + roundPixel((PowerDiscX + PowerDiscSize) * scale), y + roundPixel((PowerDiscY + PowerDiscSize) * scale) });
	DisplayString *text = ensureString(s_edges.powerText, s_edges.powerFont);
	const Int points = player->getSciencePurchasePoints();
	if (text && points != s_edges.powerShown)
	{
		UnicodeString number;
		number.format(L"%d", points);
		text->setText(number);
		s_edges.powerShown = points;
	}
	drawOutlinedText(text, x + roundPixel(PowerTextX * scale), y + roundPixel(PowerTextY * scale), GameMakeColor(255, 255, 255, 255));
}

Bool handleInput(UnsignedInt msg, Int x, Int y)
{
	const Int spell = spellAt(x, y);
	switch (msg)
	{
		case GWM_LEFT_DOWN:
			s_edges.pressed = spell;
			return spell >= 0;
		case GWM_LEFT_UP:
		{
			const Bool used = spell >= 0 || s_edges.pressed >= 0;
			s_edges.pressed = -1;
			return used;
		}
		case GWM_LEFT_DOUBLE_CLICK:
		case GWM_LEFT_DRAG:
		case GWM_RIGHT_DOWN:
		case GWM_RIGHT_UP:
		case GWM_MOUSE_POS:
			return spell >= 0;
		default:
			return FALSE;
	}
}

Bool showTooltip(Int x, Int y)
{
	const Int spell = spellAt(x, y);
	if (spell < 0)
		return FALSE;
	const CommandButton *command = s_edges.spells[spell].command;
	Mouse::RetailTooltip tooltip;
	tooltip.title = stripAccelerator(TheGameText->fetchOrSubstitute(command->getTextLabel().str(), L""));
	AsciiString key = TheHotKeyManager ? TheHotKeyManager->searchHotKey(command->getTextLabel()) : AsciiString::TheEmptyString;
	key.toUpper();
	tooltip.shortcut.translate(key);
	tooltip.body = TheGameText->fetchOrSubstitute(command->getDescriptionLabel().str(), L"");
	if (tooltip.title.isEmpty())
		return FALSE;
	TheMouse->setRetailTooltip(tooltip);
	return TRUE;
}
}

namespace RetailNotification
{
void load()
{
	INI ini;
	ini.loadFileDirectory("Data\\INI\\InGameNotificationBox", INI_LOAD_OVERWRITE, nullptr, 0);
}

static void parseFont(INI *ini, void *instance, void *, const void *)
{
	auto &settings = *static_cast<NoteSettings *>(instance);
	settings.font = ini->getNextQuotedAsciiString();
	settings.size = INI::scanInt(ini->getNextToken());
	settings.bold = INI::scanBool(ini->getNextToken());
}

static void parseType(INI *ini, void *instance, void *, const void *)
{
	static const FieldParse fields[] = {
		{ "Title", INI::parseAsciiString, nullptr, offsetof(NoteType, title) },
		{ "Icon", INI::parseAsciiString, nullptr, offsetof(NoteType, icon) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	AsciiString name = ini->getNextToken();
	name.toLower();
	NoteType type;
	ini->initFromINI(&type, fields);
	static_cast<NoteSettings *>(instance)->types[name] = type;
}

void parse(INI *ini)
{
	static const FieldParse fields[] = {
		{ "Font", parseFont, nullptr, 0 },
		{ "DefaultMessageFont", parseFont, nullptr, 0 },
		{ "DefaultMessageColor", INI::parseRGBColor, nullptr, offsetof(NoteSettings, color) },
		{ "DefaultOpenAudio", INI::parseAsciiString, nullptr, offsetof(NoteSettings, audio) },
		{ "NotificationType", parseType, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(&s_noteSettings, fields);
}

void init(GameWindow *master)
{
	s_note.window = TheWindowManager->winGetWindowFromId(master, NAMEKEY("ControlBar.wnd:Notification"));
	if (!s_note.window)
		return;
	s_note.window->winSetDrawFunc(drawNotification);
	s_note.window->winSetInputFunc(notificationInput);
}

void reset()
{
	s_note.open = FALSE;
	s_note.pressed = FALSE;
	s_note.portrait = nullptr;
	if (s_note.window)
		s_note.window->winHide(TRUE);
}

static void showNote(const UnicodeString &title, const UnicodeString &message, const Image *portrait, Real seconds)
{
	if (!s_note.window || message.isEmpty())
		return;
	GameFont *font = TheFontLibrary ? TheFontLibrary->getUiFont(s_noteSettings.font, s_noteSettings.size, s_noteSettings.bold, TheDisplay->getWidth(), TheDisplay->getHeight()) : nullptr;
	DisplayString *text = ensureString(s_note.text, font);
	if (!text)
		return;
	text->setText(message);
	if (DisplayString *heading = ensureString(s_note.title, font))
		heading->setText(title);
	s_note.portrait = portrait;
	s_note.start = timeGetTime();
	s_note.holdMs = static_cast<UnsignedInt>(MAX(0.0f, seconds) * 1000.0f + 0.5f);
	s_note.open = TRUE;
	s_note.pressed = FALSE;
	s_note.window->winHide(FALSE);
	if (TheAudio && !s_noteSettings.audio.isEmpty())
	{
		AudioEventRTS open(s_noteSettings.audio);
		TheAudio->addAudioEvent(&open);
	}
}

void show(const UnicodeString &message, const Image *portrait, Real seconds)
{
	showNote(UnicodeString(), message, portrait, seconds);
}

void showType(const AsciiString &type, const UnicodeString &message, Real seconds)
{
	AsciiString key = type;
	key.toLower();
	const auto entry = s_noteSettings.types.find(key);
	if (entry == s_noteSettings.types.end())
	{
		show(message, nullptr, seconds);
		return;
	}
	const NoteType &style = entry->second;
	const UnicodeString title = !style.title.isEmpty() && TheGameText ? TheGameText->fetch(style.title.str()) : UnicodeString();
	showNote(title, message, style.icon.isEmpty() ? nullptr : findImage(style.icon.str()), seconds);
}

void update()
{
	if (!s_note.window || !s_note.open)
		return;
	if (timeGetTime() - s_note.start >= s_note.holdMs + NoteFadeMs)
		reset();
}

Bool handleInput(UnsignedInt msg, Int x, Int y)
{
	if (!s_note.open)
		return FALSE;
	const Bool over = noteBox(uiScale(), NoteCloseX, NoteCloseY, NoteCloseWidth, NoteCloseHeight).contains(x, y);
	if (msg == GWM_LEFT_DOWN && over)
	{
		s_note.pressed = TRUE;
		return TRUE;
	}
	if (msg == GWM_LEFT_UP)
	{
		const Bool close = s_note.pressed && over;
		const Bool used = s_note.pressed || over;
		s_note.pressed = FALSE;
		if (close)
			reset();
		return used;
	}
	return over && (msg == GWM_MOUSE_POS || msg == GWM_LEFT_DRAG || msg == GWM_LEFT_DOUBLE_CLICK);
}
}
