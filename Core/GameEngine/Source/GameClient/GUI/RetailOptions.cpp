#include "PreRTS.h"

#include "GameClient/RetailOptions.h"

#include "Common/AudioAffect.h"
#include "Common/AudioSettings.h"
#include "Common/GameAudio.h"
#include "Common/GameLOD.h"
#include "Common/GlobalData.h"
#include "Common/OptionPreferences.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/HeaderTemplate.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Keyboard.h"
#include "GameClient/Mouse.h"
#include "GameClient/RetailWorldUI.h"
#include "GameClient/Shell.h"

#include <algorithm>
#include <array>
#include <tuple>
#include <vector>

namespace
{
struct Box
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

struct Layout
{
	const Real scale = std::min(TheDisplay->getWidth() / 1024.0f, TheDisplay->getHeight() / 768.0f);
	const Int x = (Int(TheDisplay->getWidth()) - Int(1024 * scale)) / 2;
	const Int y = (Int(TheDisplay->getHeight()) - Int(768 * scale)) / 2;

	Box pixels(Box box) const
	{
		return {x + Int(box.x * scale), y + Int(box.y * scale), Int(box.width * scale), Int(box.height * scale)};
	}
};

struct Label
{
	const char *key;
	const WideChar *fallback;

	UnicodeString text() const { return TheGameText->fetchOrSubstitute(key, fallback); }
};

enum Page { Video, Audio, Gameplay, Controls, PageCount };
enum Setting { Brightness, Resolution, Quality, Music, Sound, Voice, Ambient, Movie, HealthBars, Scroll, SettingCount };
enum Widget { Slider, Toggle, Stepper };

struct Row
{
	Page page;
	Setting setting;
	Label name;
	Widget widget;
};

constexpr Label titles[] = {
	{"GUI:VideoOptions", L"Video"}, {"GUI:AudioOptions", L"Audio"},
	{"GUI:GameplayOptions", L"Gameplay"}, {"APT:Controls", L"Controls"}
};

constexpr Row rows[] = {
	{Video, Brightness, {"APT:Brightness", L"Brightness"}, Slider},
	{Video, Resolution, {"APT:Resolution", L"Resolution"}, Stepper},
	{Video, Quality, {"APT:Graphics", L"Graphics quality"}, Stepper},
	{Audio, Music, {"APT:Music", L"Music"}, Slider},
	{Audio, Sound, {"APT:SoundFX", L"Sound effects"}, Slider},
	{Audio, Voice, {"APT:Voice", L"Voice"}, Slider},
	{Audio, Ambient, {"APT:Ambient", L"Ambient"}, Slider},
	{Audio, Movie, {"APT:Movie", L"Movie"}, Slider},
	{Gameplay, HealthBars, {"APT:ShowAllHealthBars", L"Show all health bars"}, Toggle},
	{Controls, Scroll, {"APT:ScrollSpeed", L"Scroll speed"}, Slider}
};

constexpr Color panelColor = 0xFF15232A;
constexpr Color sidebarColor = 0xFF10191F;
constexpr Color rowColor = 0xFF1D2E36;
constexpr Color gold = 0xFFC8AE77;
constexpr Color textColor = 0xFFF1EDE3;
constexpr Color muted = 0xFF9EADB1;
constexpr Box panel{132, 104, 760, 560};
constexpr Box resetButton{160, 600, 190, 42};
constexpr Box cancelButton{457, 600, 128, 42};
constexpr Box acceptButton{738, 600, 140, 42};

Box tabBox(Int page) { return {136, 196 + 54 * page, 201, 46}; }
Box rowBox(Int row) { return {369, 234 + 52 * row, 497, 48}; }
Box sliderBox(Box row) { return {610, row.y + 17, 201, 18}; }

using Values = std::array<Int, SettingCount>;
using Mode = std::tuple<Int, Int, Int>;

Int percent(Real value) { return std::clamp(Int(value + 0.5f), 0, 100); }

void setBrightness(Real gamma)
{
	TheWritableGlobalData->m_displayGamma = gamma;
	TheDisplay->setGamma(gamma, 0.0f, 1.0f, FALSE);
}

AudioAffect systemSetting(AudioAffect channel)
{
	return static_cast<AudioAffect>(channel | AudioAffect_SystemSetting);
}

struct Options
{
	GameWindow *returnWindow;
	Bool inMatch;
	Page page = Video;
	Setting dragging = SettingCount;
	Int focus = 0;
	Bool keyboard = FALSE;
	Bool previewing = FALSE;
	Bool saveFailed = FALSE;
	Values values{};
	Values original{};
	std::vector<Mode> modes;
	std::array<Real, 4> originalVolumes{};
	const Real originalAmbient = TheAudio->getAmbientVolume();
	const Real originalMovie = TheAudio->getMovieVolume();
	const Real originalGamma = TheGlobalData->m_displayGamma;

