#include "PreRTS.h"

#include "GameLogic/Module/RotwkParseOnlyModules.h"

#include "Common/AttributeModifierRules.h"
#include "Common/Xfer.h"
#include "GameLogic/Object.h"
#if !RTS_GENERALS
#include "GameLogic/AttributeModifierStore.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
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

#undef DEFINE_PARSE_ONLY_SNAPSHOT

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
