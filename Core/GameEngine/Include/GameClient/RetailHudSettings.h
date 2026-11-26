#pragma once

#include "Common/AsciiString.h"
#include "GameClient/Color.h"

struct FieldParse;

struct RetailHudSettings
{
	struct TextStyle
	{
		AsciiString font;
		Int pointSize;
		Bool bold;
		Color color;
	};

	struct Notification
	{
		AsciiString message;
		Real timeout = 0.0f;
	};

	TextStyle helpBoxName = { "Albertus MT", 16, FALSE, GameMakeColor(255, 255, 255, 255) };
	TextStyle helpBoxCost = { "Albertus MT", 14, FALSE, GameMakeColor(255, 204, 0, 255) };
	TextStyle helpBoxShortcut = { "Albertus MT", 14, FALSE, GameMakeColor(255, 204, 0, 255) };
	TextStyle helpBoxDescription = { "Albertus MT", 14, FALSE, GameMakeColor(255, 204, 0, 255) };
	Notification heroInitialSpawn;
	Notification heroRespawn;
	Notification heroDeath;
	Notification heroEarnedAward;
	UnsignedInt selectNearestBuilderCycleTimeOut = 3500;
	Real unitHelpTextDelay = 1.0f;

	static const FieldParse *getFieldParse();
};

extern RetailHudSettings *TheRetailHudSettings;
