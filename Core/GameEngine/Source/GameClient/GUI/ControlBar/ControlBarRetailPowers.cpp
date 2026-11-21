#include "PreRTS.h"

#include "Common/FileSystem.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "GameClient/AptScene.h"
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

#include <bit>
#include <cmath>
#include <map>
#include <string>
#include <string_view>

namespace
{
constexpr Int StoreSlots = 12;
constexpr Int PanelWidth = 560;
constexpr Int PanelHeight = 558;
constexpr Int NodeSize = 64;
constexpr std::string_view ConnectorPath = "_root.SpellStore.Buttons.Connectors.";
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

struct AptPoint
{
	Real x;
	Real y;
};

struct AptHitRegion
{
	std::vector<std::array<AptPoint, 3>> triangles;
	Real left = 0, top = 0, right = 0, bottom = 0;

	Bool contains(AptPoint point) const
	{
		if (point.x < left || point.x > right || point.y < top || point.y > bottom)
			return FALSE;
		for (const auto& triangle : triangles)
		{
			const auto side = [&point](AptPoint a, AptPoint b)
			{
				return (b.x - a.x) * (point.y - a.y) - (b.y - a.y) * (point.x - a.x);
			};
			const Real a = side(triangle[0], triangle[1]);
			const Real b = side(triangle[1], triangle[2]);
			const Real c = side(triangle[2], triangle[0]);
			if ((a >= 0 && b >= 0 && c >= 0) || (a <= 0 && b <= 0 && c <= 0))
				return TRUE;
		}
		return FALSE;
	}
};

struct StoreMovieLayout
{
	UnsignedInt fadeTime = 198;
	Real stageWidth = 1024;
	Real stageHeight = 768;
	std::array<AptHitRegion, StoreSlots> nodes;
	AptHitRegion reset, accept;
};

StoreMovieLayout storeMovieLayout;
std::unique_ptr<AptScene> storeArt;

struct Layout
{
	Real scale;
	Int x;
	Int y;

	Rect at(Real left, Real top, Real width, Real height) const
	{
		return { x + static_cast<Int>(left * scale), y + static_cast<Int>(top * scale),
			std::max(1, static_cast<Int>(width * scale)), std::max(1, static_cast<Int>(height * scale)) };
	}

	Rect bounds(const AptHitRegion& region) const
	{
		return at(region.left, region.top, region.right - region.left, region.bottom - region.top);
	}

	Rect node(Int slot) const
	{
		const AptHitRegion& region = storeMovieLayout.nodes[slot];
		if (!region.triangles.empty())
			return bounds(region);
		static constexpr Int rowStart[] = { 0, 3, 7, 10 };
		static constexpr Int rowWidth[] = { 3, 4, 3, 2 };
		Int row = 0;
		while (row < 3 && slot >= rowStart[row + 1])
			++row;
		const Int center = PanelWidth / 2 + (2 * (slot - rowStart[row]) - rowWidth[row] + 1) * 56;
		return at(center - NodeSize / 2, 90 + row * 95, NodeSize, NodeSize);
	}

	Bool hits(const AptHitRegion& region, Rect fallback, Int px, Int py) const
	{
		return region.triangles.empty() ? fallback.contains(px, py) :
			region.contains({ (px - x + 0.5f) / scale, (py - y + 0.5f) / scale });
	}

	Rect action(const AptHitRegion& region, Bool accept) const
	{
		if (region.triangles.empty())
			return at(accept ? 287 : 139, 504, 134, 38);
		const Real center = (region.left + region.right) * 0.5f;
		const Real halfWidth = (region.right - region.left) * (70.5f / (2.0f * 93.3f));
		return at(center - halfWidth, region.top, halfWidth * 2, region.bottom - region.top);
	}
};

UnsignedInt readLittleWord(const UnsignedByte* data)
{
	return static_cast<UnsignedInt>(data[0]) | static_cast<UnsignedInt>(data[1]) << 8 |
		static_cast<UnsignedInt>(data[2]) << 16 | static_cast<UnsignedInt>(data[3]) << 24;
}

struct AptTransform
{
	Real a = 1, b = 0, c = 0, d = 1;
	AptPoint origin = {};

	AptPoint apply(AptPoint point) const
	{
		return { a * point.x + c * point.y + origin.x, b * point.x + d * point.y + origin.y };
	}

