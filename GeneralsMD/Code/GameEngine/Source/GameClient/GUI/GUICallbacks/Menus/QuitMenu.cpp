/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: QuitMenu.cpp /////////////////////////////////////////////////////////////////////////////
// Author: Colin Day, October 2001
// Description: Quit menu window callbacks
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include <algorithm>
#include <array>
#include <vector>

#include "Common/AudioAffect.h"
#include "Common/AudioSettings.h"
#include "Common/FramePacer.h"
#include "Common/GameAudio.h"
#include "Common/GameEngine.h"
#include "Common/GameState.h"
#include "Common/MessageStream.h"
#include "Common/OptionPreferences.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/RandomValue.h"
#include "Common/Recorder.h"
#include "Common/RetailFileLayers.h"
#include "GameClient/GUICallbacks.h"
#include "GameClient/WindowLayout.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GameText.h"
#include "GameClient/MessageBox.h"
#include "GameClient/Shell.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/VictoryConditions.h"
#include "GameClient/ControlBar.h"
#include "GameClient/GameWindowTransitions.h"
#include "GameClient/DisconnectMenu.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameFont.h"
#include "GameClient/IMEManager.h"
#include "GameClient/Keyboard.h"
#include "GameClient/RetailOptions.h"
#include "GameClient/MapUtil.h"
#include "GameLogic/ScriptEngine.h"



// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////
static WindowLayout *quitMenuLayout = nullptr;
static WindowLayout *fullQuitMenuLayout = nullptr;
static WindowLayout *noSaveLoadQuitMenuLayout = nullptr;

static Bool isVisible = FALSE;

static GameWindow *quitConfirmationWindow = nullptr;

//external declarations of the Gadgets the callbacks can use
static WindowLayout *saveLoadMenuLayout = nullptr;

static GameWindow *buttonRestartWin	= nullptr;
static GameWindow *buttonSaveLoadWin = nullptr;
static GameWindow *buttonOptionsWin = nullptr;
static GameWindow *buttonExitWin = nullptr;

static NameKeyType buttonExit = NAMEKEY_INVALID;
static NameKeyType buttonRestart = NAMEKEY_INVALID;
static NameKeyType buttonReturn = NAMEKEY_INVALID;
static NameKeyType buttonOptions = NAMEKEY_INVALID;
static NameKeyType buttonSaveLoad = NAMEKEY_INVALID;

static void restartMissionMenu();

namespace
{
enum class PausePage { Main, Save, Load, ConfirmLeave, ConfirmRestart };
enum class PauseChoice { None = -1, Settings, Save, Load, Restart, Leave, Resume, Count };

GameWindow *retailPauseMenu = nullptr;
Bool retailMenuPausedGame = FALSE;
Bool leaveQueued = FALSE;
PausePage pausePage = PausePage::Main;
PauseChoice pauseSelection = PauseChoice::Resume;
PauseChoice pausePress = PauseChoice::None;
UnicodeString saveName;
UnicodeString saveFeedback;
struct PauseSave
{
	AvailableGameInfo game;
	UnicodeString title;
	UnicodeString map;
	UnicodeString time;
	UnicodeString date;
};
std::vector<PauseSave> saves;
Int selectedSave = 0;
Int firstSave = 0;
Bool loadQueued = FALSE;
Bool restartQueued = FALSE;

struct PauseCard
{
	Int x;
	Int y;
	Int width;
	Int height;

