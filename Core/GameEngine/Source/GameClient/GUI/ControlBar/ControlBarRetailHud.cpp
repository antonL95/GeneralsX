#include "PreRTS.h"

#include <vector>

#include "Common/INIException.h"
#include "Common/BuildAssistant.h"
#include "Common/MessageStream.h"
#include "Common/SpecialPower.h"
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
#include "GameClient/InGameUI.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/HeroRoster.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Module/RespawnModules.h"

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
		else if (command->getCommandType() == GUI_COMMAND_REVIVE)
			matches = entry->getProductionType() == PRODUCTION_UNIT && entry->getProductionObject() == getRetailHero(window);
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

Bool ControlBar::retailExtraEntryUsable(const CommandButton *command, const Object *obj) const
{
	if (!command || !obj || BitIsSet(command->getOptions(), SCRIPT_ONLY) || command->getCommandType() == GUI_COMMAND_EXIT_CONTAINER)
		return FALSE;
	const Player *player = obj->getControllingPlayer();
	const SpecialPowerTemplate *power = command->getSpecialPowerTemplate();
	if (BitIsSet(command->getOptions(), NEED_SPECIAL_POWER_SCIENCE) && power && player
		&& power->getRequiredScience() != SCIENCE_INVALID && !player->hasScience(power->getRequiredScience()))
		return FALSE;
	return TRUE;
}

Bool ControlBar::retailEntryShown(const CommandSet *commandSet, Int entry, const CommandButton *command, Object *obj, Int &hero) const
{
	hero = -1;
	if (!obj)
		return TRUE;
	if (command->getCommandType() == GUI_COMMAND_REVIVE)
	{
		hero = HeroRoster::reviveSlotForEntry(commandSet, entry);
		const HeroRules::Slot slot = HeroRoster::slotFor(obj->getControllingPlayer(), obj, command, hero);
		if (slot == HeroRules::Slot::Hidden)
			return FALSE;
		return slot != HeroRules::Slot::Disabled || !command->hasRotwkOption(CommandButtonRules::ROTWK_HIDE_WHILE_DISABLED);
	}
	if (!command->hasRotwkOption(CommandButtonRules::ROTWK_HIDE_WHILE_DISABLED))
		return TRUE;
	GameWindow *probe = m_retailHolders[MIN(entry, MAX_COMMANDS_PER_SET - 1)];
	const CommandAvailability availability = getCommandAvailability(command, obj, probe);
	return availability != COMMAND_HIDDEN && availability != COMMAND_RESTRICTED && availability != COMMAND_CANT_AFFORD;
}

void ControlBar::collectRetailPlacements(const CommandSet *commandSet, Object *obj, Int initialVisible,
	std::vector<RetailPlacement> &placements) const
{
	placements.clear();
	Int side = RetailRingSlots;
	const Int entries = commandSet ? static_cast<Int>(CommandButtonRules::RETAIL_COMMANDS_PER_SET) : static_cast<Int>(MAX_COMMANDS_PER_SET);
	for (Int i = 0; i < entries; ++i)
	{
		const CommandButton *command = nullptr;
		if (i < MAX_COMMANDS_PER_SET)
		{
			GameWindow *holder = m_retailHolders[i];
			if (holder->winIsHidden())
				continue;
			command = static_cast<const CommandButton *>(GadgetButtonGetData(holder));
		}
		else
		{
			command = commandSet->getCommandButton(i);
			if (!retailExtraEntryUsable(command, obj))
				continue;
		}
		if (!command)
			continue;
		Int slot = -1;
		Int hero = -1;
		if (i < RetailRingSlots && command->isInPalantir())
			slot = i;
		else if (command->getCommandType() != GUI_COMMAND_NONE
			&& (command->isRadial() || !command->isInPalantir())
			&& CommandButtonRules::inSideColumn(i, commandSet ? m_retailRange : CommandButtonRules::VisibleRange(), initialVisible) && side < MAX_COMMANDS_PER_SET
			&& retailEntryShown(commandSet, i, command, obj, hero))
			slot = side++;
		if (slot >= 0)
			placements.push_back(RetailPlacement{ i, command, slot, hero });
	}
}

