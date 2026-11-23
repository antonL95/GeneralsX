#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/Player.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/RetailGate.h"
#include "GameLogic/Object.h"

namespace
{
void probeNuggets(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	if (!object)
		return;
	const auto* body = object->getBodyModule();
	const auto* damage = body ? body->getLastDamageInfo() : nullptr;
	const auto* hitFrame = body ? body->getLastDamageTimestamp() : nullptr;
	const auto& conditions = object->getLogicModelConditions();
	const auto* gate = GateOpenAndCloseBehavior::find(object);
	const auto* container = object->getContainedBy();
	const auto* player = object->getControllingPlayer();
	std::fprintf(stderr,
		"TEST pkg34 frame=%u label=%s hp=%.2f stun=%d model=%d terror=%d fear=%d gate=%d impact=%d parent=%u freeze=%d hit=%u weapon=%s cash=%u fx=%d\n",
		ctx.frame, ctx.argument, body ? body->getHealth() : 0.0f,
		object->isDisabledByType(DISABLED_USER_PARALYZED) || object->isDisabledByType(DISABLED_USER_FROZEN),
		conditions.test(MODELCONDITION_USER_2),
		conditions.test(MODELCONDITION_EMOTION_TERROR),
		conditions.test(MODELCONDITION_EMOTION_UNCONTROLLABLY_AFRAID),
		gate && gate->wantsOpen(),
		conditions.test(MODELCONDITION_STUNNED_FLAILING),
		container ? static_cast<UnsignedInt>(container->getID()) : 0u,
		object->isDisabledByType(DISABLED_USER_FROZEN),
		hitFrame ? *hitFrame : 0u,
		damage && damage->retailWeaponName.isNotEmpty() ? damage->retailWeaponName.str() : "-",
		player ? player->getMoney()->countMoney() : 0u,
		damage ? damage->in.m_damageFXOverride == DAMAGE_SWORD_SLASH : 0);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage34TestCommands()
{
#if !RTS_GENERALS
	registerAction("pkg34-probe", probeNuggets);
#endif
}
}
