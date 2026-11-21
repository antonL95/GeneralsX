#include "PreRTS.h"

#include "GameClient/RetailMatchScreens.h"

#include "Common/FileSystem.h"
#include "Common/GameEngine.h"
#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "GameClient/AptScene.h"
#include "GameClient/CampaignManager.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Image.h"
#include "GameClient/Keyboard.h"
#include "GameClient/MapUtil.h"
#include "GameClient/Shell.h"
#include "GameClient/WindowLayout.h"
#include "GameNetwork/GameInfo.h"
#include "GameLogic/GameLogic.h"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace
{
	using Box = AptScene::Box;
	constexpr Color white = 0xFFFFFFFF;
	constexpr Color muted = 0xFFB3CBD5;

	UnicodeString translated(const char *text)
	{
		UnicodeString result;
		result.translate(text);
		return result;
	}

	UnicodeString localize(const char *key, const char *fallback)
	{
		Bool exists = FALSE;
		UnicodeString text = TheGameText->fetch(key, &exists);
		return exists ? text : translated(fallback);
	}

	UnicodeString number(Int n)
	{
		UnicodeString result;
		result.format(L"%d", n);
		return result;
	}

	UnicodeString aptText(const std::string &text)
	{
		if (!text.empty() && text.front() == '$')
		{
			const std::string key = "APT:" + text.substr(1);
			return localize(key.c_str(), text.c_str() + 1);
		}
		return translated(text.c_str());
	}

	class Painter
	{
	public:
		explicit Painter(const AptScene &scene) : sx(scene.xScale()), sy(scene.yScale()),
			m_text(TheDisplayStringManager->newDisplayString()) {}
		~Painter() { TheDisplayStringManager->freeDisplayString(m_text); }

		const Real sx, sy;

		void picture(const Image *image, const Box &box) const
		{
			if (image && box.valid())
				TheDisplay->drawImage(image, Int(box.left * sx), Int(box.top * sy), Int(box.right * sx), Int(box.bottom * sy));
		}

		void background(const Image *image) const
		{
			const Int width = TheDisplay->getWidth(), height = TheDisplay->getHeight();
			TheDisplay->drawFillRect(0, 0, width, height, GameMakeColor(19, 29, 30, 255));
			if (image)
				TheDisplay->drawImage(image, 0, 0, width, height, GameMakeColor(70, 99, 97, 255));
		}

		void text(UnicodeString value, const Box &box, Real size, Color color = white,
			const char *fontName = "Albertus MT", Bool centered = TRUE, Bool wrap = FALSE) const
		{
			if (value.isEmpty() || !box.valid())
				return;

			GameFont *font = TheFontLibrary->getFontPixels(fontName, std::max(1, Int(size * sy + 0.5f)), FALSE);
			if (!font)
				font = TheFontLibrary->getFontPixels("Albertus MT", std::max(1, Int(size * sy + 0.5f)), FALSE);
			if (!font)
				return;

			m_text->setFont(font);
			const Int maxWidth = std::max(1, Int((box.right - box.left) * sx));
			m_text->setWordWrap(wrap ? maxWidth : 0);
			m_text->setText(value);
			Int width, height;
			m_text->getSize(&width, &height);
			const Int left = Int(box.left * sx);
			const Int x = centered ? left + std::max(0, (maxWidth - width) / 2) : left;
			const Int y = Int(box.top * sy) + (wrap ? 0 : std::max(0, (Int((box.bottom - box.top) * sy) - height) / 2));
			IRegion2D clip{{left, Int(box.top * sy)}, {left + maxWidth, Int(box.bottom * sy)}};
			m_text->setClipRegion(&clip);
			m_text->draw(x, y, color, GameMakeColor(0, 0, 0, 230));
		}

		void label(UnicodeString value, const AptScene::Text &label, Color color) const
		{
			text(value, label.box, label.size, color, label.font.c_str(), label.alignment == 2);
		}

	private:
		DisplayString *m_text;
	};

	struct ImageDeleter
	{
		void operator()(Image *image) const { if (image) deleteInstance(image); }
	};
	using OwnedImage = std::unique_ptr<Image, ImageDeleter>;

	OwnedImage fileImage(AsciiString file)
	{
		if (!TheFileSystem->doesFileExist(file.str()))
			return {};

		OwnedImage image(newInstance(Image));
		image->setFilename(file);
		Region2D uv{{0, 0}, {1, 1}};
		image->setUV(&uv);
		return image;
	}

	OwnedImage menuBackground()
	{
		return fileImage("art/compiledtextures/sf/sfe_menubkgrnd.tga");
	}

	GameWindow *fullScreen()
	{
		WinInstanceData data;
		data.m_style = GWS_USER_WINDOW;
		return TheWindowManager->winCreate(nullptr, WIN_STATUS_ENABLED | WIN_STATUS_ABOVE, 0, 0,
			TheDisplay->getWidth(), TheDisplay->getHeight(), nullptr, &data);
	}

	const char *aiLabel(const GameSlot &slot)
	{
		switch (slot.getState())
		{
			case SLOT_MED_AI: return "GUI:MediumAI";
			case SLOT_BRUTAL_AI: return "GUI:HardAI";
			default: return "GUI:EasyAI";
		}
	}

	std::string mapCatalogueKey(const AsciiString &path)
	{
		std::string_view name(path.str());
		const size_t slash = name.find_last_of("/\\");
		name.remove_prefix(slash == std::string_view::npos ? 0 : slash + 1);
		if (name.size() > 4 && path.endsWithNoCase(".map"))
			name.remove_suffix(4);

		std::string key = "Map:";
		for (const char ch : name)
			if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9'))
				key += ch;
		return key;
	}

	Bool childOf(std::string_view path, std::string_view prefix)
	{
		return path.starts_with(prefix) && (path.size() == prefix.size() || path[prefix.size()] == '.');
	}

	Int rowAt(std::string_view path, std::string_view prefix)
	{
		if (!path.starts_with(prefix) || path.size() <= prefix.size())
			return -1;
		const char row = path[prefix.size()];
		if (row < '0' || row > '7' || (path.size() > prefix.size() + 1 && path[prefix.size() + 1] != '.'))
			return -1;
		return row - '0';
	}
}

