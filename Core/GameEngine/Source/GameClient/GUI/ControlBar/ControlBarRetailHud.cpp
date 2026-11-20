#include "PreRTS.h"

#include "Common/INIException.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Money.h"
#include "Common/Science.h"
#include "Common/ThingTemplate.h"

#include "GameClient/ControlBar.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/Drawable.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Image.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/ProductionUpdate.h"

namespace
{
constexpr Int RetailRingSlots = 6;
constexpr Real PalantirDesignWidth = 470.0f;
constexpr UnsignedInt CounterInterval = 15;

struct DesignRect
{
	Real x;
	Real y;
	Real width;
	Real height;
};

constexpr DesignRect GlobeArt = { 16.35f, 62.5f, 219.0f, 219.0f };
constexpr DesignRect CommandDisc = { 202.2f, 107.9f, 167.0f, 167.0f };
constexpr DesignRect RingSockets = { 253.55f, 91.55f, 128.0f, 198.0f };
constexpr DesignRect Frame = { 0.0f, 44.0f, 384.0f, 256.0f };
constexpr DesignRect TopPlate = { 51.0f, 60.0f, 155.0f, 64.0f };
constexpr DesignRect OptionsKey = { 70.0f, 76.0f, 31.0f, 31.0f };
constexpr DesignRect Objectives = { 156.0f, 76.0f, 31.0f, 31.0f };
constexpr DesignRect PlaqueArea = { 20.0f, 240.0f, 210.0f, 50.0f };

const Image *findImage(const char *name)
{
	return TheMappedImageCollection ? TheMappedImageCollection->findImageByName(name) : nullptr;
}

Bool playerIsEvil(const Player *player)
{
	if (!player || !TheScienceStore)
		return FALSE;
	const ScienceType evil = TheScienceStore->getScienceFromInternalName("SCIENCE_EVIL");
	return evil != SCIENCE_INVALID && player->hasScience(evil);
}

void drawDesignImage(const char *name, const DesignRect &rect, Int originX, Int originY, Real scale,
	Color color = 0xFFFFFFFF, Display::DrawImageMode mode = Display::DRAW_IMAGE_ALPHA)
{
	const Image *image = findImage(name);
	if (!TheDisplay->hasImageTexture(image))
		return;
	const Int x0 = originX + static_cast<Int>(std::floor(rect.x * scale + 0.5f));
	const Int y0 = originY + static_cast<Int>(std::floor(rect.y * scale + 0.5f));
	const Int x1 = originX + static_cast<Int>(std::floor((rect.x + rect.width) * scale + 0.5f));
	const Int y1 = originY + static_cast<Int>(std::floor((rect.y + rect.height) * scale + 0.5f));
	TheDisplay->drawImage(image, x0, y0, x1, y1, color, mode);
}

void drawFilledCircle(Int x, Int y, Int width, Int height, Color color)
{
	const Int radius = MIN(width, height) / 2;
	if (radius <= 0)
		return;
	TheDisplay->setCircularClip(x + width / 2, y + height / 2, radius);
	TheDisplay->drawFillRect(x, y, width, height, color);
	TheDisplay->setCircularClip(0, 0, 0);
}

void windowRect(const GameWindow *window, Int &x, Int &y, Int &width, Int &height)
{
	const_cast<GameWindow *>(window)->winGetScreenPosition(&x, &y);
	const_cast<GameWindow *>(window)->winGetSize(&width, &height);
}

void drawRetailPalantirBack(GameWindow *window, WinInstanceData *)
{
	Int x, y, width, height;
	windowRect(window, x, y, width, height);
	const Real scale = width / PalantirDesignWidth;
	drawDesignImage("UiRetailGlobe", GlobeArt, x, y, scale);
	const Int discX = x + static_cast<Int>(std::floor(CommandDisc.x * scale + 0.5f));
	const Int discY = y + static_cast<Int>(std::floor(CommandDisc.y * scale + 0.5f));
	const Int discSize = static_cast<Int>(std::floor(CommandDisc.width * scale + 0.5f));
	drawFilledCircle(discX, discY, discSize, discSize, GameMakeColor(10, 9, 8, 255));
}

void drawRetailPalantirFrame(GameWindow *window, WinInstanceData *)
{
	Int x, y, width, height;
	window->winGetParent()->winGetScreenPosition(&x, &y);
	window->winGetParent()->winGetSize(&width, &height);
	const Real scale = width / PalantirDesignWidth;
	const Bool evil = playerIsEvil(ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr);
	drawDesignImage(evil ? "UiRetailFrameEvil" : "UiRetailFrameGood", Frame, x, y, scale);
	TheControlBar->drawRetailSocketFills();
	drawDesignImage("UiRetailRingSockets", RingSockets, x, y, scale);
	drawDesignImage("UiRetailTopPlate", TopPlate, x, y, scale);
	drawDesignImage("UiRetailOptionsKey", OptionsKey, x, y, scale);
	drawDesignImage("UiRetailObjectives", Objectives, x, y, scale);
	TheControlBar->drawRetailTexts();
}

void drawRetailPortrait(GameWindow *window, WinInstanceData *)
{
	Int x, y, width, height;
	windowRect(window, x, y, width, height);
	const Image *image = window->winGetEnabledImage(0);
	if (!TheDisplay->hasImageTexture(image))
		return;
	TheDisplay->setCircularClip(x + width / 2, y + height / 2, MIN(width, height) / 2);
	TheDisplay->drawImage(image, x, y, x + width, y + height);
	TheDisplay->setCircularClip(0, 0, 0);
}

void drawShadowedText(DisplayString *text, Int x, Int y, Int width, Int height, Int align, Color color)
{
	if (!text || text->getTextLength() == 0)
		return;
	Int textWidth, textHeight;
	text->getSize(&textWidth, &textHeight);
	Int left = x;
	if (align == 1)
		left = x + width - textWidth;
	else if (align == 2)
		left = x + (width - textWidth) / 2;
	text->draw(left, y + (height - textHeight) / 2, color, GameMakeColor(0, 0, 0, 255), 1, 1);
}

void drawTextWindow(GameWindow *window, Int align, Color color)
{
	Int x, y, width, height;
	windowRect(window, x, y, width, height);
	const TextData *data = static_cast<const TextData *>(window->winGetUserData());
	if (data)
		drawShadowedText(data->text, x, y, width, height, align, color);
}

Bool insideCircle(Int px, Int py, Int originX, Int originY, Real scale, Real cx, Real cy, Real radius)
{
	const Real dx = (px - originX) / scale - cx;
	const Real dy = (py - originY) / scale - cy;
	return dx * dx + dy * dy <= radius * radius;
}

Bool insideRect(Int px, Int py, Int originX, Int originY, Real scale, const DesignRect &rect)
{
	const Real x = (px - originX) / scale;
	const Real y = (py - originY) / scale;
	return x >= rect.x && x <= rect.x + rect.width && y >= rect.y && y <= rect.y + rect.height;
}

Bool isMouseMessage(UnsignedInt msg)
{
	return msg == GWM_LEFT_DOWN || msg == GWM_LEFT_UP || msg == GWM_LEFT_DRAG || msg == GWM_LEFT_DOUBLE_CLICK
		|| msg == GWM_RIGHT_DOWN || msg == GWM_RIGHT_UP || msg == GWM_RIGHT_DRAG || msg == GWM_RIGHT_DOUBLE_CLICK
		|| msg == GWM_MIDDLE_DOWN || msg == GWM_MIDDLE_UP || msg == GWM_MOUSE_POS;
}

WindowMsgHandledType retailPalantirInput(GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData)
{
	if (!isMouseMessage(msg))
		return MSG_IGNORED;
	Int x, y, width, height;
	windowRect(window, x, y, width, height);
	const Real scale = width / PalantirDesignWidth;
	const Int px = mData1 & 0xFFFF;
	const Int py = mData1 >> 16;
	const Bool inside = insideCircle(px, py, x, y, scale, 125.85f, 172.0f, 118.0f)
		|| insideCircle(px, py, x, y, scale, 285.7f, 191.4f, 92.0f)
		|| insideRect(px, py, x, y, scale, PlaqueArea) || insideRect(px, py, x, y, scale, TopPlate)
		|| TheControlBar->isOverRetailSocket(px, py);
	return inside ? MSG_HANDLED : MSG_IGNORED;
}

WindowMsgHandledType retailPortraitInput(GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2)
{
	return retailPalantirInput(window->winGetParent(), msg, mData1, mData2);
}

WindowMsgHandledType retailSideBarInput(GameWindow *, UnsignedInt msg, WindowMsgData mData1, WindowMsgData)
{
	if (!isMouseMessage(msg))
		return MSG_IGNORED;
	return TheControlBar->isOverRetailSocket(mData1 & 0xFFFF, mData1 >> 16) ? MSG_HANDLED : MSG_IGNORED;
}

void drawRetailCommand(GameWindow *window, WinInstanceData *instance)
{
	Int x, y, width, height;
	windowRect(window, x, y, width, height);
	const Bool enabled = BitIsSet(window->winGetStatus(), WIN_STATUS_ENABLED);
	const Bool highlight = enabled && BitIsSet(instance->getState(), WIN_STATE_HILITED);
	const Bool pressed = BitIsSet(instance->getState(), WIN_STATE_SELECTED);
	const Int slot = TheControlBar->getRetailSlot(window);
	const Bool side = slot >= RetailRingSlots;

	if (side)
	{
		const Int pad = MAX(1, width / 21);
		const Image *frame = findImage("UiRetailSideFrame");
		if (TheDisplay->hasImageTexture(frame))
			TheDisplay->drawImage(frame, x - pad, y - pad, x + width + pad, y + height + pad);
	}

	const Int cx = x + width / 2;
	const Int cy = y + height / 2;
	const Int radius = MIN(width, height) / 2;
	const Int inset = pressed ? MAX(1, width / 18) : 0;
	drawFilledCircle(x, y, width, height, GameMakeColor(6, 6, 6, 255));

	const Image *image = window->winGetEnabledImage(0);
	TheDisplay->setCircularClip(cx, cy, radius);
	if (TheDisplay->hasImageTexture(image))
	{
		Display::DrawImageMode mode = Display::DRAW_IMAGE_ALPHA;
		if (!enabled && !BitIsSet(window->winGetStatus(), WIN_STATUS_ALWAYS_COLOR))
			mode = Display::DRAW_IMAGE_GRAYSCALE;
		TheDisplay->drawImage(image, x + inset, y + inset, x + width - inset, y + height - inset, 0xFFFFFFFF, mode);
	}

	Int count = 0;
	Real progress = 0.0f;
	TheControlBar->getRetailProduction(window, count, progress);
	if (count > 0)
	{
		const Int filled = static_cast<Int>(std::floor(height * (1.0f - progress) + 0.5f));
		if (filled > 0)
			TheDisplay->drawFillRect(x, y, width, filled, GameMakeColor(0, 0, 0, 150));
	}
	if (highlight)
		TheDisplay->drawFillRect(x, y, width, height, GameMakeColor(255, 236, 170, 48));
	TheDisplay->setCircularClip(0, 0, 0);

	drawDesignImage("UiRetailGlass", DesignRect{ 0.0f, 0.0f, static_cast<Real>(width), static_cast<Real>(height) }, x, y, 1.0f,
		highlight ? 0xFFFFFFFF : 0xD0FFFFFF);

	if (count > 1)
		drawShadowedText(instance->getTextDisplayString(), x, y + height / 4, width, height, 2, GameMakeColor(255, 255, 255, 255));
}
}

