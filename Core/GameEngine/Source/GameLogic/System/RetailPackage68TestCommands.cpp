#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Object.h"

namespace
{
void handlePackage68(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	if (!source)
		return;

	if (ctx.action == "pkg68-cast")
	{
		const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(ctx.argument);
		if (!power)
			return;
		GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_DO_SPECIAL_POWER);
		message->appendIntegerArgument(power->getID());
		message->appendIntegerArgument(0);
		message->appendObjectIDArgument(source->getID());
	}
	else if (ctx.action == "pkg68-state")
	{
		const auto* production = source->getProductionUpdateInterface();
		const ProductionEntry* entry = production ? production->firstProduction() : nullptr;
		std::fprintf(stderr, "PKG68 state frame=%u producer=%s index=%d model=%d unit=%s progress=%.3f speed=%.3f\n",
			ctx.frame, source->getTemplate()->getName().str(), ctx.ordinal,
			source->getLogicModelConditions().test(MODELCONDITION_UPGRADE_ECONOMY_BONUS),
			entry && entry->getProductionObject() ? entry->getProductionObject()->getName().str() : "-",
			entry ? entry->getPercentComplete() : -1.0f,
			source->getControllingPlayer()->getPowerProductionSpeed(AsciiString(ctx.argument)));
	}
	else if (ctx.action == "pkg68-spawn")
	{
		Bool found = FALSE;
		for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		{
			if (object->getProducerID() == source->getID() && object->getTemplate()->getName() == ctx.argument)
			{
				found = TRUE;
				break;
			}
		}
		std::fprintf(stderr, "PKG68 spawn frame=%u producer=%s index=%d unit=%s found=%d\n",
			ctx.frame, source->getTemplate()->getName().str(), ctx.ordinal, ctx.argument, found);
	}
	else if (ctx.action == "pkg68-hit")
	{
		DamageInfo damage;
		damage.in.m_damageType = std::string_view(ctx.argument) == "unresistable" ? DAMAGE_UNRESISTABLE : DAMAGE_SLASH;
		damage.in.m_deathType = DEATH_NORMAL;
		damage.in.m_amount = 10.0f;
		source->attemptDamage(&damage);
		std::fprintf(stderr, "PKG68 hit frame=%u unit=%s kind=%s protected=%d dealt=%.1f\n",
			ctx.frame, source->getTemplate()->getName().str(), ctx.argument,
			source->getLogicModelConditions().test(MODELCONDITION_INVULNERABLE),
			damage.out.m_actualDamageDealt);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage68TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg68-", handlePackage68);
#endif
}
}
