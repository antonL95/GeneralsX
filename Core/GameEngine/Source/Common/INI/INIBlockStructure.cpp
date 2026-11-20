#include "Common/INIBlockStructure.h"

#include <algorithm>
#include <iterator>

namespace INIBlockStructure
{
namespace
{
constexpr std::string_view whitespace = " \t\r\n";
constexpr std::string_view separators = " \t\r\n=";

char asciiLower(char c)
{
	return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
}

bool sameKeyword(std::string_view word, std::string_view lower)
{
	return std::equal(word.begin(), word.end(), lower.begin(), lower.end(),
		[](char a, char b) { return asciiLower(a) == b; });
}

constexpr std::string_view blockNames[] = {
	"act",
	"addmodule",
	"aieconomyassigment",
	"aiwallnodeassignment",
	"alpha",
	"animation",
	"animationstate",
	"armorset",
	"armydetailspanel",
	"armyentry",
	"armymemberdefinition",
	"armytospawn",
	"attachedmodel",
	"attribute",
	"attributemodifiernugget",
	"autoresolvearmor",
	"autoresolveweapon",
	"battleresolver",
	"behavior",
	"body",
	"bonusforlevel",
	"borderseffect",
	"brutaldifficultycheats",
	"buffnugget",
	"buildingnugget",
	"buildqueuedetailspanel",
	"camerashakervolume",
	"cancelarmymembermovebutton",
	"cancelarmymovebutton",
	"cancelbuildingconstructionbutton",
	"checklist",
	"clientbehavior",
	"clientupdate",
	"colorintensitycontrolpoint",
	"combatchaindefinition",
	"concurrentregionbonus",
	"connection",
	"createaherobling",
	"createaheroblingbinder",
	"createaheroclass",
	"createdebris",
	"createobject",
	"damagecontainednugget",
	"damagefieldnugget",
	"damagenugget",
	"decaltemplate",
	"defaultmodelconditionstate",
	"destroybuildingbutton",
	"difficultytuning",
	"disbandarmymemberbutton",
	"dotnugget",
	"draw",
	"dynamicautoresolvedialog",
	"dynamicdecal",
	"emissionvelocity",
	"emissionvolume",
	"emitter",
	"emotionweaponnugget",
	"event",
	"eyetowerpoints",
	"filledownershipeffect",
	"firelogicnugget",
	"fireweaponnugget",
	"flammability",
	"floodmember",
	"formationpreviewdecal",
	"formationpreviewitemdecal",
	"formationselection",
	"formationtemplate",
	"fxlistatbonepos",
	"grabnugget",
	"griddecaltemplate",
	"homeregionhighlight",
	"hordeattacknugget",
	"idleanimationstate",
	"imagepart",
	"inheritablemodule",
	"invisibilitynugget",
	"lightpulse",
	"livingworldvictorytype",
	"locomotorset",
	"lodoptions",
	"luaeventnugget",
	"meleebehavior",
	"metaimpactnugget",
	"mission",
	"modelconditionstate",
	"mouseouteffectflareup",
	"mouseovereffectflareup",
	"notificationtype",
	"objectaward",
	"objectivesbutton",
	"opengatenugget",
	"optionsbutton",
	"ownershipset",
	"paralyzenugget",
	"physics",
	"playerdefeatcondition",
	"productionmodifier",
	"projectilenugget",
	"radiuscursortemplate",
	"regiondetailspanelstructurespage",
	"regiondisplay",
	"regionselectioneffect",
	"replacemodule",
	"replaceobject",
	"restrictbuildings",
	"rows",
	"scenario",
	"selectiondecal",
	"sideinfo",
	"sidesound",
	"skirmishbuildlist",
	"slaveattacknugget",
	"soundstate",
	"soundupgrade",
	"spawnandfadenugget",
	"spawnarmies",
	"spawnarmy",
	"spawnbuildings",
	"specialmodelconditionnugget",
	"stance",
	"startingrestriction",
	"statsdisplay",
	"stealmoneynugget",
	"structure",
	"subclass",
	"system",
	"teamdefeatcondition",
	"teamvictorycondition",
	"terraincelltype",
	"terrainscorch",
	"thingstat",
	"threatbreakdown",
	"tintdrawable",
	"toggleselectiondetailsbutton",
	"transitionstate",
	"trigger",
	"typeimages",
	"unifiedeffect",
	"unitcategory",
	"unitdefinition",
	"unitspecificsounds",
	"update",
	"upgradeunitbutton",
	"viewinfo",
	"viewshake",
	"weaponoclnugget",
	"weaponset",
	"wind",
	"window",
};

static_assert(std::is_sorted(std::begin(blockNames), std::end(blockNames)));

bool contains(std::string_view name, const std::string_view* first, const std::string_view* last)
{
	return std::any_of(first, last, [name](std::string_view choice) { return sameKeyword(name, choice); });
}

std::string_view rawEndFor(std::string_view keyword)
{
	return sameKeyword(keyword, "beginscript") ? "endscript" : "";
}

enum class ValueClass { Empty, Attrs, Multi, Number, Name };

bool isDigit(char ch)
{
	return ch >= '0' && ch <= '9';
}

bool isNumber(std::string_view token)
{
	if (!token.empty() && (token.front() == '+' || token.front() == '-'))
		token.remove_prefix(1);
	if (!token.empty() && token.back() == '%')
		token.remove_suffix(1);
	const auto dot = token.find('.');
	const auto whole = token.substr(0, dot);
	const auto fraction = dot == std::string_view::npos ? std::string_view{} : token.substr(dot + 1);
	const auto allDigits = [](std::string_view part) { return std::all_of(part.begin(), part.end(), isDigit); };
	if (!allDigits(whole) || !allDigits(fraction))
		return false;
	return !whole.empty() || !fraction.empty();
}

ValueClass classifyValue(std::string_view value)
{
	int count = 0;
	std::string_view first;
	bool attrs = false;
	std::size_t position = 0;
	while ((position = value.find_first_not_of(separators, position)) != std::string_view::npos)
	{
		const auto end = value.find_first_of(separators, position);
		const auto token = value.substr(position, end == std::string_view::npos ? std::string_view::npos : end - position);
		if (count++ == 0)
			first = token;
		attrs = attrs || token.find(':') != std::string_view::npos;
		position = end;
	}
	if (count == 0)
		return ValueClass::Empty;
	if (attrs)
		return ValueClass::Attrs;
	if (count > 1)
		return ValueClass::Multi;
	return isNumber(first) ? ValueClass::Number : ValueClass::Name;
}
}

bool parseLineHead(std::string_view line, LineHead& head)
{
	const auto start = line.find_first_not_of(separators);
	if (start == std::string_view::npos)
		return false;

	line.remove_prefix(start);
	const auto keywordEnd = line.find_first_of(separators);
	head = {line.substr(0, keywordEnd), Shape::Bare, {}};
	const auto restStart = line.find_first_not_of(whitespace, head.keyword.size());
	if (restStart != std::string_view::npos)
	{
		head.shape = line[restStart] == '=' ? Shape::Assign : Shape::Positional;
		const auto valueStart = line.find_first_not_of(separators, restStart);
		if (valueStart != std::string_view::npos)
			head.value = line.substr(valueStart, line.find_last_not_of(separators) - valueStart + 1);
	}
	return true;
}

bool isEnd(const LineHead& head)
{
	return sameKeyword(head.keyword, "end");
}

std::string_view opensRawBlock(const LineHead& head)
{
	return rawEndFor(head.keyword).empty() ? "" : "EndScript";
}

bool opensBlock(const LineHead& head, std::string_view enclosing)
{
	const auto key = head.keyword;
	const bool assign = head.shape == Shape::Assign;
	if (assign && sameKeyword(head.value, "fcurve"))
		return true;

	constexpr std::string_view bareNames[] = {"bonus", "evaevent", "fireweapon", "particlesystem", "turret"};
	constexpr std::string_view positionalNames[] = {"object", "region", "threshold"};
	if (contains(key, std::begin(bareNames), std::end(bareNames)))
		return head.shape == Shape::Bare;
	if (contains(key, std::begin(positionalNames), std::end(positionalNames)))
		return head.shape == Shape::Positional;
	if (sameKeyword(key, "sound"))
		return !assign;
	if (sameKeyword(key, "color"))
		return assign && classifyValue(head.value) == ValueClass::Name;
	if (sameKeyword(key, "transition"))
		return assign && sameKeyword(enclosing, "window");
	if (sameKeyword(key, "addemotion"))
		return assign && classifyValue(head.value) == ValueClass::Multi;
	if (sameKeyword(key, "attackpriority"))
		return head.shape == Shape::Positional && classifyValue(head.value) == ValueClass::Name;

	const auto name = std::lower_bound(std::begin(blockNames), std::end(blockNames), key,
		[](std::string_view lower, std::string_view word) {
			return std::lexicographical_compare(lower.begin(), lower.end(), word.begin(), word.end(),
				[](char a, char b) { return asciiLower(a) < asciiLower(b); });
		});
	return name != std::end(blockNames) && sameKeyword(key, *name);
}

bool isSingleLineTopLevel(std::string_view keyword)
{
	constexpr std::string_view names[] = {
		"audiolowmhz", "benchprofile", "evaeventforwardreference", "lodpreset", "reallylowmhz"
	};
	return contains(keyword, std::begin(names), std::end(names));
}

BlockEnd::BlockEnd(std::string_view enclosing) :
	m_stack{sameKeyword(enclosing, "window") ? "window" : ""}, m_rawEnd(rawEndFor(enclosing))
{
}

bool BlockEnd::consume(std::string_view line)
{
	LineHead head;
	if (!parseLineHead(line, head))
		return false;

	if (!m_rawEnd.empty())
	{
		if (!sameKeyword(head.keyword, m_rawEnd))
			return false;
		m_rawEnd = {};
		m_stack.pop_back();
	}
	else if (isEnd(head))
	{
		m_stack.pop_back();
	}
	else
	{
		m_rawEnd = rawEndFor(head.keyword);
		if (!m_rawEnd.empty() || opensBlock(head, m_stack.back()))
			m_stack.push_back(sameKeyword(head.keyword, "window") ? "window" : "");
	}
	return m_stack.empty();
}

std::size_t findBlockEnd(std::string_view buffer, std::size_t next, std::string_view enclosing)
{
	BlockEnd block(enclosing);
	while (next < buffer.size())
	{
		const auto end = buffer.find('\n', next);
		if (block.consume(buffer.substr(next, end == std::string_view::npos ? end : end - next)))
			return next;
		if (end == std::string_view::npos)
			break;
		next = end + 1;
	}
	return std::string_view::npos;
}
}
