#include "PreRTS.h"

#include "GameLogic/CreateAHeroSystem.h"

#include <algorithm>
#include <map>

#include "Common/FileSystem.h"
#include "Common/GameCommon.h"
#include "Common/INIAudit.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/PlayerTemplate.h"
#include "Common/RetailFileLayers.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/Object.h"

namespace CreateAHeroSystem
{
namespace
{

struct SystemData
{
	Bool loaded = FALSE;
	AsciiString mapModeUpgradeName;
	AsciiString gameModeUpgradeName;
	AsciiString canBuildUpgradeName;
	AsciiString commandSetTemplate;
	AsciiString strategicDefeatStatName;
	AsciiString strategicVictoryStatName;
	AsciiString strategicMPDefeatStatName;
	AsciiString strategicMPVictoryStatName;
	AsciiString skirmishDefeatStatName;
	AsciiString skirmishVictoryStatName;
	AsciiString openPlayDefeatStatName;
	AsciiString openPlayVictoryStatName;
	AsciiString strategicCampaignVictoryStatName;
	AsciiString weaponGroupName;
	AsciiString selectedCheerAnimName;
	AsciiString examineWeaponAnimName;
	AsciiString examineSelfAnimName;
	Int examineAnimTweakValue = 0;
	Real specialAnimPercentChance = 0.0f;
	Int heroRevivalDiscount = 0;
	Int specialPowerDiscountPerLevel = 0;
	std::vector<CreateAHeroBlingDef> blings;
	std::vector<CreateAHeroBlingBinderDef> blingBinders;
	std::vector<CreateAHeroClassDef> classes;
};

SystemData s_data;
CreateAHeroRules::SelectionData s_playerSelections[MAX_PLAYER_COUNT];
Bool s_playerHasSelection[MAX_PLAYER_COUNT] = {};

const LookupListRec BlingTypeNames[] =
{
	{ "ATTRIBUTE", 0 },
	{ "APPEARANCE", 1 },
	{ nullptr, 0 }
};

void parsePercentInt(INI* ini, void*, void* store, const void*)
{
	const char* token = ini->getNextTokenOrNull();
	if (!token)
		return;
	Int* out = static_cast<Int*>(store);
	*out = std::atoi(token);
}

void parseAppendAsciiStringVector(INI* ini, void*, void* store, const void*)
{
	auto* vec = static_cast<std::vector<AsciiString>*>(store);
	for (const char* token = ini->getNextTokenOrNull(); token; token = ini->getNextTokenOrNull())
	{
		if (stricmp(token, "NONE") != 0)
			vec->emplace_back(token);
	}
}

void parseSubClassBlingUpgrades(INI* ini, void* instance, void*, const void*)
{
	auto* sub = static_cast<CreateAHeroSubClassDef*>(instance);
	for (const char* token = ini->getNextTokenOrNull(); token; token = ini->getNextTokenOrNull())
	{
		const CreateAHeroRules::ParsedBlingRef parsed = CreateAHeroRules::parseBlingToken(token);
		if (parsed.upgradeName.empty() || stricmp(parsed.upgradeName.c_str(), "NONE") == 0)
			continue;
		const AsciiString name(parsed.upgradeName.c_str());
		sub->blingUpgrades.push_back(name);
		if (parsed.isDefault)
			sub->defaultBlingUpgrades.push_back(name);
	}
}

void parseAttributeBlock(INI* ini, void* instance, void*, const void*)
{
	static const FieldParse fields[] =
	{
		{ "GroupName", INI::parseAsciiString, nullptr, offsetof(CreateAHeroAttributeDef, groupName) },
		{ "MinValueUpgrade", INI::parseAsciiString, nullptr, offsetof(CreateAHeroAttributeDef, minValueUpgrade) },
		{ "MaxValueUpgrade", INI::parseAsciiString, nullptr, offsetof(CreateAHeroAttributeDef, maxValueUpgrade) },
		{ "DefaultValueUpgrade", INI::parseAsciiString, nullptr, offsetof(CreateAHeroAttributeDef, defaultValueUpgrade) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	CreateAHeroAttributeDef attr;
	ini->initFromINI(&attr, fields);
	static_cast<CreateAHeroSubClassDef*>(instance)->attributes.push_back(attr);
}

void parseViewInfoBlock(INI* ini, void* instance, void*, const void*)
{
	static const FieldParse fields[] =
	{
		{ "FarPitch", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, farPitch) },
		{ "FarZoom", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, farZoom) },
		{ "FarFloor", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, farFloor) },
		{ "FarDist", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, farDist) },
		{ "FarShift", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, farShift) },
		{ "NearPitch", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, nearPitch) },
		{ "NearZoom", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, nearZoom) },
		{ "NearFloor", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, nearFloor) },
		{ "NearDist", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, nearDist) },
		{ "NearShift", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, nearShift) },
		{ "CloseUpPitch", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, closeUpPitch) },
		{ "CloseUpZoom", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, closeUpZoom) },
		{ "CloseUpFloor", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, closeUpFloor) },
		{ "CloseUpDist", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, closeUpDist) },
		{ "CloseUpShift", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, closeUpShift) },
		{ "PortraitPitch", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, portraitPitch) },
		{ "PortraitZoom", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, portraitZoom) },
		{ "PortraitFloor", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, portraitFloor) },
		{ "PortraitDist", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, portraitDist) },
		{ "PortraitShift", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, portraitShift) },
		{ "NormalCam", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, normalCam) },
		{ "CameraAngle", INI::parseReal, nullptr, offsetof(CreateAHeroViewInfoDef, cameraAngle) },
		{ "MapLocation", INI::parseInt, nullptr, offsetof(CreateAHeroViewInfoDef, mapLocation) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	CreateAHeroViewInfoDef viewInfo;
	ini->initFromINI(&viewInfo, fields);
	static_cast<CreateAHeroSubClassDef*>(instance)->viewInfo = viewInfo;
}

