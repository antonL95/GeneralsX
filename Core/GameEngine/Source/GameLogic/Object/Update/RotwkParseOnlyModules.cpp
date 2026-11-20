#include "PreRTS.h"

#include "GameLogic/Module/RotwkParseOnlyModules.h"

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

DEFINE_PARSE_ONLY_SNAPSHOT(StancesBehavior, BehaviorModule)
DEFINE_PARSE_ONLY_SNAPSHOT(LargeGroupAudioUpdate, UpdateModule)
DEFINE_PARSE_ONLY_SNAPSHOT(PickupStuffUpdate, UpdateModule)
DEFINE_PARSE_ONLY_SNAPSHOT(HitReactionBehavior, BehaviorModule)
DEFINE_PARSE_ONLY_SNAPSHOT(HordeMemberCollide, BehaviorModule)
DEFINE_PARSE_ONLY_SNAPSHOT(CastleMemberBehavior, BehaviorModule)

#undef DEFINE_PARSE_ONLY_SNAPSHOT