void ControlBar::assignRetailSlots(const CommandSet *commandSet, Object *obj, Int initialVisible)
{
	const UnsignedInt objectID = obj ? static_cast<UnsignedInt>(obj->getID()) : 0;
	if (commandSet && objectID != m_retailRangeObject)
	{
		m_retailRange = CommandButtonRules::VisibleRange();
		m_retailRangeObject = objectID;
	}
	for (Int i = 0; i < MAX_COMMANDS_PER_SET; ++i)
		m_retailHeroOf[i] = -1;

	std::vector<RetailPlacement> placements;
	collectRetailPlacements(commandSet, obj, initialVisible, placements);

	Bool used[MAX_COMMANDS_PER_SET] = {};
	for (const RetailPlacement &placement : placements)
		if (placement.entry < MAX_COMMANDS_PER_SET)
			used[placement.entry] = TRUE;

	m_retailShownMask = 0;
	for (const RetailPlacement &placement : placements)
	{
		Int index = placement.entry;
		if (index >= MAX_COMMANDS_PER_SET)
		{
			index = 0;
			while (index < MAX_COMMANDS_PER_SET && used[index])
				++index;
			if (index >= MAX_COMMANDS_PER_SET)
				break;
			used[index] = TRUE;
		}
		GameWindow *target = m_retailVisible[placement.slot];
		setControlCommand(target, placement.command);
		const Bool enabled = placement.entry >= MAX_COMMANDS_PER_SET
			|| BitIsSet(m_retailHolders[placement.entry]->winGetStatus(), WIN_STATUS_ENABLED);
		target->winEnable(enabled);
		target->winHide(FALSE);
		GadgetButtonSetText(target, UnicodeString::TheEmptyString);
		m_commandWindows[index] = target;
		m_retailSlotOf[index] = placement.slot;
		m_retailHeroOf[placement.slot] = placement.hero;
		if (placement.hero >= 0)
			applyRetailHeroImage(target, placement.hero);
		m_retailShownMask |= 1ull << placement.entry;
	}
	if (obj)
		updateContextCommand();
	updateRetailHud();
}

void ControlBar::checkRetailPlacements()
{
	if (m_currContext != CB_CONTEXT_COMMAND || !m_currentSelectedDrawable)
		return;
	Object *obj = m_currentSelectedDrawable->getObject();
	if (!obj)
		return;
	const CommandSet *commandSet = findCommandSet(obj->getCommandSetString());
	if (!commandSet)
		return;
	std::vector<RetailPlacement> placements;
	collectRetailPlacements(commandSet, obj, commandSet->getInitialVisible(), placements);
	unsigned long long mask = 0;
	for (const RetailPlacement &placement : placements)
		mask |= 1ull << placement.entry;
	if (mask != m_retailShownMask)
		markUIDirty();
}

Int ControlBar::getRetailHeroIndex(const GameWindow *window) const
{
	const Int slot = getRetailSlot(window);
	return slot >= 0 && slot < MAX_COMMANDS_PER_SET ? m_retailHeroOf[slot] : -1;
}

const ThingTemplate *ControlBar::getRetailHero(const GameWindow *window) const
{
	const Int hero = getRetailHeroIndex(window);
	if (hero < 0 || !m_currentSelectedDrawable || !m_currentSelectedDrawable->getObject())
		return nullptr;
	return HeroRoster::heroAt(m_currentSelectedDrawable->getObject()->getControllingPlayer(), hero);
}

void ControlBar::applyRetailHeroImage(GameWindow *window, Int hero) const
{
	const Object *obj = m_currentSelectedDrawable ? m_currentSelectedDrawable->getObject() : nullptr;
	const Player *player = obj ? obj->getControllingPlayer() : nullptr;
	const ThingTemplate *heroTemplate = HeroRoster::heroAt(player, hero);
	if (!heroTemplate)
		return;
	const Image *image = heroTemplate->getButtonImage();
	if (HeroRoster::stateAt(player, hero) == HeroRules::State::Revivable)
	{
		const ModuleInfo &modules = heroTemplate->getBehaviorModuleInfo();
		for (Int i = 0; i < modules.getCount(); ++i)
		{
			if (modules.getNthName(i).compareNoCase("RespawnUpdate") != 0)
				continue;
			const RespawnUpdateModuleData *data = static_cast<const RespawnUpdateModuleData *>(modules.getNthData(i));
			const Image *revive = data->m_buttonImage.isEmpty() ? nullptr : findImage(data->m_buttonImage.str());
			if (revive)
				image = revive;
			break;
		}
	}
	if (image)
		window->winSetEnabledImage(0, image);
}

