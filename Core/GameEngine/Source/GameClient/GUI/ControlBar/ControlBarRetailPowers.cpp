#include "PreRTS.h"

#include "Common/ActionManager.h"
#include "Common/FileSystem.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Image.h"
#include "GameClient/Keyboard.h"
#include "GameClient/Mouse.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

namespace
{
constexpr Int StoreSlots = 12;
constexpr Int PanelWidth = 560;
constexpr Int PanelHeight = 558;
constexpr Int NodeSize = 64;
constexpr Color PanelLine = 0xFF536A75;
constexpr Color Ink = 0xFFF5F1DF;
constexpr Color Gold = 0xFFE8BE72;
constexpr Color Cyan = 0xFF82D7E0;
constexpr Color Owned = 0xFF9ACAA4;

struct Rect
{
	Int x;
	Int y;
	Int w;
	Int h;

	Bool contains(Int px, Int py) const
	{
		return px >= x && px < x + w && py >= y && py < y + h;
	}
};

struct Layout
{
	Real scale;
	Int x;
	Int y;

	Rect at(Int left, Int top, Int width, Int height) const
	{
		return { x + static_cast<Int>(left * scale), y + static_cast<Int>(top * scale),
			std::max(1, static_cast<Int>(width * scale)), std::max(1, static_cast<Int>(height * scale)) };
	}