	Options(GameWindow *parent, Bool match) : returnWindow(parent), inMatch(match)
	{
		const Mode current{TheDisplay->getWidth(), TheDisplay->getHeight(), TheDisplay->getBitDepth()};
		modes.push_back(current);
		if (!inMatch)
		{
			for (Int i = 0; i < TheDisplay->getDisplayModeCount(); ++i)
			{
				Int width, height, depth;
				TheDisplay->getDisplayModeDescription(i, &width, &height, &depth);
				if (depth == std::get<2>(current) && std::ranges::find(modes, Mode{width, height, depth}) == modes.end())
					modes.emplace_back(width, height, depth);
			}
			std::sort(modes.begin(), modes.end());
		}
		OptionPreferences prefs;
		values[Brightness] = percent(prefs.getGammaValue());
		values[Resolution] = Int(std::ranges::find(modes, current) - modes.begin());
		values[Quality] = std::clamp(Int(TheGameLODManager->getStaticLODLevel()), 0, Int(STATIC_GAME_LOD_CUSTOM));
		values[Music] = percent(prefs.getMusicVolume());
		values[Sound] = percent(std::max(prefs.getSoundVolume(), prefs.get3DSoundVolume()));
		values[Voice] = percent(prefs.getSpeechVolume());
		values[Ambient] = originalAmbient < 0.0f ? values[Sound] : percent(originalAmbient * 100.0f);
		values[Movie] = percent(originalMovie * 100.0f);
		values[HealthBars] = RetailWorldUI::showAllHealthBars;
		values[Scroll] = percent(TheGlobalData->m_retailScrollFactor * 50.0f);
		original = values;
		originalVolumes = {TheAudio->getVolume(systemSetting(AudioAffect_Music)),
			TheAudio->getVolume(systemSetting(AudioAffect_Sound)), TheAudio->getVolume(systemSetting(AudioAffect_Sound3D)),
			TheAudio->getVolume(systemSetting(AudioAffect_Speech))};
	}

	Bool enabled(Setting setting) const
	{
		return (!inMatch || (setting != Resolution && setting != Quality)) &&
			(setting != Resolution || modes.size() > 1) &&
			(setting != Quality || TheGlobalData->m_enableStaticLOD);
	}

	Real effectsVolume(AudioAffect channel) const
	{
		const Real relative = std::clamp(TheAudio->getAudioSettings()->m_relative2DVolume, -1.0f, 1.0f);
		const Real attenuation = channel == AudioAffect_Sound ? std::min(relative, 0.0f) : -std::max(relative, 0.0f);
		return values[Sound] * 0.01f * (1.0f + attenuation);
	}

	void preview(Setting setting)
	{
		const Bool edited = values[setting] != original[setting];
		switch (setting)
		{
			case Brightness:
			{
				const Int value = values[Brightness];
				setBrightness(edited ? 1.0f + (value - 50) * (value < 50 ? 0.008f : 0.02f) : originalGamma);
				break;
			}
			case Music:
				TheAudio->setVolume(edited ? values[Music] * 0.01f : originalVolumes[0], systemSetting(AudioAffect_Music));
				break;
			case Sound:
				TheAudio->setVolume(edited ? effectsVolume(AudioAffect_Sound) : originalVolumes[1], systemSetting(AudioAffect_Sound));
				TheAudio->setVolume(edited ? effectsVolume(AudioAffect_Sound3D) : originalVolumes[2], systemSetting(AudioAffect_Sound3D));
				break;
			case Voice:
				TheAudio->setVolume(edited ? values[Voice] * 0.01f : originalVolumes[3], systemSetting(AudioAffect_Speech));
				break;
			case Ambient: TheAudio->setAmbientVolume(edited ? values[Ambient] * 0.01f : originalAmbient); break;
			case Movie: TheAudio->setMovieVolume(edited ? values[Movie] * 0.01f : originalMovie); break;
			default: return;
		}
		previewing = TRUE;
	}