Bool ControlBar::isOverRetailSocket(Int px, Int py) const
{
	for (Int i = 0; i < MAX_COMMANDS_PER_SET; ++i)
	{
		const GameWindow *window = m_retailVisible[i];
		if (!window || (i >= RetailRingSlots && const_cast<GameWindow *>(window)->winIsHidden()))
			continue;
		Int x, y, width, height;
		windowRect(window, x, y, width, height);
		const Int pad = MAX(1, width / 12);
		if (px >= x - pad && px <= x + width + pad && py >= y - pad && py <= y + height + pad)
			return TRUE;
	}
	return FALSE;
}

void ControlBar::drawRetailSocketFills() const
{
	for (Int i = 0; i < RetailRingSlots; ++i)
	{
		Int x, y, width, height;
		windowRect(m_retailVisible[i], x, y, width, height);
		const Int pad = MAX(1, width / 12);
		drawFilledCircle(x - pad, y - pad, width + 2 * pad, height + 2 * pad, GameMakeColor(4, 4, 4, 255));
	}
}

void ControlBar::drawRetailTexts() const
{
	const Color text = GameMakeColor(236, 228, 206, 255);
	drawTextWindow(m_retailResources, 0, text);
	drawTextWindow(m_retailCommandPoints, 1, text);
	if (!m_retailPortrait->winIsHidden())
		drawTextWindow(m_retailSelectionName, 2, GameMakeColor(255, 255, 255, 255));
}

