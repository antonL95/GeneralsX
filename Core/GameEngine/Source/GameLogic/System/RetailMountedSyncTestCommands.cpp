#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstdlib>

#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/Drawable.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"

namespace
{
void rankHero(const RetailTestCommands::Context& ctx)
{
	if (Object* hero = ctx.requireObject())
		hero->getExperienceTracker()->setMinRetailRank(std::atoi(ctx.argument));
}

void changeMaxHealth(const RetailTestCommands::Context& ctx)
{
	if (Object* hero = ctx.requireObject())
		hero->getBodyModule()->setMaxHealth(std::strtof(ctx.argument, nullptr), PRESERVE_RATIO);
}

void readyPower(const RetailTestCommands::Context& ctx)
{
	Object* hero = ctx.requireObject();
	const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(ctx.argument);
	if (auto* module = hero && power ? hero->getSpecialPowerModule(power) : nullptr)
		module->refreshPowerTimer();
}

void pausePowerRecharge(const RetailTestCommands::Context& ctx)
{
	Object* hero = ctx.requireObject();
	const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(ctx.argument);
	if (auto* module = hero && power ? hero->getSpecialPowerModule(power) : nullptr)
	{
		module->startPowerRecharge();
		module->pauseCountdown(TRUE);
	}
}

void sampleHero(const RetailTestCommands::Context& ctx)
{
	const Object* hero = ctx.requireObject();
	if (!hero)
		return;

	const Coord3D& pos = *hero->getPosition();
	const auto* tracker = hero->getExperienceTracker();
	const auto* body = hero->getBodyModule();
	auto* drawable = hero->getDrawable();
	const auto* upgrade = TheUpgradeCenter->findUpgrade("MountedSyncCarryUpgrade");
	const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(ctx.argument);
	const auto* module = power ? hero->getSpecialPowerModule(power) : nullptr;
	AsciiString modifiers;
	AttributeModifiers::describe(hero, modifiers);
	std::fprintf(stderr,
		"TEST mounted-sync %u %s id=%u pos=%.2f,%.2f,%.2f rank=%d xp=%.3f hp=%.5f maxhp=%.2f upgrade=%d mounted=%d power=%s ready=%d deadline=%u opacity=%.4f selected=%d set=%s mods=%s\n",
		ctx.frame, hero->getTemplate()->getName().str(), static_cast<UnsignedInt>(hero->getID()),
		pos.x, pos.y, pos.z, tracker->getRank(), tracker->getRetailExperience(), body->getHealth() / body->getMaxHealth(),
		body->getMaxHealth(), upgrade && hero->hasUpgrade(upgrade), hero->testWeaponSetFlag(WEAPONSET_MOUNTED), ctx.argument,
		module ? module->isReady() : -1, module ? module->getReadyFrame() : 0,
		drawable ? drawable->friend_getExplicitOpacity() * drawable->getRetailOclFadeOpacity() : -1.0f,
		drawable && drawable->isSelected(),
		hero->getCommandSetString().str(), modifiers.str());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailMountedSyncTestCommands()
{
#if !RTS_GENERALS
	registerAction("mounted-sync-state", sampleHero);
	registerAction("mounted-sync-rank", rankHero);
	registerAction("mounted-sync-max-health", changeMaxHealth);
	registerAction("mounted-sync-ready", readyPower);
	registerAction("mounted-sync-pause-recharge", pausePowerRecharge);
#endif
}
}