	Bool contains(Int px, Int py) const
	{
		return px >= x && px < x + width && py >= y && py < y + height;
	}
};

struct PauseLabel
{
	const char *key;
	const WideChar *fallback;
};

constexpr Int pauseDesignHeight = 768;
constexpr PauseCard pausePanel{360, 205, 304, 332};
constexpr PauseCard confirmationPanel{360, 312, 304, 170};
constexpr PauseCard savesPanel{230, 195, 564, 378};
constexpr PauseCard acceptButton{529, 520, 174, 36};
constexpr PauseCard cancelButton{321, 520, 174, 36};
constexpr Int firstFileRow = 272;
constexpr Int fileRowHeight = 26;
constexpr Int visibleFiles = 7;
constexpr Int saveColumnEdges[] = {260, 431, 586, 651, 764};
constexpr PauseLabel saveColumns[] = {
	{"GUI:Name", L"Name"}, {"GUI:Map", L"Map"}, {"GUI:Time", L"Time"}, {"GUI:Date", L"Date"},
};
constexpr PauseLabel mainLabels[] = {
	{"GUI:Settings", L"Settings"},
	{"GUI:SaveGame", L"Save"},
	{"GUI:LoadGame", L"Load"},
	{"GUI:Restart", L"Restart"},
	{"GUI:LeaveMatch", L"Leave Match"},
	{"GUI:Resume", L"Resume"},
};

PauseCard pauseCardOnScreen(PauseCard card)
{
	const Real scale = TheDisplay->getHeight() / static_cast<Real>(pauseDesignHeight);
	return {Int(TheDisplay->getWidth()) / 2 + Int((card.x - 512) * scale), Int(card.y * scale),
		Int(card.width * scale), Int(card.height * scale)};
}

PauseCard pauseButton(PauseChoice choice)
{
	if (pausePage == PausePage::ConfirmRestart)
		return choice == PauseChoice::Restart ? PauseCard{376, 430, 128, 36} : PauseCard{520, 430, 128, 36};
	if (pausePage == PausePage::ConfirmLeave)
		return choice == PauseChoice::Leave ? PauseCard{376, 370, 272, 42} : PauseCard{376, 421, 272, 42};
	return {376, 230 + Int(choice) * 51, 272, 39};
}

PauseCard fileRow(Int index)
{
	return {252, firstFileRow + index * fileRowHeight, 520, fileRowHeight};
}

PauseCard fileCell(Int rowY, Int column)
{
	return {saveColumnEdges[column], rowY, saveColumnEdges[column + 1] - saveColumnEdges[column] - 8, fileRowHeight};
}

Bool choiceAvailable(PauseChoice choice)
{
	if (choice == PauseChoice::Restart)
		return !TheGameLogic->isInMultiplayerGame() &&
			(!TheGameLogic->isInSkirmishGame() || TheSkirmishGameInfo != nullptr);
	if (choice == PauseChoice::Save || choice == PauseChoice::Load)
		return !TheGameLogic->isInMultiplayerGame() && !TheGameLogic->isInReplayGame();
	return TRUE;
}

PauseChoice confirmedChoice()
{
	if (pausePage == PausePage::ConfirmRestart)
		return PauseChoice::Restart;
	if (pausePage == PausePage::ConfirmLeave)
		return PauseChoice::Leave;
	return PauseChoice::None;
}

UnicodeString pauseText(PauseLabel label)
{
	return TheGameText->fetchOrSubstitute(label.key, label.fallback);
}

void drawPauseLabel(DisplayString *text, PauseCard card, UnicodeString label, Bool centered = TRUE)
{
	card = pauseCardOnScreen(card);
	Int width, height;
	text->setText(label);
	text->getSize(&width, &height);
	while (width > card.width && !label.isEmpty())
	{
		label.removeLastChar();
		text->setText(label);
		text->getSize(&width, &height);
	}
	text->draw(card.x + (centered ? (card.width - width) / 2 : 0),
		card.y + (card.height - height) / 2, 0xFFF5F6F7, 0);
}

void fillPauseCard(PauseCard card, Color color, Bool border = FALSE)
{
	card = pauseCardOnScreen(card);
	TheDisplay->drawFillRect(card.x, card.y, card.width, card.height, color);
	if (border)
		TheDisplay->drawOpenRect(card.x, card.y, card.width, card.height, 1.0f, 0xFF7897A8);
}

void drawPauseFiles(DisplayString *text)
{
	const Bool saving = pausePage == PausePage::Save;
	fillPauseCard(savesPanel, 0xFF14212A, TRUE);
	drawPauseLabel(text, {321, 210, 382, 35},
		pauseText(saving ? PauseLabel{"GUI:SaveGame", L"Save Game"} : PauseLabel{"GUI:LoadGame", L"Load Game"}));
	for (Int column = 0; column < Int(std::size(saveColumns)); ++column)
		drawPauseLabel(text, fileCell(firstFileRow - fileRowHeight, column), pauseText(saveColumns[column]), FALSE);
	for (Int row = 0; row < visibleFiles && firstSave + row < Int(saves.size()); ++row)
	{
		const Int index = firstSave + row;
		const PauseSave &save = saves[index];
		const PauseCard box = fileRow(row);
		fillPauseCard(box, !saving && index == selectedSave ? 0xFF365665 : 0xFF233741);
		drawPauseLabel(text, fileCell(box.y, 0), save.title, FALSE);
		drawPauseLabel(text, fileCell(box.y, 1), save.map, FALSE);
		drawPauseLabel(text, fileCell(box.y, 2), save.time, FALSE);
		drawPauseLabel(text, fileCell(box.y, 3), save.date, FALSE);
	}
	if (saving)
	{
		drawPauseLabel(text, {321, 461, 382, 23},
			saveFeedback.isEmpty() ? pauseText({"GUI:SaveName", L"Save name"}) : saveFeedback, FALSE);
		fillPauseCard({321, 484, 382, 27}, 0xFF233741, TRUE);
		UnicodeString editing = saveName;
		editing.concat(L'|');
		drawPauseLabel(text, {329, 484, 366, 27}, editing, FALSE);
	}
	else
		drawPauseLabel(text, {321, 487, 382, 27}, saveFeedback);
	fillPauseCard(cancelButton, 0xFF233741);
	fillPauseCard(acceptButton, saving || !saves.empty() ? 0xFF365665 : 0xFF233741);
	drawPauseLabel(text, cancelButton, pauseText({"GUI:Cancel", L"Cancel"}));
	drawPauseLabel(text, acceptButton, pauseText(mainLabels[Int(saving ? PauseChoice::Save : PauseChoice::Load)]));
}

void drawRetailPauseMenu(GameWindow *window, WinInstanceData *)
{
	const Int width = TheDisplay->getWidth();
	const Int height = TheDisplay->getHeight();
	Int oldWidth, oldHeight;
	window->winGetSize(&oldWidth, &oldHeight);
	if (oldWidth != width || oldHeight != height)
		window->winSetSize(width, height);
	TheDisplay->drawFillRect(0, 0, width, height, 0x98000000);
	const PauseCard panel = pausePage == PausePage::Main ? pausePanel : confirmationPanel;
	const PauseChoice confirm = confirmedChoice();
	if (pausePage == PausePage::Main || confirm != PauseChoice::None)
		fillPauseCard(panel, 0xFF14212A, TRUE);
	GameFont *font = TheFontLibrary->getFontPixels("Albertus MT", 18 * height / pauseDesignHeight, FALSE);
	if (!font)
		return;
	DisplayString *text = TheDisplayStringManager->newDisplayString();
	text->setFont(font);
	if (pausePage == PausePage::Main)
	{
		drawPauseLabel(text, {376, 207, 272, 22}, pauseText({"GUI:MatchMenu", L"Match Menu"}));
		for (Int i = 0; i < Int(PauseChoice::Count); ++i)
		{
			const auto choice = static_cast<PauseChoice>(i);
			if (!choiceAvailable(choice))
				continue;
			const PauseCard box = pauseButton(choice);
			fillPauseCard(box, pauseSelection == choice ? 0xFF365665 : 0xFF233741);
			drawPauseLabel(text, box, pauseText(mainLabels[i]));
		}
	}
	else if (confirm != PauseChoice::None)
	{
		drawPauseLabel(text, {376, 323, 272, 35}, pauseText(confirm == PauseChoice::Restart
			? PauseLabel{"GUI:RestartPrompt", L"Restart this match?"}
			: PauseLabel{"GUI:LeaveMatchPrompt", L"Leave this match?"}));
		for (const PauseChoice choice : {confirm, PauseChoice::Resume})
			fillPauseCard(pauseButton(choice), pauseSelection == choice ? 0xFF365665 : 0xFF233741);
		drawPauseLabel(text, pauseButton(confirm), pauseText({"GUI:Yes", L"Yes"}));
		drawPauseLabel(text, pauseButton(PauseChoice::Resume), pauseText({"GUI:No", L"No"}));
	}
	else
		drawPauseFiles(text);
	TheDisplayStringManager->freeDisplayString(text);
}

void resetRetailPauseMenu(GameWindow *window)
{
	closeRetailOptionsFor(window);
	if (TheIMEManager && TheIMEManager->isAttachedTo(window))
		TheIMEManager->detach();
	if (window != retailPauseMenu)
		return;
	retailPauseMenu = nullptr;
	saves.clear();
	loadQueued = FALSE;
	restartQueued = FALSE;
	if (TheInGameUI)
		TheInGameUI->setQuitMenuVisible(FALSE);
}

WindowMsgHandledType retailPauseSystem(GameWindow *window, UnsignedInt message, WindowMsgData state, WindowMsgData data)
{
	if (message == GWM_INPUT_FOCUS)
	{
		if (TheIMEManager)
		{
			if (state == TRUE && pausePage == PausePage::Save)
				TheIMEManager->attach(window);
			else if (TheIMEManager->isAttachedTo(window))
				TheIMEManager->detach();
		}
		*reinterpret_cast<Bool *>(data) = state == TRUE;
		return MSG_HANDLED;
	}
	if (message == GWM_DESTROY)
		resetRetailPauseMenu(window);
	return MSG_IGNORED;
}

void closeRetailPauseMenu()
{
	GameWindow *window = retailPauseMenu;
	resetRetailPauseMenu(window);
	TheWindowManager->winDestroy(window);
	if (retailMenuPausedGame && TheGameLogic->isGamePaused())
		TheGameLogic->setGamePaused(FALSE);
	retailMenuPausedGame = FALSE;
}

void showPauseMain()
{
	pausePage = PausePage::Main;
	pauseSelection = PauseChoice::Resume;
	pausePress = PauseChoice::None;
	BitClear(retailPauseMenu->winGetInstanceData()->m_style, GWS_ENTRY_FIELD);
	if (TheIMEManager && TheIMEManager->isAttachedTo(retailPauseMenu))
		TheIMEManager->detach();
}

void openPauseFiles(PausePage page)
{
	pausePage = page;
	saveName.clear();
	saveFeedback.clear();
	saves.clear();
	for (const AvailableGameInfo *info = TheGameState->refreshAvailableGames(); info; info = info->next)
	{
		if (info->saveGameInfo.saveFileType == SAVE_FILE_TYPE_NORMAL)
		{
			const SaveGameInfo &game = info->saveGameInfo;
			UnicodeString title = game.description;
			if (title.isEmpty())
				title.translate(info->filename);
			UnicodeString map;
			map.translate(game.mapLabel);
			const AsciiString path = TheGameState->portableMapPathToRealMapPath(game.pristineMapName);
			if (const MapMetaData *meta = TheMapCache->findMap(path))
				map = meta->m_displayName;
			const SaveDate &date = game.date;
			const SYSTEMTIME time{date.year, date.month, date.dayOfWeek, date.day, date.hour, date.minute,
				date.second, date.milliseconds};
			saves.push_back({{info->filename, game, nullptr, nullptr}, title,
				TheGameText->fetchOrSubstitute(game.mapLabel.str(), map.str()),
				getUnicodeTimeBuffer(time), getUnicodeDateBuffer(time)});
		}
	}
	firstSave = 0;
	selectedSave = 0;
	if (page == PausePage::Save)
	{
		BitSet(retailPauseMenu->winGetInstanceData()->m_style, GWS_ENTRY_FIELD);
		if (TheIMEManager)
			TheIMEManager->attach(retailPauseMenu);
	}
}

void acceptPauseFiles()
{
	if (pausePage == PausePage::Load)
	{
		loadQueued = !saves.empty();
		return;
	}

	saveName.trim();
	if (saveName.isEmpty())
	{
		saveFeedback = pauseText({"GUI:EnterSaveName", L"Enter a save name."});
		return;
	}
	if (TheGameState->saveGame({}, saveName, SAVE_FILE_TYPE_NORMAL).saveCode == SC_OK)
		showPauseMain();
	else
		saveFeedback = pauseText({"GUI:SaveGameFailed", L"Could not save the match."});
}

void activatePauseChoice(PauseChoice choice)
{
	if (leaveQueued || loadQueued || restartQueued)
		return;
	switch (choice)
	{
		case PauseChoice::Resume:
			if (pausePage == PausePage::Main)
				HideQuitMenu();
			else
				showPauseMain();
			break;
		case PauseChoice::Leave:
			if (pausePage == PausePage::ConfirmLeave)
			{
				leaveQueued = TRUE;
				TheGameLogic->quit(FALSE);
			}
			else
			{
				pausePage = PausePage::ConfirmLeave;
				pauseSelection = PauseChoice::Resume;
				pausePress = PauseChoice::None;
			}
			break;
		case PauseChoice::Settings:
			openRetailOptions(retailPauseMenu, TRUE);
			break;
		case PauseChoice::Save:
			openPauseFiles(PausePage::Save);
			break;
		case PauseChoice::Load:
			openPauseFiles(PausePage::Load);
			break;
		case PauseChoice::Restart:
			if (pausePage == PausePage::ConfirmRestart)
				restartQueued = TRUE;
			else
			{
				pausePage = PausePage::ConfirmRestart;
				pauseSelection = PauseChoice::Resume;
				pausePress = PauseChoice::None;
			}
			break;
		default:
			break;
	}
}

void stepPauseChoice(Int direction)
{
	if (const PauseChoice confirm = confirmedChoice(); confirm != PauseChoice::None)
	{
		pauseSelection = pauseSelection == PauseChoice::Resume ? confirm : PauseChoice::Resume;
		return;
	}
	const Int count = Int(PauseChoice::Count);
	do
	{
		pauseSelection = static_cast<PauseChoice>((Int(pauseSelection) + count + direction) % count);
	}
	while (!choiceAvailable(pauseSelection));
}

void moveSaveSelection(Int direction)
{
	if (saves.empty())
		return;
	if (pausePage == PausePage::Save)
	{
		firstSave = std::clamp(firstSave + direction, 0, std::max(0, Int(saves.size()) - visibleFiles));
		return;
	}
	selectedSave = std::clamp(selectedSave + direction, 0, Int(saves.size()) - 1);
	firstSave = std::clamp(firstSave, std::max(0, selectedSave - visibleFiles + 1), selectedSave);
}

void pauseFilesInput(UnsignedInt message, WindowMsgData point, WindowMsgData state)
{
	if (loadQueued)
		return;
	const Bool saving = pausePage == PausePage::Save;
	const Bool key = message == GWM_CHAR && (state & KEY_STATE_UP);
	if (saving && message == GWM_IME_CHAR && point >= L' ' && point != 127 && saveName.getLength() < 32)
	{
		saveFeedback.clear();
		saveName.concat(static_cast<WideChar>(point));
	}
	else if (key && saving && point == KEY_BACKSPACE && !saveName.isEmpty())
		saveName.removeLastChar();
	else if (key && point == KEY_ESC)
		showPauseMain();
	else if (key && (point == KEY_ENTER || point == KEY_KPENTER))
		acceptPauseFiles();
	else if (message == GWM_WHEEL_UP || (key && point == KEY_UP))
		moveSaveSelection(-1);
	else if (message == GWM_WHEEL_DOWN || (key && point == KEY_DOWN))
		moveSaveSelection(1);
	else if (message == GWM_LEFT_UP)
	{
		const Int x = point & 0xFFFF;
		const Int y = point >> 16;
		if (pauseCardOnScreen(cancelButton).contains(x, y))
			showPauseMain();
		else if (pauseCardOnScreen(acceptButton).contains(x, y))
			acceptPauseFiles();
		else if (!saving)
		{
			for (Int row = 0; row < visibleFiles && firstSave + row < Int(saves.size()); ++row)
				if (pauseCardOnScreen(fileRow(row)).contains(x, y))
					selectedSave = firstSave + row;
		}
	}
}

WindowMsgHandledType retailPauseInput(GameWindow *, UnsignedInt message, WindowMsgData point, WindowMsgData state)
{
	if (pausePage == PausePage::Save || pausePage == PausePage::Load)
	{
		pauseFilesInput(message, point, state);
		return MSG_HANDLED;
	}
	if (message == GWM_CHAR && (state & KEY_STATE_UP))
	{
		if (point == KEY_ESC)
			activatePauseChoice(PauseChoice::Resume);
		else if (point == KEY_ENTER || point == KEY_KPENTER)
			activatePauseChoice(pauseSelection);
		else if (point == KEY_UP || point == KEY_DOWN || point == KEY_TAB)
			stepPauseChoice(point == KEY_UP ? -1 : 1);
	}
	if (message == GWM_MOUSE_POS || message == GWM_LEFT_DOWN || message == GWM_LEFT_UP)
	{
		const Int x = point & 0xFFFF;
		const Int y = point >> 16;
		PauseChoice at = PauseChoice::None;
		const PauseChoice confirm = confirmedChoice();
		for (Int i = 0; i < Int(PauseChoice::Count); ++i)
		{
			const auto choice = static_cast<PauseChoice>(i);
			if (confirm != PauseChoice::None && choice != PauseChoice::Resume && choice != confirm)
				continue;
			if (choiceAvailable(choice) && pauseCardOnScreen(pauseButton(choice)).contains(x, y))
				at = choice;
		}
		if (at != PauseChoice::None)
			pauseSelection = at;
		if (message == GWM_LEFT_DOWN)
			pausePress = at;
		else if (message == GWM_LEFT_UP)
		{
			const PauseChoice pressed = pausePress;
			pausePress = PauseChoice::None;
			if (at == pressed)
				activatePauseChoice(at);
		}
	}
	return MSG_HANDLED;
}

void openRetailPauseMenu()
{
	WinInstanceData data;
	data.m_style = GWS_USER_WINDOW;
	retailPauseMenu = TheWindowManager->winCreate(nullptr, WIN_STATUS_ENABLED | WIN_STATUS_ABOVE, 0, 0,
		TheDisplay->getWidth(), TheDisplay->getHeight(), nullptr, &data);
	if (!retailPauseMenu)
		return;
	retailPauseMenu->winSetDrawFunc(drawRetailPauseMenu);
	retailPauseMenu->winSetInputFunc(retailPauseInput);
	retailPauseMenu->winSetSystemFunc(retailPauseSystem);
	leaveQueued = FALSE;
	showPauseMain();
	HideDiplomacy();
	HideInGameChat();
	TheControlBar->hidePurchaseScience();
	TheMouse->setCursor(Mouse::ARROW);
	if (GameWindow *grab = TheWindowManager->winGetGrabWindow())
	{
		grab->winClearStatus(WIN_STATUS_ACTIVE);
		TheWindowManager->winSetGrabWindow(nullptr);
	}
	if (GameWindow *capture = TheWindowManager->winGetCapture())
		TheWindowManager->winRelease(capture);
	TheWindowManager->winSetModal(retailPauseMenu);
	TheWindowManager->winSetFocus(retailPauseMenu);
	TheInGameUI->setQuitMenuVisible(TRUE);
	retailMenuPausedGame = !TheGameLogic->isInMultiplayerGame() && !TheGameLogic->isGamePaused();
	if (retailMenuPausedGame)
		TheGameLogic->setGamePaused(TRUE);
}
}