Int ControlBar::getRetailSlot(const GameWindow *window) const
{
	for (Int i = 0; i < MAX_COMMANDS_PER_SET; ++i)
		if (m_commandWindows[i] == window)
			return m_retailSlotOf[i];
	return -1;
}

void ControlBar::getRetailProduction(const GameWindow *window, Int &count, Real &progress) const
{
	count = 0;
	progress = 0.0f;
	const CommandButton *command = static_cast<const CommandButton *>(GadgetButtonGetData(const_cast<GameWindow *>(window)));
	if (!command || !m_currentSelectedDrawable || m_currContext != CB_CONTEXT_COMMAND)
		return;
	Object *factory = m_currentSelectedDrawable->getObject();
	ProductionUpdateInterface *production = factory ? factory->getProductionUpdateInterface() : nullptr;
	if (!production)
		return;

	Bool first = TRUE;
	for (const ProductionEntry *entry = production->firstProduction(); entry; entry = production->nextProduction(entry))
	{
		Bool matches = FALSE;
		if (command->getCommandType() == GUI_COMMAND_UNIT_BUILD)
			matches = entry->getProductionType() == PRODUCTION_UNIT && entry->getProductionObject() == command->getThingTemplate();
		else if (command->getCommandType() == GUI_COMMAND_PLAYER_UPGRADE || command->getCommandType() == GUI_COMMAND_OBJECT_UPGRADE)
			matches = entry->getProductionType() == PRODUCTION_UPGRADE && entry->getProductionUpgrade() == command->getUpgradeTemplate();
		if (matches)
		{
			if (count == 0)
				progress = first ? entry->getPercentComplete() / 100.0f : 0.0f;
			++count;
		}
		first = FALSE;
	}
}

