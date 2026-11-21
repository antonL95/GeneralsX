#include "PreRTS.h"

#include <cstddef>

#include "Common/INI.h"
#include "GameClient/RetailHudSettings.h"

RetailHudSettings *TheRetailHudSettings = nullptr;

namespace
{
#define RETAIL_TEXT_FIELDS(prefix, member) \
	{ prefix "Font", INI::parseAsciiString, nullptr, offsetof(RetailHudSettings, member) + offsetof(RetailHudSettings::TextStyle, font) }, \
	{ prefix "PointSize", INI::parseInt, nullptr, offsetof(RetailHudSettings, member) + offsetof(RetailHudSettings::TextStyle, pointSize) }, \
	{ prefix "Bold", INI::parseBool, nullptr, offsetof(RetailHudSettings, member) + offsetof(RetailHudSettings::TextStyle, bold) }, \
	{ prefix "Color", INI::parseColorInt, nullptr, offsetof(RetailHudSettings, member) + offsetof(RetailHudSettings::TextStyle, color) }

#define RETAIL_NOTIFICATION_FIELDS(prefix, member) \
	{ prefix "Message", INI::parseAsciiString, nullptr, offsetof(RetailHudSettings, member) + offsetof(RetailHudSettings::Notification, message) }, \
	{ prefix "Timeout", INI::parseReal, nullptr, offsetof(RetailHudSettings, member) + offsetof(RetailHudSettings::Notification, timeout) }

const FieldParse RetailHudFieldParse[] =
{
	RETAIL_TEXT_FIELDS("HelpBoxName", helpBoxName),
	RETAIL_TEXT_FIELDS("HelpBoxCost", helpBoxCost),
	RETAIL_TEXT_FIELDS("HelpBoxShortcut", helpBoxShortcut),
	RETAIL_TEXT_FIELDS("HelpBoxDescription", helpBoxDescription),
	RETAIL_NOTIFICATION_FIELDS("HeroInitialSpawnNotification", heroInitialSpawn),
	RETAIL_NOTIFICATION_FIELDS("HeroRespawnNotification", heroRespawn),
	RETAIL_NOTIFICATION_FIELDS("HeroDeathNotification", heroDeath),
	RETAIL_NOTIFICATION_FIELDS("HeroEarnedAwardNotification", heroEarnedAward),
	{ "SelectNearestBuilderCycleTimeOut", INI::parseUnsignedInt, nullptr, offsetof(RetailHudSettings, selectNearestBuilderCycleTimeOut) },
	{ nullptr, nullptr, nullptr, 0 }
};

#undef RETAIL_TEXT_FIELDS
#undef RETAIL_NOTIFICATION_FIELDS
}

const FieldParse *RetailHudSettings::getFieldParse()
{
	return RetailHudFieldParse;
}
