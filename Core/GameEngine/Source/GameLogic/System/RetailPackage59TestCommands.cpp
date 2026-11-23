#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/INI.h"
#include "GameClient/FXList.h"
#include "GameClient/ParticleSys.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"

namespace
{
void loadBurningDeathParticles(const RetailTestCommands::Context&)
{
	if (!TheParticleSystemManager->findTemplate("GPUUnitFire"))
	{
		INI ini;
		ini.load("Data\\INI\\FXParticleSystem.ini", INI_LOAD_OVERWRITE, nullptr);
	}
}

void probeBurningDeath(const RetailTestCommands::Context& ctx)
{
	Object* victim = ctx.findObject();
	if (!victim)
	{
		std::fprintf(stderr, "TEST pkg59 frame=%u label=%s gone\n", ctx.frame, ctx.argument);
		return;
	}

	const auto* body = dynamic_cast<const ActiveBody*>(victim->getBodyModule());
	const auto* ai = victim->getAI();
	std::fprintf(stderr, "TEST pkg59 frame=%u label=%s hp=%.0f dead=%d burning=%u model=%d\n",
		ctx.frame, ctx.argument, body ? body->getHealth() : -1.0f, victim->isEffectivelyDead(),
		ai ? ai->getRetailBurningDeathDeadline() : 0,
		victim->getLogicModelConditions().test(MODELCONDITION_BURNINGDEATH));
}

void hitBurningDeathVictim(const RetailTestCommands::Context& ctx)
{
	Object* victim = ctx.requireObject();
	if (!victim || victim->isEffectivelyDead())
		return;

	const auto* body = dynamic_cast<const ActiveBody*>(victim->getBodyModule());
	if (!body)
		return;

	DamageInfo damage;
	damage.in.m_amount = body->getMaxHealth();
	damage.in.m_kill = true;
	const std::string_view action = ctx.action;
	damage.in.m_damageType = action == "pkg59-flame" ? DAMAGE_FLAME :
		action == "pkg59-logical-fire" ? DAMAGE_LOGICAL_FIRE : DAMAGE_UNRESISTABLE;
	damage.in.m_deathType = DEATH_NORMAL;
	victim->attemptDamage(&damage);

	const auto* fx = body->getRetailBodyData()->m_burningDeathFX;
	const char* choice = !fx ? "none" :
		fx == TheFXListStore->findFXList("FX_InfantryBurningFlame") ? "infantry" :
		fx == TheFXListStore->findFXList("FX_CavalryBurningFlame") ? "cavalry" : "other";
	Int flames = 0;
	Coord3D offset = {};
	for (ParticleSystem* system : TheParticleSystemManager->getAllParticleSystems())
	{
		if (system->getAttachedObject() == victim->getID() &&
			system->getTemplate()->getName() == "GPUUnitFire")
		{
			++flames;
			system->getPosition(&offset);
		}
	}
	std::fprintf(stderr, "TEST pkg59 FX frame=%u label=%s choice=%s flames=%d offset=%.0f,%.0f,%.0f\n",
		ctx.frame, ctx.argument, choice, flames, offset.x, offset.y, offset.z);
	probeBurningDeath(ctx);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage59TestCommands()
{
#if !RTS_GENERALS
	registerAction("pkg59-state", probeBurningDeath);
	registerAction("pkg59-load-fx", loadBurningDeathParticles);
	registerAction("pkg59-flame", hitBurningDeathVictim);
	registerAction("pkg59-logical-fire", hitBurningDeathVictim);
	registerAction("pkg59-unlit", hitBurningDeathVictim);
#endif
}
}