	Rect node(Int slot) const
	{
		static constexpr Int rowStart[] = { 0, 3, 7, 10 };
		static constexpr Int rowWidth[] = { 3, 4, 3, 2 };
		Int row = 0;
		while (row < 3 && slot >= rowStart[row + 1])
			++row;
		const Int center = PanelWidth / 2 + (2 * (slot - rowStart[row]) - rowWidth[row] + 1) * 56;
		return at(center - NodeSize / 2, 90 + row * 95, NodeSize, NodeSize);
	}
};

UnsignedInt readLittleWord(const UnsignedByte* data)
{
	return static_cast<UnsignedInt>(data[0]) | static_cast<UnsignedInt>(data[1]) << 8 |
		static_cast<UnsignedInt>(data[2]) << 16 | static_cast<UnsignedInt>(data[3]) << 24;
}

UnsignedInt spellStoreFadeTime()
{
	constexpr UnsignedInt defaultFade = 198;
	File* constants = TheFileSystem->openFile("SpellStore.const", File::READ | File::BINARY);
	if (!constants)
		return defaultFade;
	UnsignedByte header[24];
	const Int headerBytes = constants->read(header, sizeof(header));
	constants->close();
	if (headerBytes != sizeof(header) || memcmp(header, "Apt constant file", 17) != 0)
		return defaultFade;
	const UnsignedInt offset = readLittleWord(header + 20);
	if (offset < 12 || offset > 8 * 1024 * 1024)
		return defaultFade;
	File* movie = TheFileSystem->openFile("SpellStore.apt", File::READ | File::BINARY);
	if (!movie)
		return defaultFade;
	UnsignedByte character[40];
	const Int movieBytes = movie->seek(offset, File::START) == static_cast<Int>(offset) ?
		movie->read(character, sizeof(character)) : 0;
	movie->close();
	if (movieBytes != sizeof(character) || readLittleWord(character) != 9 ||
		readLittleWord(character + 4) != 0x09876543)
		return defaultFade;
	const UnsignedInt frameTime = readLittleWord(character + 36);
	return frameTime >= 10 && frameTime <= 100 ? frameTime * 6 : defaultFade;
}

Layout screenLayout()
{
	const Int w = TheDisplay->getWidth();
	const Int h = TheDisplay->getHeight();
	const Real scale = std::min(w / 650.0f, h / 640.0f);
	return { scale, (w - static_cast<Int>(PanelWidth * scale)) / 2, (h - static_cast<Int>(PanelHeight * scale)) / 2 };
}

Rect windowRect(GameWindow* window)
{
	Rect rect;
	window->winGetScreenPosition(&rect.x, &rect.y);
	window->winGetSize(&rect.w, &rect.h);
	return rect;
}

Player* localPlayer()
{
	return ThePlayerList ? ThePlayerList->getLocalPlayer() : nullptr;
}

const CommandButton* spellButton(Player* player, Int slot)
{
	Object* book = player ? player->getRetailSpellBook() : nullptr;
	const CommandSet* set = book ? TheControlBar->findCommandSet(book->getCommandSetString()) : nullptr;
	const CommandButton* button = set ? set->getCommandButton(slot) : nullptr;
	return button && button->getCommandType() == GUI_COMMAND_SPELL_BOOK ? button : nullptr;
}

const CommandButton* storeButton(const Player* player, Int slot)
{
	const CommandSet* store = player ? player->getRetailSpellStore() : nullptr;
	const CommandButton* button = store ? store->getCommandButton(slot) : nullptr;
	return button && button->getCommandType() == GUI_COMMAND_PURCHASE_SCIENCE ? button : nullptr;
}

ScienceType storeScience(const Player* player, Int slot)
{
	const CommandButton* button = storeButton(player, slot);
	return button && !button->getScienceVec().empty() ? button->getScienceVec().front() : SCIENCE_INVALID;
}

Int pendingCost(const Player* player, const ScienceVec& pending)
{
	Int cost = 0;
	for (ScienceType science : pending)
		cost += player->getSciencePurchaseCost(science);
	return cost;
}

Bool containsScience(const ScienceVec& sciences, ScienceType science)
{
	return std::find(sciences.begin(), sciences.end(), science) != sciences.end();
}

Bool canQueueScience(const Player* player, ScienceType science, const ScienceVec& pending)
{
	const Int cost = player->getSciencePurchaseCost(science);
	return cost > 0 && cost <= player->getSciencePurchasePoints() - pendingCost(player, pending) &&
		!player->hasScience(science) && !containsScience(pending, science) && !player->isScienceDisabled(science) &&
		!player->isScienceHidden(science) && TheScienceStore->playerHasPrereqsForScience(player, science, pending);
}

Bool isOfferedSpell(const Player* player, const CommandButton* button)
{
	const SpecialPowerTemplate* power = button ? button->getSpecialPowerTemplate() : nullptr;
	return power && power->isUnlockedFor(player);
}

void fill(const Rect& rect, Color color)
{
	TheDisplay->drawFillRect(rect.x, rect.y, rect.w, rect.h, color);
}

void frame(const Rect& rect, Color color)
{
	TheDisplay->drawOpenRect(rect.x, rect.y, rect.w, rect.h, 1.0f, color);
}

void disc(const Rect& rect, Color color)
{
	TheDisplay->setCircularClip(rect.x + rect.w / 2, rect.y + rect.h / 2, std::min(rect.w, rect.h) / 2);
	fill(rect, color);
	TheDisplay->setCircularClip(0, 0, 0);
}

void label(GameFont* font, const Rect& rect, const UnicodeString& text, Color color)
{
	if (!font || text.isEmpty())
		return;
	DisplayString* display = TheDisplayStringManager->newDisplayString();
	display->setFont(font);
	display->setText(text);
	Int width, height;
	display->getSize(&width, &height);
	display->draw(rect.x + (rect.w - width) / 2, rect.y + (rect.h - height) / 2, color, 0xFF000000);
	TheDisplayStringManager->freeDisplayString(display);
}

GameFont* powersFont(Int pixels, Bool bold = FALSE)
{
	const Layout layout = screenLayout();
	return TheFontLibrary->getFontPixels("Albertus MT", std::max(10, static_cast<Int>(pixels * layout.scale)), bold);
}

void icon(const CommandButton* button, const Rect& rect, Color tint)
{
	const Image* image = button ? button->getButtonImage() : nullptr;
	if (TheDisplay->hasImageTexture(image))
		TheDisplay->drawImage(image, rect.x, rect.y, rect.x + rect.w, rect.y + rect.h, tint);
}

WindowMsgHandledType overlayInput(GameWindow*, UnsignedInt msg, WindowMsgData point, WindowMsgData)
{
	if (msg == GWM_CHAR && point == KEY_ESC)
		TheControlBar->showRetailPowers(FALSE);
	if (msg == GWM_LEFT_UP || msg == GWM_MOUSE_POS)
		TheControlBar->inputRetailPowers(point & 0xFFFF, point >> 16, msg == GWM_LEFT_UP);
	return MSG_HANDLED;
}

WindowMsgHandledType counterInput(GameWindow*, UnsignedInt msg, WindowMsgData, WindowMsgData)
{
	if (msg == GWM_LEFT_UP)
		TheControlBar->showRetailPowers(TRUE);
	return MSG_HANDLED;
}

WindowMsgHandledType spellInput(GameWindow* window, UnsignedInt msg, WindowMsgData, WindowMsgData)
{
	if (msg == GWM_LEFT_UP)
		TheControlBar->clickRetailSpellButton(window);
	return MSG_HANDLED;
}

void spellTooltip(GameWindow* window, WinInstanceData*, UnsignedInt)
{
	TheControlBar->showRetailSpellTooltip(window);
}

void drawScreen(GameWindow* window, WinInstanceData*) { TheControlBar->drawRetailPowers(window); }
void drawCounter(GameWindow* window, WinInstanceData*) { TheControlBar->drawRetailPowerCounter(window); }
void drawSpell(GameWindow* window, WinInstanceData*) { TheControlBar->drawRetailSpellButton(window); }
}

