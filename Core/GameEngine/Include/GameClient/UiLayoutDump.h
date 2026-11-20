#pragma once

#include <string>
#include <string_view>
#include <vector>

struct UiWindowRecord
{
	std::string name;
	std::string type;
	std::string parentPath;
	std::string layoutFile;
	int screen[4] = {0, 0, 0, 0};
	bool hasLayoutRect = false;
	int layoutRect[4] = {0, 0, 0, 0};
	int layoutResolution[2] = {0, 0};
	bool hidden = false;
	bool visible = true;
	bool hasFont = false;
	std::string fontName;
	int fontSize = 0;
	bool fontPixels = false;
	bool fontBold = false;
	int fontHeight = 0;
};

struct UiLayoutSummary
{
	int width = 0;
	int height = 0;
	double scale = 1.0;
	std::string layout;
};

struct UiSpecimenLine
{
	std::string font;
	int designSize = 0;
	std::string text;
};

std::string formatUiLayoutJson(const UiLayoutSummary& summary, std::vector<UiWindowRecord> windows);
std::vector<UiSpecimenLine> parseUiSpecimen(std::string_view spec);
std::string uiJsonEscape(std::string_view text);