void updateRetailPauseMenu()
{
	if (restartQueued)
	{
		restartQueued = FALSE;
		restartMissionMenu();
		return;
	}
	if (!loadQueued)
		return;

	const AvailableGameInfo selected = saves[selectedSave].game;
	loadQueued = FALSE;
	SaveCode result = SC_ERROR;
	try
	{
		result = TheGameState->loadGame(selected);
	}
	catch (...)
	{
	}
	if (result == SC_OK)
		return;
	const UnicodeString failure = pauseText({"GUI:SaveGameLoadFailed", L"Could not load the match."});
	if (retailPauseMenu && TheGameLogic->isInGame())
		saveFeedback = failure;
	else
	{
		TheWritableGlobalData->m_initialFile.clear();
		TheGameEngine->setQuitting(FALSE);
		TheShell->showShell(TRUE);
		MessageBoxOk(pauseText({"GUI:LoadGame", L"Load Game"}), failure, nullptr);
	}
}

static void initGadgetsFullQuit()
{
	buttonExit = TheNameKeyGenerator->nameToKey( "QuitMenu.wnd:ButtonExit" );
	buttonRestart = TheNameKeyGenerator->nameToKey( "QuitMenu.wnd:ButtonRestart" );
	buttonReturn = TheNameKeyGenerator->nameToKey( "QuitMenu.wnd:ButtonReturn" );
	buttonOptions = TheNameKeyGenerator->nameToKey( "QuitMenu.wnd:ButtonOptions" );
	buttonSaveLoad = TheNameKeyGenerator->nameToKey( "QuitMenu.wnd:ButtonSaveLoad" );

	buttonRestartWin	= TheWindowManager->winGetWindowFromId( nullptr, buttonRestart );
	buttonSaveLoadWin = TheWindowManager->winGetWindowFromId( nullptr, buttonSaveLoad );
	buttonOptionsWin = TheWindowManager->winGetWindowFromId( nullptr, buttonOptions );
	buttonExitWin = TheWindowManager->winGetWindowFromId( nullptr, buttonExit );
}

