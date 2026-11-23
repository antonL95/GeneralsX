#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstdlib>

#include "Common/INI.h"
#include "Common/RandomValue.h"
#include "Common/ThingTemplate.h"
#include "GameClient/FXList.h"
#include "GameClient/ParticleSys.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

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

void probeReaction(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	const auto* body = object ? dynamic_cast<const ActiveBody*>(object->getBodyModule()) : nullptr;
	if (!body)
		return;
	const auto* data = body->getRetailBodyData();
	std::fprintf(stderr, "TEST pkg60 state frame=%u label=%s hp=%.0f state=%d radius=%.0f cheer=%d front=%d\n",
		ctx.frame, ctx.argument, body->getHealth(), body->getDamageState(), data->m_cheerRadius,
		object->getLogicModelConditions().test(MODELCONDITION_EMOTION_CELEBRATING),
		object->getLogicModelConditions().test(MODELCONDITION_DESTROYED_FRONT));
}

void applyReactionDamage(const RetailTestCommands::Context& ctx)
{
	Object* victim = ctx.requireObject(true);
	if (!victim)
		return;
	DamageInfo damage;
	damage.in.m_damageType = DAMAGE_UNRESISTABLE;
	if (ctx.action == "pkg60-kill")
	{
		const Object* killer = ctx.findTarget();
		if (!killer)
			return;
		damage.in.m_sourceID = killer->getID();
		damage.in.m_kill = TRUE;
		damage.in.m_amount = victim->getBodyModule()->getMaxHealth();
	}
	else
		damage.in.m_amount = ctx.position.x;
	victim->attemptDamage(&damage);
}

void probeHealing(const RetailTestCommands::Context& ctx)
{
	Object* patient = ctx.requireObject(true);
	const auto* body = patient ? dynamic_cast<const ActiveBody*>(patient->getBodyModule()) : nullptr;
	if (!body)
		return;
	const Real before = body->getHealth();
	const auto particles = [patient]()
	{
		Int count = 0;
		for (ParticleSystem* system : TheParticleSystemManager->getAllParticleSystems())
			if (system->getAttachedObject() == patient->getID() &&
				system->getTemplate()->getName() == "GPUUnitFire")
				++count;
		return count;
	};
	const Int beforeFX = particles();
	patient->attemptHealing(ctx.position.x, nullptr);
	std::fprintf(stderr, "TEST pkg60 heal frame=%u label=%s gain=%.0f configured=%d effects=%d\n",
		ctx.frame, ctx.argument, body->getHealth() - before, body->getRetailBodyData()->m_healingBuffFX != nullptr,
		particles() - beforeFX);
}

void probeDodge(const RetailTestCommands::Context& ctx)
{
	const Object* shooter = ctx.requireObject(true);
	Object* target = ctx.findTarget();
	const auto* body = target ? dynamic_cast<const ActiveBody*>(target->getBodyModule()) : nullptr;
	if (!shooter || !body)
		return;
	const Bool dodgeable = ctx.action == "pkg60-dodge";
	const WeaponTemplate* weapon = TheWeaponStore->findWeaponTemplate(dodgeable ? "Pkg60DodgeShot" : "Pkg60DodgeControl");
	const Real dodge = body->getRetailBodyData()->m_dodgePercent;
	const Int shots = static_cast<Int>(ctx.position.x);
	Int hits = 0;
	Bool resultsMatch = TRUE;
	Bool streamMatches = TRUE;
	for (Int shot = 0; shot < shots; ++shot)
	{
		const GameLogicRandomState initial = getGameLogicRandomState();
		const Bool accuracyHit = GameLogicRandomValue(0, 99) < 50;
		const Bool dodged = dodgeable && dodge > 0.0f && GameLogicRandomValueReal(0.0f, 1.0f) <= dodge;
		const UnsignedInt expectedStream = GetGameLogicRandomSeedCRC();
		setGameLogicRandomState(initial);

		const Real before = body->getHealth();
		TheWeaponStore->createAndFireTempWeapon(weapon, shooter, target, TRUE);
		const Bool hit = body->getHealth() < before;
		hits += hit;
		resultsMatch &= hit == (accuracyHit && !dodged);
		streamMatches &= GetGameLogicRandomSeedCRC() == expectedStream;
	}
	std::fprintf(stderr, "TEST pkg60 dodge frame=%u actor=%s chance=%.2f dodgeable=%d hits=%d/%d results=%d stream=%d\n",
		ctx.frame, target->getTemplate()->getName().str(), dodge, dodgeable, hits, shots, resultsMatch, streamMatches);
}

void probeDebris(const RetailTestCommands::Context& ctx)
{
	const Object* shooter = ctx.requireObject(true);
	Object* target = ctx.findTarget();
	if (!shooter || !target)
		return;
	const auto countDebris = [](const char* prefix)
	{
		Int count = 0;
		for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
			count += object->getTemplate()->getName().startsWith(prefix);
		return count;
	};
	const Int beforeBits = countDebris("BuildingDamageBits");
	const Int beforeChunks = countDebris("BuildingMBHrdPlcBit");
	const char* name = ctx.action == "pkg60-rock" ? "Pkg60SiegeRock" : "Pkg60SiegeControl";
	TheWeaponStore->createAndFireTempWeapon(TheWeaponStore->findWeaponTemplate(name), shooter, target, TRUE);
	std::fprintf(stderr, "TEST pkg60 debris frame=%u hp=%.0f state=%d bits=%d chunks=%d front=%d\n",
		ctx.frame, target->getBodyModule()->getHealth(), target->getBodyModule()->getDamageState(),
		countDebris("BuildingDamageBits") - beforeBits, countDebris("BuildingMBHrdPlcBit") - beforeChunks,
		target->getLogicModelConditions().test(MODELCONDITION_DESTROYED_FRONT));
}

void configureRecoveryControl(const RetailTestCommands::Context& ctx)
{
	Object* chest = ctx.requireObject(true);
	const auto* body = chest ? dynamic_cast<const ActiveBody*>(chest->getBodyModule()) : nullptr;
	if (!body)
		return;
	auto* data = const_cast<ActiveBodyModuleData*>(body->getRetailBodyData());
	if (std::getenv("GENERALSX_PKG60_RECOVERY_CONTROL"))
		data->m_recoveryTime = 0;
	std::fprintf(stderr, "TEST pkg60 recovery frame=%u duration=%u\n", ctx.frame, data->m_recoveryTime);
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
	registerAction("pkg60-state", probeReaction);
	registerAction("pkg60-kill", applyReactionDamage);
	registerAction("pkg60-hit", applyReactionDamage);
	registerAction("pkg60-heal", probeHealing);
	registerAction("pkg60-dodge", probeDodge);
	registerAction("pkg60-undodgeable", probeDodge);
	registerAction("pkg60-rock", probeDebris);
	registerAction("pkg60-siege-control", probeDebris);
	registerAction("pkg60-recovery", configureRecoveryControl);
#endif
}
}
