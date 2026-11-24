#include "PreRTS.h"

#include "GameClient/RetailMatchScreens.h"
#include "GameClient/RetailOptions.h"

#include "Common/FileSystem.h"
#include "Common/GameEngine.h"
#include "Common/GameState.h"
#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/MultiplayerSettings.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/RetailFileLayers.h"
#include "Common/UserPreferences.h"
#include "GameClient/AptScene.h"
#include "GameClient/CampaignManager.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Image.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Keyboard.h"
#include "GameClient/MapUtil.h"
#include "GameClient/Mouse.h"
#include "GameClient/RetailHudExtras.h"
#include "GameClient/Shell.h"
#include "GameClient/UiFrame.h"
#include "GameClient/WindowLayout.h"
#include "GameNetwork/GameInfo.h"
#include "GameLogic/GameLogic.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
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

	AsciiString profileStatKey(Bool victory, const AsciiString &name)
	{
		AsciiString key(victory ? "Wins." : "Losses.");
		key.concat(name);
		return key;
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
			case SLOT_RETAIL_BRUTAL_AI: return "GUI:BrutalAI";
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

	struct ObjectiveEntry
	{
		AsciiString text;
		Bool visible = FALSE;
		Bool completed = FALSE;
		Bool bonus = FALSE;
	};

	std::vector<ObjectiveEntry> objectiveEntries;
	GameWindow *objectivesWindow = nullptr;
	std::unique_ptr<AptScene> objectivesScene;
	std::unique_ptr<AptScene> objectiveChecks;
	Bool objectivesEnabled = TRUE;
	Bool objectivesPausedGame = FALSE;
	Bool playerStatusMode = FALSE;
	Bool resumePressed = FALSE;
	constexpr const char *playerStatusClose = "_root.OuterFrame.Pages.Cancel";

	template<Bool Bonus>
	void parseObjective(INI *ini, void *instance, void *, const void *)
	{
		auto &entries = *static_cast<std::vector<ObjectiveEntry> *>(instance);
		entries.push_back({ini->getNextToken(), FALSE, FALSE, Bonus});
	}

	ObjectiveEntry *objectiveByNumber(Int number)
	{
		return number >= 1 && size_t(number) <= objectiveEntries.size() ? &objectiveEntries[number - 1] : nullptr;
	}

	std::unique_ptr<AptScene> readPlayerStatusScene()
	{
		return std::make_unique<AptScene>("PlayerTribute", "#5",
			std::initializer_list<std::pair<const char *, const char *>>{
				{"_root.OuterFrame", "#27"},
				{"_root.OuterFrame.Pages", "#11"},
				{"_root.OuterFrame.Pages.Cancel", "#19"},
				{"_root.OuterFrame.Pages.StatusPage.Table", "#2"}
			});
	}

	void drawPlayerStatus(const AptScene &scene, const Painter &painter)
	{
		const GameInfo *game = TheGameLogic->isInSkirmishGame() ? TheSkirmishGameInfo : TheGameInfo;
		scene.draw([](const std::string &path) {
			return !childOf(path, "_root.OuterFrame.Pages.TributePage");
		}, [&](const std::string &path, const AptScene::Text &label) {
			const Int row = rowAt(path, "_root.OuterFrame.Pages.StatusPage.Table.");
			if (row < 0)
			{
				if (childOf(path, "_root.OuterFrame.Pages.Cancel"))
					painter.label(localize("APT:Cancel", "Cancel"), label, white);
				else if (childOf(path, "_root.OuterFrame.Pages.@1.Tab1"))
					painter.label(localize("APT:StatusTab", "Status"), label, white);
				else if (childOf(path, "_root.OuterFrame.Pages.@1.Tab2"))
					painter.label(localize("APT:TributeTab", "Tribute"), label, muted);
				else if (label.value == "$TributeScreenTitle")
					painter.label(localize("APT:TributeScreenTitle", "Players"), label, white);
				else if (label.value.starts_with("$Header"))
					painter.label(aptText(label.value), label, white);
				return;
			}

			Player *player = ThePlayerList->getPlayerFromSlotIndex(row);
			const GameSlot *slot = game ? game->getConstSlot(row) : nullptr;
			if (!player || !slot || !slot->isOccupied() || slot->getPlayerTemplate() == PLAYERTEMPLATE_OBSERVER)
				return;
			UnicodeString value;
			if (path.ends_with(".Name"))
			{
				value = player->getPlayerDisplayName();
				if (value.isEmpty() && slot->isAI())
					value = localize(aiLabel(*slot), "Computer");
			}
			else if (path.ends_with(".@2"))
				value = slot->getApparentPlayerTemplateDisplayName();
			else if (path.ends_with(".@3"))
				value = slot->getTeamNumber() < 0 ? localize("APT:None", "-") : number(slot->getTeamNumber() + 1);
			else if (path.ends_with(".@4"))
				value = player->isPlayerDead() ? localize("APT:Defeated", "Defeated") : localize("APT:Playing", "Playing");
			painter.label(value, label, player->getPlayerColor() | 0xFF000000);
		});
	}

	void drawCampaignObjectives(const AptScene &scene, const Painter &painter)
	{
		scene.draw([](const std::string &path) {
			return !childOf(path, "_root.objectives");
		}, [&](const std::string &path, const AptScene::Text &label) {
			if (childOf(path, "_root.Resume"))
				painter.label(localize("APT:ResumeGame", "Resume Game"), label, white);
			else if (label.value == "$PAGETITLE")
				painter.label(localize("APT:Objectives", "Objectives"), label, white);
		});

		Int row = 0;
		for (const ObjectiveEntry &entry : objectiveEntries)
		{
			if (!entry.visible)
				continue;
			const std::string path = "_root.objectives.Objective" + std::to_string(++row);
			const Box text = scene.bounds(path + ".Text.textClip.textField");
			if (!text.valid())
				break;
			painter.text(TheGameText->fetch(entry.text), text, 14, entry.bonus ? muted : white, "Albertus MT", FALSE, TRUE);
			const Box icon = scene.bounds(path + ".Icon");
			if (entry.completed && objectiveChecks && objectiveChecks->valid())
			{
				const Box iconArt = objectiveChecks->bounds("objective_checked");
				objectiveChecks->drawFrameAt(0, (icon.left - iconArt.left) * painter.sx,
					(icon.top - iconArt.top) * painter.sy, painter.sx, painter.sy);
			}
			else
				scene.drawAt([&](const std::string &element) { return childOf(element, path + ".Icon"); },
					0, 0, painter.sx, painter.sy, 1.0f);
		}
	}

	void drawObjectives(GameWindow *window, WinInstanceData *)
	{
		window->winSetSize(TheDisplay->getWidth(), TheDisplay->getHeight());
		const AptScene &scene = *objectivesScene;
		const Painter painter(scene);
		if (playerStatusMode)
			drawPlayerStatus(scene, painter);
		else
			drawCampaignObjectives(scene, painter);
	}

	WindowMsgHandledType objectivesInput(GameWindow *, UnsignedInt message, WindowMsgData point, WindowMsgData state)
	{
		if (message == GWM_CHAR && point == KEY_ESC && (state & KEY_STATE_UP))
			RetailObjectives::close();
		if (message == GWM_LEFT_DOWN || message == GWM_LEFT_UP)
		{
			const AptScene &scene = *objectivesScene;
			const Box resume = scene.bounds(playerStatusMode ? playerStatusClose : "_root.Resume");
			const Bool overResume = resume.contains((point & 0xFFFF) / scene.xScale(), (point >> 16) / scene.yScale());
			if (message == GWM_LEFT_DOWN)
				resumePressed = overResume;
			else if (resumePressed && overResume)
				RetailObjectives::close();
		}
		return MSG_HANDLED;
	}

	WindowMsgHandledType objectivesSystem(GameWindow *window, UnsignedInt message, WindowMsgData state, WindowMsgData result)
	{
		if (message == GWM_INPUT_FOCUS && state)
		{
			*reinterpret_cast<Bool *>(result) = TRUE;
			return MSG_HANDLED;
		}
		if (message == GWM_DESTROY && window == objectivesWindow)
			objectivesWindow = nullptr;
		return MSG_IGNORED;
	}
}