static void initGadgetsNoSaveQuit()
{
	buttonExit = TheNameKeyGenerator->nameToKey( "QuitNoSave.wnd:ButtonExit" );
	buttonRestart = TheNameKeyGenerator->nameToKey( "QuitNoSave.wnd:ButtonRestart" );
	buttonReturn = TheNameKeyGenerator->nameToKey( "QuitNoSave.wnd:ButtonReturn" );
	buttonOptions = TheNameKeyGenerator->nameToKey( "QuitNoSave.wnd:ButtonOptions" );
	buttonSaveLoad = NAMEKEY_INVALID;

	buttonRestartWin	= TheWindowManager->winGetWindowFromId( nullptr, buttonRestart );
	buttonOptionsWin = TheWindowManager->winGetWindowFromId( nullptr, buttonOptions );
	buttonSaveLoadWin = nullptr;
	buttonExitWin = TheWindowManager->winGetWindowFromId( nullptr, buttonExit );

}

// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////////////////////////

void destroyQuitMenu()
{
	if (retailPauseMenu)
		closeRetailPauseMenu();
  // destroy the quit menu
	quitConfirmationWindow = nullptr;
	if(fullQuitMenuLayout)
	{
		fullQuitMenuLayout->destroyWindows();
		deleteInstance(fullQuitMenuLayout);
		fullQuitMenuLayout = nullptr;
	}
	if(noSaveLoadQuitMenuLayout)
	{
		noSaveLoadQuitMenuLayout->destroyWindows();
		deleteInstance(noSaveLoadQuitMenuLayout);
		noSaveLoadQuitMenuLayout = nullptr;
	}
	quitMenuLayout = nullptr;
	isVisible = FALSE;

	TheInGameUI->setQuitMenuVisible(FALSE);
}

