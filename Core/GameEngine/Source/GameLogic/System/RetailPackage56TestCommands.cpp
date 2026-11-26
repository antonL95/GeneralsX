#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/Upgrade.h"
#include "GameClient/Drawable.h"
#include "GameLogic/CreateAHeroSystem.h"
#include "GameLogic/ExperienceLevelStore.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"

#include <cstdio>

namespace
{
void loadFixtureLevels(const RetailTestCommands::Context&)
{
	INI ini;
	ini.loadText("pkg56-levels", R"(
ExperienceLevel Pkg56First
	TargetNames = Pkg56PromotionProbe
	RequiredExperience = 1
	Rank = 1
End

ExperienceLevel Pkg56Second
	TargetNames = Pkg56PromotionProbe
	RequiredExperience = 15
	Rank = 2
	EmotionType = CHEER
	ShowLevelUpTint = Yes
	LevelUpTintColor = R:223 G:66 B:12
	LevelUpTintPreColorTime = 500
	LevelUpTintPostColorTime = 3000
	LevelUpTintSustainColorTime = 500
	LevelUpTintFrequency = 2.5
	LevelUpTintAmplitude = 0.75
End

ExperienceLevel Pkg56Third
	TargetNames = Pkg56PromotionProbe
	RequiredExperience = 35
	Rank = 3
	ShowLevelUpTint = Yes
	LevelUpTintColor = R:223 G:66 B:12
	LevelUpTintPreColorTime = 500
	LevelUpTintPostColorTime = 3000
	LevelUpTintSustainColorTime = 500
	LevelUpTintFrequency = 2.5
	LevelUpTintAmplitude = 0.75
End
)", INI_LOAD_OVERWRITE, "ExperienceLevel", ExperienceLevelStore::parseDefinition);
}

void promote(const RetailTestCommands::Context& ctx)
{
	if (Object* object = ctx.requireObject(true))
	{
		auto* tracker = object->getExperienceTracker();
		if (ctx.action == "pkg56-promote-quiet")
			tracker->setMinRetailRank(2, FALSE);
		else
			tracker->gainRetailLevels(1);
		tracker->addRetailExperience(3.5f, FALSE);
	}
}

void probePromotion(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject(true);
	if (!object || !object->getDrawable())
		return;
	Vector3 tint;
	std::fprintf(stderr, "PKG56 promotion %u rank=%d cheer=%d tint=%d leveled=%d\n", ctx.frame,
		object->getExperienceTracker()->getRank(),
		object->getLogicModelConditions().test(MODELCONDITION_EMOTION_CELEBRATING),
		object->getDrawable()->getTintColor(tint) != nullptr,
		object->getLogicModelConditions().test(MODELCONDITION_LEVELED));
}

void probeSavedState(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject(true);
	if (!object)
		return;
	const auto* tracker = object->getExperienceTracker();
	const auto* first = TheUpgradeCenter->findUpgrade("Upgrade_ObjectLevel1");
	const auto* second = TheUpgradeCenter->findUpgrade("Upgrade_ObjectLevel2");
	const auto* creation = TheUpgradeCenter->findUpgrade("Upgrade_AmrothKnights");
	const auto* creationObject = TheUpgradeCenter->findUpgrade("Upgrade_MiniHordeLvl2");
	const auto* horde = HordeContain::get(object);
	Int members = 0;
	Int promoted = 0;
	Int banners = 0;
	Int cheering = 0;
	if (horde)
		for (const Object* member : *horde->getContainedItemsList())
		{
			if (member->isEffectivelyDead())
				continue;
			++members;
			promoted += member->getExperienceTracker()->getRank() >= 2;
			banners += member->isKindOf(KINDOF_BANNER);
			if (!member->isKindOf(KINDOF_BANNER))
				cheering += member->getLogicModelConditions().test(MODELCONDITION_EMOTION_CELEBRATING);
		}
	std::fprintf(stderr, "PKG56 saved %u %s rank=%d xp=%.3f start=%d first=%d second=%d creation=%d objectGrant=%d members=%d promoted=%d banner=%d cheering=%d\n",
		ctx.frame, ctx.targetName, tracker->getRank(), tracker->getRetailExperience(), tracker->getRetailStartingRank(),
		first && object->hasUpgrade(first), second && object->hasUpgrade(second),
		creation && object->getControllingPlayer()->hasUpgradeComplete(creation),
		creationObject && object->hasUpgrade(creationObject), members, promoted, banners, cheering);
}

void probePlayerGrant(const RetailTestCommands::Context& ctx)
{
	const auto* player = ctx.findPlayer();
	const auto* upgrade = TheUpgradeCenter->findUpgrade("Upgrade_AmrothKnights");
	if (player && upgrade)
		std::fprintf(stderr, "PKG56 creation %u %d\n", ctx.frame, player->hasUpgradeComplete(upgrade));
}

constexpr const char* taskmasterClass = "Upgrade_CreateAHero_ClassDwarf";
constexpr const char* taskmasterSubclass = "Upgrade_CreateAHero_SubClass_0";

void configureTaskmaster(const RetailTestCommands::Context& ctx)
{
	const auto* player = ctx.findPlayer();
	CreateAHeroRules::SelectionData defaults;
	if (!player || !CreateAHeroSystem::buildDefaultSelection(taskmasterClass, taskmasterSubclass, defaults))
		return;
	defaults.heroName = "Promotion Taskmaster";
	const auto encoded = CreateAHeroSystem::encodeSelectionCompact(defaults);
	const Bool configured = CreateAHeroSystem::setPlayerSelectionEncoded(player->getPlayerIndex(), encoded, player->getSide());
	std::fprintf(stderr, "PKG56 taskmaster setup %u valid=%d\n", ctx.frame, configured);
}

void selectTaskmaster(const RetailTestCommands::Context& ctx)
{
	Object* hero = ctx.requireObject(true);
	if (!hero)
		return;
	ctx.selectObject(hero);
	const auto* profile = CreateAHeroSystem::getPlayerSelection(hero->getControllingPlayer()->getPlayerIndex());
	const auto* heroClass = TheUpgradeCenter->findUpgrade(taskmasterClass);
	const auto* subclass = TheUpgradeCenter->findUpgrade(taskmasterSubclass);
	const bool matches = profile && profile->classUpgrade == taskmasterClass &&
		profile->subClassUpgrade == taskmasterSubclass && profile->heroName == "Promotion Taskmaster";
	std::fprintf(stderr, "PKG56 taskmaster %u rank=%d profile=%d class=%d subclass=%d appearance=%d\n", ctx.frame,
		hero->getExperienceTracker()->getRank(), matches,
		heroClass && hero->hasUpgrade(heroClass), subclass && hero->hasUpgrade(subclass), hero->getCreateAHeroColors() != nullptr);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage56TestCommands()
{
#if RTS_ZEROHOUR
	registerAction("pkg56-levels", loadFixtureLevels);
	registerAction("pkg56-promote", promote);
	registerAction("pkg56-promote-quiet", promote);
	registerAction("pkg56-promotion", probePromotion);
	registerAction("pkg56-saved", probeSavedState);
	registerAction("pkg56-creation", probePlayerGrant);
	registerAction("pkg56-taskmaster-configure", configureTaskmaster);
	registerAction("pkg56-taskmaster-select", selectTaskmaster);
#endif
}
}
