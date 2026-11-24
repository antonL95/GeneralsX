#pragma once

#include <string>
#include <vector>

#include "Common/AsciiString.h"
#include "Common/GameType.h"
#include "GameLogic/CreateAHeroRules.h"

class INI;
class Object;
class Player;
class Xfer;
class CommandButton;
class CommandCatalog;
class CommandSet;
class GameInfo;
class GameSlot;
class ThingTemplate;

struct CreateAHeroBlingDef
{
	AsciiString nameTag;
	AsciiString descriptionTag;
	AsciiString groupName;
	AsciiString blingUpgradeName;
};

struct CreateAHeroBlingBinderDef
{
	AsciiString groupName;
	AsciiString descriptionTag;
	AsciiString labelTag;
	Int uiSlot = 0;
	Int blingType = 0;
};

struct CreateAHeroAttributeDef
{
	AsciiString groupName;
	AsciiString minValueUpgrade;
	AsciiString maxValueUpgrade;
	AsciiString defaultValueUpgrade;
};

struct CreateAHeroViewInfoDef
{
	Real farPitch = 0.0f;
	Real farZoom = 0.0f;
	Real farFloor = 0.0f;
	Real farDist = 0.0f;
	Real farShift = 0.0f;
	Real nearPitch = 0.0f;
	Real nearZoom = 0.0f;
	Real nearFloor = 0.0f;
	Real nearDist = 0.0f;
	Real nearShift = 0.0f;
	Real closeUpPitch = 0.0f;
	Real closeUpZoom = 0.0f;
	Real closeUpFloor = 0.0f;
	Real closeUpDist = 0.0f;
	Real closeUpShift = 0.0f;
	Real portraitPitch = 0.0f;
	Real portraitZoom = 0.0f;
	Real portraitFloor = 0.0f;
	Real portraitDist = 0.0f;
	Real portraitShift = 0.0f;
	Real normalCam = 0.0f;
	Real cameraAngle = 0.0f;
	Int mapLocation = 0;
};

struct CreateAHeroSubClassDef
{
	AsciiString nameTag;
	AsciiString descriptionTag;
	AsciiString powersDescTag;
	AsciiString upgradeName;
	AsciiString iconImage;
	AsciiString buttonImage;
	AsciiString defaultFaction;
	std::vector<AsciiString> usableFactions;
	Int spendableAttributePoints = 0;
	std::vector<AsciiString> awards;
	std::vector<AsciiString> stats;
	std::vector<AsciiString> blingUpgrades;
	std::vector<AsciiString> defaultBlingUpgrades;
	RGBColor defaultPrimaryColor = { 160.0f / 255.0f, 160.0f / 255.0f, 160.0f / 255.0f };
	RGBColor defaultSecondaryColor = { 160.0f / 255.0f, 160.0f / 255.0f, 160.0f / 255.0f };
	RGBColor defaultTertiaryColor = { 160.0f / 255.0f, 160.0f / 255.0f, 160.0f / 255.0f };
	std::vector<CreateAHeroAttributeDef> attributes;
	CreateAHeroViewInfoDef viewInfo;
};

struct CreateAHeroClassDef
{
	AsciiString nameTag;
	AsciiString descriptionTag;
	AsciiString powersDescTag;
	AsciiString upgradeName;
	AsciiString iconImage;
	std::vector<CreateAHeroSubClassDef> subClasses;
};

namespace CreateAHeroSystem
{
	void load();
	void loadHouseColors();
	void parseDefinition(INI* ini);
	void resetAllPlayers();
	AsciiString findColorMask(AsciiString baseTexture);
	void resetPlayer(Int playerIndex);

	Bool isLoaded();
	const AsciiString& getMapModeUpgradeName();
	const AsciiString& getGameModeUpgradeName();
	const AsciiString& getCanBuildUpgradeName();
	const AsciiString& getCommandSetTemplate();
	Int getHeroRevivalDiscount();
	Int getSpecialPowerDiscountPerLevel();
	const std::vector<CreateAHeroBlingDef>& getBlings();
	const std::vector<CreateAHeroBlingBinderDef>& getBlingBinders();
	const std::vector<CreateAHeroClassDef>& getClasses();
	std::vector<const CommandButton*> getDesignerPowerButtons();
	std::vector<AsciiString> getGroupUpgrades(const AsciiString& groupName);

	const CreateAHeroClassDef* findClassByUpgrade(const AsciiString& classUpgrade);
	const CreateAHeroSubClassDef* findSubClassByUpgrade(const CreateAHeroClassDef* heroClass, const AsciiString& subClassUpgrade);
	const CreateAHeroBlingDef* findBlingByUpgrade(const AsciiString& blingUpgrade);

	Bool buildDefaultSelection(const AsciiString& classUpgrade, const AsciiString& subClassUpgrade, CreateAHeroRules::SelectionData& out);
	Bool validateSelection(const CreateAHeroRules::SelectionData& sel, const AsciiString& factionName, CreateAHeroRules::SelectionData& normalized);
	Bool isPowerButtonLegalForSelection(const CommandButton* button, const CreateAHeroRules::SelectionData& sel, Int selectedLevel, const std::vector<std::string>& chosenBefore);
	Int effectivePowerCost(const CommandButton* button, Int selectedLevel);
	Int selectionPowerCost(const CreateAHeroRules::SelectionData& selection);
	Int selectedPowerRank(const Object* hero, const CommandButton* button);
	const CreateAHeroRules::SelectionData* getTemplateSelection(const Player* player, const ThingTemplate* unit);
	void grantPowerUpgrades(Object* hero, Int previousRank, Int rank);

	AsciiString encodeSelectionCompact(const CreateAHeroRules::SelectionData& sel);
	Bool decodeSelectionCompact(std::string_view encoded, CreateAHeroRules::SelectionData& out);

	AsciiString validateSetupSelection(std::string_view encoded, const GameSlot& slot);
	void setPlayerSelection(Int playerIndex, const CreateAHeroRules::SelectionData& sel);
	Bool setPlayerSelectionEncoded(Int playerIndex, const AsciiString& encoded, const AsciiString& factionName);
	const CreateAHeroRules::SelectionData* getPlayerSelection(Int playerIndex);
	AsciiString getPlayerSelectionEncoded(Int playerIndex);
	void ingestGameInfo(const GameInfo* gameInfo);

	Bool applyToHero(Object* hero);
	Bool populateCommandSet(CommandCatalog& catalog, CommandSet& commands);
	inline constexpr std::string_view COMMAND_SET_PREFIX = "CreateAHeroProfile:";
	void xferPlayer(const Player* player, Xfer* xfer);
}