class RetailLoadingScreen::Data
{
public:
	struct Row
	{
		UnicodeString name, faction, team;
		const Image *rank = nullptr;
		Int slot, progress;
		Color color;
		Bool ai;
	};

	AptScene scene{"LoadScreen", "#11"};
	OwnedImage background = menuBackground();
	OwnedImage landscape;
	OwnedImage previewFile;
	const Image *preview = nullptr;
	const Image *startMarker = nullptr;
	UnicodeString mapName, description;
	std::vector<Row> rows;
	std::vector<Coord2D> starts;

	explicit Data(GameInfo &game)
	{
		if (!scene.valid())
			return;

		const AsciiString path = game.getMap();
		const MapMetaData *meta = TheMapCache->findMap(path);
		if (meta)
		{
			mapName = meta->m_displayName;
			if (mapName.isEmpty())
			{
				mapName.translate(path);
			}
			const std::string key = meta->m_nameLookupTag.isEmpty() ? mapCatalogueKey(path) : meta->m_nameLookupTag.str();
			description = localize((key + "/Desc").c_str(), "");
			if (meta->m_nameLookupTag.isEmpty())
			{
				UnicodeString localized = localize(key.c_str(), "");
				if (!localized.isEmpty())
				{
					UnicodeString count;
					count.format(L" (%d)", meta->m_numPlayers);
					localized.concat(count);
					mapName = localized;
				}
			}
			const Real width = meta->m_extent.hi.x - meta->m_extent.lo.x;
			const Real height = meta->m_extent.hi.y - meta->m_extent.lo.y;
			if (width > 0 && height > 0)
			{
				for (Int i = 0; i < std::min(MAX_SLOTS, meta->m_numPlayers); ++i)
				{
					AsciiString name;
					name.format("Player_%d_Start", i + 1);
					const auto waypoint = meta->m_waypoints.find(name);
					if (waypoint == meta->m_waypoints.end())
						starts.push_back({-1, -1});
					else
						starts.push_back({(waypoint->second.x - meta->m_extent.lo.x) / width,
							(meta->m_extent.hi.y - waypoint->second.y) / height});
				}
			}
		}
		else
			mapName.translate(path);

		if (path.getLength() > 4)
		{
			AsciiString stem = path;
			stem.truncateBy(4);
			AsciiString landscapePath = stem;
			landscapePath.concat("_pic.tga");
			landscape = fileImage(landscapePath);
			stem.concat("_art.tga");
			previewFile = fileImage(stem);
			preview = previewFile ? previewFile.get() : getMapPreviewImage(path);
		}
		startMarker = TheMappedImageCollection->findImageByName("AptPlayerStart");

		for (Int i = 0; i < MAX_SLOTS; ++i)
		{
			const GameSlot *slot = game.getConstSlot(i);
			if (!slot || !slot->isOccupied())
				continue;

			Row row{};
			row.slot = i;
			row.ai = slot->isAI();
			row.progress = row.ai ? -1 : 0;
			row.name = slot->getName();
			if (row.name.isEmpty() && row.ai)
				row.name = localize(aiLabel(*slot), "Computer");
			row.team = slot->getTeamNumber() < 0 ? translated("-") : number(slot->getTeamNumber() + 1);
			row.color = i == game.getLocalSlotNum() ? GameMakeColor(133, 230, 87, 255) : white;
			row.faction = slot->getApparentPlayerTemplateDisplayName();
			const Int faction = slot->getApparentPlayerTemplate();
			const PlayerTemplate *player = faction >= 0 && faction < ThePlayerTemplateStore->getPlayerTemplateCount() ?
				ThePlayerTemplateStore->getNthPlayerTemplate(faction) : nullptr;
			if (player)
			{
				AsciiString image;
				image.format("AptRankIcon%s1", player->getSide().str());
				row.rank = TheMappedImageCollection->findImageByName(image);
			}
			rows.push_back(row);
		}
	}