	void undoPreview()
	{
		if (!previewing)
			return;
		setBrightness(originalGamma);
		TheAudio->setVolume(originalVolumes[0], systemSetting(AudioAffect_Music));
		TheAudio->setVolume(originalVolumes[1], systemSetting(AudioAffect_Sound));
		TheAudio->setVolume(originalVolumes[2], systemSetting(AudioAffect_Sound3D));
		TheAudio->setVolume(originalVolumes[3], systemSetting(AudioAffect_Speech));
		TheAudio->setAmbientVolume(originalAmbient);
		TheAudio->setMovieVolume(originalMovie);
		previewing = FALSE;
	}

	void reset()
	{
		const AudioSettings *audio = TheAudio->getAudioSettings();
		values[Brightness] = 50;
		values[Music] = percent(audio->m_defaultMusicVolume * 100.0f);
		values[Sound] = percent(std::max(audio->m_defaultSoundVolume, audio->m_default3DSoundVolume) * 100.0f);
		values[Voice] = percent(audio->m_defaultSpeechVolume * 100.0f);
		values[Ambient] = values[Sound];
		values[Movie] = 100;
		values[HealthBars] = FALSE;
		values[Scroll] = percent(TheGlobalData->m_retailDefaultScrollFactor * 50.0f);
		for (Setting setting : {Brightness, Music, Sound, Voice, Ambient, Movie})
			preview(setting);
	}

	Bool accept()
	{
		OptionPreferences prefs;
		const auto changed = [&](Setting setting) { return values[setting] != original[setting]; };
		if (changed(Brightness)) prefs.setInt("Gamma", values[Brightness]);
		if (changed(Music)) prefs.setInt("MusicVolume", values[Music]);
		if (changed(Sound))
		{
			prefs.setInt("SFXVolume", percent(effectsVolume(AudioAffect_Sound) * 100.0f));
			prefs.setInt("SFX3DVolume", percent(effectsVolume(AudioAffect_Sound3D) * 100.0f));
		}
		if (changed(Voice)) prefs.setInt("VoiceVolume", values[Voice]);
		if (changed(Ambient)) prefs.setInt("AmbientVolume", values[Ambient]);
		if (changed(Movie)) prefs.setInt("MovieVolume", values[Movie]);
		if (changed(HealthBars)) prefs.setBool("ShowAllHealthBars", values[HealthBars]);
		if (changed(Scroll)) prefs.setInt("ScrollFactor", values[Scroll]);
		if (changed(Quality))
			prefs.setAsciiString("RetailStaticGameLOD",
				TheGameLODManager->getStaticGameLODLevelName(StaticGameLODLevel(values[Quality])));

		const Bool resize = changed(Resolution);
		if (resize)
		{
			const auto [width, height, depth] = modes[values[Resolution]];
			if (!TheDisplay->setDisplayMode(width, height, depth, TheDisplay->getWindowed()))
				return FALSE;
			AsciiString resolution;
			resolution.format("%d %d", width, height);
			prefs.setAsciiString("Resolution", resolution);
		}
		if (values != original && !prefs.write())
		{
			if (resize)
			{
				const auto [width, height, depth] = modes[original[Resolution]];
				TheDisplay->setDisplayMode(width, height, depth, TheDisplay->getWindowed());
			}
			return FALSE;
		}
		if (changed(Scroll)) prefs.applyScrollFactor();
		if (changed(HealthBars)) RetailWorldUI::showAllHealthBars = values[HealthBars];
		if (changed(Quality)) TheGameLODManager->setStaticLODLevel(StaticGameLODLevel(values[Quality]));
		previewing = FALSE;
		return TRUE;
	}
};

GameWindow *activeOptions = nullptr;

class Painter
{
public:
	explicit Painter(const Layout &layout) : m_layout(layout), m_text(TheDisplayStringManager->newDisplayString()) {}
	~Painter() { TheDisplayStringManager->freeDisplayString(m_text); }

	void fill(Box box, Color color) const
	{
		box = m_layout.pixels(box);
		TheDisplay->drawFillRect(box.x, box.y, box.width, box.height, color);
	}