CommandAvailability ControlBar::getRetailCommandAvailability(const CommandButton *command, Object *obj, GameWindow *win) const
{
	switch (command->getCommandType())
	{
		case GUI_COMMAND_PUSH_VISIBLE_COMMAND_RANGE:
		case GUI_COMMAND_POP_VISIBLE_COMMAND_RANGE:
			return COMMAND_AVAILABLE;
		case GUI_COMMAND_REVIVE:
		{
			if (getRetailSlot(win) < 0)
				return COMMAND_AVAILABLE;
			const Int hero = getRetailHeroIndex(win);
			const Player *player = obj->getControllingPlayer();
			switch (HeroRoster::slotFor(player, obj, command, hero))
			{
				case HeroRules::Slot::Hidden:
					return COMMAND_HIDDEN;
				case HeroRules::Slot::Disabled:
					return COMMAND_RESTRICTED;
				case HeroRules::Slot::Busy:
					return COMMAND_AVAILABLE;
				default:
					break;
			}
			const CanMakeType makeType = TheBuildAssistant->canMakeUnit(obj, HeroRoster::heroAt(player, hero));
			return makeType == CANMAKE_OK ? COMMAND_AVAILABLE : COMMAND_RESTRICTED;
		}
		default:
			return COMMAND_RESTRICTED;
	}
}

CBCommandStatus ControlBar::processRetailCommand(const CommandButton *command, GameWindow *control, Object *obj)
{
	switch (command->getCommandType())
	{
		case GUI_COMMAND_PUSH_VISIBLE_COMMAND_RANGE:
			m_retailRange = CommandButtonRules::pushRange(command->getCommandRangeStart(), command->getCommandRangeCount());
			markUIDirty();
			return CBC_COMMAND_USED;
		case GUI_COMMAND_POP_VISIBLE_COMMAND_RANGE:
			m_retailRange = CommandButtonRules::VisibleRange();
			markUIDirty();
			return CBC_COMMAND_USED;
		case GUI_COMMAND_REVIVE:
		{
			const ThingTemplate *hero = getRetailHero(control);
			applyRetailHeroImage(control, getRetailHeroIndex(control));
			ProductionUpdateInterface *production = obj ? obj->getProductionUpdateInterface() : nullptr;
			if (!hero || !production)
				return CBC_COMMAND_NOT_USED;
			for (const ProductionEntry *entry = production->firstProduction(); entry; entry = production->nextProduction(entry))
			{
				if (entry->getProductionType() == PRODUCTION_UNIT && entry->getProductionObject() == hero)
				{
					if (command->hasRotwkOption(CommandButtonRules::ROTWK_CANCELABLE))
					{
						GameMessage *msg = TheMessageStream->appendMessage(GameMessage::MSG_CANCEL_UNIT_CREATE);
						msg->appendIntegerArgument(entry->getProductionID());
					}
					return CBC_COMMAND_USED;
				}
			}
			const CanMakeType makeType = TheBuildAssistant->canMakeUnit(obj, hero);
			if (makeType == CANMAKE_NO_MONEY)
			{
				TheInGameUI->message("GUI:NotEnoughMoneyToBuild");
				return CBC_COMMAND_USED;
			}
			if (makeType != CANMAKE_OK)
				return CBC_COMMAND_USED;
			GameMessage *msg = TheMessageStream->appendMessage(GameMessage::MSG_QUEUE_UNIT_CREATE);
			msg->appendIntegerArgument(hero->getTemplateID());
			msg->appendIntegerArgument(production->requestUniqueUnitID());
			return CBC_COMMAND_USED;
		}
		default:
			return CBC_COMMAND_NOT_USED;
	}
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

	const Int used = player->getCommandPointsUsed();
	const Int maximum = player->getCommandPointsTotal();

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
	checkRetailPlacements();
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