	void draw() const
	{
		const Painter painter(scene);
		const Box landscapeBox = scene.bounds("_root.frames.@1.AptMapPreview::Picture");
		const Box previewBox = scene.bounds("_root.frames.@1.CurrentMap");
		painter.background(background.get());

		scene.draw([&](const std::string &path) -> Bool {
			for (const char *hidden : {"_root.frames.@1.CurrentMap", "_root.frames.@1.AptMapPreview::Picture",
				"_root.UIClip.Fellowship", "_root.UIClip.Level", "_root.UIClip.@109"})
				if (childOf(path, hidden))
				return FALSE;

			constexpr std::string_view prefix = "_root.UIClip.ProgressBars.Bar";
			const Int row = rowAt(path, prefix);
			return row < 0 || (row < Int(rows.size()) && !rows[row].ai && path.find(".barFullSize") == std::string::npos);
		}, [&](const std::string &path, const AptScene::Text &label) {
			if (label.value == "$PAGETITLE")
				painter.label(localize("APT:Loading", "Loading"), label, muted);
			else if (label.value == "$MapTitle")
				painter.label(mapName, label, white);
			else if (childOf(path, "_root.textAnim"))
			{
				const std::string key = "APT:" + (label.value == "Players" ? std::string("HeaderPlayerName") : label.value);
				painter.label(localize(key.c_str(), label.value == "Players" ? "Player Name" : label.value.c_str()), label, muted);
			}
		}, [&](const std::string &path) {
			const Int row = rowAt(path, "_root.UIClip.ProgressBars.Bar");
			if (row < 0 || row >= Int(rows.size()) || path.find(".bar.") == std::string::npos)
				return 1.0f;

			const std::string parent = "_root.UIClip.ProgressBars.Bar" + std::to_string(row);
			const Box natural = scene.bounds(parent + ".bar");
			const Box full = scene.bounds(parent + ".barFullSize");
			return natural.valid() && full.valid() ? (full.right - full.left) * rows[row].progress /
				((natural.right - natural.left) * 100.0f) : 0.0f;
		});

		painter.picture(landscape.get(), landscapeBox);
		painter.picture(preview, previewBox);
		for (size_t i = 0; i < starts.size(); ++i)
		{
			const Coord2D &position = starts[i];
			if (position.x < 0 || position.x > 1 || position.y < 0 || position.y > 1 || !previewBox.valid())
				continue;
			const Real x = previewBox.left + position.x * (previewBox.right - previewBox.left);
			const Real y = previewBox.top + position.y * (previewBox.bottom - previewBox.top);
			const Box marker{x - 8, y - 10, x + 8, y + 10};
			painter.picture(startMarker, marker);
			painter.text(number(Int(i + 1)), marker, 13);
		}

		Box descriptionBox = scene.bounds("_root.UIClip.@109.MapInfo");
		descriptionBox.left += 13;
		descriptionBox.top += 5;
		descriptionBox.right -= 13;
		painter.text(description, descriptionBox, 16, white, "Albertus MT", FALSE, TRUE);

		const char *columns[] = {"PlayerName", "Level", "TeamNumber", "ArmyName"};
		for (size_t i = 0; i < rows.size(); ++i)
		{
			const Row &row = rows[i];
			for (Int column = 0; column < 4; ++column)
			{
				const std::string root = std::string("_root.UIClip.") + columns[column];
				if (column == 1)
				{
					const Box first = scene.bounds(root);
					const Box line = scene.bounds("_root.UIClip.Rank." + std::to_string(i));
					const Real x = (first.left + first.right) / 2;
					const Box icon{x - 10, line.top + 1, x + 10, line.bottom - 1};
					if (row.rank)
						painter.picture(row.rank, icon);
					else if (!row.faction.isEmpty())
						painter.text(number(1), icon, 14, row.color);
				}
				else
				{
					Box line = scene.bounds(root + "." + std::to_string(i));
					const char *header = column == 0 ? "Players" : (column == 2 ? "Team" : "Army");
					const std::string section = column == 0 ? "@3" : (column == 2 ? "@5" : "@7");
					const Box heading = scene.bounds("_root.textAnim.@1." + section + "." + header);
					if (heading.valid())
					{
						line.left = heading.left;
						line.right = heading.right;
					}
					painter.text(column == 0 ? row.name : (column == 2 ? row.team : row.faction), line, 14, row.color);
				}
			}
			if (!row.ai)
			{
				UnicodeString value;
				value.format(L"%d%%", row.progress);
				const Box line = scene.bounds("_root.UIClip.ProgressBars.Bar" + std::to_string(i));
				painter.text(value, line, 13);
			}
		}
	}
};

