#include "PreRTS.h"

#include "GameLogic/Module/RotwkParseOnlyModules.h"

#include "Common/AttributeModifierRules.h"
#include "Common/Xfer.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#if !RTS_GENERALS
#include "Common/ThingTemplate.h"
#include "GameLogic/AttributeModifierStore.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/RotwkModifierModules.h"
#include "GameLogic/PartitionManager.h"
#endif

void StancesBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	BehaviorModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "StanceTemplate", INI::parseAsciiString, nullptr, offsetof(StancesBehaviorModuleData, m_stanceTemplate) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

void LargeGroupAudioUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "Key", INI::parseAsciiStringVectorAppend, nullptr, offsetof(LargeGroupAudioUpdateModuleData, m_keys) },
		{ "UnitWeight", INI::parseInt, nullptr, offsetof(LargeGroupAudioUpdateModuleData, m_unitWeight) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

void PickupStuffUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "SkirmishAIOnly", INI::parseBool, nullptr, offsetof(PickupStuffUpdateModuleData, m_skirmishAIOnly) },
		{ "StuffToPickUp", SpecialPowerObjectFilter::parse, nullptr, offsetof(PickupStuffUpdateModuleData, m_stuffToPickUp) },
		{ "ScanRange", INI::parseReal, nullptr, offsetof(PickupStuffUpdateModuleData, m_scanRange) },
		{ "ScanIntervalSeconds", INI::parseReal, nullptr, offsetof(PickupStuffUpdateModuleData, m_scanIntervalSeconds) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

void HitReactionBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	BehaviorModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "HitReactionLifeTimer1", INI::parseInt, nullptr, offsetof(HitReactionBehaviorModuleData, m_hitReactionLifeTimer1) },
		{ "HitReactionLifeTimer2", INI::parseInt, nullptr, offsetof(HitReactionBehaviorModuleData, m_hitReactionLifeTimer2) },
		{ "HitReactionLifeTimer3", INI::parseInt, nullptr, offsetof(HitReactionBehaviorModuleData, m_hitReactionLifeTimer3) },
		{ "HitReactionThreshold1", INI::parseReal, nullptr, offsetof(HitReactionBehaviorModuleData, m_hitReactionThreshold1) },
		{ "HitReactionThreshold2", INI::parseReal, nullptr, offsetof(HitReactionBehaviorModuleData, m_hitReactionThreshold2) },
		{ "HitReactionThreshold3", INI::parseReal, nullptr, offsetof(HitReactionBehaviorModuleData, m_hitReactionThreshold3) },
		{ "FastHitsResetReaction", INI::parseBool, nullptr, offsetof(HitReactionBehaviorModuleData, m_fastHitsResetReaction) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

void CastleMemberBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	BehaviorModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "CountsForEvaCastleBreached", INI::parseBool, nullptr, offsetof(CastleMemberBehaviorModuleData, m_countsForEvaCastleBreached) },
		{ "StoreUpgradePrice", INI::parseBool, nullptr, offsetof(CastleMemberBehaviorModuleData, m_storeUpgradePrice) },
		{ "BeingBuiltSound", INI::parseAsciiString, nullptr, offsetof(CastleMemberBehaviorModuleData, m_beingBuiltSound) },
		{ "CampDestroyedAllyEvaEvent", INI::parseAsciiString, nullptr, offsetof(CastleMemberBehaviorModuleData, m_campDestroyedAllyEvaEvent) },
		{ "CampDestroyedAttackerEvaEvent", INI::parseAsciiString, nullptr, offsetof(CastleMemberBehaviorModuleData, m_campDestroyedAttackerEvaEvent) },
		{ "CampDestroyedOwnerEvaEvent", INI::parseAsciiString, nullptr, offsetof(CastleMemberBehaviorModuleData, m_campDestroyedOwnerEvaEvent) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

#define DEFINE_PARSE_ONLY_SNAPSHOT(Module, Base) \
	Module::~Module() = default; \
	void Module::crc(Xfer* xfer) { Base::crc(xfer); } \
	void Module::xfer(Xfer* xfer) { Base::xfer(xfer); } \
	void Module::loadPostProcess() { Base::loadPostProcess(); }

DEFINE_PARSE_ONLY_SNAPSHOT(LargeGroupAudioUpdate, UpdateModule)
DEFINE_PARSE_ONLY_SNAPSHOT(PickupStuffUpdate, UpdateModule)
DEFINE_PARSE_ONLY_SNAPSHOT(HitReactionBehavior, BehaviorModule)
DEFINE_PARSE_ONLY_SNAPSHOT(HordeMemberCollide, BehaviorModule)
DEFINE_PARSE_ONLY_SNAPSHOT(CastleMemberBehavior, BehaviorModule)
#if !RTS_GENERALS
DEFINE_PARSE_ONLY_SNAPSHOT(HordeNotifyTargetsOfImminentProbableCrushingUpdate, UpdateModule)
#endif

#undef DEFINE_PARSE_ONLY_SNAPSHOT

#if !RTS_GENERALS
void HordeNotifyTargetsOfImminentProbableCrushingUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse fields[] =
	{
		{ "ScanWidth", INI::parseReal, nullptr, offsetof(HordeNotifyTargetsOfImminentProbableCrushingUpdateModuleData, m_scanWidth) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse(p);
	p.add(fields);
}

UpdateSleepTime HordeNotifyTargetsOfImminentProbableCrushingUpdate::update()
{
	constexpr UnsignedInt scanInterval = 6;
	Object* self = getObject();
	const HordeContain* horde = HordeContain::get(self);
	const auto* physics = self->getPhysics();
	const auto* ai = self->getAI();
	const Real halfWidth = getHordeNotifyTargetsOfImminentProbableCrushingUpdateModuleData()->m_scanWidth * 0.5f;
	if (!horde || !physics || !ai || !ai->isMoving() || halfWidth <= 0.0f)
		return UPDATE_SLEEP(scanInterval);

	const Coord3D& velocity = *physics->getVelocity();
	const Real speed = WWMath::SqrtfOrigin(velocity.x * velocity.x + velocity.y * velocity.y);
	if (speed <= 0.0f || speed < self->getTemplate()->getRotWKObjectData().minCrushVelocityPercent * ai->getCurLocomotorSpeed())
		return UPDATE_SLEEP(scanInterval);

	const Real ahead = self->getGeometryInfo().getBoundingCircleRadius() + speed * scanInterval;
	const Coord3D* center = self->getPosition();
	const Object* crusher = horde->findNearestLivingMember(*center);
	if (!crusher)
		return UPDATE_SLEEP(scanInterval);
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(center, ahead + halfWidth, FROM_CENTER_2D);
	MemoryPoolObjectHolder holder(iter);
	Object* nearest = nullptr;
	Real nearestAlong = ahead;
	for (Object* victim = iter->first(); victim; victim = iter->next())
	{
		auto* victimAI = victim->getAI();
		if (!victimAI || !crusher->canCrushOrSquish(victim))
			continue;
		const Real dx = victim->getPosition()->x - center->x;
		const Real dy = victim->getPosition()->y - center->y;
		const Real along = (dx * velocity.x + dy * velocity.y) / speed;
		const Real across = (dx * velocity.y - dy * velocity.x) / speed;
		if (along <= 0.0f || along > ahead || std::abs(across) > halfWidth)
			continue;
		if (!EmotionTrackerUpdate::signal(victim, EMOTION_BRACE, crusher))
			victimAI->braceForCrush(TheGameLogic->getFrame() + 2 * scanInterval);
		if (!nearest || along < nearestAlong || (along == nearestAlong && victim->getID() < nearest->getID()))
		{
			nearest = victim;
			nearestAlong = along;
		}
	}
	if (nearest)
		EmotionTrackerUpdate::signal(self, EMOTION_PRE_CRUSH, nearest);
	return UPDATE_SLEEP(scanInterval);
}
#endif

StancesBehavior::StancesBehavior(Thing* thing, const ModuleData* data) :
	BehaviorModule(thing, data),
	m_stance(AttributeModifierRules::STANCE_BATTLE)
{
}

StancesBehavior::~StancesBehavior() = default;

StancesBehavior* StancesBehavior::find(const Object* obj)
{
	if (!obj)
		return nullptr;
	static const NameKeyType key = NAMEKEY("StancesBehavior");
	for (BehaviorModule** module = obj->getBehaviorModules(); *module; ++module)
	{
		if ((*module)->getModuleNameKey() == key)
			return static_cast<StancesBehavior*>(*module);
	}
	return nullptr;
}

void StancesBehavior::onObjectCreated()
{
	setStance(m_stance);
}

Bool StancesBehavior::holdsGround(Int stance)
{
	return stance == AttributeModifierRules::STANCE_HOLD_GROUND ||
		stance == AttributeModifierRules::STANCE_HOLD_GROUND_MOVING ||
		stance == AttributeModifierRules::STANCE_PORCUPINE;
}

const StancesBehavior* StancesBehavior::findForUnit(const Object* obj)
{
	const Object* container = obj ? obj->getContainedBy() : nullptr;
	return find(HordeContain::get(container) ? container : obj);
}

void StancesBehavior::setStance(Int stance)
{
	if (stance <= AttributeModifierRules::STANCE_UNINITIALIZED || stance >= AttributeModifierRules::STANCE_COUNT)
		return;
	m_stance = stance;
#if !RTS_GENERALS
	if (!TheAttributeModifierStore)
		return;
	Object* self = getObject();
	const StanceTemplateData* stances = TheAttributeModifierStore->findStanceTemplate(getStancesBehaviorModuleData()->m_stanceTemplate);
	const AsciiString next = stances ? stances->m_modifiers[stance] : AsciiString::TheEmptyString;
	if (next == m_appliedModifier)
		return;
	if (m_appliedModifier.isNotEmpty())
		AttributeModifiers::remove(self, m_appliedModifier, AttributeModifiers::ownerOf(getObject(), getModuleTagNameKey()));
	m_appliedModifier = next;
	if (m_appliedModifier.isNotEmpty())
		AttributeModifiers::apply(self, m_appliedModifier, 0, 0, AttributeModifiers::ownerOf(getObject(), getModuleTagNameKey()));
#endif
}

void StancesBehavior::inheritStance(Int stance, const AsciiString& modifier, NameKeyType moduleTag)
{
	m_appliedModifier = modifier;
#if !RTS_GENERALS
	if (moduleTag != getModuleTagNameKey() && m_appliedModifier.isNotEmpty())
	{
		AttributeModifiers::remove(getObject(), m_appliedModifier, AttributeModifiers::ownerOf(getObject(), moduleTag));
		m_appliedModifier.clear();
	}
#endif
	setStance(stance);
}

void StancesBehavior::crc(Xfer* xfer)
{
	BehaviorModule::crc(xfer);
	xfer->xferInt(&m_stance);
}

void StancesBehavior::xfer(Xfer* xfer)
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	BehaviorModule::xfer(xfer);
	xfer->xferInt(&m_stance);
	xfer->xferAsciiString(&m_appliedModifier);
}

void StancesBehavior::loadPostProcess()
{
	BehaviorModule::loadPostProcess();
}
