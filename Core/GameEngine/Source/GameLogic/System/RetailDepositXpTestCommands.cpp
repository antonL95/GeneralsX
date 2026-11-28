#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include "Common/INI.h"
#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/ExperienceLevelStore.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/HordeAIUpdate.h"
#include "GameLogic/Object.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace
{
void loadLevels(const RetailTestCommands::Context& ctx)
{
	std::ifstream input(ctx.argument);
	if (!input)
		return;
	std::ostringstream text;
	text << input.rdbuf();
	INI ini;
	ini.loadText(ctx.argument, text.str().c_str(), INI_LOAD_OVERWRITE, "ExperienceLevel", ExperienceLevelStore::parseDefinition);
}

void probeDeposit(const RetailTestCommands::Context& ctx)
{
	Object* building = ctx.requireObject(true);
	const ExperienceTracker* tracker = building ? building->getExperienceTracker() : nullptr;
	if (!building || !tracker)
		return;
	const ContainModuleInterface* contain = building->getContain();
	std::fprintf(stderr, "TEST deposit-xp frame=%u label=%s rank=%d xp=%.2f cash=%u contained=%u\n",
		ctx.frame, ctx.argument, tracker->getRank(), tracker->getRetailExperience(),
		building->getControllingPlayer()->getMoney()->countMoney(), contain ? contain->getContainCount() : 0U);
}

void applyModifier(const RetailTestCommands::Context& ctx)
{
	if (Object* building = ctx.requireObject(true))
		AttributeModifiers::apply(building, AsciiString(ctx.argument));
}

void setRubble(const RetailTestCommands::Context& ctx)
{
	if (Object* building = ctx.requireObject(true))
		building->setModelConditionState(MODELCONDITION_RUBBLE);
}

void probeRaid(const RetailTestCommands::Context& ctx)
{
	const Object* source = ctx.requireObject(true);
	const Object* target = ctx.findTarget(ctx.argument, true);
	const AIUpdateInterface* ai = source ? source->getAI() : nullptr;
	if (!ai || !target)
		return;
	const auto* horde = static_cast<const HordeAIUpdate*>(source->findUpdateModule(NAMEKEY("HordeAIUpdate")));
	const Object* victim = horde ? horde->getAttackTarget() : ai->getCurrentVictim();
	std::fprintf(stderr, "TEST deposit-xp-raid frame=%u template=%s attacking=%d\n",
		ctx.frame, source->getTemplate()->getName().str(), victim == target ? 1 : 0);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailDepositXpTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("deposit-xp-load-levels", loadLevels);
	registerAction("deposit-xp-probe", probeDeposit);
	registerAction("deposit-xp-modifier", applyModifier);
	registerAction("deposit-xp-rubble", setRubble);
	registerAction("deposit-xp-raid-probe", probeRaid);
#endif
}
}