RetailLoadingScreen::RetailLoadingScreen(GameInfo &game) : m_data(std::make_unique<Data>(game)) {}
RetailLoadingScreen::~RetailLoadingScreen() = default;

Bool RetailLoadingScreen::valid() const
{
	return m_data->scene.valid() && m_data->scene.bounds("_root.UIClip.PlayerName.0").valid();
}

void RetailLoadingScreen::update(Int slot, Int percent)
{
	for (auto &row : m_data->rows)
		if (row.slot == slot && !row.ai)
			row.progress = std::max(row.progress, std::clamp(percent, 0, 100));
}

void RetailLoadingScreen::draw() const
{
	m_data->draw();
}

RetailMatchResults collectRetailMatchResults()
{
	RetailMatchResults results;
	Player *local = ThePlayerList->getLocalPlayer();
	for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
	{
		Player *player = ThePlayerList->getNthPlayer(i);
		if (!player || !player->isPlayableSide() || !player->getListInScoreScreen())
			continue;

		ScoreKeeper score = *player->getScoreKeeper();
		UnicodeString name = player->getPlayerDisplayName();
		if (name.isEmpty() && player->getPlayerTemplate())
			name = player->getPlayerTemplate()->getDisplayName();
		results.players.push_back({name, {score.getTotalUnitsBuilt(),
			score.getTotalUnitsLost(), score.getTotalUnitsDestroyed(), score.getTotalBuildingsBuilt(),
			score.getTotalBuildingsLost(), score.getTotalBuildingsDestroyed(), score.getTotalMoneyEarned(),
			score.calculateScore()}, player->isPlayerDead()});
		if (player == local && !local->isPlayerObserver())
		{
			results.hasLocalResult = TRUE;
			results.localVictory = TheCampaignManager ? TheCampaignManager->isVictorious() : !player->isPlayerDead();
		}
	}
	return results;
}