/**
 *  quits the program
 */
static void exitQuitMenu()
{
	TheGameLogic->quit(FALSE);
  // destroy the quit menu
	destroyQuitMenu();
}
static void noExitQuitMenu()
{
	quitConfirmationWindow = nullptr;
}

static void quitToDesktopQuitMenu()
{
	TheGameLogic->quit(TRUE);
  // destroy the quit menu
	destroyQuitMenu();
}

static void surrenderQuitMenu()
{
  // destroy the quit menu
	destroyQuitMenu();

	if (TheVictoryConditions->isLocalAlliedVictory())
		return;

	GameMessage *msg = TheMessageStream->appendMessage(GameMessage::MSG_SELF_DESTRUCT);
	msg->appendBooleanArgument(TRUE);

	TheInGameUI->setClientQuiet( TRUE );
}

static void restartMissionMenu()
{
	// destroy the quit menu
	destroyQuitMenu();

	GameMode gameMode = TheGameLogic->getGameMode();
	AsciiString mapName = TheGlobalData->m_mapName;

	// TheSuperHackers @bugfix Caball009 07/02/2026 Reuse the previous seed value for the new skirmish match to prevent mismatches.
	// Campaign, challenge, and skirmish single-player scenarios all use GAME_SINGLE_PLAYER and are expected to use 0 as seed value.
	DEBUG_ASSERTCRASH((TheSkirmishGameInfo != nullptr) == (gameMode == GAME_SKIRMISH), ("Unexpected game mode on map / mission restart"));
	const Int seed = TheSkirmishGameInfo ? TheSkirmishGameInfo->getSeed() : 0;

	//
	// if the map name was from a save game it will have "Save/" at the front of it,
	// we want to go back to the original pristine map string for the map name when restarting
	//
	if (TheGameState->isInSaveDirectory(mapName))
		mapName = TheGameState->getPristineMapName();

	// End the current game
	AsciiString replayFile = TheRecorder->getCurrentReplayFilename();
	if (TheRecorder->getMode() == RECORDERMODETYPE_RECORD)
	{
		TheRecorder->stopRecording();
	}

	Int rankPointsStartedWith = TheGameLogic->getRankPointsToAddAtGameStart();// must write down before reset
	GameDifficulty diff = TheScriptEngine->getGlobalDifficulty();
	Int fps = TheFramePacer->getFramesPerSecondLimit();

	TheGameLogic->clearGameData(FALSE);
	TheGameEngine->setQuitting(FALSE);

	if (replayFile.isNotEmpty())
	{
		TheRecorder->playbackFile(replayFile);
	}
	else
	{
		// send a message to the logic for a new game
		TheWritableGlobalData->m_pendingFile = mapName;
		GameMessage *msg = TheMessageStream->appendMessage( GameMessage::MSG_NEW_GAME );
		msg->appendIntegerArgument((Int)gameMode);
		msg->appendIntegerArgument(diff);
		msg->appendIntegerArgument(rankPointsStartedWith);
		msg->appendIntegerArgument(fps);
		DEBUG_LOG(("Restarting game mode %d, Diff=%d, RankPoints=%d", (Int)gameMode,
																																		TheScriptEngine->getGlobalDifficulty(),
																																		rankPointsStartedWith)
							);

		InitRandom(seed);
	}
	//TheTransitionHandler->remove("QuitFull"); //KRISMORNESS ADD
	//quitMenuLayout = nullptr; //KRISMORNESS ADD
	//isVisible = TRUE; //KRISMORNESS ADD
	//HideQuitMenu();	//KRISMORNESS ADD
	TheInGameUI->setClientQuiet( TRUE );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void HideQuitMenu()
{
	if (retailPauseMenu)
	{
		closeRetailPauseMenu();
		return;
	}
	// Note: This is called as a safety a lot, without checking for the presence of the quit menu.
	// So don't do anything that counts on that menu actually being here.
	if(!isVisible)
		return;
	if(quitMenuLayout && quitMenuLayout == noSaveLoadQuitMenuLayout)
		TheTransitionHandler->reverse("QuitNoSaveBack");
	else if( quitMenuLayout && quitMenuLayout == fullQuitMenuLayout)
		TheTransitionHandler->reverse("QuitFullBack");

	TheInGameUI->setQuitMenuVisible( FALSE );
	isVisible = FALSE;
	if (quitConfirmationWindow)
		TheWindowManager->winDestroy(quitConfirmationWindow);
	quitConfirmationWindow = nullptr;
	if ( !TheGameLogic->isInMultiplayerGame() )
			TheGameLogic->setGamePaused(FALSE);

}

Bool canOpenQuitMenu()
{
	return (TheGameEngine != nullptr && TheGameEngine->isActive() &&
		TheGameLogic != nullptr &&
		(!TheInGameUI || !TheInGameUI->isQuitMenuVisible()) &&
		!TheGameLogic->isLoadingMap() &&
		!TheGameLogic->isLoadingSave() &&
		!TheGameLogic->isIntroMoviePlaying() &&
		(TheScriptEngine == nullptr || !TheScriptEngine->isGameEnding()));
}

//-------------------------------------------------------------------------------------------------
/** Toggle visibility of the quit menu */
//-------------------------------------------------------------------------------------------------
void ToggleQuitMenu()
{
	if (retailPauseMenu)
	{
		activatePauseChoice(PauseChoice::Resume);
		return;
	}
	if (!isVisible && !canOpenQuitMenu())
		return;

	// BGC- If we are currently in the disconnect screen, don't let the quit menu come up.
	if (TheDisconnectMenu != nullptr) {
		if (TheDisconnectMenu->isScreenVisible() == TRUE) {
			return;
		}
	}

	if (RetailFileLayers::enabled())
	{
		if (TheGameLogic->isInGame() && !TheGameLogic->isInShellGame() && !TheShell->isShellActive())
			openRetailPauseMenu();
		return;
	}

	// BGC- this is kind of hackish, but its the safest way to do it I think.
	// Basically we're seeing if either the save/load window or the options window is up
	// and if one of them is, we quit out of them rather than toggle the quit menu.
	if (TheShell->getOptionsLayout(FALSE) != nullptr) {
		WindowLayout *optLayout = TheShell->getOptionsLayout(FALSE);
		GameWindow *optionsParent = optLayout->getFirstWindow();
		DEBUG_ASSERTCRASH(optionsParent != nullptr, ("Not able to get the options layout parent window"));
		GameWindow *optionsBack = TheWindowManager->winGetWindowFromId(optionsParent, TheNameKeyGenerator->nameToKey( "OptionsMenu.wnd:ButtonBack" ));
		DEBUG_ASSERTCRASH(optionsBack != nullptr, ("Not able to get the back button window from the options menu"));
		TheWindowManager->winSendSystemMsg(optLayout->getFirstWindow(), GBM_SELECTED, (WindowMsgData)optionsBack, 0);
		return;
	}
	if ((saveLoadMenuLayout != nullptr) && (saveLoadMenuLayout->isHidden() == FALSE))
	{
		GameWindow *saveLoadParent = saveLoadMenuLayout->getFirstWindow();
		DEBUG_ASSERTCRASH(saveLoadParent != nullptr, ("Not able to get the save/load layout parent window"));
		GameWindow *saveLoadBack = TheWindowManager->winGetWindowFromId(saveLoadParent, TheNameKeyGenerator->nameToKey( "PopupSaveLoad.wnd:ButtonBack" ));
		DEBUG_ASSERTCRASH(saveLoadBack != nullptr, ("Not able to get the back button window from the save/load menu"));
		TheWindowManager->winSendSystemMsg(saveLoadMenuLayout->getFirstWindow(), GBM_SELECTED, (WindowMsgData)saveLoadBack, 0);
		saveLoadMenuLayout = nullptr;
		return;
	}

	// if we're visible hide our quit menu
	if(isVisible && quitMenuLayout)
	{

		isVisible = FALSE;

		if (quitConfirmationWindow)
			TheWindowManager->winDestroy(quitConfirmationWindow);
		quitConfirmationWindow = nullptr;

		if ( !TheGameLogic->isInMultiplayerGame() )
			TheGameLogic->setGamePaused(FALSE);
		if(quitMenuLayout && quitMenuLayout == noSaveLoadQuitMenuLayout)
			TheTransitionHandler->reverse("QuitNoSaveBack");
		else if( quitMenuLayout && quitMenuLayout == fullQuitMenuLayout )
		{
			TheTransitionHandler->reverse("QuitFullBack");
			//begin KRISMORNESS
			//TheTransitionHandler->reverse("QuitFull");
			//if( TheTransitionHandler->areTransitionsEnabled() )
			//else
			//{
			//	TheTransitionHandler->remove("QuitFull");
			//	quitMenuLayout = nullptr;
			//	isVisible = TRUE;
			//	HideQuitMenu();
			//}
			//end KRISMORNESS
		}
	}
	else
	{

		TheMouse->setCursor( Mouse::ARROW );

		TheControlBar->hidePurchaseScience();
		if ( TheGameLogic->isInMultiplayerGame()  || TheGameLogic->isInReplayGame() )
		{
			// we don't want to show the save load button.
			if(!noSaveLoadQuitMenuLayout)
				noSaveLoadQuitMenuLayout = TheWindowManager->winCreateLayout( "Menus/QuitNoSave.wnd" );
			quitMenuLayout = noSaveLoadQuitMenuLayout;
			initGadgetsNoSaveQuit();
			TheTransitionHandler->remove("QuitNoSave");
			TheTransitionHandler->setGroup("QuitNoSave");
		}
		else
		{
			if(!fullQuitMenuLayout)
				fullQuitMenuLayout= TheWindowManager->winCreateLayout( "Menus/QuitMenu.wnd" );
			quitMenuLayout = fullQuitMenuLayout;
			initGadgetsFullQuit();
			TheTransitionHandler->remove("QuitFull");
			TheTransitionHandler->setGroup("QuitFull");
		}

		// load the quit menu from the layout file if needed
		if( quitMenuLayout == nullptr )
		{
			if (!RetailFileLayers::enabled())
				DEBUG_CRASH(("Could not load a quit menu layout"));
			isVisible = FALSE;
			TheInGameUI->setQuitMenuVisible(FALSE);
			return;
		}

		//quitMenuLayout->hide(FALSE);

		// if we are watching a cinematic, we need to disable the save/load button
		// because the save load window doesn't fit in the screen in letterbox mode.
		if (TheInGameUI->getInputEnabled() == FALSE) {
			if(buttonSaveLoadWin)
				buttonSaveLoadWin->winEnable(FALSE);
			buttonOptionsWin->winEnable(FALSE);
		} else if (buttonSaveLoadWin)
		{
			if(buttonSaveLoadWin)
				buttonSaveLoadWin->winEnable(TRUE);
			buttonOptionsWin->winEnable(TRUE);
		}

		// Disable the restart, load, save, etc buttons in network games
		if ( TheGameLogic->isInMultiplayerGame() || TheGameLogic->isInSkirmishGame() )
		{
			buttonRestartWin->winEnable(TRUE);
			if (TheGameLogic->isInSkirmishGame() == FALSE) {
				GadgetButtonSetText(buttonRestartWin, TheGameText->fetch("GUI:Surrender"));

			}

			if (TheGameLogic->isInSkirmishGame() == TRUE) {
				TheGameLogic->setGamePaused(TRUE);
			}

			if ((!ThePlayerList->getLocalPlayer()->isPlayerActive() || TheVictoryConditions->isLocalAlliedVictory()) &&
						(TheGameLogic->isInSkirmishGame() == FALSE))
			{
				buttonRestartWin->winEnable(FALSE); // can't surrender when you're dead
			}
		}
		else
		{
			buttonRestartWin->winEnable(TRUE);
			if(!TheGameLogic->isInReplayGame())
			{
				GadgetButtonSetText(buttonRestartWin, TheGameText->fetch("GUI:RestartMission"));
				GadgetButtonSetText(buttonExitWin, TheGameText->fetch("GUI:ExitMission"));

			}
			//if we're not in a multiplayer game, pause the game
			TheGameLogic->setGamePaused(TRUE);
		}


		if (quitConfirmationWindow)
			TheWindowManager->winDestroy(quitConfirmationWindow);
		quitConfirmationWindow = nullptr;
		HideDiplomacy();
		HideInGameChat();
		TheControlBar->hidePurchaseScience();
		isVisible = TRUE;
	}

	TheInGameUI->setQuitMenuVisible(isVisible);

}

//-------------------------------------------------------------------------------------------------
/** Quit menu window system callback */
//-------------------------------------------------------------------------------------------------
WindowMsgHandledType QuitMenuSystem( GameWindow *window, UnsignedInt msg,
																		 WindowMsgData mData1, WindowMsgData mData2 )
{
	switch( msg )
	{

		// --------------------------------------------------------------------------------------------
		case GWM_CREATE:
		{

			break;

		}

		//---------------------------------------------------------------------------------------------
		case GWM_DESTROY:
		{

			break;

		}

		//---------------------------------------------------------------------------------------------
		case GBM_SELECTED:
		{
			GameWindow *control = (GameWindow *)mData1;
			Int controlID = control->winGetWindowId();

      if( controlID == buttonSaveLoad )
      {

				//
        // these commented lines (12-11-2002) will allow access to load only when were
        // viewing an in-game cinema ... but it's brittle, so I'm disableing it for
        // now and just using the grey button for the whole save/load button for now
        // during a cinema
        //

//				SaveLoadLayoutType layoutType = SLLT_SAVE_AND_LOAD;

				//
				// if input is disabled we are in an in-game cinematic and can load only, since we
				// are in the quit menu we are paused and therefore input is disabled all
				// the time ... in order to figure out if the *game* has input disabled we need
				// to query for the input enabled memory in the game logic which represents the
				// input enabled status of the game if we were not currently paused
				//
//				if( TheGameLogic->getInputEnabledMemory() == FALSE )
//					layoutType = SLLT_LOAD_ONLY;

        saveLoadMenuLayout = TheShell->getSaveLoadMenuLayout();
//				saveLoadMenuLayout->runInit( &layoutType );
				saveLoadMenuLayout->runInit();
				saveLoadMenuLayout->hide( FALSE );
				saveLoadMenuLayout->bringForward();
      }
			else if( controlID == buttonExit )
			{
        quitConfirmationWindow = QuitMessageBoxYesNo(TheGameText->fetch("GUI:QuitPopupTitle"), TheGameText->fetch("GUI:QuitPopupMessage"),/*quitCallback*/exitQuitMenu,noExitQuitMenu);
			}
			else if( controlID == buttonReturn )
			{

				// hide this menu
				ToggleQuitMenu();

			}
			else if( buttonOptions == controlID )
			{
				WindowLayout *optLayout = TheShell->getOptionsLayout(TRUE);
				DEBUG_ASSERTCRASH(optLayout != nullptr, ("options menu layout is null"));
				optLayout->runInit();
				optLayout->hide(FALSE);
				optLayout->bringForward();
			}
//			else if( controlID == buttonQuitToDesktop )
//			{
//				quitConfirmationWindow = MessageBoxYesNo(TheGameText->fetch("GUI:QuitPopupTitle"), TheGameText->fetch("GUI:QuitToDesktopConf"),/*quitCallback*/quitToDesktopQuitMenu,noExitQuitMenu);
//
//			}  // end else if
			else if( controlID == buttonRestart )
			{
				if ( TheGameLogic->isInMultiplayerGame() )
				{
					// we really want to surrender
					quitConfirmationWindow = MessageBoxYesNo(TheGameText->fetch("GUI:SurrenderConfirmationTitle"),
																			TheGameText->fetch("GUI:SurrenderConfirmation"),
																			/*quitCallback*/surrenderQuitMenu,noExitQuitMenu);
				}
				else
				{
					//we really want to restart
					quitConfirmationWindow = MessageBoxYesNo(TheGameText->fetch("GUI:RestartConfirmationTitle"),
																			TheGameText->fetch("GUI:RestartConfirmation"),
																			/*quitCallback*/restartMissionMenu,noExitQuitMenu);
				}
			}

			break;

		}

		default:
			return MSG_IGNORED;

	}

	return MSG_HANDLED;

}