void ControlBar::initRetailHud()
{
	GameWindow *master = m_contextParent[CP_MASTER];
	m_contextParent[CP_COMMAND] = TheWindowManager->winGetWindowFromId(master, NAMEKEY("ControlBar.wnd:CommandWindow"));
	m_retailPortrait = TheWindowManager->winGetWindowFromId(master, NAMEKEY("ControlBar.wnd:CameoWindow"));
	m_retailSelectionName = TheWindowManager->winGetWindowFromId(master, NAMEKEY("ControlBar.wnd:SelectionName"));
	m_retailResources = TheWindowManager->winGetWindowFromId(master, NAMEKEY("ControlBar.wnd:Resources"));
	m_retailCommandPoints = TheWindowManager->winGetWindowFromId(master, NAMEKEY("ControlBar.wnd:CommandPoints"));
	GameWindow *palantir = TheWindowManager->winGetWindowFromId(master, NAMEKEY("ControlBar.wnd:Palantir"));
	GameWindow *frame = TheWindowManager->winGetWindowFromId(master, NAMEKEY("ControlBar.wnd:PalantirFrame"));
	if (!m_contextParent[CP_COMMAND] || !m_retailPortrait || !m_retailSelectionName || !m_retailResources
		|| !m_retailCommandPoints || !palantir || !frame)
		throw INIException("RetailHud.wnd is missing a required window");

	palantir->winSetDrawFunc(drawRetailPalantirBack);
	palantir->winSetInputFunc(retailPalantirInput);
	GameWindow *sideBar = TheWindowManager->winGetWindowFromId(master, NAMEKEY("ControlBar.wnd:SideBar"));
	if (!sideBar)
		throw INIException("RetailHud.wnd is missing the side bar");
	sideBar->winSetInputFunc(retailSideBarInput);
	frame->winSetDrawFunc(drawRetailPalantirFrame);
	m_retailPortrait->winSetDrawFunc(drawRetailPortrait);
	m_retailPortrait->winSetInputFunc(retailPortraitInput);

	for (Int i = 0; i < MAX_COMMANDS_PER_SET; ++i)
	{
		AsciiString name;
		if (i < RetailRingSlots)
			name.format("ControlBar.wnd:RingButton%d", i + 1);
		else
			name.format("ControlBar.wnd:SideButton%02d", i - RetailRingSlots + 1);
		m_retailVisible[i] = TheWindowManager->winGetWindowFromId(master, TheNameKeyGenerator->nameToKey(name));
		name.format("ControlBar.wnd:ButtonCommand%02d", i + 1);
		m_retailHolders[i] = TheWindowManager->winGetWindowFromId(m_contextParent[CP_COMMAND], TheNameKeyGenerator->nameToKey(name));
		if (!m_retailVisible[i] || !m_retailHolders[i])
			throw INIException("RetailHud.wnd is missing a command button");
		m_retailVisible[i]->winSetOwner(master);
		m_retailVisible[i]->winSetDrawFunc(drawRetailCommand);
		m_retailHolders[i]->winSetOwner(master);
		m_commandWindows[i] = m_retailHolders[i];
		m_retailSlotOf[i] = -1;
	}
	m_contextParent[CP_COMMAND]->winHide(TRUE);
	switchToContext(CB_CONTEXT_NONE, nullptr);
}