namespace
{
	struct Stat
	{
		const char *key, *fallback;
		Int total;
	};

	constexpr Stat statistics[] = {
		{"APT:UnitsCreated", "Units built", 0}, {"APT:UnitsLost", "Units lost", 1},
		{"APT:UnitsDestroyed", "Units killed", 2}, {"APT:StructuresCreated", "Structures built", 3},
		{"APT:StructuresLost", "Structures lost", 4}, {"APT:StructuresDestroyed", "Structures destroyed", 5},
		{"APT:ResourcesCollected", "Resources gathered", 6}, {"APT:FinalScoreTab", "Final score", 7}
	};

	struct Tab
	{
		const char *path, *key, *label;
		Int first, count;
	};

	constexpr Tab tabs[] = {
		{"Units", "APT:UnitsTab", "Units", 0, 3}, {"Structures", "APT:StructuresTab", "Structures", 3, 3},
		{"Resources", "APT:ResourcesTab", "Resources", 6, 1}, {"FinalScore", "APT:FinalScoreTab", "Final score", 0, 8}
	};

	constexpr auto continuePath = "_root.Main.MainButtons.Continue";

	class ResultsScreen
	{
	public:
		explicit ResultsScreen(RetailMatchResults value) : results(std::move(value)) {}

		AptScene scene{"Timeline", "#59"};
		OwnedImage background = menuBackground();
		RetailMatchResults results;
		Int activeTab = 3;
		size_t activePlayer = 0;

