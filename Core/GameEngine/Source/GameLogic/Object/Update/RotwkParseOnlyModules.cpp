#include "PreRTS.h"

#include "GameLogic/Module/RotwkParseOnlyModules.h"

#include "Common/AttributeModifierRules.h"
#include "Common/Xfer.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#if !RTS_GENERALS
#include "Common/RetailFileLayers.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/AttributeModifierStore.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/RotwkModifierModules.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/RetailTestCommands.h"
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
DEFINE_PARSE_ONLY_SNAPSHOT(NotifyTargetsOfImminentProbableCrushingUpdate, UpdateModule)
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

namespace
{
constexpr UnsignedInt crushScanFrames = 10;

struct CrushWarningTarget
{
	Object* object = nullptr;
	Real distance = 0.0f;
};

void warnCrushTargets(const Object* crusher, Real scanWidth, CrushWarningTarget& nearest)
{
	const auto* physics = crusher->getPhysics();
	if (!physics || physics->getIsStunned() || crusher->isEffectivelyDead() || crusher->isAirborneTarget())
		return;

	const Coord3D& velocity = *physics->getVelocity();
	const Real speed = WWMath::SqrtfOrigin(velocity.x * velocity.x + velocity.y * velocity.y);
	const auto* ai = crusher->getAI();
	const Real threshold = crusher->getTemplate()->getRotWKObjectData().minCrushVelocityPercent *
		AttributeModifiers::value(crusher, AttributeModifierRules::MINIMUM_CRUSH_VELOCITY);
	const Real ahead = speed * crushScanFrames;
	if (ahead < 1.0f || (ai && speed < threshold * ai->getCurLocomotorSpeed()))
		return;

	const auto& geometry = crusher->getGeometryInfo();
	const Real halfWidth = scanWidth > 0.0f ? scanWidth * 0.5f : geometry.getBoundingCircleRadius();
	const Real halfLength = ahead * 0.5f + geometry.getBoundingCircleRadius();
	const Real dirX = velocity.x / speed;
	const Real dirY = velocity.y / speed;
	const Coord3D& start = *crusher->getPosition();
	const Coord3D center = { start.x + dirX * ahead * 0.5f, start.y + dirY * ahead * 0.5f, start.z };
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(&center,
		WWMath::SqrtfOrigin(halfLength * halfLength + halfWidth * halfWidth), FROM_BOUNDINGSPHERE_2D);
	MemoryPoolObjectHolder holder(iter);
	for (Object* victim = iter->first(); victim; victim = iter->next())
	{
		auto* victimAI = victim->getAI();
		if (victim == crusher || !victimAI || !crusher->canCrushOrSquish(victim))
			continue;
		const Real dx = victim->getPosition()->x - center.x;
		const Real dy = victim->getPosition()->y - center.y;
		const Real along = dx * dirX + dy * dirY;
		const Real across = dx * dirY - dy * dirX;
		const auto& victimGeometry = victim->getGeometryInfo();
		const Real radius = victimGeometry.getBoundingCircleRadius();
		const Real dz = victim->getPosition()->z - start.z;
		const Real distance = along + ahead * 0.5f;
		if (distance <= 0.0f || std::abs(along) > halfLength + radius ||
			std::abs(across) > halfWidth + radius || dz > geometry.getMaxHeightAbovePosition() ||
			dz < -victimGeometry.getMaxHeightAbovePosition())
			continue;
		if (!EmotionTrackerUpdate::signal(victim, EMOTION_BRACE, crusher))
			victimAI->braceForCrush(TheGameLogic->getFrame() + 2 * crushScanFrames);
		if (!nearest.object || distance < nearest.distance ||
			(distance == nearest.distance && victim->getID() < nearest.object->getID()))
			nearest = { victim, distance };
		if (RetailTestCommands::tracesCombat())
			std::fprintf(stderr, "TEST crush warning frame=%u source=%u(%s) victim=%u(%s)\n",
				TheGameLogic->getFrame(), unsigned(crusher->getID()), crusher->getTemplate()->getName().str(),
				unsigned(victim->getID()), victim->getTemplate()->getName().str());
	}
}
}

UpdateSleepTime HordeNotifyTargetsOfImminentProbableCrushingUpdate::update()
{
	if (!RetailFileLayers::enabled())
		return UPDATE_SLEEP_FOREVER;
	const HordeContain* horde = HordeContain::get(getObject());
	if (!horde)
		return UPDATE_SLEEP_FOREVER;
	const Real width = getHordeNotifyTargetsOfImminentProbableCrushingUpdateModuleData()->m_scanWidth;
	CrushWarningTarget nearest;
	for (const Object* member : *horde->getContainedItemsList())
		warnCrushTargets(member, width, nearest);
	if (nearest.object)
		EmotionTrackerUpdate::signal(getObject(), EMOTION_PRE_CRUSH, nearest.object);
	return UPDATE_SLEEP(crushScanFrames);
}

UpdateSleepTime NotifyTargetsOfImminentProbableCrushingUpdate::update()
{
	if (!RetailFileLayers::enabled())
		return UPDATE_SLEEP_FOREVER;
	CrushWarningTarget nearest;
	warnCrushTargets(getObject(), 0.0f, nearest);
	if (nearest.object)
		EmotionTrackerUpdate::signal(getObject(), EMOTION_PRE_CRUSH, nearest.object);
	return UPDATE_SLEEP(crushScanFrames);
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