namespace RetailObjectives
{
void parse(INI *ini)
{
	static const FieldParse fields[] = {
		{"MissionObjectiveTag", parseObjective<FALSE>, nullptr, 0},
		{"BonusMissionObjectiveTag", parseObjective<TRUE>, nullptr, 0},
		{nullptr, nullptr, nullptr, 0}
	};
	ini->initFromINI(&objectiveEntries, fields);
}

void show()
{
	if (!objectivesEnabled || objectivesWindow || !TheGameLogic->isInGame() || TheInGameUI->isQuitMenuVisible())
		return;
	playerStatusMode = TheGameLogic->isInSkirmishGame() || TheGameLogic->isInMultiplayerGame();
	objectivesScene = playerStatusMode ? readPlayerStatusScene() : std::make_unique<AptScene>("Objectives");
	const char *closePath = playerStatusMode ? playerStatusClose : "_root.Resume";
	if (!objectivesScene->valid() || !objectivesScene->bounds(closePath).valid())
	{
		DEBUG_LOG(("RetailObjectives: missing %s in %s", closePath, playerStatusMode ? "PlayerTribute" : "Objectives"));
		objectivesScene.reset();
		return;
	}
	if (!playerStatusMode)
		objectiveChecks = std::make_unique<AptScene>("MenuExport", AptScene::AnimatedExport{"objective_checked"});
	objectivesWindow = fullScreen();
	if (!objectivesWindow)
	{
		objectivesScene.reset();
		objectiveChecks.reset();
		return;
	}
	objectivesWindow->winSetInputFunc(objectivesInput);
	objectivesWindow->winSetSystemFunc(objectivesSystem);
	objectivesWindow->winSetDrawFunc(drawObjectives);
	TheInGameUI->setGUICommand(nullptr);
	if (GameWindow *grab = TheWindowManager->winGetGrabWindow())
	{
		grab->winClearStatus(WIN_STATUS_ACTIVE);
		TheWindowManager->winSetGrabWindow(nullptr);
	}
	TheWindowManager->winSetModal(objectivesWindow);
	TheWindowManager->winSetFocus(objectivesWindow);
	resumePressed = FALSE;
	objectivesPausedGame = !playerStatusMode && !TheGameLogic->isGamePaused();
	if (objectivesPausedGame)
		TheGameLogic->setGamePaused(TRUE);
}

void close()
{
	if (objectivesWindow)
		TheWindowManager->winDestroy(objectivesWindow);
	objectivesWindow = nullptr;
	objectivesScene.reset();
	objectiveChecks.reset();
	if (objectivesPausedGame && TheGameLogic && TheGameLogic->isGamePaused())
		TheGameLogic->setGamePaused(FALSE);
	objectivesPausedGame = FALSE;
}

void reset()
{
	close();
	objectiveEntries.clear();
	objectivesEnabled = TRUE;
}

void setEnabled(Bool enabled)
{
	objectivesEnabled = enabled;
	if (!enabled)
		close();
}

void setVisible(Int objective, Bool visible)
{
	if (ObjectiveEntry *entry = objectiveByNumber(objective))
		entry->visible = visible;
}

void setCompleted(Int objective, Bool completed)
{
	if (ObjectiveEntry *entry = objectiveByNumber(objective))
		entry->completed = completed;
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
	const Image *parchment = nullptr;
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
		parchment = TheMappedImageCollection->findImageByName("UiRetailGlobe");

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
			const auto *house = TheMultiplayerSettings->getColor(slot->getColor());
			row.color = house ? house->getColor() | 0xFF000000 : white;
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
		if (previewFile && parchment && previewBox.valid())
		{
			const Region2D paper{{0.16f, 0.16f}, {0.84f, 0.84f}};
			TheDisplay->drawImagePart(parchment, Int(previewBox.left * painter.sx), Int(previewBox.top * painter.sy),
				Int(previewBox.right * painter.sx), Int(previewBox.bottom * painter.sy), paper);
		}
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
		painter.text(description, descriptionBox, 18, white, "Albertus MT", FALSE, TRUE);

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
	if (results.hasLocalResult && (results.localVictory || local->isPlayerDead()) && TheGameLogic->isInSkirmishGame() &&
		TheGameInfo == TheSkirmishGameInfo && TheGameInfo && TheGameInfo->getNumNonObserverPlayers() > 1 &&
		TheGameInfo->getRetailSkirmishRules().commandPointFactor > 0)
	{
		UserPreferences profiles;
		profiles.load("RetailSkirmishProfiles.ini");
		AsciiString name;
		name.translate(local->getPlayerDisplayName());
		const AsciiString key = profileStatKey(results.localVictory, name);
		profiles.setInt(key, std::clamp(profiles.getInt(key, 0), 0, 999999) + 1);
		profiles.write();
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
		{
			delete static_cast<ResultsScreen *>(window->winGetUserData());
			window->winSetUserData(nullptr);
		}
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

	constexpr Int resourceValues[] = {500, 750, 1000, 1200, 1400, 1600, 1800, 2000, 2500, 3000, 4000};

	struct Factor
	{
		Int percent;
		const char *key;
		const char *label;
	};

	constexpr Factor factors[] = {
		{33, "VALUE:ThirdX", "1/3x"}, {50, "VALUE:HalfX", "1/2x"},
		{100, "VALUE:1X", "1x"}, {200, "VALUE:2X", "2x"},
		{400, "VALUE:4X", "4x"}, {800, "VALUE:8X", "8x"}, {10000, "VALUE:100X", "100x"}
	};

	enum class MenuChoice { None, Resources, Factor, Slot, Faction, Team, Color, StartPosition, StartAt, Profile, SelectProfile, DeleteProfile };
	enum class ProfilePage { None, Create, Stats };

	struct Choice
	{
		Int value;
		UnicodeString text;
		Color color = 0;
	};

	struct SetupSlot
	{
		SlotState state = SLOT_CLOSED;
		Int faction = -1;
		Int team = -1;
		Int color = -1;
		Int start = -1;

		Bool occupied() const { return state != SLOT_OPEN && state != SLOT_CLOSED; }
	};

	struct SetupMap
	{
		AsciiString path;
		UnicodeString name;
		Int players;
		Bool official;
	};

	UnicodeString slotName(SlotState state)
	{
		switch (state)
		{
			case SLOT_OPEN: return localize("GUI:Open", "Open");
			case SLOT_EASY_AI: return localize("GUI:EasyAI", "Easy AI");
			case SLOT_MED_AI: return localize("GUI:MediumAI", "Medium AI");
			case SLOT_BRUTAL_AI: return localize("GUI:HardAI", "Hard AI");
			case SLOT_RETAIL_BRUTAL_AI: return localize("GUI:BrutalAI", "Brutal AI");
			default: return localize("GUI:Closed", "Closed");
		}
	}

	std::optional<AvailableGameInfo> pendingShellLoad;
	Bool reopenShellLoad = FALSE;

	struct ShellLoadPage
	{
		static constexpr Int visibleRows = 9;

		std::vector<AvailableGameInfo> saves;
		Int first = 0;
		Int selected = -1;
		Bool open = FALSE;
		Bool failed = FALSE;

		void show()
		{
			open = TRUE;
			failed = FALSE;
			first = 0;
			selected = -1;
			saves.clear();
			for (const auto *save = TheGameState->refreshAvailableGames(); save; save = save->next)
				saves.push_back({save->filename, save->saveGameInfo, nullptr, nullptr});
		}

		void moveSelection(Int direction)
		{
			if (saves.empty())
				return;
			selected = selected < 0 ? first : std::clamp(selected + direction, 0, Int(saves.size()) - 1);
			first = std::clamp(first, std::max(0, selected - visibleRows + 1), selected);
			failed = FALSE;
		}

		void scroll(Int direction)
		{
			first = std::clamp(first + direction, 0, std::max(0, Int(saves.size()) - visibleRows));
		}
	};

	class ReturnMenu
	{
	public:
		AptScene scene{"LoadScreen", "#11"};
		AptScene skirmishArt{"Skirmish", "#14"};
		AptScene setupArt{"SkirmishOpenPlay", "#20"};
		OwnedImage background = menuBackground();
		OwnedImage previewFile;
		const Image *preview = nullptr;
		UnicodeString description;
		ShellLoadPage loadPage;
		Bool setup = FALSE;
		Bool rulesTab = FALSE;
		Bool sortByPlayers = FALSE;
		Bool sandboxConfirmed = FALSE;
		std::vector<SetupMap> maps;
		std::vector<Int> factions;
		std::array<SetupSlot, MAX_SLOTS> slots{};
		Int mapIndex = 0;
		Int mapScroll = 0;
		Int resources = 1000;
		RetailSkirmishRules rules{100, TRUE, TRUE};
		UserPreferences profilePrefs;
		std::vector<AsciiString> profiles;
		Int activeProfile = 0;
		ProfilePage profilePage = ProfilePage::None;
		AsciiString newProfile;
		UnicodeString profileFeedback;
		MenuChoice choice = MenuChoice::None;
		std::vector<Choice> choices;
		Box choiceBox{};
		Int choiceRow = 0;
		Int choiceScroll = 0;

		static constexpr Int visibleMaps = 6;
		static constexpr Int visibleChoices = 11;

		static AsciiString profileKey(Int index)
		{
			AsciiString key;
			key.format("Profile%d", index);
			return key;
		}

		void loadProfiles()
		{
			profiles.clear();
			profilePrefs.clear();
			profilePrefs.load("RetailSkirmishProfiles.ini");
			for (Int i = 0, count = std::clamp(profilePrefs.getInt("Count", 0), 0, 100); i < count; ++i)
			{
				const AsciiString name = profilePrefs.getAsciiString(profileKey(i), "");
				if (name.isNotEmpty())
					profiles.push_back(name);
			}
			if (profiles.empty())
				profiles.emplace_back("Player");
			activeProfile = std::clamp(profilePrefs.getInt("Active", 0), 0, Int(profiles.size()) - 1);
		}

		void saveProfiles()
		{
			profilePrefs.setInt("Count", Int(profiles.size()));
			profilePrefs.setInt("Active", activeProfile);
			for (size_t i = 0; i < profiles.size(); ++i)
				profilePrefs.setAsciiString(profileKey(Int(i)), profiles[i]);
			if (!profilePrefs.write())
				profileFeedback = localize("APT:SaveFailed", "Could not save profiles");
		}

		UnicodeString profileName() const
		{
			return translated(profiles[activeProfile].str());
		}

		void createProfile()
		{
			newProfile.trim();
			if (newProfile.isEmpty())
				return;
			for (const auto &profile : profiles)
				if (profile.compareNoCase(newProfile) == 0)
				{
					profileFeedback = localize("APT:DuplicateProfile", "A profile with this name already exists");
					return;
				}
			if (profiles.size() >= 100)
			{
				profileFeedback = localize("APT:MaximumProfiles", "Profile limit reached");
				return;
			}
			profiles.push_back(newProfile);
			activeProfile = Int(profiles.size()) - 1;
			profilePage = ProfilePage::None;
			saveProfiles();
		}

		void openSetup()
		{
			setup = TRUE;
			rulesTab = FALSE;
			sandboxConfirmed = FALSE;
			choice = MenuChoice::None;
			profilePage = ProfilePage::None;
			profileFeedback.clear();
			loadProfiles();
			maps.clear();
			factions.clear();
			TheMapCache->updateCache();
			for (const auto &[path, map] : *TheMapCache)
			{
				if (map.m_doesExist && map.m_isMultiplayer && map.m_numPlayers > 0)
				{
					UnicodeString name;
					if (map.m_nameLookupTag.isEmpty())
						name = localize(mapCatalogueKey(path).c_str(), "");
					if (name.isEmpty())
					{
						name = map.m_displayName;
						UnicodeString count;
						count.format(L" (%d)", map.m_numPlayers);
						if (name.endsWith(count))
							name.truncateBy(count.getLength());
					}
					maps.push_back({path, name, map.m_numPlayers, map.m_isOfficial});
				}
			}
			sortByPlayers = FALSE;
			sortMaps();
			for (Int i = 0; i < ThePlayerTemplateStore->getPlayerTemplateCount(); ++i)
			{
				const auto *faction = ThePlayerTemplateStore->getNthPlayerTemplate(i);
				if (faction->isPlayableSide())
					factions.push_back(i);
			}
			slots.fill({});
			slots[0] = {SLOT_PLAYER, factions.empty() ? -1 : factions.front(), 0, 0, 0};
			for (Int faction : factions)
				if (ThePlayerTemplateStore->getNthPlayerTemplate(faction)->getName() == "FactionMen")
					slots[0].faction = faction;
			mapScroll = 0;
			selectMap(0);
		}

		void sortMaps()
		{
			std::sort(maps.begin(), maps.end(), [this](const SetupMap &a, const SetupMap &b) {
				if (a.official != b.official)
					return !a.official;
				if (sortByPlayers && a.players != b.players)
					return a.players < b.players;
				const Int byName = a.name.compareNoCase(b.name);
				return byName != 0 ? byName < 0 : a.path.compareNoCase(b.path) < 0;
			});
		}

		void setMapSort(Bool byPlayers)
		{
			if (maps.empty() || byPlayers == sortByPlayers)
				return;
			const AsciiString selected = maps[mapIndex].path;
			sortByPlayers = byPlayers;
			sortMaps();
			const auto it = std::find_if(maps.begin(), maps.end(), [&](const SetupMap &map) { return map.path == selected; });
			mapIndex = Int(it - maps.begin());
			mapScroll = std::clamp(mapScroll, std::max(0, mapIndex - visibleMaps + 1), mapIndex);
		}

		const MapMetaData *currentMap() const
		{
			return maps.empty() ? nullptr : TheMapCache->findMap(maps[mapIndex].path);
		}

		Int slotCount() const
		{
			const auto *map = currentMap();
			return map ? std::min(map->m_numPlayers, MAX_SLOTS) : 0;
		}

		void selectMap(Int index)
		{
			if (index < 0 || index >= Int(maps.size()))
				return;
			mapIndex = index;
			mapScroll = std::clamp(mapScroll, std::max(0, index - visibleMaps + 1), index);
			sandboxConfirmed = FALSE;
			const auto *map = currentMap();
			const Int count = slotCount();
			for (Int i = 0; i < count; ++i)
				if (slots[i].start >= count)
					slots[i].start = -1;
			std::fill(slots.begin() + count, slots.end(), SetupSlot{});

			AsciiString stem = map->m_fileName;
			stem.truncateBy(4);
			stem.concat("_art.tga");
			previewFile = fileImage(stem);
			preview = previewFile ? previewFile.get() : getMapPreviewImage(map->m_fileName);
			const std::string key = map->m_nameLookupTag.isEmpty() ? mapCatalogueKey(map->m_fileName) : map->m_nameLookupTag.str();
			description = localize((key + "/Desc").c_str(), "");
		}

		void play()
		{
			const MapMetaData *map = currentMap();
			if (!map || factions.empty())
				return;
			const Int count = slotCount();
			const Bool solo = std::none_of(slots.begin() + 1, slots.begin() + count,
				[](const SetupSlot &slot) { return slot.occupied(); });
			if (solo && !sandboxConfirmed)
			{
				sandboxConfirmed = TRUE;
				return;
			}

			if (!TheSkirmishGameInfo)
				TheSkirmishGameInfo = NEW SkirmishGameInfo;
			else if (TheSkirmishGameInfo->isInGame())
			{
				if (TheSkirmishGameInfo->isGameInProgress())
					TheSkirmishGameInfo->endGame();
				else
					TheSkirmishGameInfo->leaveGame();
			}
			TheSkirmishGameInfo->enterGame();
			TheSkirmishGameInfo->clearSlotList();
			for (Int i = 0; i < count; ++i)
			{
				const auto &selection = slots[i];
				if (!selection.occupied())
					continue;
				GameSlot slot;
				slot.setState(selection.state, i == 0 ? profileName() : UnicodeString());
				slot.setPlayerTemplate(selection.faction);
				slot.setColor(selection.color);
				slot.setTeamNumber(selection.team);
				slot.setStartPos(selection.start);
				TheSkirmishGameInfo->setSlot(i, slot);
				if (i == 0)
					TheSkirmishGameInfo->setLocalIP(slot.getIP());
			}
			Money cash;
			cash.init();
			cash.deposit(resources, FALSE, FALSE);
			TheSkirmishGameInfo->setStartingCash(cash);
			TheSkirmishGameInfo->setRetailSkirmishRules(rules);
			TheSkirmishGameInfo->setMap(map->m_fileName);
			TheSkirmishGameInfo->setMapCRC(map->m_CRC);
			TheSkirmishGameInfo->setMapSize(map->m_filesize);
			startRematch();
		}

		void assignUnique(Int SetupSlot::*property, Int row, Int value)
		{
			if (value >= 0)
				for (Int i = 0, count = slotCount(); i < count; ++i)
					if (i != row && slots[i].occupied() && slots[i].*property == value)
						slots[i].*property = slots[row].*property;
			slots[row].*property = value;
		}

		void choose(MenuChoice type, Int value)
		{
			sandboxConfirmed = FALSE;
			auto &slot = slots[choiceRow];
			switch (type)
			{
				case MenuChoice::Resources: resources = value; break;
				case MenuChoice::Factor: rules.commandPointFactor = value; break;
				case MenuChoice::Slot:
				{
					const auto state = static_cast<SlotState>(value);
					if (state == SLOT_CLOSED || state == SLOT_OPEN)
						slot = {};
					slot.state = state;
					break;
				}
				case MenuChoice::Faction: slot.faction = value; break;
				case MenuChoice::Team: slot.team = value; break;
				case MenuChoice::Color: assignUnique(&SetupSlot::color, choiceRow, value); break;
				case MenuChoice::StartPosition: assignUnique(&SetupSlot::start, choiceRow, value); break;
				case MenuChoice::StartAt:
					if (value >= 0)
						assignUnique(&SetupSlot::start, value, choiceRow);
					else
						for (auto &entry : slots)
							if (entry.start == choiceRow)
								entry.start = -1;
					break;
				case MenuChoice::Profile:
					if (value == 1)
					{
						profilePage = ProfilePage::Create;
						newProfile.clear();
					}
					else if (value == 3)
						profilePage = ProfilePage::Stats;
					break;
				case MenuChoice::SelectProfile:
					activeProfile = value;
					saveProfiles();
					break;
				case MenuChoice::DeleteProfile:
					if (profiles.size() > 1)
					{
						profilePrefs.erase(profileStatKey(TRUE, profiles[value]));
						profilePrefs.erase(profileStatKey(FALSE, profiles[value]));
						profiles.erase(profiles.begin() + value);
						if (value < activeProfile || activeProfile == Int(profiles.size()))
							--activeProfile;
						saveProfiles();
					}
					break;
				case MenuChoice::None: break;
			}
		}

		static Bool canRematch()
		{
			return TheSkirmishGameInfo && TheSkirmishGameInfo->getMap().isNotEmpty();
		}

		static void startRematch()
		{
			TheSkirmishGameInfo->setSeed(GetTickCount());
			TheSkirmishGameInfo->queueStart(TheGlobalData->m_framesPerSecondLimit);
		}
	};

	constexpr Box skirmishButton{412, 272, 612, 320};
	constexpr Box rematchButton{412, 328, 612, 376};
	constexpr Box exitButton{412, 384, 612, 432};
	constexpr Box settingsButton{412, 442, 612, 490};
	constexpr Box mapTab{104, 144, 225, 179};
	constexpr Box rulesTab{231, 144, 352, 179};
	constexpr Box mapPanel{104, 185, 920, 395};
	constexpr Box previewBox{509, 197, 692, 381};
	constexpr Box mapUp{468, 196, 495, 224};
	constexpr Box mapDown{468, 352, 495, 380};
	constexpr Box resourcesBox{367, 208, 550, 241};
	constexpr Box factorBox{367, 247, 550, 280};
	constexpr Box customBox{586, 208, 890, 241};
	constexpr Box ringBox{586, 247, 890, 280};
	constexpr Box resetBox{745, 343, 891, 378};
	constexpr Box setupBack{104, 668, 285, 712};
	constexpr Box profileBox{421, 668, 603, 712};
	constexpr Box setupPlay{739, 668, 920, 712};
	constexpr Box profilePanel{310, 264, 714, 460};
	constexpr Box profileCancel{337, 405, 475, 442};
	constexpr Box profileAccept{550, 405, 688, 442};
	constexpr Real slotEdges[] = {104, 276, 463, 610, 696, 920};
	constexpr Box mapNameSort{115, 194, 375, 222};
	constexpr Box mapPlayersSort{376, 194, 462, 222};
	constexpr Real choiceHeight = 25;
	constexpr Box loadMenuButton{412, 498, 612, 542};
	constexpr Box loadCard{122, 110, 902, 658};
	constexpr Box saveList{144, 204, 602, 571};
	constexpr Box saveDetails{622, 204, 880, 571};
	constexpr Box previousSaves{522, 170, 558, 198};
	constexpr Box nextSaves{566, 170, 602, 198};
	constexpr Box closeLoad{144, 598, 278, 642};
	constexpr Box confirmLoad{746, 598, 880, 642};
	constexpr Real saveRowHeight = 40;

	Box slotCell(Int row, Int column)
	{
		const Real top = 432.0f + 24.0f * row;
		return {slotEdges[column], top, slotEdges[column + 1] - 3, top + 23};
	}

	Box mapLine(Int row)
	{
		const Real top = 225.0f + 26.0f * row;
		return {115, top, 462, top + 24};
	}

	Box inset(Box box, Real left, Real right = 0)
	{
		box.left += left;
		box.right -= right;
		return box;
	}

	void fill(const Painter &p, const Box &box, Color color)
	{
		TheDisplay->drawFillRect(Int(box.left * p.sx), Int(box.top * p.sy),
			Int((box.right - box.left) * p.sx), Int((box.bottom - box.top) * p.sy), color);
	}

	Bool moviePart(const Painter &p, const AptScene &scene, const std::string &path, const Box &box, Real opacity = 1.0f)
	{
		const Box source = scene.bounds(path);
		if (!source.valid())
			return FALSE;
		const Real sx = p.sx * (box.right - box.left) / (source.right - source.left);
		const Real sy = p.sy * (box.bottom - box.top) / (source.bottom - source.top);
		scene.drawAt([&](const std::string &part) { return childOf(part, path); },
			box.left * p.sx - source.left * sx, box.top * p.sy - source.top * sy, sx, sy, opacity);
		return TRUE;
	}

	void frame(const Painter &p, const Box &box, const char *name)
	{
		const auto *art = TheUiFrameCollection ? TheUiFrameCollection->find(name) : nullptr;
		if (art)
			art->draw(Int(box.left * p.sx), Int(box.top * p.sy), Int(box.right * p.sx), Int(box.bottom * p.sy));
	}

	void button(const Painter &p, const Box &box, UnicodeString label)
	{
		fill(p, inset(box, 6, 6), 0xE6172F3C);
		frame(p, box, "MenuButtonMed");
		p.text(label, box, 16);
	}

	void checkbox(const Painter &p, const Box &box, Bool checked)
	{
		const auto *art = TheMappedImageCollection->findImageByName(checked ? "AptCheckboxCheckedEnabled" : "AptCheckboxUncheckedEnabled");
		if (art)
			p.picture(art, box);
		else
			p.text(translated(checked ? "[X]" : "[ ]"), box, 16);
	}

	void control(const Painter &p, const Box &box, UnicodeString label, Bool selected = FALSE,
		Bool dropdown = FALSE, Color color = 0)
	{
		fill(p, box, selected ? 0xFF294A59 : 0xE90A141B);
		if (selected)
			p.picture(TheMappedImageCollection->findImageByName("AptListBoxHiliteSelectedItem"), box);
		if (color)
			fill(p, {box.left + 9, box.top + 5, box.left + 31, box.bottom - 5}, color);
		p.text(label, inset(box, color ? 38 : 9, dropdown ? 22 : 6), 16, white, "Albertus MT", !dropdown);
		if (dropdown)
		{
			const auto *arrow = TheMappedImageCollection->findImageByName("AptVSliderDownButtonEnabled");
			const Box arrowBox{box.right - 22, box.top, box.right, box.bottom};
			if (arrow)
				p.picture(arrow, arrowBox);
			else
				p.text(translated("v"), arrowBox, 14, muted);
		}
	}

	Box saveRow(Int row)
	{
		const Real top = saveList.top + saveRowHeight * row;
		return {saveList.left, top, saveList.right, top + saveRowHeight - 2};
	}

	UnicodeString saveName(const AvailableGameInfo &save)
	{
		return save.saveGameInfo.description.isEmpty() ? translated(save.filename.str()) : save.saveGameInfo.description;
	}

	void drawShellLoad(const ShellLoadPage &page, const Painter &p)
	{
		fill(p, loadCard, 0xFF233A48);
		p.text(localize("APT:LoadSavedGame", "Load Saved Game"), {144, 118, 880, 166}, 28);
		p.text(localize("APT:SavedFiles", "Saved Games"), {144, 170, 502, 198}, 18, muted, "Albertus MT", FALSE);
		fill(p, saveList, 0xFF0A141B);
		fill(p, saveDetails, 0xFF12232D);
		if (page.first > 0)
			control(p, previousSaves, translated("▲"));
		if (page.first + page.visibleRows < Int(page.saves.size()))
			control(p, nextSaves, translated("▼"));
		for (Int row = 0; row < page.visibleRows && page.first + row < Int(page.saves.size()); ++row)
		{
			const Int index = page.first + row;
			const auto &save = page.saves[index];
			const Box box = saveRow(row);
			fill(p, box, index == page.selected ? 0xFF365665 : 0xFF182A35);
			p.text(saveName(save), inset(box, 12, 12), 17, white, "Albertus MT", FALSE);
		}
		if (page.saves.empty())
			p.text(localize("GUI:NoSavedGames", "No saved games"), saveList, 18, muted);
		if (page.selected >= 0)
		{
			const auto &save = page.saves[page.selected].saveGameInfo;
			const SaveDate &date = save.date;
			const SYSTEMTIME time{date.year, date.month, date.dayOfWeek, date.day, date.hour,
				date.minute, date.second, date.milliseconds};
			UnicodeString map = localize(save.mapLabel.str(), "");
			if (map.isEmpty() && save.mapLabel.endsWithNoCase(".map"))
				map = localize(mapCatalogueKey(save.mapLabel).c_str(), "");
			if (map.isEmpty())
				map = translated(save.mapLabel.str());
			p.text(saveName(page.saves[page.selected]), {638, 224, 864, 280}, 21, white, "Albertus MT", FALSE, TRUE);
			p.text(localize("APT:Map", "Map"), {638, 293, 864, 318}, 16, muted, "Albertus MT", FALSE);
			p.text(map, {638, 319, 864, 384}, 18, white, "Albertus MT", FALSE, TRUE);
			p.text(localize("GUI:SaveDate", "Saved on"), {638, 398, 864, 424}, 16, muted, "Albertus MT", FALSE);
			UnicodeString timestamp = getUnicodeDateBuffer(time);
			timestamp.concat(L"  ");
			timestamp.concat(getUnicodeTimeBuffer(time));
			p.text(timestamp, {638, 424, 864, 461}, 18, white, "Albertus MT", FALSE);
		}
		else if (!page.saves.empty())
			p.text(localize("GUI:ChooseSavedGame", "Select a saved game"), saveDetails, 18, muted);
		if (page.failed)
			p.text(localize("GUI:SaveGameLoadFailed", "Could not load the saved game."),
				{292, 598, 730, 642}, 17, 0xFFFFC6A2);
		control(p, closeLoad, localize("APT:Cancel", "Cancel"), TRUE);
		if (page.selected >= 0)
			control(p, confirmLoad, localize("APT:Load", "Load"), TRUE);
	}

	void shellLoadInput(ShellLoadPage &page, UnsignedInt message, WindowMsgData point, Bool key, Real x, Real y)
	{
		if (pendingShellLoad)
			return;
		const Bool click = message == GWM_LEFT_UP;
		if ((key && point == KEY_ESC) || (click && closeLoad.contains(x, y)))
			page.open = FALSE;
		else if ((key && point == KEY_ENTER) || (click && confirmLoad.contains(x, y)))
		{
			if (page.selected >= 0)
				pendingShellLoad = page.saves[page.selected];
		}
		else if (key && (point == KEY_UP || point == KEY_DOWN))
			page.moveSelection(point == KEY_UP ? -1 : 1);
		else if (message == GWM_WHEEL_UP || (click && previousSaves.contains(x, y) && page.first > 0))
			page.scroll(-1);
		else if (message == GWM_WHEEL_DOWN ||
			(click && nextSaves.contains(x, y) && page.first + page.visibleRows < Int(page.saves.size())))
			page.scroll(1);
		else if (click)
		{
			for (Int row = 0; row < page.visibleRows && page.first + row < Int(page.saves.size()); ++row)
				if (saveRow(row).contains(x, y))
				{
					page.selected = page.first + row;
					page.failed = FALSE;
					break;
				}
		}
	}

	const Factor &selectedFactor(Int percent)
	{
		for (const auto &factor : factors)
			if (percent == factor.percent)
				return factor;
		return factors[2];
	}

	Choice colorChoice(Int i)
	{
		if (i < 0)
			return {i, UnicodeString()};
		const auto *definition = TheMultiplayerSettings->getColor(i);
		return {i, definition ? localize(definition->getTooltipName().str(), "") : UnicodeString(),
			definition ? static_cast<Color>(definition->getColor() | 0xFF000000) : 0};
	}

	UnicodeString randomLabel()
	{
		return localize("GUI:Random", "Random");
	}

	UnicodeString factionName(Int faction)
	{
		return faction < 0 ? randomLabel() : ThePlayerTemplateStore->getNthPlayerTemplate(faction)->getDisplayName();
	}

	Box startMarker(const MapMetaData &map, Int index);

	void openChoices(ReturnMenu &menu, MenuChoice kind, const Box &anchor, Int row = 0)
	{
		menu.choice = kind;
		menu.choiceRow = row;
		menu.choiceScroll = 0;
		menu.choices.clear();
		auto &options = menu.choices;
		switch (kind)
		{
			case MenuChoice::Resources:
				for (Int value : resourceValues)
					options.push_back({value, number(value)});
				break;
			case MenuChoice::Factor:
				for (const auto &factor : factors)
					options.push_back({factor.percent, localize(factor.key, factor.label)});
				break;
			case MenuChoice::Slot:
				for (SlotState state : {SLOT_OPEN, SLOT_CLOSED, SLOT_EASY_AI, SLOT_MED_AI, SLOT_BRUTAL_AI, SLOT_RETAIL_BRUTAL_AI})
					options.push_back({state, slotName(state)});
				break;
			case MenuChoice::Faction:
				options.push_back({-1, randomLabel()});
				for (Int faction : menu.factions)
					options.push_back({faction, factionName(faction)});
				break;
			case MenuChoice::Team:
				options.push_back({-1, localize("GUI:NoTeam", "None")});
				for (Int i = 0; i < MAX_SLOTS / 2; ++i)
					options.push_back({i, number(i + 1)});
				break;
			case MenuChoice::Color:
				options.push_back({-1, randomLabel()});
				for (Int i = 0, count = TheMultiplayerSettings->getNumColors(); i < count; ++i)
					options.push_back(colorChoice(i));
				break;
			case MenuChoice::StartPosition:
				options.push_back({-1, randomLabel()});
				if (const auto *map = menu.currentMap())
					for (Int i = 0; i < menu.slotCount(); ++i)
						if (startMarker(*map, i).valid())
							options.push_back({i, number(i + 1)});
				break;
			case MenuChoice::StartAt:
				options.push_back({-1, localize("GUI:Open", "Open")});
				for (Int i = 0; i < menu.slotCount(); ++i)
				{
					const auto &slot = menu.slots[i];
					if (!slot.occupied())
						continue;
					UnicodeString label = number(i + 1);
					label.concat(L". ");
					label.concat(i == 0 ? menu.profileName() : slotName(slot.state));
					options.push_back({i, label});
				}
				break;
			case MenuChoice::Profile:
				options.push_back({0, localize("APT:ChangeProfile", "Change profile")});
				options.push_back({1, localize("APT:CreateNew", "Create new")});
				if (menu.profiles.size() > 1)
					options.push_back({2, localize("APT:DeleteProfile", "Delete profile")});
				options.push_back({3, localize("APT:ProfileStats", "Profile stats")});
				break;
			case MenuChoice::SelectProfile:
			case MenuChoice::DeleteProfile:
				for (size_t i = 0; i < menu.profiles.size(); ++i)
					options.push_back({Int(i), translated(menu.profiles[i].str())});
				break;
			case MenuChoice::None: break;
		}
		const Real height = choiceHeight * std::min(Int(options.size()), ReturnMenu::visibleChoices);
		const Real top = anchor.bottom + height <= 663 ? anchor.bottom : std::max(0.0f, anchor.top - height);
		const Real left = std::min(anchor.left, 768.0f);
		menu.choiceBox = {left, top, std::max(anchor.right, left + 150), top + height};
	}

	Box startMarker(const MapMetaData &map, Int index)
	{
		const Real width = map.m_extent.hi.x - map.m_extent.lo.x;
		const Real height = map.m_extent.hi.y - map.m_extent.lo.y;
		AsciiString key;
		key.format("Player_%d_Start", index + 1);
		const auto point = map.m_waypoints.find(key);
		if (point == map.m_waypoints.end() || width <= 0 || height <= 0)
			return {};
		const Real x = previewBox.left + (previewBox.right - previewBox.left) *
			std::clamp((point->second.x - map.m_extent.lo.x) / width, 0.0f, 1.0f);
		const Real y = previewBox.top + (previewBox.bottom - previewBox.top) *
			std::clamp((map.m_extent.hi.y - point->second.y) / height, 0.0f, 1.0f);
		return {x - 10, y - 10, x + 10, y + 10};
	}

	void drawMap(const ReturnMenu &menu, const Painter &p)
	{
		const auto *map = menu.currentMap();
		if (!map)
		{
			p.text(localize("GUI:CantFindMap", "No skirmish maps"), mapPanel, 20);
			return;
		}
		control(p, mapNameSort, localize("APT:MapName", "Map Name"), !menu.sortByPlayers);
		control(p, mapPlayersSort, localize("APT:Players", "Players"), menu.sortByPlayers);
		for (Int row = 0; row < ReturnMenu::visibleMaps && menu.mapScroll + row < Int(menu.maps.size()); ++row)
		{
			const Int index = menu.mapScroll + row;
			const Box box = mapLine(row);
			const auto &entry = menu.maps[index];
			control(p, box, UnicodeString(), index == menu.mapIndex);
			p.text(entry.name, {box.left + 9, box.top, mapNameSort.right - 6, box.bottom},
				16, white, "Albertus MT", FALSE);
			p.text(number(entry.players), {mapPlayersSort.left, box.top, box.right, box.bottom}, 16);
		}
		p.picture(TheMappedImageCollection->findImageByName("AptVSliderUpButtonEnabled"), mapUp);
		p.picture(TheMappedImageCollection->findImageByName("AptVSliderDownButtonEnabled"), mapDown);
		frame(p, {previewBox.left - 4, previewBox.top - 4, previewBox.right + 4, previewBox.bottom + 4}, "MapPreviewFrame");
		if (menu.previewFile)
		{
			const auto *parchment = TheMappedImageCollection->findImageByName("UiRetailGlobe");
			if (parchment)
			{
				const Region2D paper{{0.16f, 0.16f}, {0.84f, 0.84f}};
				TheDisplay->drawImagePart(parchment, Int(previewBox.left * p.sx), Int(previewBox.top * p.sy),
					Int(previewBox.right * p.sx), Int(previewBox.bottom * p.sy), paper);
			}
		}
		p.picture(menu.preview, previewBox);
		for (Int i = 0, count = menu.slotCount(); i < count; ++i)
		{
			const Box marker = startMarker(*map, i);
			if (!marker.valid())
				continue;
			const auto owner = std::find_if(menu.slots.begin(), menu.slots.begin() + count,
				[i](const SetupSlot &slot) { return slot.occupied() && slot.start == i; });
			const Bool assigned = owner != menu.slots.begin() + count;
			const Color color = assigned ? colorChoice(owner->color).color : muted;
			p.picture(TheMappedImageCollection->findImageByName("AptPlayerStart"), marker);
			p.text(number(i + 1), marker, 13, assigned && color ? color : white);
		}
		p.text(menu.maps[menu.mapIndex].name, {707, 199, 908, 225}, 17, white, "Albertus MT", FALSE);
		p.text(menu.description, {707, 232, 908, 381}, 15, muted, "Albertus MT", FALSE, TRUE);
	}

	void drawRules(const ReturnMenu &menu, const Painter &p)
	{
		p.text(localize("RULE:InitialResources", "Initial resources"), {126, 208, 355, 241}, 18, muted, "Albertus MT", FALSE);
		p.text(localize("RULE:CommandPointFactor", "Command point factor"), {126, 247, 355, 280}, 18, muted, "Albertus MT", FALSE);
		control(p, resourcesBox, number(menu.resources), FALSE, TRUE);
		const auto &factor = selectedFactor(menu.rules.commandPointFactor);
		control(p, factorBox, localize(factor.key, factor.label), FALSE, TRUE);
		checkbox(p, {customBox.left, customBox.top, customBox.left + 29, customBox.bottom}, menu.rules.customHeroes);
		checkbox(p, {ringBox.left, ringBox.top, ringBox.left + 29, ringBox.bottom}, menu.rules.ringHeroes);
		p.text(localize("RULE:AllowCustomHeroes", "Allow custom heroes"), inset(customBox, 40), 18, muted, "Albertus MT", FALSE);
		p.text(localize("RULE:AllowRingHeroes", "Allow ring heroes"), inset(ringBox, 40), 18, muted, "Albertus MT", FALSE);
		button(p, resetBox, localize("APT:Reset", "Reset"));
	}

	void drawSetup(const ReturnMenu &menu, const Painter &p)
	{
		p.text(localize("APT:GameSetup", "Game Setup"), {328, 100, 696, 133}, 23);
		const std::string settings = "_root.MpMapAndSettings.";
		const auto tab = [&](const Box &box, UnicodeString label, Bool selected) {
			fill(p, box, 0xE90A141B);
			if (moviePart(p, menu.setupArt, settings + "TabMap.@2", box, selected ? 1.0f : 0.45f))
				p.text(label, box, 17, selected ? white : muted);
			else
				control(p, box, label, selected);
		};
		tab(mapTab, localize("APT:Map", "Map"), !menu.rulesTab);
		tab(rulesTab, localize("APT:Rules", "Rules"), menu.rulesTab);
		fill(p, mapPanel, 0xEC10191C);
		moviePart(p, menu.setupArt, settings + "@51", mapPanel, 0.8f);
		if (menu.rulesTab)
			drawRules(menu, p);
		else
			drawMap(menu, p);

		constexpr const char *headers[][2] = {{"APT:Player", "Player"}, {"APT:Army", "Army"},
			{"GUI:StartPosition", "Start Position"}, {"APT:Team", "Team"}, {"APT:Color", "Color"}};
		const Box table{slotEdges[0], 403, slotEdges[std::size(headers)], 624};
		fill(p, table, 0xE908151D);
		moviePart(p, menu.setupArt, settings + "@51.@2", {table.left, table.top, table.right, 430});
		for (Int column = 0; column < Int(std::size(headers)); ++column)
		{
			const Box header{slotEdges[column], 403, slotEdges[column + 1] - 3, 430};
			p.text(localize(headers[column][0], headers[column][1]), header, 17, muted);
		}
		for (Int row = 0; row < MAX_SLOTS; ++row)
		{
			const auto &slot = menu.slots[row];
			const Bool available = row < menu.slotCount();
			control(p, slotCell(row, 0), row == 0 ? menu.profileName() : slotName(available ? slot.state : SLOT_CLOSED),
				FALSE, available && row > 0);
			if (!available || !slot.occupied())
			{
				for (Int column = 1; column < Int(std::size(headers)); ++column)
					control(p, slotCell(row, column), translated("-"));
				continue;
			}
			control(p, slotCell(row, 1), factionName(slot.faction), FALSE, TRUE);
			control(p, slotCell(row, 2), slot.start < 0 ? randomLabel() : number(slot.start + 1), FALSE, TRUE);
			control(p, slotCell(row, 3), slot.team < 0 ? translated("-") : number(slot.team + 1), FALSE, TRUE);
			const auto color = colorChoice(slot.color);
			control(p, slotCell(row, 4), slot.color < 0 ? translated("?") : UnicodeString(), FALSE, TRUE, color.color);
		}
		if (menu.sandboxConfirmed)
			p.text(localize("GUI:SandboxWarning", "Play again to start without opponents."),
				{104, 627, 920, 661}, 16, muted, "Albertus MT", TRUE, TRUE);
		button(p, setupBack, localize("APT:MainMenu", "Main Menu"));
		button(p, profileBox, localize("APT:Profile", "Profile"));
		if (menu.currentMap() && !menu.factions.empty())
			button(p, setupPlay, localize("APT:Play", "Play"));
		if (menu.profilePage != ProfilePage::None)
		{
			fill(p, {0, 0, 1024, 768}, 0xA0000000);
			fill(p, profilePanel, 0xFF182830);
			const Bool create = menu.profilePage == ProfilePage::Create;
			p.text(create ? localize("APT:CreateNew", "Create new") : localize("APT:ProfileStats", "Profile stats"),
				{332, 276, 693, 310}, 23);
			if (create)
				control(p, {337, 317, 688, 355}, translated(menu.newProfile.str()), TRUE);
			else
			{
				p.text(menu.profileName(), {337, 314, 688, 349}, 20);
				UnicodeString counts;
				const AsciiString name = menu.profiles[menu.activeProfile];
				counts.format(L"%ls  %d     %ls  %d", localize("APT:Wins", "Wins").str(),
					menu.profilePrefs.getInt(profileStatKey(TRUE, name), 0), localize("APT:Losses", "Losses").str(),
					menu.profilePrefs.getInt(profileStatKey(FALSE, name), 0));
				p.text(counts, {337, 352, 688, 384}, 16);
			}
			button(p, profileCancel, localize("APT:Cancel", "Cancel"));
			if (create)
				button(p, profileAccept, localize("APT:Accept", "Accept"));
		}
		p.text(menu.profileFeedback, {315, 370, 708, 399}, 15, muted);
		if (menu.choice != MenuChoice::None)
		{
			for (Int i = 0; i < ReturnMenu::visibleChoices && i + menu.choiceScroll < Int(menu.choices.size()); ++i)
			{
				const auto &item = menu.choices[i + menu.choiceScroll];
				const Real top = menu.choiceBox.top + i * choiceHeight;
				control(p, {menu.choiceBox.left, top, menu.choiceBox.right, top + choiceHeight - 1},
					item.text, TRUE, FALSE, item.color);
			}
		}
	}

	void setupInput(ReturnMenu &menu, UnsignedInt message, WindowMsgData point, Bool key, Real x, Real y)
	{
		const Bool click = message == GWM_LEFT_UP;
		const Int scroll = message == GWM_WHEEL_UP || (key && point == KEY_UP) ? -1 :
			message == GWM_WHEEL_DOWN || (key && point == KEY_DOWN) ? 1 : 0;
		if (menu.profilePage != ProfilePage::None)
		{
			if ((key && point == KEY_ESC) || (click && profileCancel.contains(x, y)))
			{
				menu.profilePage = ProfilePage::None;
				menu.profileFeedback.clear();
			}
			else if (menu.profilePage == ProfilePage::Create)
			{
				if ((key && point == KEY_ENTER) || (click && profileAccept.contains(x, y)))
					menu.createProfile();
				else if (key && point == KEY_BACKSPACE && menu.newProfile.isNotEmpty())
					menu.newProfile.truncateBy(1);
				else if (message == GWM_IME_CHAR && menu.newProfile.getLength() < 24 &&
					((point >= 'A' && point <= 'Z') || (point >= 'a' && point <= 'z') ||
					 (point >= '0' && point <= '9') || point == ' ' || point == '_' || point == '-'))
					menu.newProfile.concat(static_cast<Char>(point));
			}
			return;
		}
		if (menu.choice != MenuChoice::None)
		{
			if (scroll)
				menu.choiceScroll = std::clamp(menu.choiceScroll + scroll, 0,
					std::max(0, Int(menu.choices.size()) - ReturnMenu::visibleChoices));
			else if (click || (key && point == KEY_ESC))
			{
				const auto type = menu.choice;
				menu.choice = MenuChoice::None;
				if (click && menu.choiceBox.contains(x, y))
				{
					const Int index = menu.choiceScroll + Int((y - menu.choiceBox.top) / choiceHeight);
					if (index < Int(menu.choices.size()))
					{
						const Int value = menu.choices[index].value;
						if (type == MenuChoice::Profile && (value == 0 || value == 2))
							openChoices(menu, value == 0 ? MenuChoice::SelectProfile : MenuChoice::DeleteProfile, profileBox);
						else
							menu.choose(type, value);
					}
				}
			}
			return;
		}
		if ((key && point == KEY_ESC) || (click && setupBack.contains(x, y)))
			menu.setup = FALSE;
		else if ((key && point == KEY_ENTER) || (click && setupPlay.contains(x, y)))
			menu.play();
		else if (click && profileBox.contains(x, y))
			openChoices(menu, MenuChoice::Profile, profileBox);
		else if (click && (mapTab.contains(x, y) || rulesTab.contains(x, y)))
			menu.rulesTab = rulesTab.contains(x, y);
		else if (!menu.rulesTab && key && (point == KEY_LEFT || point == KEY_RIGHT))
			menu.selectMap(menu.mapIndex + (point == KEY_LEFT ? -1 : 1));
		else if (!menu.rulesTab && (scroll || (click && (mapUp.contains(x, y) || mapDown.contains(x, y)))))
		{
			const Int step = scroll ? scroll : (mapUp.contains(x, y) ? -1 : 1);
			menu.mapScroll = std::clamp(menu.mapScroll + step, 0,
				std::max(0, Int(menu.maps.size()) - ReturnMenu::visibleMaps));
		}
		else if (click)
		{
			if (menu.rulesTab)
			{
				if (resourcesBox.contains(x, y))
					openChoices(menu, MenuChoice::Resources, resourcesBox);
				else if (factorBox.contains(x, y))
					openChoices(menu, MenuChoice::Factor, factorBox);
				else if (customBox.contains(x, y))
					menu.rules.customHeroes = !menu.rules.customHeroes;
				else if (ringBox.contains(x, y))
					menu.rules.ringHeroes = !menu.rules.ringHeroes;
				else if (resetBox.contains(x, y))
				{
					menu.resources = 1000;
					menu.rules = {100, TRUE, TRUE};
				}
			}
			else
			{
				if (mapNameSort.contains(x, y) || mapPlayersSort.contains(x, y))
				{
					menu.setMapSort(mapPlayersSort.contains(x, y));
					return;
				}
				for (Int i = 0; i < ReturnMenu::visibleMaps; ++i)
					if (mapLine(i).contains(x, y))
						menu.selectMap(menu.mapScroll + i);
				if (const auto *map = menu.currentMap())
					for (Int i = 0, count = menu.slotCount(); i < count; ++i)
					{
						const Box marker = startMarker(*map, i);
						if (marker.contains(x, y))
						{
							openChoices(menu, MenuChoice::StartAt, marker, i);
							return;
						}
					}
			}

			constexpr MenuChoice columns[] = {MenuChoice::Slot, MenuChoice::Faction, MenuChoice::StartPosition,
				MenuChoice::Team, MenuChoice::Color};
			for (Int i = 0, count = menu.slotCount(); i < count; ++i)
				for (Int col = 0; col < Int(std::size(columns)); ++col)
					if ((col == 0 ? i != 0 : menu.slots[i].occupied()) && slotCell(i, col).contains(x, y))
						openChoices(menu, columns[col], slotCell(i, col), i);
		}
	}

	void drawReturnMenu(GameWindow *window, WinInstanceData *)
	{
		const auto &menu = *static_cast<ReturnMenu *>(window->winGetUserData());
		const Painter painter(menu.scene);
		painter.background(menu.background.get());
		if (menu.loadPage.open)
		{
			drawShellLoad(menu.loadPage, painter);
			return;
		}
		const auto &shellArt = menu.setup && menu.skirmishArt.bounds("_root.Title").valid() ? menu.skirmishArt : menu.scene;
		shellArt.draw([](const std::string &path) {
			return childOf(path, "_root.Title") || childOf(path, "_root.patternShell");
		}, [&](const std::string &, const AptScene::Text &label) {
			if (label.value == "$PAGETITLE")
				painter.label(menu.setup ? localize("APT:Skirmish", "Skirmish") :
					localize("APT:MainMenu", "Main Menu"), label, muted);
		});
		if (menu.setup)
		{
			drawSetup(menu, painter);
			return;
		}
		painter.text(localize("APT:Skirmish", "Skirmish"), skirmishButton, 22);
		if (menu.canRematch())
			painter.text(localize("GUI:PlayAgain", "Play Again"), rematchButton, 22);
		painter.text(localize("APT:ExitGame", "Exit Game"), exitButton, 22);
		painter.text(localize("APT:Load", "Load"), loadMenuButton, 22);
		painter.text(localize("APT:Settings", "Settings"), settingsButton, 22);
	}

	WindowMsgHandledType menuSystem(GameWindow *window, UnsignedInt message, WindowMsgData state, WindowMsgData result)
	{
		if (message == GWM_INPUT_FOCUS && state == TRUE)
		{
			*reinterpret_cast<Bool *>(result) = TRUE;
			return MSG_HANDLED;
		}
		if (message == GWM_DESTROY)
		{
			closeRetailOptionsFor(window);
			delete static_cast<ReturnMenu *>(window->winGetUserData());
			window->winSetUserData(nullptr);
		}
		return MSG_IGNORED;
	}

	WindowMsgHandledType menuInput(GameWindow *window, UnsignedInt message, WindowMsgData point, WindowMsgData state)
	{
		auto &menu = *static_cast<ReturnMenu *>(window->winGetUserData());
		const Real x = (point & 0xFFFF) / menu.scene.xScale();
		const Real y = (point >> 16) / menu.scene.yScale();
		const Bool key = message == GWM_CHAR && (state & KEY_STATE_DOWN);
		const Bool click = message == GWM_LEFT_UP;
		if (click && TheWindowManager->winGetFocus() != window)
			TheWindowManager->winSetFocus(window);
		if (menu.loadPage.open)
		{
			shellLoadInput(menu.loadPage, message, point, key, x, y);
			return MSG_HANDLED;
		}
		if (menu.setup)
		{
			setupInput(menu, message, point, key, x, y);
			if (menu.profilePage == ProfilePage::Create)
				BitSet(window->winGetInstanceData()->m_style, GWS_ENTRY_FIELD);
			else
				BitClear(window->winGetInstanceData()->m_style, GWS_ENTRY_FIELD);
			return MSG_HANDLED;
		}
		if ((key && point == KEY_ESC) || (click && exitButton.contains(x, y)))
			TheGameEngine->setQuitting(TRUE);
		else if (click && skirmishButton.contains(x, y))
			menu.openSetup();
		else if ((key && point == KEY_L) || (click && loadMenuButton.contains(x, y)))
			menu.loadPage.show();
		else if (click && settingsButton.contains(x, y))
			openRetailOptions(window, FALSE);
		else if (menu.canRematch() && ((key && point == KEY_ENTER) || (click && rematchButton.contains(x, y))))
			menu.startRematch();
		return MSG_HANDLED;
	}
}

void updateRetailShellLoad()
{
	if (!pendingShellLoad)
		return;
	AvailableGameInfo save = std::move(*pendingShellLoad);
	pendingShellLoad.reset();
	if (!TheShell->isShellActive())
		return;

	TheGameLogic->prepareNewGame(GAME_SINGLE_PLAYER, DIFFICULTY_NORMAL, 0);
	SaveCode result = SC_ERROR;
	try
	{
		result = TheGameState->loadGame(save);
	}
	catch (...)
	{
		result = SC_INVALID_DATA;
	}
	if (result == SC_OK)
		return;

	TheGameLogic->setLoadingSave(FALSE);
	TheGameLogic->setLoadingMap(FALSE);
	if (TheGameLogic->isInGame())
		TheGameLogic->clearGameData(FALSE);
	TheGameEngine->reset();
	TheGameEngine->setQuitting(FALSE);
	reopenShellLoad = TRUE;
	TheShell->showShell(TRUE);
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
	if (!RetailFileLayers::enabled())
		return FALSE;
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
		GameWindow *window = screen->getFirstWindow();
		auto &menu = *static_cast<ReturnMenu *>(window->winGetUserData());
		menu.setup = FALSE;
		menu.loadPage.open = FALSE;
		if (reopenShellLoad)
		{
			reopenShellLoad = FALSE;
			menu.loadPage.show();
			menu.loadPage.failed = TRUE;
		}
		TheWritableGlobalData->m_breakTheMovie = FALSE;
		TheMouse->setVisibility(TRUE);
		screen->hide(FALSE);
		TheWindowManager->winSetFocus(window);
	});
	layout.setShutdown([](WindowLayout *screen, void *) {
		screen->hide(TRUE);
		TheShell->shutdownComplete(screen);
	});
	return TRUE;
}