		void draw() const
		{
			const Painter painter(scene);
			painter.background(background.get());

			scene.draw([&](const std::string &path) -> Bool {
				if (childOf(path, "_root.@1") || childOf(path, "_root.Main.Axis") ||
					childOf(path, "_root.Main.$UnitsTab") || childOf(path, "_root.Main.@403"))
					return FALSE;
				if (childOf(path, "_root.Main.MainButtons") && !childOf(path, continuePath))
					return FALSE;
				const Int row = rowAt(path, "_root.Main.Button");
				return row < 0 || row < Int(results.players.size());
			}, [&](const std::string &path, const AptScene::Text &label) {
				if (label.value == "$PAGETITLE")
					painter.label(localize("APT:Score", "Score"), label, muted);
				else if (childOf(path, continuePath))
					painter.label(localize("APT:Continue", "Continue"), label, white);
				else if (path == "_root.Main.BonusObjectives")
					painter.label(results.hasLocalResult ? (results.localVictory ? localize("APT:Victory", "Victory") :
						localize("APT:Defeated", "Defeat")) : localize("APT:Results", "Results"), label, white);
				else if (label.value == "$Standings")
					painter.label(aptText(label.value), label, muted);
				else if (const Int row = rowAt(path, "_root.Main.Button"); row >= 0 && row < Int(results.players.size()))
					painter.label(results.players[row].name, label, row == Int(activePlayer) ? white : muted);
				else
				{
					for (size_t i = 0; i < std::size(tabs); ++i)
					{
						if (childOf(path, tabPath(tabs[i])))
							painter.label(localize(tabs[i].key, tabs[i].label), label, Int(i) == activeTab ? white : muted);
					}
				}
			});

			const Box panel = scene.bounds("_root.Main.@2");
			const Box standings = scene.bounds("_root.Main.EmptySlots");
			const Box headings = scene.bounds("_root.Main.TabButtons");
			if (!panel.valid() || !standings.valid() || !headings.valid() || results.players.empty())
				return;

			const Tab &tab = tabs[activeTab];
			const auto &player = results.players[activePlayer];
			const Real bottom = panel.bottom - 18;
			const Real left = panel.left + 22;
			const Real top = headings.bottom + 15;
			const Real right = standings.left - 18;
			const Real rowHeight = std::min(44.0f, (bottom - top) / (tab.count + 1));
			painter.text(player.name, {left, top, right - 100, top + rowHeight}, 20, white, "Albertus MT", FALSE);
			if (player.defeated)
				painter.text(localize("APT:Defeated", "Defeated"), {right - 100, top, right, top + rowHeight}, 16, muted);
			for (Int i = 0; i < tab.count; ++i)
			{
				const Stat &stat = statistics[tab.first + i];
				const Real y = top + rowHeight * (i + 1);
				painter.text(localize(stat.key, stat.fallback), {left, y, right - 100, y + rowHeight}, 17, muted, "Albertus MT", FALSE);
				painter.text(number(player.totals[stat.total]), {right - 100, y, right, y + rowHeight}, 17);
			}
		}

		static std::string tabPath(const Tab &tab)
		{
			return std::string("_root.Main.TabButtons.") + tab.path;
		}

		Bool click(Real x, Real y)
		{
			if (scene.bounds(continuePath).contains(x, y))
				return TRUE;
			for (size_t i = 0; i < std::size(tabs); ++i)
				if (scene.bounds(tabPath(tabs[i])).contains(x, y))
					activeTab = Int(i);
			for (size_t i = 0; i < std::min<size_t>(results.players.size(), MAX_SLOTS); ++i)
				if (scene.bounds("_root.Main.Button" + std::to_string(i)).contains(x, y))
					activePlayer = i;
			return FALSE;
		}
	};

	void drawResults(GameWindow *window, WinInstanceData *)
	{
		static_cast<ResultsScreen *>(window->winGetUserData())->draw();
	}

	WindowMsgHandledType resultsSystem(GameWindow *window, UnsignedInt message, WindowMsgData, WindowMsgData)
	{
		if (message == GWM_DESTROY)
			delete static_cast<ResultsScreen *>(window->winGetUserData());
		return MSG_IGNORED;
	}

	WindowMsgHandledType resultsInput(GameWindow *window, UnsignedInt message, WindowMsgData point, WindowMsgData state)
	{
		auto *screen = static_cast<ResultsScreen *>(window->winGetUserData());
		const Bool keyboard = message == GWM_CHAR && (state & KEY_STATE_DOWN) && (point == KEY_ENTER || point == KEY_ESC);
		if (keyboard || (message == GWM_LEFT_UP && screen->click((point & 0xFFFF) / screen->scene.xScale(),
			(point >> 16) / screen->scene.yScale())))
		{
			TheWindowManager->winDestroy(window);
			TheShell->showShell(TRUE);
		}
		return MSG_HANDLED;
	}

	class ReturnMenu
	{
	public:
		AptScene scene{"LoadScreen", "#11"};
		OwnedImage background = menuBackground();

