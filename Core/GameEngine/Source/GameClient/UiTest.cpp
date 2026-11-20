#include "PreRTS.h"

#include "GameClient/UiTest.h"

#include "Common/GameEngine.h"
#include "Common/GlobalData.h"
#include "GameClient/Display.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/UiLayoutDump.h"
#include "GameClient/UiScale.h"
#include "GameClient/WindowLayout.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
	enum class Stage
	{
		WaitingForShell,
		Created,
		Done
	};

	constexpr Int CaptureFramesAfterCreation = 1;

	struct State
	{
		Bool active = FALSE;
		std::string layout;
		Stage stage = Stage::WaitingForShell;
		Int framesSinceCreation = 0;
		Bool captured = FALSE;
		Bool layoutLoaded = FALSE;
	};

	State& state()
	{
		static State instance;
		return instance;
	}

	const char* environment(const char* name)
	{
		const char* value = std::getenv(name);
		return value && *value ? value : nullptr;
	}

	const char* windowType(UnsignedInt style)
	{
		static const struct { UnsignedInt bit; const char* name; } kTypes[] = {
			{ GWS_PUSH_BUTTON, "PUSHBUTTON" },
			{ GWS_RADIO_BUTTON, "RADIOBUTTON" },
			{ GWS_CHECK_BOX, "CHECKBOX" },
			{ GWS_VERT_SLIDER, "VERTSLIDER" },
			{ GWS_HORZ_SLIDER, "HORZSLIDER" },
			{ GWS_SCROLL_LISTBOX, "SCROLLLISTBOX" },
			{ GWS_ENTRY_FIELD, "ENTRYFIELD" },
			{ GWS_STATIC_TEXT, "STATICTEXT" },
			{ GWS_PROGRESS_BAR, "PROGRESSBAR" },
			{ GWS_COMBO_BOX, "COMBOBOX" },
			{ GWS_TAB_CONTROL, "TABCONTROL" },
			{ GWS_TAB_PANE, "TABPANE" },
			{ GWS_USER_WINDOW, "USER" },
		};
		for (const auto& type : kTypes)
		{
			if (BitIsSet(style, type.bit))
				return type.name;
		}
		return "UNKNOWN";
	}

	std::string windowLabel(GameWindow* window)
	{
		const WinInstanceData* data = window->winGetInstanceData();
		if (data && data->m_decoratedNameString.isNotEmpty())
			return data->m_decoratedNameString.str();
		return std::string("#") + windowType(window->winGetStyle());
	}

	std::string layoutFileOf(GameWindow* window)
	{
		while (window->winGetParent())
			window = window->winGetParent();
		WindowLayout* layout = window->winGetLayout();
		return layout ? std::string(layout->getFilename().str()) : std::string();
	}

	void collect(GameWindow* window, const std::string& parentPath, Bool parentVisible, std::vector<UiWindowRecord>& out)
	{
		for (; window; window = window->winGetNext())
		{
			UiWindowRecord record;
			record.name = windowLabel(window);
			record.type = windowType(window->winGetStyle());
			record.parentPath = parentPath;
			record.layoutFile = layoutFileOf(window);
			window->winGetScreenPosition(&record.screen[0], &record.screen[1]);
			window->winGetSize(&record.screen[2], &record.screen[3]);

			IRegion2D region;
			ICoord2D resolution;
			record.hasLayoutRect = window->winGetLayoutRegion(&region, &resolution);
			if (record.hasLayoutRect)
			{
				record.layoutRect[0] = region.lo.x;
				record.layoutRect[1] = region.lo.y;
				record.layoutRect[2] = region.hi.x;
				record.layoutRect[3] = region.hi.y;
				record.layoutResolution[0] = resolution.x;
				record.layoutResolution[1] = resolution.y;
			}

			record.hidden = window->winIsHidden() ? true : false;
			record.visible = parentVisible && !record.hidden;

			if (GameFont* font = window->winGetFont())
			{
				record.hasFont = true;
				record.fontName = font->nameString.str();
				record.fontPixels = font->pixelSize > 0;
				record.fontSize = record.fontPixels ? font->pixelSize : font->pointSize;
				record.fontBold = font->bold ? true : false;
				record.fontHeight = font->height;
			}

			const std::string path = parentPath.empty() ? record.name : parentPath + "/" + record.name;
			const Bool visible = record.visible ? TRUE : FALSE;
			out.push_back(std::move(record));
			collect(window->winGetChild(), path, visible, out);
		}
	}

	void createSpecimen(const char* spec)
	{
		const std::vector<UiSpecimenLine> lines = parseUiSpecimen(spec);
		if (lines.empty())
			return;

		const Int width = TheDisplay->getWidth();
		const Int height = TheDisplay->getHeight();
		const UiScale::PixelRect canvas = UiScale::canvasToScreen(0, 0, UiScale::DesignWidth, UiScale::DesignHeight, width, height);

		WinInstanceData rootData;
		rootData.m_style = GWS_USER_WINDOW;
		rootData.m_decoratedNameString = "UiTest:Specimen";
		GameWindow* root = TheWindowManager->winCreate(nullptr, WIN_STATUS_ENABLED,
			canvas.x0, canvas.y0, canvas.x1 - canvas.x0, canvas.y1 - canvas.y0, nullptr, &rootData);
		if (root == nullptr)
			return;
		IRegion2D rootRegion = { { 0, 0 }, { UiScale::DesignWidth, UiScale::DesignHeight } };
		ICoord2D design = { UiScale::DesignWidth, UiScale::DesignHeight };
		root->winSetLayoutRegion(rootRegion, design);

		Int top = 48;
		Int index = 0;
		for (const UiSpecimenLine& line : lines)
		{
			const Int bottom = top + line.designSize * 2;
			const UiScale::PixelRect rect = UiScale::canvasToScreen(32, top, 992, bottom, width, height);

			WinInstanceData textData;
			textData.m_style = GWS_STATIC_TEXT;
			textData.m_decoratedNameString.format("UiTest:Specimen%d", index);
			TextData text = {};
			text.centeredVertically = TRUE;

			GameFont* font = TheFontLibrary->getUiFont(line.font.c_str(), line.designSize, FALSE, width, height);
			GameWindow* window = TheWindowManager->gogoGadgetStaticText(root, WIN_STATUS_ENABLED,
				rect.x0 - canvas.x0, rect.y0 - canvas.y0, rect.x1 - rect.x0, rect.y1 - rect.y0,
				&textData, &text, font, TRUE);
			if (window)
			{
				if (font)
					GadgetStaticTextSetFont(window, font);
				window->winSetEnabledTextColors(GameMakeColor(232, 222, 196, 255), GameMakeColor(0, 0, 0, 255));
				GadgetStaticTextSetEnabledColor(window, WIN_COLOR_UNDEFINED);
				GadgetStaticTextSetEnabledBorderColor(window, WIN_COLOR_UNDEFINED);
				UnicodeString value;
				value.translate(line.text.c_str());
				GadgetStaticTextSetText(window, value);
				IRegion2D region = { { 32, top }, { 992, bottom } };
				window->winSetLayoutRegion(region, design);
			}
			std::fprintf(stderr, "UI test: specimen %d '%s' %d units -> %s %d%s\n", index, line.font.c_str(), line.designSize,
				font ? font->nameString.str() : "(none)", font ? font->pixelSize : 0, font ? "px" : "");
			top = bottom + 8;
			++index;
		}
	}

	void createWindows()
	{
		TheWritableGlobalData->m_breakTheMovie = FALSE;
		State& current = state();
		if (!current.layout.empty())
		{
			WindowLayout* layout = TheWindowManager->winCreateLayout(current.layout.c_str());
			current.layoutLoaded = layout != nullptr;
			if (layout)
			{
				layout->hide(FALSE);
				layout->bringForward();
			}
			std::fprintf(stderr, "UI test: layout '%s' %s\n", current.layout.c_str(), layout ? "created" : "could not be loaded");
		}
		if (const char* spec = environment("GENERALSX_UITEST_SPECIMEN"))
			createSpecimen(spec);
		std::fflush(stderr);
	}

	void writeDump()
	{
		const char* path = environment("GENERALSX_UITEST_JSON");
		if (!path)
			return;

		std::vector<UiWindowRecord> windows;
		collect(TheWindowManager->winGetWindowList(), std::string(), TRUE, windows);

		UiLayoutSummary summary;
		summary.width = TheDisplay->getWidth();
		summary.height = TheDisplay->getHeight();
		summary.scale = UiScale::scale(summary.width, summary.height);
		summary.layout = state().layout;
		const std::string json = formatUiLayoutJson(summary, std::move(windows));

		FILE* file = std::fopen(path, "wb");
		if (!file)
		{
			std::fprintf(stderr, "UI test: cannot write '%s'\n", path);
			return;
		}
		std::fwrite(json.data(), 1, json.size(), file);
		std::fclose(file);
		std::fprintf(stderr, "UI test: wrote %s\n", path);
	}
}