void ControlBar::resetRetailCommandWindows()
{
	for (Int i = 0; i < MAX_COMMANDS_PER_SET; ++i)
	{
		m_retailVisible[i]->winHide(TRUE);
		m_commandWindows[i] = m_retailHolders[i];
		m_retailSlotOf[i] = -1;
		m_retailCountShown[i] = 0;
	}
}

void ControlBar::assignRetailSlots(Int initialVisible)
{
	Int side = RetailRingSlots;
	for (Int i = 0; i < MAX_COMMANDS_PER_SET; ++i)
	{
		GameWindow *holder = m_retailHolders[i];
		if (holder->winIsHidden())
			continue;
		const CommandButton *command = static_cast<const CommandButton *>(GadgetButtonGetData(holder));
		Int slot = -1;
		if (command && i < RetailRingSlots && command->isInPalantir())
			slot = i;
		else if (command && command->getCommandType() != GUI_COMMAND_NONE
			&& (command->isRadial() || !command->isInPalantir())
			&& (initialVisible <= 0 || i < initialVisible) && side < MAX_COMMANDS_PER_SET)
			slot = side++;
		if (slot < 0)
			continue;
		GameWindow *target = m_retailVisible[slot];
		setControlCommand(target, command);
		target->winEnable(BitIsSet(holder->winGetStatus(), WIN_STATUS_ENABLED));
		target->winHide(FALSE);
		GadgetButtonSetText(target, UnicodeString::TheEmptyString);
		m_commandWindows[i] = target;
		m_retailSlotOf[i] = slot;
	}
	updateRetailHud();
}

void ControlBar::populateRetailUnderConstruction(Object *obj)
{
	const CommandButton *cancel = findCommandButton("Command_CancelConstruction");
	GameWindow *target = m_retailVisible[0];
	if (obj && cancel && obj->isLocallyControlled())
	{
		setControlCommand(target, cancel);
		target->winEnable(TRUE);
		target->winHide(FALSE);
		m_commandWindows[0] = target;
		m_retailSlotOf[0] = 0;
	}
	setPortraitByObject(obj);
	updateRetailHud();
}

void ControlBar::updateRetailCounters()
{
	const Player *player = ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
	if (!player)
		return;
	const UnsignedInt now = TheGameLogic ? TheGameLogic->getFrame() : 0;
	if (m_retailMoney >= 0 && now >= m_retailCounterFrame && now - m_retailCounterFrame < CounterInterval)
		return;
	m_retailCounterFrame = now;

	Int used = 0;
	Int maximum = 0;
	for (const Object *obj = TheGameLogic ? TheGameLogic->getFirstObject() : nullptr; obj; obj = obj->getNextObject())
	{
		if (obj->getControllingPlayer() != player || obj->isEffectivelyDead())
			continue;
		const ThingTemplate::RotWKObjectData &data = obj->getTemplate()->getRotWKObjectData();
		used += data.commandPoints;
		if (!obj->getStatusBits().test(OBJECT_STATUS_UNDER_CONSTRUCTION))
			maximum += data.commandPointBonus;
	}

	const Int money = static_cast<Int>(player->getMoney()->countMoney());
	UnicodeString text;
	if (money != m_retailMoney)
	{
		m_retailMoney = money;
		text.format(L"%d", money);
		GadgetStaticTextSetText(m_retailResources, text);
	}
	if (used != m_retailUsedPoints || maximum != m_retailMaxPoints)
	{
		m_retailUsedPoints = used;
		m_retailMaxPoints = maximum;
		text.format(L"%d/%d", used, maximum);
		GadgetStaticTextSetText(m_retailCommandPoints, text);
	}
}

void ControlBar::updateRetailHud()
{
	for (Int i = 0; i < MAX_COMMANDS_PER_SET; ++i)
	{
		if (m_retailSlotOf[i] < 0)
			continue;
		GameWindow *window = m_commandWindows[i];
		Int count = 0;
		Real progress = 0.0f;
		getRetailProduction(window, count, progress);
		if (count != m_retailCountShown[i])
		{
			m_retailCountShown[i] = count;
			UnicodeString text;
			if (count > 1)
				text.format(L"%d", count);
			GadgetButtonSetText(window, text);
		}
	}
	updateRetailCounters();
}