		static Bool canRematch()
		{
			return TheSkirmishGameInfo && TheSkirmishGameInfo->getMap().isNotEmpty();
		}

		static void startRematch()
		{
			TheWritableGlobalData->m_pendingFile = TheSkirmishGameInfo->getMap();
			TheMessageStream->appendMessage(GameMessage::MSG_NEW_GAME)->appendIntegerArgument(GAME_SKIRMISH);
		}
	};

	constexpr Box rematchButton{412, 328, 612, 376};
	constexpr Box exitButton{412, 384, 612, 432};

	void drawReturnMenu(GameWindow *window, WinInstanceData *)
	{
		const auto &menu = *static_cast<ReturnMenu *>(window->winGetUserData());
		const Painter painter(menu.scene);
		painter.background(menu.background.get());
		menu.scene.draw([](const std::string &path) {
			return childOf(path, "_root.Title") || childOf(path, "_root.patternShell");
		}, [&](const std::string &, const AptScene::Text &label) {
			if (label.value == "$PAGETITLE")
				painter.label(localize("APT:MainMenu", "Main Menu"), label, muted);
		});
		if (menu.canRematch())
			painter.text(localize("GUI:PlayAgain", "Play Again"), rematchButton, 22);
		painter.text(localize("APT:ExitGame", "Exit Game"), exitButton, 22);
	}

	WindowMsgHandledType menuSystem(GameWindow *window, UnsignedInt message, WindowMsgData, WindowMsgData)
	{
		if (message == GWM_DESTROY)
			delete static_cast<ReturnMenu *>(window->winGetUserData());
		return MSG_IGNORED;
	}

	WindowMsgHandledType menuInput(GameWindow *window, UnsignedInt message, WindowMsgData point, WindowMsgData state)
	{
		const auto &menu = *static_cast<ReturnMenu *>(window->winGetUserData());
		const Real x = (point & 0xFFFF) / menu.scene.xScale();
		const Real y = (point >> 16) / menu.scene.yScale();
		const Bool key = message == GWM_CHAR && (state & KEY_STATE_DOWN);
		const Bool click = message == GWM_LEFT_UP;
		if ((key && point == KEY_ESC) || (click && exitButton.contains(x, y)))
			TheGameEngine->setQuitting(TRUE);
		else if (menu.canRematch() && ((key && point == KEY_ENTER) || (click && rematchButton.contains(x, y))))
			menu.startRematch();
		return MSG_HANDLED;
	}
}

Bool showRetailMatchResults(RetailMatchResults results)
{
	if (results.players.empty())
		return FALSE;
	auto screen = std::make_unique<ResultsScreen>(std::move(results));
	if (!screen->scene.bounds(continuePath).valid() || !screen->scene.bounds("_root.Main.TabButtons").valid() ||
		!screen->scene.bounds("_root.Main.EmptySlots").valid() || !screen->scene.bounds("_root.Main.@2").valid())
		return FALSE;

	GameWindow *window = fullScreen();
	if (!window)
		return FALSE;
	window->winSetUserData(screen.release());
	window->winSetDrawFunc(drawResults);
	window->winSetSystemFunc(resultsSystem);
	window->winSetInputFunc(resultsInput);
	TheWindowManager->winSetFocus(window);
	return TRUE;
}

Bool createRetailReturnMenu(WindowLayout &layout)
{
	auto menu = std::make_unique<ReturnMenu>();
	if (!menu->scene.valid())
		return FALSE;
	GameWindow *window = fullScreen();
	if (!window)
		return FALSE;
	window->winSetUserData(menu.release());
	window->winSetDrawFunc(drawReturnMenu);
	window->winSetSystemFunc(menuSystem);
	window->winSetInputFunc(menuInput);
	layout.addWindow(window);
	layout.setInit([](WindowLayout *screen, void *) {
		screen->hide(FALSE);
		TheWindowManager->winSetFocus(screen->getFirstWindow());
	});
	return TRUE;
}
