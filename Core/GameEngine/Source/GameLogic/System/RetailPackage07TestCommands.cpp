#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <algorithm>
#include <cstdio>
#include <string_view>
#include <vector>

#include "Common/ActionManager.h"
#include "Common/AttributeModifierRules.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameLogic/AI.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HordeAIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/RotwkParseOnlyModules.h"
#include "GameLogic/Object.h"
#include "GameLogic/Weapon.h"

void runRetailPackage07TestCommand(std::string_view action, Object* source, Object* target, UnsignedInt frame)
{
	const auto* horde = HordeContain::get(source);
	if (action == "pkg07-state" && horde)
	{
		const auto& slots = horde->getActiveFormation()->m_layout.slots();
		std::vector<Object*> members;
		for (Object* member : *horde->getContainedItemsList())
			if (!member->isEffectivelyDead())
				members.push_back(member);
		std::sort(members.begin(), members.end(), [](const Object* a, const Object* b) { return a->getID() < b->getID(); });
		const auto* stances = StancesBehavior::find(source);
		const auto* ai = source->getAI();
		std::fprintf(stderr, "TEST pkg07 horde frame=%u name=%s members=%zu xp=%.1f x=%.1f y=%.1f facing=%.3f stance=%d moving=%d\n",
			frame, source->getTemplate()->getName().str(), members.size(), source->getExperienceTracker()->getRetailExperience(),
			source->getPosition()->x, source->getPosition()->y, source->getOrientation(), stances ? stances->getStance() : -1,
			ai && ai->isMoving());
		for (Object* member : members)
		{
			const Int slot = horde->getMemberSlot(member->getID());
			auto* memberAI = member->getAI();
			const auto* goal = memberAI ? memberAI->getGoalObject() : nullptr;
			const auto* weapon = member->getCurrentWeapon();
			std::fprintf(stderr, "TEST pkg07 member frame=%u horde=%s id=%u name=%s rank=%d toggle=%d x=%.1f y=%.1f facing=%.3f target=%u weapon=%s\n",
				frame, source->getTemplate()->getName().str(), unsigned(member->getID()), member->getTemplate()->getName().str(),
				slot < 0 ? 0 : slots[slot].rank, member->testWeaponSetFlag(WEAPONSET_TOGGLE_1), member->getPosition()->x,
				member->getPosition()->y, member->getOrientation(), goal ? unsigned(goal->getID()) : 0,
				weapon ? weapon->getTemplate()->getName().str() : "-");
		}
	}
	else if ((action == "pkg07-formation-on" || action == "pkg07-formation-off") && horde)
		const_cast<HordeContain*>(horde)->setAlternateFormation(action == "pkg07-formation-on");
	else if (action == "pkg07-vision" && horde && target)
	{
		class TargetFilter final : public PartitionFilter
		{
		public:
			explicit TargetFilter(const Object* target) : m_target(target) {}
			Bool allow(Object* candidate) override { return candidate == m_target; }
#if defined(RTS_DEBUG)
			const char* debugGetName() override { return "TargetFilter"; }
#endif
		private:
			const Object* m_target;
		} filter(target);
		const Bool acquired = TheAI->findClosestEnemy(source, source->getVisionRange(),
			AI::USE_HORDE_VISION | AI::ATTACK_BUILDINGS, nullptr, &filter) == target;
		std::fprintf(stderr, "TEST pkg07 vision frame=%u scale=%.2f acquired=%d\n", frame, horde->directionalVisionScale(target), acquired);
	}
	else if (action == "pkg07-rank-stop" && horde)
	{
		Int stopping = 0;
		for (const Object* member : *horde->getContainedItemsList())
			stopping += !member->isEffectivelyDead() && horde->rankStopsAdvance(member);
		std::fprintf(stderr, "TEST pkg07 stop-rank frame=%u members=%d\n", frame, stopping);
	}
	else if (action == "pkg07-approach" && horde && target)
	{
		Int stoppingInRange = 0;
		Int othersInRange = 0;
		for (const Object* member : *horde->getContainedItemsList())
			if (const auto* weapon = member->getCurrentWeapon(); !member->isEffectivelyDead() &&
				weapon && weapon->isWithinAttackRange(member, target))
			{
				if (horde->rankStopsAdvance(member))
					++stoppingInRange;
				else
					++othersInRange;
			}
		const auto* ai = source->getAI();
		std::fprintf(stderr, "TEST pkg07 approach frame=%u stopping=%d other=%d moving=%d\n",
			frame, stoppingInRange, othersInRange, ai && ai->isMoving());
	}
	else if (action == "pkg07-promote")
		source->getExperienceTracker()->setMinRetailRank(2);
	else if (action == "pkg07-add-xp")
	{
		auto* tracker = source->getExperienceTracker();
		tracker->advanceRetailLevel(tracker->getRetailExperience() + 23.5f, FALSE);
	}
	else if (action == "pkg07-enable-banner")
	{
		if (const auto* upgrade = TheUpgradeCenter->findUpgrade("Upgrade_MenFortressHouseOfHealing"))
			source->giveUpgrade(upgrade);
	}
	else if (action == "pkg07-banner-gate")
		std::fprintf(stderr, "TEST pkg07 tower frame=%u allows-banner=%d\n", frame, HordeContain::allowsBannerSpawn(source));
	else if (action == "pkg07-stance-rule" && horde)
	{
		const auto* stance = StancesBehavior::find(source);
		const Int current = horde->getActiveFormation()->m_isPorcupineFormation ?
			AttributeModifierRules::STANCE_PORCUPINE : (stance ? stance->getStance() : -1);
		const auto* rule = stance ? stance->getMeleeBehavior(current) : nullptr;
		std::fprintf(stderr, "TEST pkg07 stance-rule frame=%u stance=%d rule=%s\n",
			frame, current, rule ? rule->name.str() : "-");
	}
	else if (action == "pkg07-banner" && horde)
	{
		Int banners = 0;
		for (const Object* member : *horde->getContainedItemsList())
			banners += !member->isEffectivelyDead() && horde->isBannerCarrier(member);
		std::fprintf(stderr, "TEST pkg07 banner frame=%u rank=%d contained=%d banners=%d\n",
			frame, source->getExperienceTracker()->getRank(), source->getContainedBy() != nullptr, banners);
	}
	else if ((action == "pkg07-contact" || action == "pkg07-contact-state") && horde)
	{
		if (action == "pkg07-contact" && target)
		{
			Object* member = horde->findNearestLivingMember(*target->getPosition());
			for (BehaviorModule** module = member ? member->getBehaviorModules() : nullptr; module && *module; ++module)
				if ((*module)->getModuleNameKey() == NAMEKEY("HordeMemberCollide"))
					if (auto* collide = (*module)->getCollide())
						collide->onCollide(target, nullptr, nullptr);
		}
		const auto* ai = source->getAI();
		const Bool contact = ai && ai->getModuleNameKey() == NAMEKEY("HordeAIUpdate") &&
			static_cast<const HordeAIUpdate*>(ai)->hasRecentContact();
		std::fprintf(stderr, "TEST pkg07 contact frame=%u active=%d\n", frame, contact);
	}
	else if (action == "pkg07-split")
	{
		const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilitySplitHorde");
		const Bool allowed = power && TheActionManager->canDoSpecialPower(source, power, CMD_FROM_PLAYER, 0);
		std::fprintf(stderr, "TEST pkg07 split frame=%u name=%s allowed=%d\n", frame, source->getTemplate()->getName().str(), allowed);
		if (allowed)
			source->doSpecialPower(power, 0);
	}
	else if (action == "pkg07-casualty" && horde)
	{
		if (Object* member = horde->findNearestLivingMember(*source->getPosition()))
			member->kill();
	}
}
#endif

namespace RetailTestCommands
{
void registerRetailPackage07TestCommands()
{
#if !RTS_GENERALS
	registerPrefix("pkg07-", [](const Context& ctx) {
		Object* source = ctx.requireObject();
		if (!source)
			return;
		runRetailPackage07TestCommand(ctx.action, source, ctx.target, ctx.frame);
	});
#endif
}
}