void ControlBar::initRetailPowers(GameWindow* master, GameWindow* palantir)
{
	m_retailPowersFadeTime = spellStoreFadeTime();
	m_retailPowersScreen = TheWindowManager->winCreate(nullptr, WIN_STATUS_ENABLED | WIN_STATUS_ABOVE | WIN_STATUS_HIDDEN,
		0, 0, TheDisplay->getWidth(), TheDisplay->getHeight(), nullptr, nullptr);
	m_retailPowersScreen->winSetInputFunc(overlayInput);
	m_retailPowersScreen->winSetDrawFunc(drawScreen);
	m_retailPowersCounter = TheWindowManager->winCreate(palantir, WIN_STATUS_ENABLED | WIN_STATUS_HIDDEN,
		0, 0, 1, 1, nullptr, nullptr);
	m_retailPowersCounter->winSetInputFunc(counterInput);
	m_retailPowersCounter->winSetDrawFunc(drawCounter);
	for (GameWindow*& window : m_retailSpellButtons)
	{
		window = TheWindowManager->winCreate(master, WIN_STATUS_ENABLED | WIN_STATUS_HIDDEN, 0, 0, 1, 1, nullptr, nullptr);
		window->winSetInputFunc(spellInput);
		window->winSetDrawFunc(drawSpell);
		window->winSetTooltipFunc(spellTooltip);
	}
}

void ControlBar::destroyRetailPowers()
{
	if (TheWindowManager && m_retailPowersScreen)
		TheWindowManager->winDestroy(m_retailPowersScreen);
	m_retailPowersScreen = nullptr;
}

void ControlBar::showRetailPowers(Bool show)
{
	if (!m_retailPowersScreen)
		return;
	if (show)
	{
		Player* player = localPlayer();
		if (!player || !player->getRetailSpellStore() || m_isObserverCommandBar ||
			!m_contextParent[CP_MASTER] || m_contextParent[CP_MASTER]->winIsHidden())
			return;
		TheInGameUI->setGUICommand(nullptr);
		m_retailPowersOpenedAt = timeGetTime();
		m_retailPowersScreen->winSetSize(TheDisplay->getWidth(), TheDisplay->getHeight());
		m_retailPowersScreen->winHide(FALSE);
		m_retailPowersScreen->winBringToTop();
		TheWindowManager->winSetModal(m_retailPowersScreen);
		TheWindowManager->winSetFocus(m_retailPowersScreen);
	}
	else if (!m_retailPowersScreen->winIsHidden())
	{
		TheWindowManager->winUnsetModal(m_retailPowersScreen);
		m_retailPowersScreen->winHide(TRUE);
		m_retailPendingSciences.clear();
		m_retailHoveredPower = -1;
	}
}