	AptTransform compose(const AptTransform& child) const
	{
		return { a * child.a + c * child.b, b * child.a + d * child.b,
			a * child.c + c * child.d, b * child.c + d * child.d, apply(child.origin) };
	}
};

struct InvalidStoreMovie {};

class StoreMovie
{
	struct Placement
	{
		size_t character = 0;
		std::string_view name;
		AptTransform transform;
	};

	using Placements = std::map<UnsignedInt, Placement>;

public:
	explicit StoreMovie(const std::vector<UnsignedByte>& bytes) : m_bytes(bytes) {}

	StoreMovieLayout read(size_t movie)
	{
		enum { Movie = 9, Sprite = 5 };
		if (characterType(movie) != Movie)
			throw InvalidStoreMovie{};
		m_characterCount = word(movie + 20);
		m_characters = word(movie + 24);
		checkTable(m_characters, m_characterCount, 4);
		const UnsignedInt width = word(movie + 28);
		const UnsignedInt height = word(movie + 32);
		if (width == 0 || height == 0 || width > 8192 || height > 8192)
			throw InvalidStoreMovie{};

		StoreMovieLayout layout;
		layout.stageWidth = width;
		layout.stageHeight = height;
		const UnsignedInt frameTime = word(movie + 36);
		if (frameTime >= 10 && frameTime <= 100)
			layout.fadeTime = frameTime * 6;

		const Placement store = named(displayList(movie), "SpellStore");
		if (characterType(store.character) != Sprite)
			return layout;
		const Placement buttons = named(displayList(store.character, "_multiplayer"), "Buttons");
		if (characterType(buttons.character) != Sprite)
			return layout;
		const Real scale = static_cast<Real>(PanelHeight) / height;
		const Real wideScale = scale * 4.0f / 3.0f;
		const AptTransform panel{ wideScale, 0, 0, scale, { (PanelWidth - width * wideScale) / 2, 0 } };
		const AptTransform transform = panel.compose(store.transform).compose(buttons.transform);
		const Placements placements = displayList(buttons.character);
		for (Int slot = 0; slot < StoreSlots; ++slot)
			layout.nodes[slot] = hitRegion(named(placements, "Spell" + std::to_string(slot + 1)), transform, "_active_stopped");

		const Placement controls = named(displayList(buttons.character, {}, TRUE), "ButtonsMain");
		if (characterType(controls.character) == Sprite)
		{
			const Placements actions = displayList(controls.character);
			const AptTransform parent = transform.compose(controls.transform);
			layout.reset = hitRegion(named(actions, "Reset"), parent, "_up");
			layout.accept = hitRegion(named(actions, "Accept"), parent, "_up");
		}
		return layout;
	}

private:
	const std::vector<UnsignedByte>& m_bytes;
	size_t m_characters = 0;
	size_t m_characterCount = 0;

	void checkTable(size_t offset, size_t count, size_t stride) const
	{
		if (offset > m_bytes.size() || count > (m_bytes.size() - offset) / stride)
			throw InvalidStoreMovie{};
	}

	UnsignedInt word(size_t offset) const
	{
		checkTable(offset, 1, 4);
		return readLittleWord(m_bytes.data() + offset);
	}

	Real number(size_t offset) const
	{
		const Real value = std::bit_cast<Real>(word(offset));
		if (!std::isfinite(value) || value < -16384.0f || value > 16384.0f)
			throw InvalidStoreMovie{};
		return value;
	}

	std::string_view string(size_t offset) const
	{
		if (!offset)
			return {};
		checkTable(offset, 1, 1);
		const auto* start = reinterpret_cast<const char*>(m_bytes.data() + offset);
		const auto* end = static_cast<const char*>(memchr(start, 0, std::min<size_t>(128, m_bytes.size() - offset)));
		if (!end)
			throw InvalidStoreMovie{};
		return { start, static_cast<size_t>(end - start) };
	}

	UnsignedInt characterType(size_t offset) const
	{
		if (offset == 0)
			return 0;
		if (word(offset + 4) != 0x09876543)
			throw InvalidStoreMovie{};
		return word(offset);
	}