void UiTest::enable(const char* layout)
{
	state().active = TRUE;
	state().layout = layout ? layout : "";
}

Bool UiTest::isActive()
{
	return state().active;
}

void UiTest::update()
{
	State& current = state();
	if (!current.active)
		return;

	if (current.stage == Stage::WaitingForShell)
	{
		createWindows();
		current.stage = Stage::Created;
		current.framesSinceCreation = 0;
		return;
	}
	if (current.stage == Stage::Created)
		++current.framesSinceCreation;
}

const char* UiTest::capturePath()
{
	const State& current = state();
	if (!current.active || current.stage != Stage::Created || current.captured ||
		current.framesSinceCreation < CaptureFramesAfterCreation || TheGlobalData->m_headless)
		return nullptr;
	return environment("GENERALSX_UITEST_PNG");
}

void UiTest::markCaptured()
{
	state().captured = TRUE;
	std::fprintf(stderr, "UI test: captured %s\n", environment("GENERALSX_UITEST_PNG"));
}

void UiTest::finishFrame()
{
	State& current = state();
	if (!current.active || current.stage != Stage::Created || current.framesSinceCreation < CaptureFramesAfterCreation)
		return;

	writeDump();
	if (environment("GENERALSX_UITEST_PNG") && !TheGlobalData->m_headless && !current.captured)
		std::fprintf(stderr, "UI test: no frame was captured\n");
	if (!current.layout.empty() && !current.layoutLoaded)
		std::fprintf(stderr, "UI test: requested layout was not loaded\n");
	std::fflush(stderr);
	current.stage = Stage::Done;
	TheGameEngine->setQuitting(TRUE);
}