void parseSubClassBlock(INI* ini, void* instance, void*, const void*)
{
	static const FieldParse fields[] =
	{
		{ "NameTag", INI::parseAsciiString, nullptr, offsetof(CreateAHeroSubClassDef, nameTag) },
		{ "DescriptionTag", INI::parseAsciiString, nullptr, offsetof(CreateAHeroSubClassDef, descriptionTag) },
		{ "PowersDescTag", INI::parseAsciiString, nullptr, offsetof(CreateAHeroSubClassDef, powersDescTag) },
		{ "UpgradeName", INI::parseAsciiString, nullptr, offsetof(CreateAHeroSubClassDef, upgradeName) },
		{ "IconImage", INI::parseAsciiString, nullptr, offsetof(CreateAHeroSubClassDef, iconImage) },
		{ "ButtonImage", INI::parseAsciiString, nullptr, offsetof(CreateAHeroSubClassDef, buttonImage) },
		{ "DefaultFaction", INI::parseAsciiString, nullptr, offsetof(CreateAHeroSubClassDef, defaultFaction) },
		{ "UsableFactions", parseAppendAsciiStringVector, nullptr, offsetof(CreateAHeroSubClassDef, usableFactions) },
		{ "SpendableAttributePoints", INI::parseInt, nullptr, offsetof(CreateAHeroSubClassDef, spendableAttributePoints) },
		{ "Awards", parseAppendAsciiStringVector, nullptr, offsetof(CreateAHeroSubClassDef, awards) },
		{ "Stats", parseAppendAsciiStringVector, nullptr, offsetof(CreateAHeroSubClassDef, stats) },
		{ "BlingUpgrades", parseSubClassBlingUpgrades, nullptr, 0 },
		{ "DefaultPrimaryColor", INI::parseRGBColor, nullptr, offsetof(CreateAHeroSubClassDef, defaultPrimaryColor) },
		{ "DefaultSecondaryColor", INI::parseRGBColor, nullptr, offsetof(CreateAHeroSubClassDef, defaultSecondaryColor) },
		{ "DefaultTertiaryColor", INI::parseRGBColor, nullptr, offsetof(CreateAHeroSubClassDef, defaultTertiaryColor) },
		{ "Attribute", parseAttributeBlock, nullptr, 0 },
		{ "ViewInfo", parseViewInfoBlock, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	CreateAHeroSubClassDef sub;
	ini->initFromINI(&sub, fields);
	static_cast<CreateAHeroClassDef*>(instance)->subClasses.push_back(sub);
}

void parseBlingBlock(INI* ini, void* instance, void*, const void*)
{
	static const FieldParse fields[] =
	{
		{ "NameTag", INI::parseAsciiString, nullptr, offsetof(CreateAHeroBlingDef, nameTag) },
		{ "DescriptionTag", INI::parseAsciiString, nullptr, offsetof(CreateAHeroBlingDef, descriptionTag) },
		{ "GroupName", INI::parseAsciiString, nullptr, offsetof(CreateAHeroBlingDef, groupName) },
		{ "BlingUpgradeName", INI::parseAsciiString, nullptr, offsetof(CreateAHeroBlingDef, blingUpgradeName) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	CreateAHeroBlingDef bling;
	ini->initFromINI(&bling, fields);
	static_cast<SystemData*>(instance)->blings.push_back(bling);
}

void parseBlingBinderBlock(INI* ini, void* instance, void*, const void*)
{
	static const FieldParse fields[] =
	{
		{ "GroupName", INI::parseAsciiString, nullptr, offsetof(CreateAHeroBlingBinderDef, groupName) },
		{ "DescriptionTag", INI::parseAsciiString, nullptr, offsetof(CreateAHeroBlingBinderDef, descriptionTag) },
		{ "LabelTag", INI::parseAsciiString, nullptr, offsetof(CreateAHeroBlingBinderDef, labelTag) },
		{ "UISlot", INI::parseInt, nullptr, offsetof(CreateAHeroBlingBinderDef, uiSlot) },
		{ "BlingType", INI::parseLookupList, BlingTypeNames, offsetof(CreateAHeroBlingBinderDef, blingType) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	CreateAHeroBlingBinderDef binder;
	ini->initFromINI(&binder, fields);
	static_cast<SystemData*>(instance)->blingBinders.push_back(binder);
}

void parseClassBlock(INI* ini, void* instance, void*, const void*)
{
	static const FieldParse fields[] =
	{
		{ "NameTag", INI::parseAsciiString, nullptr, offsetof(CreateAHeroClassDef, nameTag) },
		{ "DescriptionTag", INI::parseAsciiString, nullptr, offsetof(CreateAHeroClassDef, descriptionTag) },
		{ "PowersDescTag", INI::parseAsciiString, nullptr, offsetof(CreateAHeroClassDef, powersDescTag) },
		{ "UpgradeName", INI::parseAsciiString, nullptr, offsetof(CreateAHeroClassDef, upgradeName) },
		{ "IconImage", INI::parseAsciiString, nullptr, offsetof(CreateAHeroClassDef, iconImage) },
		{ "SubClass", parseSubClassBlock, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	CreateAHeroClassDef heroClass;
	ini->initFromINI(&heroClass, fields);
	static_cast<SystemData*>(instance)->classes.push_back(heroClass);
}

std::vector<AsciiString> groupUpgrades(const AsciiString& groupName)
{
	std::vector<AsciiString> out;
	for (const CreateAHeroBlingDef& b : s_data.blings)
	{
		if (b.groupName.compareNoCase(groupName) == 0)
			out.push_back(b.blingUpgradeName);
	}
	return out;
}

Int findUpgradeIndex(const std::vector<AsciiString>& list, const AsciiString& name)
{
	for (size_t i = 0; i < list.size(); ++i)
	{
		if (list[i].compareNoCase(name) == 0)
			return static_cast<Int>(i);
	}
	return -1;
}

void grantNamedUpgrade(Object* hero, const AsciiString& upgradeName)
{
	if (!hero || !TheUpgradeCenter || upgradeName.isEmpty())
		return;
	const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(upgradeName);
	if (!upgrade)
		return;
	if (upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER)
	{
		if (Player* player = hero->getControllingPlayer())
		{
			if (!player->hasUpgradeComplete(upgrade))
				player->addUpgrade(upgrade, UPGRADE_STATUS_COMPLETE);
		}
	}
	else if (!hero->hasUpgrade(upgrade))
	{
		hero->giveUpgrade(upgrade);
	}
}

}

void parseDefinition(INI* ini)
{
	static const FieldParse fields[] =
	{
		{ "CreateAHeroMapModeUpgradeName", INI::parseAsciiString, nullptr, offsetof(SystemData, mapModeUpgradeName) },
		{ "CreateAHeroGameModeUpgradeName", INI::parseAsciiString, nullptr, offsetof(SystemData, gameModeUpgradeName) },
		{ "CanBuildCreateAHeroUpgradeName", INI::parseAsciiString, nullptr, offsetof(SystemData, canBuildUpgradeName) },
		{ "CommandSetTemplate", INI::parseAsciiString, nullptr, offsetof(SystemData, commandSetTemplate) },
		{ "StratigicDefeatStatName", INI::parseAsciiString, nullptr, offsetof(SystemData, strategicDefeatStatName) },
		{ "StratigicVictoryStatName", INI::parseAsciiString, nullptr, offsetof(SystemData, strategicVictoryStatName) },
		{ "StratigicMPDefeatStatName", INI::parseAsciiString, nullptr, offsetof(SystemData, strategicMPDefeatStatName) },
		{ "StratigicMPVictoryStatName", INI::parseAsciiString, nullptr, offsetof(SystemData, strategicMPVictoryStatName) },
		{ "SkirmishDefeatStatName", INI::parseAsciiString, nullptr, offsetof(SystemData, skirmishDefeatStatName) },
		{ "SkirmishVictoryStatName", INI::parseAsciiString, nullptr, offsetof(SystemData, skirmishVictoryStatName) },
		{ "OpenPlayDefeatStatName", INI::parseAsciiString, nullptr, offsetof(SystemData, openPlayDefeatStatName) },
		{ "OpenPlayVictoryStatName", INI::parseAsciiString, nullptr, offsetof(SystemData, openPlayVictoryStatName) },
		{ "StratigicCampainVictoryStatName", INI::parseAsciiString, nullptr, offsetof(SystemData, strategicCampaignVictoryStatName) },
		{ "WeaponGroupName", INI::parseAsciiString, nullptr, offsetof(SystemData, weaponGroupName) },
		{ "SelectedCheerAninName", INI::parseAsciiString, nullptr, offsetof(SystemData, selectedCheerAnimName) },
		{ "ExamineWeaponAninName", INI::parseAsciiString, nullptr, offsetof(SystemData, examineWeaponAnimName) },
		{ "ExamineSelfAninName", INI::parseAsciiString, nullptr, offsetof(SystemData, examineSelfAnimName) },
		{ "ExamineAnimTweakValue", INI::parseInt, nullptr, offsetof(SystemData, examineAnimTweakValue) },
		{ "SpecialAnimPercentChance", INI::parseReal, nullptr, offsetof(SystemData, specialAnimPercentChance) },
		{ "HeroRevivalDiscount", parsePercentInt, nullptr, offsetof(SystemData, heroRevivalDiscount) },
		{ "SpecialPowerDiscountPerLevel", parsePercentInt, nullptr, offsetof(SystemData, specialPowerDiscountPerLevel) },
		{ "CreateAHeroBling", parseBlingBlock, nullptr, 0 },
		{ "CreateAHeroBlingBinder", parseBlingBinderBlock, nullptr, 0 },
		{ "CreateAHeroClass", parseClassBlock, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	s_data = SystemData{};
	ini->initFromINI(&s_data, fields);
	s_data.loaded = TRUE;
}

void load()
{
	if (!RetailFileLayers::enabled() && !INIAudit::isEnabled())
		return;
	s_data = SystemData{};
	const char* path = "Data\\INI\\CreateAHeroSystem.ini";
	if (!TheFileSystem || !TheFileSystem->doesFileExist(path))
		return;
	INI ini;
	ini.load(AsciiString(path), INI_LOAD_OVERWRITE, nullptr, "CreateAHeroSystem", parseDefinition);
}

void resetAllPlayers()
{
	for (Int i = 0; i < MAX_PLAYER_COUNT; ++i)
		resetPlayer(i);
}

void resetPlayer(Int playerIndex)
{
	if (playerIndex < 0 || playerIndex >= MAX_PLAYER_COUNT)
		return;
	s_playerSelections[playerIndex] = CreateAHeroRules::SelectionData{};
	s_playerHasSelection[playerIndex] = FALSE;
}

Bool isLoaded() { return s_data.loaded; }
const AsciiString& getMapModeUpgradeName() { return s_data.mapModeUpgradeName; }
const AsciiString& getGameModeUpgradeName() { return s_data.gameModeUpgradeName; }
const AsciiString& getCanBuildUpgradeName() { return s_data.canBuildUpgradeName; }
const AsciiString& getCommandSetTemplate() { return s_data.commandSetTemplate; }
Int getHeroRevivalDiscount() { return s_data.heroRevivalDiscount; }
Int getSpecialPowerDiscountPerLevel() { return s_data.specialPowerDiscountPerLevel; }
const std::vector<CreateAHeroBlingDef>& getBlings() { return s_data.blings; }
const std::vector<CreateAHeroBlingBinderDef>& getBlingBinders() { return s_data.blingBinders; }
const std::vector<CreateAHeroClassDef>& getClasses() { return s_data.classes; }

const CreateAHeroClassDef* findClassByUpgrade(const AsciiString& classUpgrade)
{
	for (const CreateAHeroClassDef& c : s_data.classes)
	{
		if (c.upgradeName.compareNoCase(classUpgrade) == 0)
			return &c;
	}
	return nullptr;
}

const CreateAHeroSubClassDef* findSubClassByUpgrade(const CreateAHeroClassDef* heroClass, const AsciiString& subClassUpgrade)
{
	if (!heroClass)
		return nullptr;
	for (const CreateAHeroSubClassDef& s : heroClass->subClasses)
	{
		if (s.upgradeName.compareNoCase(subClassUpgrade) == 0)
			return &s;
	}
	return nullptr;
}

const CreateAHeroBlingDef* findBlingByUpgrade(const AsciiString& blingUpgrade)
{
	for (const CreateAHeroBlingDef& b : s_data.blings)
	{
		if (b.blingUpgradeName.compareNoCase(blingUpgrade) == 0)
			return &b;
	}
	return nullptr;
}

Bool buildDefaultSelection(const AsciiString& classUpgrade, const AsciiString& subClassUpgrade, CreateAHeroRules::SelectionData& out)
{
	const CreateAHeroClassDef* heroClass = findClassByUpgrade(classUpgrade);
	const CreateAHeroSubClassDef* sub = findSubClassByUpgrade(heroClass, subClassUpgrade);
	if (!heroClass || !sub)
		return FALSE;
	out = CreateAHeroRules::SelectionData{};
	out.classUpgrade = heroClass->upgradeName.str();
	out.subClassUpgrade = sub->upgradeName.str();
	out.heroName = sub->nameTag.str();
	for (const AsciiString& defBling : sub->defaultBlingUpgrades)
		out.blingUpgrades.emplace_back(defBling.str());
	for (const CreateAHeroAttributeDef& attr : sub->attributes)
	{
		if (!attr.defaultValueUpgrade.isEmpty())
			out.attributeUpgrades.emplace_back(attr.defaultValueUpgrade.str());
	}
	out.primaryColor[0] = static_cast<int>(sub->defaultPrimaryColor.red * 255.0f + 0.5f);
	out.primaryColor[1] = static_cast<int>(sub->defaultPrimaryColor.green * 255.0f + 0.5f);
	out.primaryColor[2] = static_cast<int>(sub->defaultPrimaryColor.blue * 255.0f + 0.5f);
	out.secondaryColor[0] = static_cast<int>(sub->defaultSecondaryColor.red * 255.0f + 0.5f);
	out.secondaryColor[1] = static_cast<int>(sub->defaultSecondaryColor.green * 255.0f + 0.5f);
	out.secondaryColor[2] = static_cast<int>(sub->defaultSecondaryColor.blue * 255.0f + 0.5f);
	out.tertiaryColor[0] = static_cast<int>(sub->defaultTertiaryColor.red * 255.0f + 0.5f);
	out.tertiaryColor[1] = static_cast<int>(sub->defaultTertiaryColor.green * 255.0f + 0.5f);
	out.tertiaryColor[2] = static_cast<int>(sub->defaultTertiaryColor.blue * 255.0f + 0.5f);
	return TRUE;
}

Bool isPowerButtonLegalForSelection(
	const CommandButton* button,
	const CreateAHeroRules::SelectionData& sel,
	Int selectedLevel,
	const std::vector<std::string>& chosenBefore)
{
	if (!button)
		return FALSE;
	std::vector<std::string> allowable;
	for (const AsciiString& u : button->getCreateAHeroUIAllowableUpgrades())
		allowable.emplace_back(u.str());
	std::vector<std::string> heroUpgrades = { sel.classUpgrade, sel.subClassUpgrade };
	if (!CreateAHeroRules::isPowerAllowedForUpgrades(allowable, heroUpgrades))
		return FALSE;
	return CreateAHeroRules::isPowerEligibleAtLevel(
		button->getCreateAHeroUIMinimumLevel(),
		selectedLevel,
		button->getCreateAHeroUIPrerequisiteButtonName().str(),
		chosenBefore) ? TRUE : FALSE;
}

Int effectivePowerCost(const CommandButton* button, Int selectedLevel)
{
	if (!button)
		return 0;
	return CreateAHeroRules::discountedPowerCost(
		button->getCreateAHeroUICostIfSelected(),
		button->getCreateAHeroUIMinimumLevel(),
		selectedLevel,
		s_data.specialPowerDiscountPerLevel);
}

Bool validateSelection(
	const CreateAHeroRules::SelectionData& sel,
	const AsciiString& factionName,
	CreateAHeroRules::SelectionData& normalized)
{
	if (!s_data.loaded || sel.empty())
		return FALSE;
	const CreateAHeroClassDef* heroClass = findClassByUpgrade(AsciiString(sel.classUpgrade.c_str()));
	const CreateAHeroSubClassDef* sub = findSubClassByUpgrade(heroClass, AsciiString(sel.subClassUpgrade.c_str()));
	if (!heroClass || !sub)
		return FALSE;
	if (!factionName.isEmpty())
	{
		std::vector<std::string> usable;
		for (const AsciiString& f : sub->usableFactions)
			usable.emplace_back(f.str());
		if (!CreateAHeroRules::isFactionAllowed(usable, factionName.str()))
			return FALSE;
	}

	normalized = sel;
	normalized.classUpgrade = heroClass->upgradeName.str();
	normalized.subClassUpgrade = sub->upgradeName.str();

	std::map<std::string, std::string> blingByGroup;
	for (const AsciiString& defBling : sub->defaultBlingUpgrades)
	{
		if (const CreateAHeroBlingDef* b = findBlingByUpgrade(defBling))
			blingByGroup[b->groupName.str()] = b->blingUpgradeName.str();
	}
	for (const std::string& chosen : sel.blingUpgrades)
	{
		const AsciiString chosenAscii(chosen.c_str());
		if (findUpgradeIndex(sub->blingUpgrades, chosenAscii) < 0)
			return FALSE;
		if (const CreateAHeroBlingDef* b = findBlingByUpgrade(chosenAscii))
			blingByGroup[b->groupName.str()] = b->blingUpgradeName.str();
		else
			return FALSE;
	}
	normalized.blingUpgrades.clear();
	for (const auto& kv : blingByGroup)
		normalized.blingUpgrades.push_back(kv.second);

	std::vector<CreateAHeroRules::AttributeRange> ranges;
	std::vector<int> chosenIndices;
	std::vector<std::string> resolvedAttributes;
	for (const CreateAHeroAttributeDef& attr : sub->attributes)
	{
		const std::vector<AsciiString> groupList = groupUpgrades(attr.groupName);
		const Int minIdx = std::max(0, findUpgradeIndex(groupList, attr.minValueUpgrade));
		const Int maxIdx = std::max(minIdx, findUpgradeIndex(groupList, attr.maxValueUpgrade));
		const Int defIdx = std::clamp(findUpgradeIndex(groupList, attr.defaultValueUpgrade), minIdx, maxIdx);
		Int pickedIdx = defIdx;
		for (const std::string& chosenAttr : sel.attributeUpgrades)
		{
			const Int idx = findUpgradeIndex(groupList, AsciiString(chosenAttr.c_str()));
			if (idx >= 0)
			{
				pickedIdx = idx;
				break;
			}
		}
		ranges.push_back({ attr.groupName.str(), minIdx, maxIdx, defIdx });
		chosenIndices.push_back(pickedIdx);
		if (pickedIdx >= 0 && static_cast<size_t>(pickedIdx) < groupList.size())
			resolvedAttributes.emplace_back(groupList[pickedIdx].str());
	}
	if (!ranges.empty() && !CreateAHeroRules::validateAttributeSpend(ranges, chosenIndices, sub->spendableAttributePoints))
		return FALSE;
	normalized.attributeUpgrades = std::move(resolvedAttributes);

	if (TheRetailCommandCatalog)
	{
		std::vector<std::string> validatedPowers;
		for (size_t i = 0; i < sel.powerButtons.size(); ++i)
		{
			const CommandButton* btn = TheRetailCommandCatalog->findCommandButton(AsciiString(sel.powerButtons[i].c_str()));
			const Int levelSlot = static_cast<Int>(i) + 1;
			if (!isPowerButtonLegalForSelection(btn, normalized, levelSlot, validatedPowers))
				return FALSE;
			validatedPowers.emplace_back(btn->getName().str());
		}
		normalized.powerButtons = std::move(validatedPowers);
	}
	return TRUE;
}

void setPlayerSelection(Int playerIndex, const CreateAHeroRules::SelectionData& sel)
{
	if (playerIndex < 0 || playerIndex >= MAX_PLAYER_COUNT)
		return;
	if (sel.empty())
	{
		resetPlayer(playerIndex);
		return;
	}
	s_playerSelections[playerIndex] = sel;
	s_playerHasSelection[playerIndex] = TRUE;
}

Bool setPlayerSelectionEncoded(Int playerIndex, const AsciiString& encoded, const AsciiString& factionName)
{
	if (playerIndex < 0 || playerIndex >= MAX_PLAYER_COUNT)
		return FALSE;
	CreateAHeroRules::SelectionData decoded;
	if (!CreateAHeroRules::decodeSelection(encoded.str(), decoded))
		return FALSE;
	CreateAHeroRules::SelectionData normalized;
	if (!validateSelection(decoded, factionName, normalized))
		return FALSE;
	setPlayerSelection(playerIndex, normalized);
	return TRUE;
}

const CreateAHeroRules::SelectionData* getPlayerSelection(Int playerIndex)
{
	if (playerIndex < 0 || playerIndex >= MAX_PLAYER_COUNT || !s_playerHasSelection[playerIndex])
		return nullptr;
	return &s_playerSelections[playerIndex];
}

AsciiString getPlayerSelectionEncoded(Int playerIndex)
{
	const CreateAHeroRules::SelectionData* sel = getPlayerSelection(playerIndex);
	if (!sel)
		return AsciiString::TheEmptyString;
	return AsciiString(CreateAHeroRules::encodeSelection(*sel).c_str());
}

Bool applyToHero(Object* hero)
{
	if (!RetailFileLayers::enabled() || !s_data.loaded || !hero || !hero->getTemplate())
		return FALSE;
	if (hero->getTemplate()->getName().compareNoCase("CreateAHero") != 0)
		return FALSE;
	const Player* player = hero->getControllingPlayer();
	if (!player)
		return FALSE;
	const CreateAHeroRules::SelectionData* sel = getPlayerSelection(player->getPlayerIndex());
	if (!sel || sel->empty())
		return FALSE;

	grantNamedUpgrade(hero, s_data.gameModeUpgradeName);
	grantNamedUpgrade(hero, AsciiString(sel->classUpgrade.c_str()));
	grantNamedUpgrade(hero, AsciiString(sel->subClassUpgrade.c_str()));
	for (const std::string& bling : sel->blingUpgrades)
		grantNamedUpgrade(hero, AsciiString(bling.c_str()));
	for (const std::string& attr : sel->attributeUpgrades)
		grantNamedUpgrade(hero, AsciiString(attr.c_str()));
	return TRUE;
}

void xferPlayer(const Player* player, Xfer* xfer)
{
	if (!player)
		return;
	const Int index = player->getPlayerIndex();
	if (index < 0 || index >= MAX_PLAYER_COUNT)
		return;
	Bool hasSel = s_playerHasSelection[index];
	xfer->xferBool(&hasSel);
	if (!hasSel)
	{
		if (xfer->getXferMode() == XFER_LOAD)
			resetPlayer(index);
		return;
	}
	CreateAHeroRules::SelectionData& sel = s_playerSelections[index];
	if (xfer->getXferMode() == XFER_LOAD)
	{
		sel = CreateAHeroRules::SelectionData{};
		s_playerHasSelection[index] = TRUE;
	}
	auto xferStr = [&](std::string& value)
	{
		AsciiString tmp(value.c_str());
		xfer->xferAsciiString(&tmp);
		if (xfer->getXferMode() == XFER_LOAD)
			value = tmp.str();
	};
	auto xferVec = [&](std::vector<std::string>& vec)
	{
		UnsignedInt count = static_cast<UnsignedInt>(vec.size());
		xfer->xferUnsignedInt(&count);
		if (xfer->getXferMode() == XFER_LOAD)
			vec.resize(count);
		for (std::string& item : vec)
			xferStr(item);
	};
	xferStr(sel.classUpgrade);
	xferStr(sel.subClassUpgrade);
	xferStr(sel.heroName);
	xferVec(sel.blingUpgrades);
	xferVec(sel.attributeUpgrades);
	xferVec(sel.powerButtons);
	for (Int c = 0; c < 3; ++c)
	{
		Int p = sel.primaryColor[c];
		Int s = sel.secondaryColor[c];
		Int t = sel.tertiaryColor[c];
		xfer->xferInt(&p);
		xfer->xferInt(&s);
		xfer->xferInt(&t);
		if (xfer->getXferMode() == XFER_LOAD)
		{
			sel.primaryColor[c] = p;
			sel.secondaryColor[c] = s;
			sel.tertiaryColor[c] = t;
		}
	}
}

}