	AptTransform transformAt(size_t offset) const
	{
		return { number(offset), number(offset + 4), number(offset + 8), number(offset + 12),
			{ number(offset + 16), number(offset + 20) } };
	}

	Placements displayList(size_t character, std::string_view label = {}, Bool finalFrame = FALSE) const
	{
		enum { FrameLabel = 2, PlaceObject = 3, RemoveObject = 4 };
		enum { Move = 1, HasCharacter = 2, HasMatrix = 4, HasName = 32 };
		const size_t frames = word(character + 12);
		const size_t frameCount = word(character + 8);
		if (frameCount > 512)
			throw InvalidStoreMovie{};
		checkTable(frames, frameCount, 8);
		Placements placements;
		for (size_t frame = frames; frame < frames + frameCount * 8; frame += 8)
		{
			Bool stop = label.empty();
			const size_t items = word(frame + 4);
			const size_t itemCount = word(frame);
			if (itemCount > 4096)
				throw InvalidStoreMovie{};
			checkTable(items, itemCount, 4);
			for (size_t item = items; item < items + itemCount * 4; item += 4)
			{
				const size_t offset = word(item);
				switch (word(offset))
				{
					case FrameLabel:
						stop |= string(word(offset + 4)) == label;
						break;
					case PlaceObject:
					{
						const UnsignedInt flags = word(offset + 4);
						Placement& placement = placements[word(offset + 8)];
						if (!(flags & Move))
							placement = {};
						if (flags & HasCharacter)
						{
							const size_t id = word(offset + 12);
							if (id >= m_characterCount)
								throw InvalidStoreMovie{};
							placement.character = word(m_characters + id * 4);
						}
						if (flags & HasMatrix)
							placement.transform = transformAt(offset + 16);
						if (flags & HasName)
							placement.name = string(word(offset + 52));
						break;
					}
					case RemoveObject:
						placements.erase(word(offset + 4));
						break;
				}
			}
			if (stop && !finalFrame)
				return placements;
		}
		return finalFrame ? placements : Placements{};
	}

	static Placement named(const Placements& placements, std::string_view name)
	{
		for (const auto& [depth, placement] : placements)
			if (placement.name == name)
				return placement;
		return {};
	}

	AptHitRegion hitRegion(const Placement& clip, const AptTransform& parent, std::string_view label = {}) const
	{
		enum { Button = 4, Sprite = 5 };
		const AptTransform transform = parent.compose(clip.transform);
		if (characterType(clip.character) == Button)
			return buttonRegion(clip.character, transform);
		if (characterType(clip.character) == Sprite)
			for (const auto& [depth, placed] : displayList(clip.character, label))
				if (characterType(placed.character) == Button)
					return buttonRegion(placed.character, transform.compose(placed.transform));
		return {};
	}