void ControlBar::updateRetailPowers()
{
	if (!m_retailPowersCounter)
		return;
	Player* player = localPlayer();
	const Bool usable = player && !m_isObserverCommandBar && player->getRetailSpellStore();
	m_retailPowersCounter->winHide(!usable);
	const Rect palantir = windowRect(m_retailPowersCounter->winGetParent());
	const Real scale = palantir.w / 470.0f;
	const Int counterSize = std::max(1, static_cast<Int>(49 * scale));
	m_retailPowersCounter->winSetSize(counterSize, counterSize);
	m_retailPowersCounter->winSetPosition(static_cast<Int>(104 * scale), static_cast<Int>(61 * scale));
	if (!m_retailPowersScreen->winIsHidden())
		m_retailPowersScreen->winSetSize(TheDisplay->getWidth(), TheDisplay->getHeight());

	Int count = 0;
	for (Int slot = 0; slot < static_cast<Int>(m_retailSpellButtons.size()); ++slot)
		if (usable && isOfferedSpell(player, spellButton(player, slot)))
			++count;
	const Int bottom = palantir.y + static_cast<Int>(54 * scale);
	const Int pitch = std::clamp(bottom / std::max(1, count), 1, std::max(1, static_cast<Int>(46 * scale)));
	const Int size = std::max(1, pitch - 3);
	Int visible = 0;
	for (Int slot = 0; slot < static_cast<Int>(m_retailSpellButtons.size()); ++slot)
	{
		GameWindow* window = m_retailSpellButtons[slot];
		const Bool show = usable && isOfferedSpell(player, spellButton(player, slot));
		window->winHide(!show);
		if (show)
		{
			window->winSetPosition(8, bottom - (++visible) * pitch);
			window->winSetSize(size, size);
		}
	}
}

void ControlBar::drawRetailPowerCounter(GameWindow* window) const
{
	Player* player = localPlayer();
	if (!player)
		return;
	const Rect rect = windowRect(window);
	const Int points = player->getSciencePurchasePoints();
	if (points > 0)
	{
		const UnsignedInt phase = TheGameLogic ? TheGameLogic->getFrame() % 48 : 0;
		const Int expansion = 2 + (phase <= 24 ? phase : 48 - phase) / 6;
		disc({ rect.x - expansion, rect.y - expansion, rect.w + expansion * 2, rect.h + expansion * 2 },
			GameMakeColor(93, 195, 238, 122 - expansion * 13));
		disc(rect, Cyan);
	}
	disc({ rect.x + 3, rect.y + 3, rect.w - 6, rect.h - 6 }, points > 0 ? 0xFF245572 : 0xFF202E3B);
	UnicodeString text;
	text.format(L"%d", points);
	label(powersFont(18, TRUE), rect, text, Ink);
}

void ControlBar::drawRetailSpellButton(GameWindow* window) const
{
	Player* player = localPlayer();
	const auto slot = std::find(m_retailSpellButtons.begin(), m_retailSpellButtons.end(), window) - m_retailSpellButtons.begin();
	const CommandButton* button = spellButton(player, slot);
	Object* book = player ? player->getRetailSpellBook() : nullptr;
	if (!book || !isOfferedSpell(player, button))
		return;
	const Rect rect = windowRect(window);
	SpecialPowerModuleInterface* power = book->getSpecialPowerModule(button->getSpecialPowerTemplate());
	const Bool ready = power && power->isReady();
	fill(rect, 0xFF141E28);
	icon(button, { rect.x + 2, rect.y + 2, rect.w - 4, rect.h - 4 }, ready ? 0xFFFFFFFF : 0xFF737E87);
	if (power && !ready)
	{
		const Int width = static_cast<Int>((rect.w - 4) * std::clamp(power->getPercentReady(), 0.0f, 1.0f));
		fill({ rect.x + 2, rect.y + rect.h - 4, width, 2 }, Cyan);
	}
	frame(rect, ready ? Gold : PanelLine);
}

void ControlBar::clickRetailSpellButton(GameWindow* window)
{
	const auto slot = std::find(m_retailSpellButtons.begin(), m_retailSpellButtons.end(), window) - m_retailSpellButtons.begin();
	Player* player = localPlayer();
	const CommandButton* button = spellButton(player, slot);
	if (!isOfferedSpell(player, button))
		return;
	Object* book = player->getRetailSpellBook();
	SpecialPowerModuleInterface* power = book ? book->getSpecialPowerModule(button->getSpecialPowerTemplate()) : nullptr;
	if (!power || !power->isReady())
		return;
	TheInGameUI->placeBuildAvailable(nullptr, nullptr);
	if (BitIsSet(button->getOptions(), COMMAND_OPTION_NEED_TARGET))
		TheInGameUI->setGUICommand(button);
	else
		targetRetailSpell(button, nullptr, nullptr, TRUE);
}

