#include "PreRTS.h"

#include "GameLogic/Module/RotwkParseOnlyModules.h"

#include "Common/AttributeModifierRules.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/HordeAIUpdate.h"
#include "GameLogic/Object.h"
#if !RTS_GENERALS
#include "Common/RetailFileLayers.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameLogic/AttributeModifierStore.h"
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
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "HitReactionLifeTimer1", INI::parseDurationUnsignedInt, nullptr, offsetof(HitReactionBehaviorModuleData, m_hitReactionLifeTimer1) },
		{ "HitReactionLifeTimer2", INI::parseDurationUnsignedInt, nullptr, offsetof(HitReactionBehaviorModuleData, m_hitReactionLifeTimer2) },
		{ "HitReactionLifeTimer3", INI::parseDurationUnsignedInt, nullptr, offsetof(HitReactionBehaviorModuleData, m_hitReactionLifeTimer3) },
		{ "HitReactionThreshold1", INI::parseReal, nullptr, offsetof(HitReactionBehaviorModuleData, m_hitReactionThreshold1) },
		{ "HitReactionThreshold2", INI::parseReal, nullptr, offsetof(HitReactionBehaviorModuleData, m_hitReactionThreshold2) },
		{ "HitReactionThreshold3", INI::parseReal, nullptr, offsetof(HitReactionBehaviorModuleData, m_hitReactionThreshold3) },
		{ "FastHitsResetReaction", INI::parseBool, nullptr, offsetof(HitReactionBehaviorModuleData, m_fastHitsResetReaction) },
		{ "HitsParalyze", INI::parseBool, nullptr, offsetof(HitReactionBehaviorModuleData, m_hitsParalyze) },
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
DEFINE_PARSE_ONLY_SNAPSHOT(HordeMemberCollide, BehaviorModule)

void HordeMemberCollide::onCollide(Object* other, const Coord3D*, const Coord3D*)
{
	HordeAIUpdate::memberCollided(getObject(), other);
}
DEFINE_PARSE_ONLY_SNAPSHOT(CastleMemberBehavior, BehaviorModule)
#if !RTS_GENERALS
DEFINE_PARSE_ONLY_SNAPSHOT(HordeNotifyTargetsOfImminentProbableCrushingUpdate, UpdateModule)
DEFINE_PARSE_ONLY_SNAPSHOT(NotifyTargetsOfImminentProbableCrushingUpdate, UpdateModule)
#endif

#undef DEFINE_PARSE_ONLY_SNAPSHOT

HitReactionBehavior::~HitReactionBehavior() = default;

UpdateSleepTime HitReactionBehavior::update()
{
	Object* object = getObject();
	const auto* body = object->getBodyModule();
	if (!body)
		return UPDATE_SLEEP_FOREVER;

	const Real health = body->getHealth();
	const Real damage = m_previousHealth - health;
	m_previousHealth = health;
	const auto* data = getHitReactionBehaviorModuleData();
	const Bool dead = object->isEffectivelyDead() || health <= 0.0f;
	if (m_remaining && (--m_remaining == 0 || dead))
	{
		object->clearModelConditionState(MODELCONDITION_HIT_REACTION);
		object->clearModelConditionState(static_cast<ModelConditionFlagType>(m_level));
		m_remaining = 0;
	}
	if (dead)
		return UPDATE_SLEEP_FOREVER;
	if (!(damage > 0.0f) || (m_remaining && !data->m_fastHitsResetReaction))
		return UPDATE_SLEEP_NONE;

	const Real thresholds[] = {data->m_hitReactionThreshold1, data->m_hitReactionThreshold2, data->m_hitReactionThreshold3};
	const UnsignedInt durations[] = {data->m_hitReactionLifeTimer1, data->m_hitReactionLifeTimer2, data->m_hitReactionLifeTimer3};
	const ModelConditionFlagType levels[] = {MODELCONDITION_HIT_LEVEL_1, MODELCONDITION_HIT_LEVEL_2, MODELCONDITION_HIT_LEVEL_3};
	for (Int index = 2; index >= 0; --index)
	{
		if (damage < thresholds[index])
			continue;
		if (durations[index])
		{
			if (m_remaining)
				object->clearModelConditionState(static_cast<ModelConditionFlagType>(m_level));
			m_level = levels[index];
			m_remaining = durations[index];
			object->setModelConditionState(MODELCONDITION_HIT_REACTION);
			object->setModelConditionState(levels[index]);
			if (data->m_hitsParalyze)
				object->setDisabledUntil(DISABLED_PARALYZED, TheGameLogic->getFrame() + m_remaining);
		}
		break;
	}
	return UPDATE_SLEEP_NONE;
}

