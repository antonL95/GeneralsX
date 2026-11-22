#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/XferCRC.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"

void runRetailPackage14TestCommand(std::string_view action, Object* object, const Object* killer,
	const char* argument, UnsignedInt frame)
{
	if (action == "pkg14-state")
	{
		const Matrix3D& matrix = *object->getTransformMatrix();
		const Coord3D& position = *object->getPosition();
		const Drawable* drawable = object->getDrawable();
		const auto& model = object->getLogicModelConditions();
		std::fprintf(stderr, "TEST pkg14 state frame=%u name=%s id=%u hp=%.1f damage=%d z=%.2f tilt=%.3f,%.3f opacity=%.3f status=%d%d%d%d model=%d%d%d%d\n",
			frame, object->getTemplate()->getName().str(), static_cast<UnsignedInt>(object->getID()),
			object->getBodyModule()->getHealth(), object->getBodyModule()->getDamageState(), position.z,
			matrix[2][0], matrix[2][1], drawable ? drawable->getEffectiveOpacity() : 1.0f,
			object->testStatus(OBJECT_STATUS_DEATH_1), object->testStatus(OBJECT_STATUS_DEATH_2),
			object->testStatus(OBJECT_STATUS_DEATH_3), object->testStatus(OBJECT_STATUS_DEATH_4),
			model.test(MODELCONDITION_DEATH_1), model.test(MODELCONDITION_DEATH_2),
			model.test(MODELCONDITION_DEATH_3), model.test(MODELCONDITION_DEATH_4));
	}
	else if (action == "pkg14-construction")
	{
		object->setStatus(OBJECT_STATUS_UNDER_CONSTRUCTION);
	}
	else if (action == "pkg14-death-crc")
	{
		const auto checksum = [object] {
			XferCRC xfer;
			xfer.open("Package14");
			xfer.xferSnapshot(object);
			const UnsignedInt result = xfer.getCRC();
			xfer.close();
			return result;
		};
		const UnsignedInt baseline = checksum();
		UnsignedInt changed = 0;
		for (Int bit = OBJECT_STATUS_DEATH_1; bit <= OBJECT_STATUS_DEATH_5; ++bit)
		{
			const auto status = static_cast<ObjectStatusTypes>(bit);
			const Bool original = object->testStatus(status);
			object->setStatus(MAKE_OBJECT_STATUS_MASK(status), !original);
			changed += checksum() != baseline;
			object->setStatus(MAKE_OBJECT_STATUS_MASK(status), original);
		}
		std::fprintf(stderr, "TEST pkg14 death-crc frame=%u changed=%u restored=%d\n",
			frame, changed, checksum() == baseline);
	}
	else if (action == "pkg14-lift")
	{
		object->setPositionZ(object->getPosition()->z + std::strtof(argument, nullptr));
	}
	else if (action == "pkg14-player-upgrade")
	{
		if (const UpgradeTemplate* upgrade = TheUpgradeCenter->findUpgrade(argument))
			object->getControllingPlayer()->addUpgrade(upgrade, UPGRADE_STATUS_COMPLETE);
	}
	else if (action == "pkg14-fatal" || action == "pkg14-fade" || action == "pkg14-knockback")
	{
		DamageInfo damage;
		damage.in.m_damageType = DAMAGE_UNRESISTABLE;
		damage.in.m_deathType = action == "pkg14-fade" ? DEATH_FADED :
			action == "pkg14-knockback" ? DEATH_KNOCKBACK : DEATH_NORMAL;
		damage.in.m_kill = true;
		damage.in.m_sourceID = killer ? killer->getID() : INVALID_ID;
		damage.in.m_amount = object->getBodyModule()->getMaxHealth();
		object->attemptDamage(&damage);
	}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage14TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg14-", [](const Context& ctx) {
		Object* source = ctx.requireObject();
		if (!source)
			return;
		runRetailPackage14TestCommand(ctx.action, source, ctx.target, ctx.argument, ctx.frame);
	});
#endif
}
}