void ControlBar::showRetailSpellTooltip(GameWindow* window) const
{
	const auto slot = std::find(m_retailSpellButtons.begin(), m_retailSpellButtons.end(), window) - m_retailSpellButtons.begin();
	if (const CommandButton* button = spellButton(localPlayer(), slot))
	{
		UnicodeString text = TheGameText->fetchOrSubstitute(button->getTextLabel().str(), L"");
		const UnicodeString description = TheGameText->fetchOrSubstitute(button->getDescriptionLabel().str(), L"");
		if (!description.isEmpty())
		{
			text.concat(L"\n");
			text.concat(description);
		}
		TheMouse->setRetailCommandTooltip(text);
	}
}

Bool ControlBar::targetRetailSpell(const CommandButton* button, Object* target, const Coord3D* position, Bool submit)
{
	Player* player = localPlayer();
	Object* book = player ? player->getRetailSpellBook() : nullptr;
	if (!book || !isOfferedSpell(player, button))
		return FALSE;
	const CommandSet* set = findCommandSet(book->getCommandSetString());
	Int slot = 0;
	while (set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET && set->getCommandButton(slot) != button)
		++slot;
	if (!set || slot == CommandButtonRules::RETAIL_COMMANDS_PER_SET)
		return FALSE;
	const SpecialPowerTemplate* power = button->getSpecialPowerTemplate();
	const UnsignedInt options = button->getOptions();
	Bool valid;
	if (BitIsSet(options, COMMAND_OPTION_NEED_OBJECT_TARGET))
		valid = target && TheActionManager->canDoSpecialPowerAtObject(book, target, CMD_FROM_PLAYER, power, options);
	else if (BitIsSet(options, NEED_TARGET_POS))
		valid = position && TheActionManager->canDoSpecialPowerAtLocation(book, position, CMD_FROM_PLAYER, power, target, options);
	else
		valid = TheActionManager->canDoSpecialPower(book, power, CMD_FROM_PLAYER, options);
	if (valid && submit)
	{
		GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPELLBOOK_SPECIAL_POWER);
		msg->appendIntegerArgument(slot);
		msg->appendLocationArgument(position ? *position : Coord3D{});
		msg->appendObjectIDArgument(target ? target->getID() : INVALID_ID);
	}
	return valid;
}