void HitReactionBehavior::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	xfer->xferReal(&m_previousHealth);
	xfer->xferUnsignedInt(&m_remaining);
	xfer->xferInt(&m_level);
}

void HitReactionBehavior::xfer(Xfer* xfer)
{
	XferVersion version = 2;
	xfer->xferVersion(&version, 2);
	if (version == 1)
	{
		ObjectModule::xfer(xfer);
		return;
	}
	UpdateModule::xfer(xfer);
	xfer->xferReal(&m_previousHealth);
	xfer->xferUnsignedInt(&m_remaining);
	xfer->xferInt(&m_level);
}

void HitReactionBehavior::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}

#if !RTS_GENERALS
void OathbreakersFadeAwayBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] = {
		{ "FadeOutTime", INI::parseDurationUnsignedInt, nullptr, offsetof(OathbreakersFadeAwayBehaviorModuleData, m_frames) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

OathbreakersFadeAwayBehavior::~OathbreakersFadeAwayBehavior() = default;

Bool OathbreakersFadeAwayBehavior::begin(Object* object)
{
	auto* fade = static_cast<OathbreakersFadeAwayBehavior*>(object->findUpdateModule(NAMEKEY("OathbreakersFadeAwayBehavior")));
	if (!fade || object->isEffectivelyDead())
		return FALSE;
	if (fade->m_endFrame)
		return TRUE;

	const UnsignedInt frames = std::max(1u, fade->getOathbreakersFadeAwayBehaviorModuleData()->m_frames);
	fade->m_endFrame = TheGameLogic->getFrame() + frames;
	object->setModelConditionState(MODELCONDITION_OATH_FULLFILLED);
	object->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNSELECTABLE));
	if (HordeContain::get(object))
	{
		for (Object* member : *HordeContain::get(object)->getContainedItemsList())
			begin(member);
	}
	else if (auto* drawable = object->getDrawable())
		drawable->fadeOut(frames);
	fade->setWakeFrame(object, UPDATE_SLEEP(frames));
	return TRUE;
}

UpdateSleepTime OathbreakersFadeAwayBehavior::update()
{
	const UnsignedInt now = TheGameLogic->getFrame();
	if (!m_endFrame || getObject()->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;
	if (now < m_endFrame)
		return UPDATE_SLEEP(m_endFrame - now);
	TheGameLogic->destroyObject(getObject());
	return UPDATE_SLEEP_FOREVER;
}

void OathbreakersFadeAwayBehavior::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	xfer->xferUnsignedInt(&m_endFrame);
}

void OathbreakersFadeAwayBehavior::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
	xfer->xferUnsignedInt(&m_endFrame);
}

void OathbreakersFadeAwayBehavior::loadPostProcess() { UpdateModule::loadPostProcess(); }
#endif

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

#if !RTS_GENERALS
const HordeContainModuleData::MeleeBehavior* StancesBehavior::getMeleeBehavior(Int stance) const
{
	const auto* data = TheAttributeModifierStore ?
		TheAttributeModifierStore->findStanceTemplate(getStancesBehaviorModuleData()->m_stanceTemplate) : nullptr;
	if (!data || stance < 0 || stance >= AttributeModifierRules::STANCE_COUNT || data->m_melee[stance].name.isEmpty())
		return nullptr;
	return &data->m_melee[stance];
}
#endif

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