	void text(Box box, const UnicodeString &value, Int pixels, Color color = textColor, Bool centered = FALSE) const
	{
		box = m_layout.pixels(box);
		GameFont *font = TheFontLibrary->getFontPixels("Albertus MT", std::max(1, Int(pixels * m_layout.scale)), FALSE);
		if (!font)
			return;
		m_text->setFont(font);
		m_text->setText(value);
		Int width, height;
		m_text->getSize(&width, &height);
		IRegion2D clip{{box.x, box.y}, {box.x + box.width, box.y + box.height}};
		m_text->setClipRegion(&clip);
		m_text->draw(box.x + (centered ? std::max(0, (box.width - width) / 2) : 0),
			box.y + (box.height - height) / 2, color, 0);
	}

private:
	Layout m_layout;
	DisplayString *m_text;
};

UnicodeString rowValue(const Options &options, Setting setting)
{
	UnicodeString value;
	if (setting == Resolution)
	{
		const auto [width, height, depth] = options.modes[options.values[Resolution]];
		value.format(L"%d × %d", width, height);
	}
	else if (setting == Quality)
	{
		constexpr Label quality[] = {
			{"GUI:Low", L"Low"}, {"GUI:Medium", L"Medium"}, {"GUI:High", L"High"},
			{"GUI:VeryHigh", L"Ultra High"}, {"GUI:Custom", L"Custom"}
		};
		value = quality[options.values[Quality]].text();
	}
	else
		value.format(L"%d%%", options.values[setting]);
	return value;
}

void drawRow(const Painter &p, const Options &options, const Row &row, Box box, Bool focused)
{
	const Bool enabled = options.enabled(row.setting);
	p.fill(box, rowColor);
	if (focused)
		p.fill({box.x, box.y, 2, box.height}, gold);
	p.text({box.x + 16, box.y, 216, box.height}, row.name.text(), 17, enabled ? textColor : muted);
	if (row.widget == Slider)
	{
		const Box track = sliderBox(box);
		const Int length = (track.width - 1) * options.values[row.setting] / 100;
		p.fill({track.x, track.y + 7, track.width, 4}, sidebarColor);
		if (length > 0)
			p.fill({track.x, track.y + 7, length, 4}, gold);
		p.fill({track.x + length - 3, track.y + 1, 7, 16}, gold);
		p.text({821, box.y, 43, box.height}, rowValue(options, row.setting), 14, muted, TRUE);
	}
	else if (row.widget == Toggle)
	{
		p.fill({600, box.y + 14, 20, 20}, sidebarColor);
		if (options.values[row.setting])
			p.fill({604, box.y + 18, 12, 12}, gold);
		p.text({630, box.y, 159, box.height}, options.values[row.setting] ?
			Label{"GUI:On", L"On"}.text() : Label{"GUI:Off", L"Off"}.text(), 16);
	}
	else
	{
		p.text({617, box.y, 201, box.height}, rowValue(options, row.setting), 17, enabled ? textColor : muted, TRUE);
		if (enabled)
		{
			p.text({594, box.y, 30, box.height}, UnicodeString(L"‹"), 25, gold, TRUE);
			p.text({817, box.y, 30, box.height}, UnicodeString(L"›"), 25, gold, TRUE);
		}
	}
}

void drawOptions(GameWindow *window, WinInstanceData *)
{
	const auto &options = *static_cast<Options *>(window->winGetUserData());
	const Int width = TheDisplay->getWidth();
	const Int height = TheDisplay->getHeight();
	Int previousWidth, previousHeight;
	window->winGetSize(&previousWidth, &previousHeight);
	if (previousWidth != width || previousHeight != height)
		window->winSetSize(width, height);
	TheDisplay->drawFillRect(0, 0, width, height, 0xAE060C10);
	const Painter p(Layout{});
	p.fill(panel, panelColor);
	p.fill({132, 145, 207, 431}, sidebarColor);
	p.fill({132, 145, 760, 2}, gold);
	p.text({156, 114, 690, 27}, Label{"APT:Options", L"Options"}.text(), 24);
	for (Int i = 0; i < PageCount; ++i)
	{
		Box tab = tabBox(i);
		if (options.page == i)
		{
			p.fill(tab, rowColor);
			p.fill({tab.x, tab.y, 3, tab.height}, gold);
		}
		p.text({tab.x + 22, tab.y, tab.width - 22, tab.height}, titles[i].text(),
			18, options.page == i ? textColor : muted);
	}
	p.text({369, 175, 497, 43}, titles[options.page].text(), 26);
	Int index = 0;
	for (const Row &row : rows)
		if (row.page == options.page)
		{
			drawRow(p, options, row, rowBox(index), options.keyboard && options.focus == index);
			++index;
		}
	if (options.page == Video && options.inMatch)
		p.text({383, 391, 473, 38}, Label{"GUI:DisplayChangeShellOnly", L"Change resolution and graphics quality from the main menu."}.text(), 15, muted);
	if (options.page == Audio)
		p.text({383, 500, 473, 38}, Label{"GUI:LegacyAudioUnavailable", L"EAX3 and High Audio Quality require the original audio driver."}.text(), 15, muted);
	if (options.saveFailed)
		p.text({370, 523, 493, 45}, Label{"GUI:SettingsApplyFailed", L"Could not apply settings."}.text(), 16, gold);
	p.fill({152, 579, 720, 1}, 0xFF4B5C60);
	p.fill(resetButton, rowColor);
	p.fill(cancelButton, rowColor);
	p.fill(acceptButton, gold);
	p.text(resetButton, Label{"APT:ResetToDefaults", L"Reset to defaults"}.text(), 16, textColor, TRUE);
	p.text(cancelButton, Label{"APT:Cancel", L"Cancel"}.text(), 17, textColor, TRUE);
	p.text(acceptButton, Label{"APT:Accept", L"Accept"}.text(), 17, sidebarColor, TRUE);
}

void closeOptions(GameWindow *window, Bool accepted, Bool restoreFocus)
{
	auto &options = *static_cast<Options *>(window->winGetUserData());
	if (!accepted)
		options.undoPreview();
	GameWindow *parent = options.returnWindow;
	const Bool inMatch = options.inMatch;
	activeOptions = nullptr;
	TheWindowManager->winDestroy(window);
	if (restoreFocus && parent && !(parent->winGetStatus() & WIN_STATUS_DESTROYED))
	{
		if (inMatch)
		{
			parent->winHide(FALSE);
			TheWindowManager->winSetModal(parent);
		}
		TheWindowManager->winSetFocus(parent);
	}
}

void acceptOptions(GameWindow *window, Options &options)
{
	if (!options.accept())
	{
		options.saveFailed = TRUE;
		return;
	}
	const Bool resize = options.values[Resolution] != options.original[Resolution];
	closeOptions(window, TRUE, TRUE);
	if (!resize)
		return;
	TheWritableGlobalData->m_xResolution = TheDisplay->getWidth();
	TheWritableGlobalData->m_yResolution = TheDisplay->getHeight();
	TheHeaderTemplateManager->onResolutionChanged();
	TheMouse->onResolutionChanged();
	TheShell->onResolutionChanged();
	TheInGameUI->recreateControlBar();
	TheInGameUI->refreshCustomUiResources();
}

void adjust(Options &options, const Row &row, Int step)
{
	if (!options.enabled(row.setting))
		return;
	Int &value = options.values[row.setting];
	if (row.widget == Toggle)
		value = !value;
	else if (row.widget == Slider)
		value = std::clamp(value + step, 0, 100);
	else
	{
		const Int count = row.setting == Resolution ? Int(options.modes.size()) : Int(STATIC_GAME_LOD_COUNT);
		value = (value + step + count) % count;
	}
	options.preview(row.setting);
}

void dragSlider(Options &options, Setting setting, Int x)
{
	const Box track = sliderBox(rowBox(0));
	const Int value = percent((x - track.x) * 100.0f / (track.width - 1));
	if (value != options.values[setting])
	{
		options.values[setting] = value;
		options.preview(setting);
	}
}

WindowMsgHandledType optionsInput(GameWindow *window, UnsignedInt message, WindowMsgData point, WindowMsgData state)
{
	auto &options = *static_cast<Options *>(window->winGetUserData());
	if (message == GWM_CHAR && (state & KEY_STATE_UP))
	{
		options.keyboard = TRUE;
		if (point == KEY_ESC)
			closeOptions(window, FALSE, TRUE);
		else if (point == KEY_ENTER)
			acceptOptions(window, options);
		else if (point == KEY_TAB)
		{
			options.page = Page((options.page + 1) % PageCount);
			options.focus = 0;
		}
		else
		{
			Int count = 0;
			for (const Row &row : rows)
			{
				if (row.page != options.page)
					continue;
				if (options.focus == count && (point == KEY_LEFT || point == KEY_RIGHT || point == KEY_SPACE))
					adjust(options, row, point == KEY_LEFT ? -1 : 1);
				++count;
			}
			if (point == KEY_DOWN || point == KEY_UP)
				options.focus = (options.focus + (point == KEY_DOWN ? 1 : -1) + count) % count;
		}
		return MSG_HANDLED;
	}

	const Layout layout;
	const Int x = Int((Int(point & 0xFFFF) - layout.x) / layout.scale);
	const Int y = Int((Int(point >> 16) - layout.y) / layout.scale);
	if (options.dragging != SettingCount && (message == GWM_MOUSE_POS || message == GWM_LEFT_DRAG || message == GWM_LEFT_UP))
	{
		dragSlider(options, options.dragging, x);
		if (message == GWM_LEFT_UP)
		{
			options.dragging = SettingCount;
			TheWindowManager->winRelease(window);
		}
		return MSG_HANDLED;
	}
	if (message == GWM_LEFT_DOWN)
	{
		options.keyboard = FALSE;
		Int index = 0;
		for (const Row &row : rows)
		{
			if (row.page != options.page)
				continue;
			if (row.widget == Slider && sliderBox(rowBox(index)).contains(x, y))
			{
				options.dragging = row.setting;
				TheWindowManager->winCapture(window);
				dragSlider(options, row.setting, x);
				break;
			}
			++index;
		}
	}
	else if (message == GWM_LEFT_UP)
	{
		if (acceptButton.contains(x, y))
			acceptOptions(window, options);
		else if (cancelButton.contains(x, y))
			closeOptions(window, FALSE, TRUE);
		else if (resetButton.contains(x, y))
			options.reset();
		else
		{
			for (Int i = 0; i < PageCount; ++i)
				if (tabBox(i).contains(x, y))
				{
					options.page = Page(i);
					options.focus = 0;
					return MSG_HANDLED;
				}
			Int index = 0;
			for (const Row &row : rows)
				if (row.page == options.page && rowBox(index++).contains(x, y) && row.widget != Slider)
					adjust(options, row, x < 714 ? -1 : 1);
		}
	}
	return MSG_HANDLED;
}

WindowMsgHandledType optionsSystem(GameWindow *window, UnsignedInt message, WindowMsgData state, WindowMsgData data)
{
	if (message == GWM_INPUT_FOCUS && state == TRUE)
	{
		*reinterpret_cast<Bool *>(data) = TRUE;
		return MSG_HANDLED;
	}
	if (message == GWM_DESTROY)
	{
		auto *options = static_cast<Options *>(window->winGetUserData());
		if (activeOptions == window)
		{
			activeOptions = nullptr;
			options->undoPreview();
		}
		delete options;
		window->winSetUserData(nullptr);
	}
	return MSG_IGNORED;
}
}

Bool openRetailOptions(GameWindow *returnWindow, Bool inMatch)
{
	if (activeOptions)
		return TRUE;
	WinInstanceData data;
	data.m_style = GWS_USER_WINDOW;
	GameWindow *window = TheWindowManager->winCreate(nullptr, WIN_STATUS_ENABLED | WIN_STATUS_ABOVE, 0, 0,
		TheDisplay->getWidth(), TheDisplay->getHeight(), nullptr, &data);
	if (!window)
		return FALSE;
	window->winSetUserData(new Options(returnWindow, inMatch));
	window->winSetDrawFunc(drawOptions);
	window->winSetInputFunc(optionsInput);
	window->winSetSystemFunc(optionsSystem);
	if (inMatch && returnWindow)
		returnWindow->winHide(TRUE);
	TheWindowManager->winSetModal(window);
	TheWindowManager->winSetFocus(window);
	activeOptions = window;
	return TRUE;
}

void closeRetailOptionsFor(GameWindow *returnWindow)
{
	if (activeOptions && static_cast<Options *>(activeOptions->winGetUserData())->returnWindow == returnWindow)
		closeOptions(activeOptions, FALSE, FALSE);
}
