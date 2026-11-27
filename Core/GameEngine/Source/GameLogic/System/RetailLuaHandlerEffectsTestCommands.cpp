#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Damage.h"
#include "GameLogic/LuaScriptEngine.h"
#include "GameLogic/Module/FoundationAIUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "../../../../GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DModelDraw.h"

namespace
{
void condition(const RetailTestCommands::Context& ctx)
{
	if (Object* object = ctx.requireObject(true))
	{
		const Int bit = ModelConditionFlags::getSingleBitFromName(ctx.argument);
		if (bit >= 0)
			object->setModelConditionState(static_cast<ModelConditionFlagType>(bit));
	}
}

void power(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject(true);
	const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(ctx.argument);
	const auto* module = object && power ? object->getSpecialPowerModule(power) : nullptr;
	if (module)
		std::fprintf(stderr, "TEST lua handler power frame=%u power=%s ready=%d\n",
			ctx.frame, ctx.argument, module->isReady());
}

void present(const RetailTestCommands::Context& ctx)
{
	std::fprintf(stderr, "TEST lua handler object frame=%u template=%s present=%d\n",
		ctx.frame, ctx.targetName, ctx.findObject(true) != nullptr);
}

void hit(const RetailTestCommands::Context& ctx)
{
	const Object* source = ctx.requireObject(true);
	Object* target = ctx.findTarget(true);
	if (!source || !target)
		return;
	DamageInfo damage;
	damage.in.m_sourceID = source->getID();
	damage.in.m_damageType = DAMAGE_UNRESISTABLE;
	damage.in.m_deathType = DEATH_NORMAL;
	damage.in.m_amount = 10.0f;
	target->attemptDamage(&damage);
	std::fprintf(stderr, "TEST lua handler hit frame=%u owner=%s\n",
		ctx.frame, target->getControllingPlayer()->getPlayerName().str());
}

void removeUpgrade(const RetailTestCommands::Context& ctx)
{
	auto* player = ctx.findPlayer();
	const auto* upgrade = TheUpgradeCenter->findUpgrade(ctx.argument);
	if (player && upgrade)
		player->removeUpgrade(upgrade);
}

void build(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject(true);
	auto* plot = object ? FoundationAIUpdate::find(*object) : nullptr;
	const auto* unit = TheThingFactory->findTemplate(ctx.argument, FALSE);
	if (plot && unit && plot->construct(unit, object->getControllingPlayer(), TRUE))
		std::fprintf(stderr, "TEST lua handler build frame=%u built=1\n", ctx.frame);
}

void geometry(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject(true);
	if (!object)
		return;
	const AsciiString name(ctx.argument);
	const auto& data = object->getTemplate()->getRotWKObjectData();
	Int count = 0;
	Int active = 0;
	const auto check = [&](const ThingTemplate::GeometryDetails& geometry)
	{
		if (geometry.name == name)
		{
			++count;
			active += object->getLuaGeometryActive(name, geometry.active);
		}
	};
	check(data.primaryGeometry);
	for (const auto& entry : data.additionalGeometries)
		check(entry.details);
	std::fprintf(stderr, "TEST lua handler geometry frame=%u name=%s count=%d active=%d\n",
		ctx.frame, ctx.argument, count, active);
}

void appearance(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject(true);
	const Drawable* drawable = object ? object->getDrawable() : nullptr;
	if (!drawable)
		return;
	for (auto module = drawable->getDrawModules(); *module; ++module)
	{
		const auto* model = dynamic_cast<const W3DModelDraw*>(*module);
		Bool hidden = FALSE;
		if (model && model->getSubObjectHidden(ctx.argument, hidden))
		{
			std::fprintf(stderr, "TEST lua handler subobject frame=%u name=%s hidden=%d\n",
				ctx.frame, ctx.argument, hidden);
			return;
		}
	}
}

void scripted(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject(true);
	auto* lua = LuaScriptEngine::active();
	if (object && lua)
	{
		const bool ran = lua->dispatchScripted(*object, ctx.argument);
		std::fprintf(stderr, "TEST lua handler scripted frame=%u event=%s ran=%d\n",
			ctx.frame, ctx.argument, ran);
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailLuaHandlerEffectsTestCommands()
{
#if !RTS_GENERALS
	registerAction("lua-handler-condition", condition);
	registerAction("lua-handler-power", power);
	registerAction("lua-handler-present", present);
	registerAction("lua-handler-hit", hit);
	registerAction("lua-handler-remove-upgrade", removeUpgrade);
	registerAction("lua-handler-build", build);
	registerAction("lua-handler-geometry", geometry);
	registerAction("lua-handler-appearance", appearance);
	registerAction("lua-handler-scripted", scripted);
#endif
}
}
