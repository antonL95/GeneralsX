#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <cstdlib>

#include "Common/MessageStream.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"
#include "WWMath/wwmath.h"

namespace
{
void sampleHeroSpeed(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	const auto* ai = object ? object->getAI() : nullptr;
	const auto* locomotor = ai ? ai->getCurLocomotor() : nullptr;
	const auto* physics = object ? object->getPhysics() : nullptr;
	if (!locomotor || !physics)
		return;

	const Coord3D& pos = *object->getPosition();
	const Coord3D& velocity = *physics->getVelocity();
	const Real planarSpeed = WWMath::SqrtfOrigin(velocity.x * velocity.x + velocity.y * velocity.y);
	const Real framesPerSecond = static_cast<Real>(LOGICFRAMES_PER_SECOND);

	const auto damage = object->getBodyModule()->getDamageState();
	const auto* weapon = object->getWeaponInWeaponSlot(PRIMARY_WEAPON);
	const auto* container = object->getContainedBy();
	const auto* horde = HordeContain::get(container);
	AsciiString lists;
	AsciiString hordeLists;
	AttributeModifiers::describe(object, lists);
	if (horde)
		AttributeModifiers::describe(container, hordeLists);

	std::fprintf(stderr, "TEST hero-speed %u %s %s rank=%d pos=%.4f,%.4f vel=%.5f,%.5f actual=%.3f max=%.3f base=%.3f factor=%.3f loco=%s set=%d desired=%.3f groupRoute=%d horde=%d weapon=%s toggle=%d lists=[%s] hordeLists=[%s]\n",
		ctx.frame, object->getTemplate()->getName().str(), ctx.argument, object->getExperienceTracker()->getRank(),
		pos.x, pos.y, velocity.x, velocity.y, planarSpeed * framesPerSecond,
		locomotor->getMaxSpeedForCondition(damage, object) * framesPerSecond,
		locomotor->getMaxSpeedForCondition(damage) * framesPerSecond,
		AttributeModifiers::speedFactor(object), locomotor->getTemplateName().str(), ai->getCurLocomotorSetType(),
		ai->getDesiredSpeed(), ai->isFollowingRetailGroupRoute(), horde != nullptr,
		weapon ? weapon->getName().str() : "-", object->testWeaponSetFlag(WEAPONSET_TOGGLE_1),
		lists.str(), hordeLists.str());
}

void rankHero(const RetailTestCommands::Context& ctx)
{
	if (Object* object = ctx.requireObject())
		object->getExperienceTracker()->setMinRetailRank(std::atoi(ctx.argument));
}

void toggleHeroWeapon(const RetailTestCommands::Context& ctx)
{
	Object* source = ctx.requireObject();
	const CommandSet* set = source ? TheRetailCommandCatalog->findCommandSet(source->getCommandSetString()) : nullptr;
	for (Int slot = 0; set && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
	{
		const CommandButton* button = set->getCommandButton(slot);
		if (!button || button->getCommandType() != GUI_COMMAND_TOGGLE_WEAPONSET)
			continue;

		ctx.selectObject(source);
		GameMessage* message = TheMessageStream->appendMessage(GameMessage::MSG_RETAIL_HORDE_COMMAND);
		message->appendObjectIDArgument(source->getID());
		message->appendIntegerArgument(slot);
		message->appendIntegerArgument(-1);
		return;
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailHeroSpeedTestCommands()
{
#if !RTS_GENERALS
	registerAction("hero-speed", sampleHeroSpeed);
	registerAction("hero-speed-rank", rankHero);
	registerAction("hero-speed-toggle", toggleHeroWeapon);
#endif
}
}
