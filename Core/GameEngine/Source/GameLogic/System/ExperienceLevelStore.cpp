#include "PreRTS.h"

#include "GameLogic/ExperienceLevelStore.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Common/INI.h"
#include "Common/ModelState.h"
#include "Common/RetailFileLayers.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameLogic/ExperienceLevelRules.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

namespace
{
struct SelectionDecal
{
	AsciiString texture;
	AsciiString texture2;
	AsciiString style;
	Real opacityMin = 0.0f;
	Real opacityMax = 0.0f;
	Real minRadius = 0.0f;
	Real maxRadius = 0.0f;
	Int maxSelectedUnits = 0;
};

struct Level
{
	AsciiString name;
	std::vector<AsciiString> targetNames;
	Int requiredExperience = 0;
	Int experienceAward = 0;
	Int experienceAwardOwnGuysDie = 0;
	Int rank = 0;
	std::vector<AsciiString> upgrades;
	std::vector<AsciiString> attributeModifiers;
	AsciiString levelUpFx;
	AsciiString levelUpOcl;
	ModelConditionFlags conditions;
	SelectionDecal selectionDecal;
	Bool showLevelUpTint = FALSE;
	RGBColor levelUpTintColor = {0.0f, 0.0f, 0.0f};
	Int levelUpTintPreColorTime = 0;
	Int levelUpTintPostColorTime = 0;
	Int levelUpTintSustainColorTime = 0;
	Real levelUpTintFrequency = 0.0f;
	Real levelUpTintAmplitude = 0.0f;
	Bool informUpdateModule = FALSE;
	AsciiString emotionType;
	Bool singlePlayerOnly = FALSE;
	Bool multiPlayerOnly = FALSE;
};

struct Pending
{
	ObjectID object;
	std::size_t level;
};

std::vector<Level> s_levels;
std::unordered_map<std::string, std::vector<std::size_t>> s_levelsByTarget;
std::vector<Pending> s_pending;
Bool s_indexed = FALSE;

void parseSelectionDecal(INI* ini, void*, void* store, const void*)
{
	static const FieldParse decalFields[] =
	{
		{ "Texture", INI::parseAsciiString, nullptr, offsetof(SelectionDecal, texture) },
		{ "Texture2", INI::parseAsciiString, nullptr, offsetof(SelectionDecal, texture2) },
		{ "Style", INI::parseAsciiString, nullptr, offsetof(SelectionDecal, style) },
		{ "OpacityMin", INI::parsePercentToReal, nullptr, offsetof(SelectionDecal, opacityMin) },
		{ "OpacityMax", INI::parsePercentToReal, nullptr, offsetof(SelectionDecal, opacityMax) },
		{ "MinRadius", INI::parseReal, nullptr, offsetof(SelectionDecal, minRadius) },
		{ "MaxRadius", INI::parseReal, nullptr, offsetof(SelectionDecal, maxRadius) },
		{ "MaxSelectedUnits", INI::parseInt, nullptr, offsetof(SelectionDecal, maxSelectedUnits) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(store, decalFields);
}

const FieldParse s_levelFields[] =
{
	{ "TargetNames", INI::parseAsciiStringVectorAppend, nullptr, offsetof(Level, targetNames) },
	{ "RequiredExperience", INI::parseInt, nullptr, offsetof(Level, requiredExperience) },
	{ "ExperienceAward", INI::parseInt, nullptr, offsetof(Level, experienceAward) },
	{ "ExperienceAwardOwnGuysDie", INI::parseInt, nullptr, offsetof(Level, experienceAwardOwnGuysDie) },
	{ "Rank", INI::parseInt, nullptr, offsetof(Level, rank) },
	{ "Upgrades", INI::parseAsciiStringVectorAppend, nullptr, offsetof(Level, upgrades) },
	{ "AttributeModifiers", INI::parseAsciiStringVectorAppend, nullptr, offsetof(Level, attributeModifiers) },
	{ "LevelUpFx", INI::parseAsciiString, nullptr, offsetof(Level, levelUpFx) },
	{ "LevelUpOCL", INI::parseAsciiString, nullptr, offsetof(Level, levelUpOcl) },
	{ "ModelConditionState", ModelConditionFlags::parseFromINI, nullptr, offsetof(Level, conditions) },
	{ "SelectionDecal", parseSelectionDecal, nullptr, offsetof(Level, selectionDecal) },
	{ "ShowLevelUpTint", INI::parseBool, nullptr, offsetof(Level, showLevelUpTint) },
	{ "LevelUpTintColor", INI::parseRGBColor, nullptr, offsetof(Level, levelUpTintColor) },
	{ "LevelUpTintPreColorTime", INI::parseInt, nullptr, offsetof(Level, levelUpTintPreColorTime) },
	{ "LevelUpTintPostColorTime", INI::parseInt, nullptr, offsetof(Level, levelUpTintPostColorTime) },
	{ "LevelUpTintSustainColorTime", INI::parseInt, nullptr, offsetof(Level, levelUpTintSustainColorTime) },
	{ "LevelUpTintFrequency", INI::parseReal, nullptr, offsetof(Level, levelUpTintFrequency) },
	{ "LevelUpTintAmplitude", INI::parseReal, nullptr, offsetof(Level, levelUpTintAmplitude) },
	{ "InformUpdateModule", INI::parseBool, nullptr, offsetof(Level, informUpdateModule) },
	{ "EmotionType", INI::parseAsciiString, nullptr, offsetof(Level, emotionType) },
	{ "SinglePlayerOnly", INI::parseBool, nullptr, offsetof(Level, singlePlayerOnly) },
	{ "MultiPlayerOnly", INI::parseBool, nullptr, offsetof(Level, multiPlayerOnly) },
	{ nullptr, nullptr, nullptr, 0 }
};

void buildIndex()
{
	s_levelsByTarget.clear();
	for (std::size_t i = 0; i < s_levels.size(); ++i)
		for (const AsciiString& target : s_levels[i].targetNames)
			s_levelsByTarget[target.str()].push_back(i);
	s_indexed = TRUE;
}

Bool tracing()
{
	static const char* commands = std::getenv("GENERALSX_TEST_COMMANDS");
	return commands != nullptr && *commands != '\0';
}

Bool isMultiplayer()
{
	return TheGameLogic->isInMultiplayerGame() || TheGameLogic->isInSkirmishGame();
}

void apply(Object* obj, const Level& level)
{
	std::string granted;
	for (const AsciiString& name : level.upgrades)
	{
		const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(name);
		if (upgrade == nullptr || obj->hasUpgrade(upgrade))
			continue;
		obj->giveUpgrade(upgrade);
		if (!granted.empty())
			granted += ',';
		granted += name.str();
	}
	if (level.conditions.any())
		obj->setModelConditionFlags(level.conditions);
	if (tracing())
		std::fprintf(stderr, "TEST level %u object %u %s level %s granted %s\n", TheGameLogic->getFrame(),
			static_cast<unsigned>(obj->getID()), obj->getTemplate()->getName().str(), level.name.str(),
			granted.empty() ? "-" : granted.c_str());
}
}

namespace ExperienceLevelStore
{
void load()
{
	if (!RetailFileLayers::enabled())
		return;
	try
	{
		INI ini;
		ini.load("Data\\INI\\ExperienceLevels.ini", INI_LOAD_OVERWRITE, nullptr, "ExperienceLevel", parseDefinition);
	}
	catch (...)
	{
		std::fprintf(stderr, "Experience levels: Data\\INI\\ExperienceLevels.ini could not be read\n");
	}
	buildIndex();
}

void parseDefinition(INI* ini)
{
	const char* token = ini->getNextToken();
	AsciiString name = token;
	std::size_t index = s_levels.size();
	for (std::size_t i = 0; i < s_levels.size(); ++i)
	{
		if (s_levels[i].name == name)
		{
			index = i;
			break;
		}
	}
	Level level;
	level.name = name;
	ini->initFromINI(&level, s_levelFields);
	if (index == s_levels.size())
		s_levels.push_back(std::move(level));
	else
		s_levels[index] = std::move(level);
	s_indexed = FALSE;
}

void onObjectCreated(const Object* obj)
{
	if (!RetailFileLayers::enabled() || obj == nullptr || s_levels.empty())
		return;
	if (!s_indexed)
		buildIndex();
	const auto found = s_levelsByTarget.find(obj->getTemplate()->getName().str());
	if (found == s_levelsByTarget.end())
		return;
	std::vector<ExperienceLevelRules::Candidate> candidates;
	candidates.reserve(found->second.size());
	for (std::size_t index : found->second)
	{
		const Level& level = s_levels[index];
		candidates.push_back({level.requiredExperience, level.singlePlayerOnly != FALSE, level.multiPlayerOnly != FALSE});
	}
	const auto first = ExperienceLevelRules::firstLevel(candidates, isMultiplayer() != FALSE);
	if (first)
		s_pending.push_back({obj->getID(), found->second[*first]});
}

void update()
{
	for (std::size_t i = 0; i < s_pending.size(); ++i)
	{
		const Pending pending = s_pending[i];
		Object* obj = TheGameLogic->findObjectByID(pending.object);
		if (obj == nullptr || obj->isDestroyed())
			continue;
		apply(obj, s_levels[pending.level]);
	}
	s_pending.clear();
}

void reset()
{
	s_pending.clear();
}
}