	AptHitRegion buttonRegion(size_t button, const AptTransform& transform) const
	{
		const size_t triangleCount = word(button + 28);
		const size_t vertexCount = word(button + 32);
		const size_t vertices = word(button + 36);
		const size_t triangles = word(button + 40);
		if (vertexCount > 1024 || triangleCount > 2048)
			throw InvalidStoreMovie{};
		checkTable(vertices, vertexCount, 8);
		checkTable(triangles, triangleCount, 6);
		AptHitRegion region;
		for (size_t offset = triangles; offset < triangles + triangleCount * 6; offset += 6)
		{
			std::array<AptPoint, 3> points;
			for (size_t i = 0; i < points.size(); ++i)
			{
				const UnsignedByte* index = m_bytes.data() + offset + i * 2;
				const size_t vertex = index[0] | static_cast<size_t>(index[1]) << 8;
				if (vertex >= vertexCount)
					throw InvalidStoreMovie{};
				points[i] = transform.apply({ number(vertices + vertex * 8), number(vertices + vertex * 8 + 4) });
				if (points[i].x < -PanelWidth || points[i].x > 2 * PanelWidth ||
					points[i].y < -PanelHeight || points[i].y > 2 * PanelHeight)
					throw InvalidStoreMovie{};
			}
			const auto& [a, b, c] = points;
			if ((b.x - a.x) * (c.y - a.y) == (b.y - a.y) * (c.x - a.x))
				continue;
			if (region.triangles.empty())
			{
				region.left = region.right = a.x;
				region.top = region.bottom = a.y;
			}
			for (const AptPoint& point : points)
			{
				region.left = std::min(region.left, point.x);
				region.top = std::min(region.top, point.y);
				region.right = std::max(region.right, point.x);
				region.bottom = std::max(region.bottom, point.y);
			}
			region.triangles.push_back(points);
		}
		return region;
	}
};

StoreMovieLayout readSpellStore()
{
	File* constants = TheFileSystem->openFile("SpellStore.const", File::READ | File::BINARY);
	if (!constants)
		return {};
	UnsignedByte header[24];
	const Int headerBytes = constants->read(header, sizeof(header));
	constants->close();
	if (headerBytes != sizeof(header) || memcmp(header, "Apt constant file", 17) != 0)
		return {};

	File* movie = TheFileSystem->openFile("SpellStore.apt", File::READ | File::BINARY);
	if (!movie)
		return {};
	const Int size = movie->size();
	std::vector<UnsignedByte> bytes;
	if (size >= 12 && size <= 8 * 1024 * 1024)
		bytes.resize(size);
	const Int read = bytes.empty() ? 0 : movie->read(bytes.data(), size);
	movie->close();
	if (read < 12 || read != size || memcmp(bytes.data(), "Apt Data:", 9) != 0)
		return {};
	try
	{
		return StoreMovie(bytes).read(readLittleWord(header + 20));
	}
	catch (const InvalidStoreMovie&)
	{
		return {};
	}
}

Layout screenLayout()
{
	const Int w = TheDisplay->getWidth();
	const Int h = TheDisplay->getHeight();
	const Real scale = std::min(w / 650.0f, h / static_cast<Real>(PanelHeight));
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

Bool drawStoreArt(const char* name, const Rect& rect, Color tint = 0xFFFFFFFF)
{
	const Image* image = TheMappedImageCollection ? TheMappedImageCollection->findImageByName(name) : nullptr;
	if (!TheDisplay->hasImageTexture(image))
		return FALSE;
	TheDisplay->drawImage(image, rect.x, rect.y, rect.x + rect.w, rect.y + rect.h, tint);
	return TRUE;
}

std::vector<std::string> prerequisiteClips(const Player* player)
{
	std::vector<std::string> clips;
	for (Int slot = 0; slot < StoreSlots; ++slot)
	{
		const ScienceType science = storeScience(player, slot);
		if (science == SCIENCE_INVALID)
			continue;
		const auto* alternatives = TheScienceStore->getPrerequisiteAlternatives(science);
		if (!alternatives)
			continue;
		for (Int candidate = 0; candidate < slot; ++candidate)
		{
			const ScienceType prerequisite = storeScience(player, candidate);
			if (prerequisite == SCIENCE_INVALID ||
				!std::any_of(alternatives->begin(), alternatives->end(), [player, prerequisite](const ScienceVec& group)
				{
					return containsScience(group, prerequisite) &&
						std::none_of(group.begin(), group.end(), [player](ScienceType required)
						{
							return player->getSciencePurchaseCost(required) == 0 &&
								!TheScienceStore->isScienceGrantable(required) && !player->hasScience(required);
						});
				}))
				continue;

			clips.push_back("c" + std::to_string(candidate + 1) + "_" + std::to_string(slot + 1));
		}
	}
	return clips;
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

void drawScreen(GameWindow* window, WinInstanceData*) { TheControlBar->drawRetailPowers(window); }
}

void ControlBar::initRetailPowers(GameWindow* palantir)
{
	storeMovieLayout = readSpellStore();
	storeArt = std::make_unique<AptScene>("SpellStore", nullptr,
		std::initializer_list<std::pair<const char*, const char*>>{
			{"_root.SpellStore", "_multiplayer"},
			{"_root.SpellStore.Buttons", "#45"}
		});
	m_retailPowersFadeTime = storeMovieLayout.fadeTime;
	m_retailPowersScreen = TheWindowManager->winCreate(nullptr, WIN_STATUS_ENABLED | WIN_STATUS_ABOVE | WIN_STATUS_HIDDEN,
		0, 0, TheDisplay->getWidth(), TheDisplay->getHeight(), nullptr, nullptr);
	m_retailPowersScreen->winSetInputFunc(overlayInput);
	m_retailPowersScreen->winSetDrawFunc(drawScreen);
	m_retailPowersCounter = TheWindowManager->winCreate(palantir, WIN_STATUS_ENABLED | WIN_STATUS_HIDDEN,
		0, 0, 1, 1, nullptr, nullptr);
	m_retailPowersCounter->winSetInputFunc(counterInput);
}

void ControlBar::destroyRetailPowers()
{
	if (TheWindowManager && m_retailPowersScreen)
		TheWindowManager->winDestroy(m_retailPowersScreen);
	m_retailPowersScreen = nullptr;
	storeArt.reset();
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
}

void ControlBar::clickRetailSpellButton(const CommandButton* button)
{
	Player* player = localPlayer();
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
	const Bool valid = button->canCastRetailSpell(book, target, position);
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
	fill(windowRect(window), GameMakeColor(0, 7, 10, shade));
	const Layout layout = screenLayout();
	const Bool authored = storeArt && storeArt->valid();
	const Real movieYScale = PanelHeight * layout.scale / storeMovieLayout.stageHeight;
	Rect titleRect = layout.at(0, 6, PanelWidth, 29);
	GameFont* titleFont = nullptr;
	if (authored)
	{
		const Real sy = movieYScale;
		const Real sx = sy * 4.0f / 3.0f;
		const Real artX = layout.x + (PanelWidth * layout.scale - sx * storeMovieLayout.stageWidth) / 2.0f;
		const AptScene::Box globe = storeArt->bounds("_root.SpellStore.@1");
		if (globe.valid())
		{
			const Int width = static_cast<Int>((globe.right - globe.left) * sx * 1.49f);
			const Int height = static_cast<Int>((globe.bottom - globe.top) * sy * 1.49f);
			const Int x = static_cast<Int>(artX + (globe.left + globe.right) * sx * 0.5f);
			const Int y = static_cast<Int>(layout.y + (globe.top + globe.bottom) * sy * 0.5f);
			drawStoreArt("UiRetailPowersAura", {x - width / 2, y - height / 2, width, height}, 0xFF63D3FF);
		}
		disc(layout.at(70, 68, 420, 420), 0xA0041019);
		const auto clips = prerequisiteClips(player);
		storeArt->drawAt([&clips](const std::string& path) {
			if (path.starts_with(ConnectorPath))
			{
				auto clip = std::string_view(path).substr(ConnectorPath.size());
				clip = clip.substr(0, clip.find('.'));
				return std::find(clips.begin(), clips.end(), clip) != clips.end();
			}
			if (path.starts_with("_root.SpellStore.Buttons.ButtonsMain."))
				return path == "_root.SpellStore.Buttons.ButtonsMain.@2";
			return path.starts_with("_root.SpellStore.") &&
				!path.starts_with("_root.SpellStore.Buttons.Spell");
		}, artX, layout.y, sx, sy, 1.0f);
		storeArt->drawAt([](const std::string& path) { return path.starts_with("_root.Title."); },
			0, 0, storeArt->xScale(), sy, 1.0f);
		const AptScene::Box title = storeArt->bounds("_root.Title.Title_Text.Text");
		if (title.valid())
		{
			const Real titleXScale = storeArt->xScale();
			titleRect = {static_cast<Int>(title.left * titleXScale), static_cast<Int>(title.top * sy),
				static_cast<Int>((title.right - title.left) * titleXScale), static_cast<Int>((title.bottom - title.top) * sy)};
		}
		titleFont = TheFontLibrary->getFontPixels("Omnia LT Std", std::max(10, static_cast<Int>(32.0f * sy)), FALSE);
	}
	else
		disc(layout.at(0, 64, PanelWidth, PanelWidth - 98), 0xFF152A39);
	GameFont* font = powersFont(16);
	label(titleFont ? titleFont : powersFont(26, TRUE), titleRect, UnicodeString(L"POWERS"), 0xFFC2E6EE);
	UnicodeString points;
	points.format(L"%d", player->getSciencePurchasePoints() - pendingCost(player, m_retailPendingSciences));
	label(font, layout.at(0, 92, PanelWidth, 28),
		TheGameText->fetchOrSubstitute("APT:SpellStoreSpellPointsLabel", L"Power points to spend:"), Ink);
	label(font, layout.at(0, 116, PanelWidth, 24), points, Ink);
	const Rect close = layout.at(515, 8, 35, 29);
	label(font, close, UnicodeString(L"×"), Ink);

	const Int diameter = std::max(2, static_cast<Int>(72 * movieYScale));
	const Int badgeSize = std::max(12, static_cast<Int>(26 * movieYScale + 0.5f));
	GameFont* costFont = TheFontLibrary->getFontPixels("Albertus MT", std::max(10, static_cast<Int>(13 * movieYScale + 0.5f)), FALSE);
	for (Int i = 0; i < StoreSlots; ++i)
	{
		const ScienceType science = storeScience(player, i);
		if (science == SCIENCE_INVALID)
			continue;
		const Bool pending = containsScience(m_retailPendingSciences, science);
		const Bool owned = player->hasScience(science);
		const Bool available = canQueueScience(player, science, m_retailPendingSciences);
		const Rect tile = layout.node(i);
		const Rect portrait = {tile.x + (tile.w - diameter) / 2, tile.y + (tile.h - diameter) / 2, diameter, diameter};
		const Int border = std::max(2, diameter / 16);
		disc(portrait, 0xFF0A1720);
		const Rect inset = {portrait.x + border, portrait.y + border, diameter - border * 2, diameter - border * 2};
		TheDisplay->setCircularClip(inset.x + inset.w / 2, inset.y + inset.h / 2, inset.w / 2);
		icon(storeButton(player, i), inset, owned || available || pending ? 0xFFFFFFFF : 0xFF626C75);
		TheDisplay->setCircularClip(0, 0, 0);
		drawStoreArt("UiRetailPowersNode", portrait, owned || pending ? Cyan : available ? 0xFFFFFFFF : 0xAA8699A6);
		const Rect badge = {tile.x + (tile.w - badgeSize) / 2,
			tile.y + (tile.h - badgeSize) / 2 + static_cast<Int>(35.05f * movieYScale), badgeSize, badgeSize};
		if (!drawStoreArt("UiRetailPowersCost", badge))
		{
			disc(badge, available || owned || pending ? 0xFF8CAABD : PanelLine);
			disc({badge.x + 1, badge.y + 1, badge.w - 2, badge.h - 2}, 0xFF142A38);
		}
		UnicodeString cost;
		cost.format(L"%d", player->getSciencePurchaseCost(science));
		label(costFont, badge, cost, owned ? Owned : Ink);
	}
	if (const CommandButton* button = m_retailHoveredPower >= 0 ? storeButton(player, m_retailHoveredPower) : nullptr)
	{
		UnicodeString title = TheGameText->fetchOrSubstitute(button->getTextLabel().str(), L"");
		label(font, layout.at(0, 464, PanelWidth, 25), title, Ink);
	}
	const Bool canCommit = !m_retailPendingSciences.empty();
	const Rect reset = layout.action(storeMovieLayout.reset, FALSE);
	const Rect accept = layout.action(storeMovieLayout.accept, TRUE);
	const char* plate = canCommit ? "UiRetailPowersActionActive" : "UiRetailPowersAction";
	if (!drawStoreArt(plate, reset))
		frame(reset, PanelLine);
	if (!drawStoreArt(plate, accept))
		frame(accept, canCommit ? Cyan : PanelLine);
	label(font, reset, UnicodeString(L"Reset"), canCommit ? Ink : Cyan);
	label(font, accept, UnicodeString(L"Accept"), canCommit ? Ink : Cyan);
}

void ControlBar::inputRetailPowers(Int x, Int y, Bool click)
{
	Player* player = localPlayer();
	if (!player)
		return;
	const Layout layout = screenLayout();
	m_retailHoveredPower = -1;
	for (Int i = 0; i < StoreSlots; ++i)
		if (layout.hits(storeMovieLayout.nodes[i], layout.node(i), x, y))
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
	const Rect reset = layout.action(storeMovieLayout.reset, FALSE);
	const Rect accept = layout.action(storeMovieLayout.accept, TRUE);
	if (reset.contains(x, y))
		m_retailPendingSciences.clear();
	else if (accept.contains(x, y))
	{
		for (ScienceType science : m_retailPendingSciences)
		{
			GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_PURCHASE_SCIENCE);
			msg->appendIntegerArgument(science);
		}
		m_retailPendingSciences.clear();
	}
}