void ControlBar::drawRetailPowers(GameWindow* window) const
{
	Player* player = localPlayer();
	if (!player)
		return;
	const UnsignedInt elapsed = timeGetTime() - m_retailPowersOpenedAt;
	const Int shade = 168 * std::min(elapsed, m_retailPowersFadeTime) / m_retailPowersFadeTime;
	fill(windowRect(window), GameMakeColor(0, 10, 18, shade));
	const Layout layout = screenLayout();
	const Rect panel = layout.at(0, 0, PanelWidth, PanelHeight);
	fill(panel, 0xFF10212E);
	frame(panel, PanelLine);
	disc(layout.at(69, 45, 422, 422), 0xFF294D60);
	disc(layout.at(73, 49, 414, 414), 0xFF152A39);
	const Image* orb = TheMappedImageCollection ? TheMappedImageCollection->findImageByName("UiRetailPowersOrb") : nullptr;
	if (TheDisplay->hasImageTexture(orb))
	{
		const Rect ring = layout.at(69, 45, 422, 422);
		TheDisplay->drawImage(orb, ring.x, ring.y, ring.x + ring.w, ring.y + ring.h, 0x664B9DA8);
	}
	GameFont* font = powersFont(16);
	label(powersFont(26, TRUE), layout.at(0, 6, PanelWidth, 29), UnicodeString(L"Powers"), Ink);
	UnicodeString points;
	points.format(L"Power points to spend  %d", player->getSciencePurchasePoints() - pendingCost(player, m_retailPendingSciences));
	label(font, layout.at(0, 36, PanelWidth, 28), points, Cyan);
	const Rect close = layout.at(515, 8, 35, 29);
	frame(close, PanelLine);
	label(font, close, UnicodeString(L"×"), Ink);

	for (Int from = 0; from < StoreSlots; ++from)
	{
		const ScienceType science = storeScience(player, from);
		const auto* groups = TheScienceStore->getPrerequisiteAlternatives(science);
		if (!groups)
			continue;
		const Rect a = layout.node(from);
		for (Int to = 0; to < from; ++to)
		{
			const ScienceType prerequisite = storeScience(player, to);
			if (prerequisite == SCIENCE_INVALID || !std::any_of(groups->begin(), groups->end(),
				[prerequisite](const ScienceVec& group) { return containsScience(group, prerequisite); }))
				continue;
			const Rect b = layout.node(to);
			TheDisplay->drawLine(a.x + a.w / 2, a.y, b.x + b.w / 2, b.y + b.h, 2.0f, PanelLine);
		}
	}

	for (Int i = 0; i < StoreSlots; ++i)
	{
		const ScienceType science = storeScience(player, i);
		if (science == SCIENCE_INVALID)
			continue;
		const Bool pending = containsScience(m_retailPendingSciences, science);
		const Bool owned = player->hasScience(science);
		const Bool available = canQueueScience(player, science, m_retailPendingSciences);
		const Color edge = pending ? Cyan : owned ? Owned : available ? Gold : PanelLine;
		const Rect tile = layout.node(i);
		fill(tile, pending ? 0xFF326A79 : 0xFF17212B);
		icon(storeButton(player, i), { tile.x + 3, tile.y + 3, tile.w - 6, tile.h - 6 },
			owned || available || pending ? 0xFFFFFFFF : 0xFF777F87);
		frame(tile, edge);
		Rect badge = { tile.x + tile.w / 4, tile.y + tile.h - tile.h / 5, tile.w / 2, tile.h / 3 };
		fill(badge, 0xFF17212B);
		frame(badge, edge);
		UnicodeString cost;
		cost.format(L"%d", player->getSciencePurchaseCost(science));
		label(font, badge, cost, owned ? Owned : Ink);
	}
	if (const CommandButton* button = m_retailHoveredPower >= 0 ? storeButton(player, m_retailHoveredPower) : nullptr)
	{
		UnicodeString title = TheGameText->fetchOrSubstitute(button->getTextLabel().str(), L"");
		label(font, layout.at(0, 464, PanelWidth, 25), title, Ink);
	}
	if (!m_retailPendingSciences.empty())
	{
		const Rect reset = layout.at(139, 504, 134, 38);
		const Rect accept = layout.at(287, 504, 134, 38);
		fill(reset, 0xFF253948);
		fill(accept, 0xFF27586B);
		frame(reset, PanelLine);
		frame(accept, Cyan);
		label(font, reset, UnicodeString(L"Reset"), Ink);
		label(font, accept, UnicodeString(L"Accept"), Ink);
	}
}

void ControlBar::inputRetailPowers(Int x, Int y, Bool click)
{
	Player* player = localPlayer();
	if (!player)
		return;
	const Layout layout = screenLayout();
	m_retailHoveredPower = -1;
	for (Int i = 0; i < StoreSlots; ++i)
		if (layout.node(i).contains(x, y))
		{
			m_retailHoveredPower = i;
			const ScienceType science = storeScience(player, i);
			if (click && science != SCIENCE_INVALID && canQueueScience(player, science, m_retailPendingSciences))
				m_retailPendingSciences.push_back(science);
			return;
		}
	if (!click)
		return;
	if (layout.at(515, 8, 35, 29).contains(x, y))
	{
		showRetailPowers(FALSE);
		return;
	}
	if (m_retailPendingSciences.empty())
		return;
	if (layout.at(139, 504, 134, 38).contains(x, y))
		m_retailPendingSciences.clear();
	if (layout.at(287, 504, 134, 38).contains(x, y))
	{
		for (ScienceType science : m_retailPendingSciences)
		{
			GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_PURCHASE_SCIENCE);
			msg->appendIntegerArgument(science);
		}
		m_retailPendingSciences.clear();
	}
}
