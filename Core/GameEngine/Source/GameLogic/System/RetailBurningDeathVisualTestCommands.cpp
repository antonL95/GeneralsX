#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/INI.h"
#include "GameClient/FXList.h"
#include "GameClient/ParticleSys.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/ActiveBody.h"
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

void checkBurningDeathFX(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	if (!object)
		return;
	const auto* body = dynamic_cast<const ActiveBody*>(object->getBodyModule());
	if (!body)
		return;

	const std::string_view kind = ctx.argument;
	if (kind == "aflame")
		object->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_AFLAME));

	DamageInfo damage;
	damage.in.m_amount = body->getMaxHealth();
	damage.in.m_kill = TRUE;
	damage.in.m_damageType = kind == "fire" ? DAMAGE_FLAME :
		kind == "logical-fire" ? DAMAGE_LOGICAL_FIRE : DAMAGE_UNRESISTABLE;
	damage.in.m_deathType = kind == "burned-death-type" ? DEATH_BURNED : DEATH_NORMAL;
	object->attemptDamage(&damage);
	if (kind == "fire")
		object->attemptDamage(&damage);

	const Bool authoredFX = body->getRetailBodyData()->m_burningDeathFX ==
		TheFXListStore->findFXList("FX_InfantryBurningFlame");
	Int flames = 0;
	Coord3D offset = {};
	for (ParticleSystem* system : TheParticleSystemManager->getAllParticleSystems())
	{
		if (system->getAttachedObject() == object->getID() &&
			system->getTemplate()->getName() == "GPUUnitFire")
		{
			++flames;
			system->getPosition(&offset);
		}
	}
	std::fprintf(stderr, "TEST burning-death-fx frame=%u case=%s authored=%d flames=%d offset=%.0f,%.0f,%.0f\n",
		ctx.frame, ctx.argument, authoredFX, flames, offset.x, offset.y, offset.z);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailBurningDeathVisualTestCommands()
{
#if !RTS_GENERALS
	registerAction("burning-death-load-particles", loadBurningDeathParticles);
	registerAction("burning-death-visual", checkBurningDeathFX);
#endif
}
}
