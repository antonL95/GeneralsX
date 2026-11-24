#include "PreRTS.h"

#define DEFINE_LOCOMOTORSET_NAMES

#include "Common/BitFlagsIO.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameClient/FXList.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/LogicRandomValue.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/HordeAIUpdate.h"
#include "GameLogic/Module/HordeWorkerAIUpdate.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/HordeAIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Pathfinder/Path.h"
#include "GameLogic/RetailTestCommands.h"
#include "GameLogic/ExperienceTracker.h"
#include "Common/GameState.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailSaveBlocks.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponSet.h"
#include "GameLogic/WeaponSetFlags.h"
#include "Lib/trig.h"
#if !RTS_GENERALS
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/Module/RotwkModifierModules.h"
#include "GameLogic/Module/RotwkParseOnlyModules.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

#if !RTS_GENERALS
Bool HordeContain::allowsBannerSpawn(const Object* horde)
{
	const Object* building = horde;
	while (const Object* container = building->getContainedBy())
		building = container;
	if (!building->isKindOf(KINDOF_STRUCTURE))
		return TRUE;
	for (BehaviorModule** module = building->getBehaviorModules(); *module; ++module)
		if ((*module)->getModuleNameKey() == NAMEKEY("AllowBannerSpawnUpgrade"))
			return (*module)->getUpgrade()->isAlreadyUpgraded();
	return TRUE;
}
#endif

namespace
{
std::vector<std::string> readAttributeTokens(INI* ini)
{
	std::vector<std::string> tokens;
	while (const char* token = ini->getNextTokenOrNull(INI::getSepsColon()))
		tokens.emplace_back(token);
	return tokens;
}

Int weaponConditionIndex(const std::string& name)
{
	if (name.empty())
		return -1;
	return INI::scanIndexList(name.c_str(), WeaponSetFlags::getBitNames());
}

const BannerCarrierUpdateModuleData* findBannerData(const ThingTemplate* type)
{
	if (!type)
		return nullptr;
	const auto& modules = type->getBehaviorModuleInfo();
	for (Int index = 0; index < modules.getCount(); ++index)
		if (modules.getNthName(index).compareNoCase("BannerCarrierUpdate") == 0)
			return static_cast<const BannerCarrierUpdateModuleData*>(modules.getNthData(index));
	return nullptr;
}

bool isAttackingMelee(const Object* member)
{
	const auto* weapon = member->getCurrentWeapon();
	const auto* ai = member->getAI();
	return !member->isEffectivelyDead() && weapon && weapon->isContactWeapon() && ai && ai->isAttacking();
}

bool bannerUpgradeComplete(const Object* source, const BannerCarrierUpdateModuleData& banner)
{
	if (banner.m_upgradeRequired.isEmpty())
		return true;
	const auto* upgrade = TheUpgradeCenter ? TheUpgradeCenter->findUpgrade(banner.m_upgradeRequired) : nullptr;
	if (!upgrade)
		return false;
	const auto* owner = source->getControllingPlayer();
	return upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER ? owner && owner->hasUpgradeComplete(upgrade) : source->hasUpgrade(upgrade);
}
}

HordeContainModuleData::HordeContainModuleData()
{
	SET_ALL_KINDOFMASK_BITS(m_allowInsideKindOf);
	m_randomOffset.x = 0.0f;
	m_randomOffset.y = 0.0f;
	m_leaderPosition.zero();
	m_bannerCarrierHordeDeathType = DEATH_TYPE_FLAGS_NONE;
	m_frontAngle = 360.0f;
	m_meleeAttackLeashDistance = 0.0f;
	m_backUpMinDistance = 0.0f;
	m_backUpMaxDistance = 0.0f;
	m_backupPercentage = 0.0f;
	m_cowerRadius = 0.0f;
	m_visionRearOverride = -1.0f;
	m_visionSideOverride = -1.0f;
	m_backUpMinDelayTime = 0;
	m_backUpMaxDelayTime = 0;
	m_flankedDelay = 0;
	m_flankedDuration = 5 * LOGICFRAMES_PER_SECOND;
	m_minimumHordeSize = 0;
	m_leaderRank = 0;
	m_splitHordeNumber = 0;
	m_forcedLocomotorSet = -1;
	m_bannerCarrierMinLevel = 0;
	m_showPips = FALSE;
	m_thisFormationIsTheMainFormation = FALSE;
	m_bannerCarrierDestroyHordeOnDeath = FALSE;
	m_isPorcupineFormation = FALSE;
	m_machineAllowed = FALSE;
	m_useSlowHordeMovement = FALSE;
	m_rankSplit = FALSE;
	m_notComboFormation = FALSE;
	m_useMarchingAnims = FALSE;
}

void HordeContainModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse dataFieldParse[] =
	{
		{ "InitialPayload", parseInitialPayload, nullptr, 0 },
		{ "Slots", parseSlots, nullptr, 0 },
		{ "ManualPickUpFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof( HordeContainModuleData, m_manualPickUpFilter ) },
		{ "ThisFormationIsTheMainFormation", INI::parseBool, nullptr, offsetof( HordeContainModuleData, m_thisFormationIsTheMainFormation ) },
		{ "RankInfo", parseRankInfo, nullptr, 0 },
		{ "RanksThatStopAdvance", parseRankList, nullptr, offsetof( HordeContainModuleData, m_ranksThatStopAdvance ) },
		{ "RanksToReleaseWhenAttacking", parseRankList, nullptr, offsetof( HordeContainModuleData, m_ranksToReleaseWhenAttacking ) },
		{ "RanksToJustFreeWhenAttacking", parseRankList, nullptr, offsetof( HordeContainModuleData, m_ranksToJustFreeWhenAttacking ) },
		{ "ComboHorde", parseComboHorde, nullptr, offsetof( HordeContainModuleData, m_comboHordes ) },
		{ "AlternateFormation", INI::parseAsciiString, nullptr, offsetof( HordeContainModuleData, m_alternateFormation ) },
		{ "RandomOffset", INI::parseCoord2D, nullptr, offsetof( HordeContainModuleData, m_randomOffset ) },
		{ "LeaderPosition", INI::parseCoord3D, nullptr, offsetof( HordeContainModuleData, m_leaderPosition ) },
		{ "LeadersAllowed", INI::parseAsciiStringVector, nullptr, offsetof( HordeContainModuleData, m_leadersAllowed ) },
		{ "BannerCarrierPosition", parseBannerCarrierPosition, nullptr, offsetof( HordeContainModuleData, m_bannerCarrierPositions ) },
		{ "BannerCarriersAllowed", INI::parseAsciiStringVector, nullptr, offsetof( HordeContainModuleData, m_bannerCarriersAllowed ) },
		{ "BannerCarrierDestroyHordeOnDeath", INI::parseBool, nullptr, offsetof( HordeContainModuleData, m_bannerCarrierDestroyHordeOnDeath ) },
		{ "BannerCarrierHordeDeathType", INI::parseDeathTypeFlags, nullptr, offsetof( HordeContainModuleData, m_bannerCarrierHordeDeathType ) },
		{ "LeaderRank", INI::parseInt, nullptr, offsetof( HordeContainModuleData, m_leaderRank ) },
		{ "BackUpMinDelayTime", INI::parseDurationUnsignedInt, nullptr, offsetof( HordeContainModuleData, m_backUpMinDelayTime ) },
		{ "BackUpMaxDelayTime", INI::parseDurationUnsignedInt, nullptr, offsetof( HordeContainModuleData, m_backUpMaxDelayTime ) },
		{ "BackUpMinDistance", INI::parseReal, nullptr, offsetof( HordeContainModuleData, m_backUpMinDistance ) },
		{ "BackUpMaxDistance", INI::parseReal, nullptr, offsetof( HordeContainModuleData, m_backUpMaxDistance ) },
		{ "BackupPercentage", INI::parsePercentToReal, nullptr, offsetof( HordeContainModuleData, m_backupPercentage ) },
		{ "CowerRadius", INI::parseReal, nullptr, offsetof( HordeContainModuleData, m_cowerRadius ) },
		{ "AttributeModifiers", INI::parseAsciiStringVector, nullptr, offsetof( HordeContainModuleData, m_attributeModifiers ) },
		{ "IsPorcupineFormation", INI::parseBool, nullptr, offsetof( HordeContainModuleData, m_isPorcupineFormation ) },
		{ "ForcedLocomotorSet", INI::parseIndexList, TheLocomotorSetNames, offsetof( HordeContainModuleData, m_forcedLocomotorSet ) },
		{ "MachineAllowed", INI::parseBool, nullptr, offsetof( HordeContainModuleData, m_machineAllowed ) },
		{ "MachineType", INI::parseAsciiString, nullptr, offsetof( HordeContainModuleData, m_machineType ) },
		{ "UseSlowHordeMovement", INI::parseBool, nullptr, offsetof( HordeContainModuleData, m_useSlowHordeMovement ) },
		{ "SplitHorde", parseSplitHorde, nullptr, offsetof( HordeContainModuleData, m_splitHordes ) },
		{ "MeleeAttackLeashDistance", INI::parseReal, nullptr, offsetof( HordeContainModuleData, m_meleeAttackLeashDistance ) },
		{ "EvaEventLastMemberDeath", INI::parseAsciiString, nullptr, offsetof( HordeContainModuleData, m_evaEventLastMemberDeath ) },
		{ "RankSplit", INI::parseBool, nullptr, offsetof( HordeContainModuleData, m_rankSplit ) },
		{ "SplitHordeNumber", INI::parseInt, nullptr, offsetof( HordeContainModuleData, m_splitHordeNumber ) },
		{ "NotComboFormation", INI::parseBool, nullptr, offsetof( HordeContainModuleData, m_notComboFormation ) },
		{ "UseMarchingAnims", INI::parseBool, nullptr, offsetof( HordeContainModuleData, m_useMarchingAnims ) },
		{ "FrontAngle", INI::parseReal, nullptr, offsetof( HordeContainModuleData, m_frontAngle ) },
		{ "FlankedDelay", INI::parseDurationUnsignedInt, nullptr, offsetof( HordeContainModuleData, m_flankedDelay ) },
		{ "FlankedDuration", INI::parseDurationUnsignedInt, nullptr, offsetof( HordeContainModuleData, m_flankedDuration ) },
		{ "MeleeBehavior", parseMeleeBehavior, nullptr, offsetof( HordeContainModuleData, m_meleeBehavior ) },
		{ "MinimumHordeSize", INI::parseUnsignedInt, nullptr, offsetof( HordeContainModuleData, m_minimumHordeSize ) },
		{ "VisionRearOverride", INI::parsePercentToReal, nullptr, offsetof( HordeContainModuleData, m_visionRearOverride ) },
		{ "VisionSideOverride", INI::parsePercentToReal, nullptr, offsetof( HordeContainModuleData, m_visionSideOverride ) },
		{ "BannerCarrierMinLevel", INI::parseUnsignedByte, nullptr, offsetof( HordeContainModuleData, m_bannerCarrierMinLevel ) },
		{ "LivingWorldOverloadTemplate", INI::parseAsciiString, nullptr, offsetof( HordeContainModuleData, m_livingWorldOverloadTemplate ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
	TransportContainModuleData::buildFieldParse(p);
}

void HordeContainModuleData::parseInitialPayload(INI* ini, void* instance, void*, const void*)
{
	HordeContainModuleData* self = static_cast<HordeContainModuleData*>(instance);
	const char* name = ini->getNextToken();
	const char* countToken = ini->getNextTokenOrNull();
	const Int count = countToken ? INI::scanInt(countToken) : 1;
	if (count < 0)
		throw INI_INVALID_DATA;
	self->m_layout.addPayload(name, count);
}

void HordeContainModuleData::parseSlots(INI* ini, void* instance, void*, const void*)
{
	HordeContainModuleData* self = static_cast<HordeContainModuleData*>(instance);
	self->m_slotCapacity = INI::scanInt(ini->getNextToken());
	self->m_layout.setCapacity(self->m_slotCapacity);
}

void HordeContainModuleData::parseRankInfo(INI* ini, void* instance, void*, const void*)
{
	HordeContainModuleData* self = static_cast<HordeContainModuleData*>(instance);
	RankInfo info;
	if (!HordeAttributes::parseRankInfo(readAttributeTokens(ini), info.rank))
		throw INI_INVALID_DATA;
	info.revokedWeaponCondition = weaponConditionIndex(info.rank.revokedWeaponCondition);
	info.grantedWeaponCondition = weaponConditionIndex(info.rank.grantedWeaponCondition);
	self->m_layout.addRank(info.rank);
	self->m_rankInfos.push_back(std::move(info));
}

void HordeContainModuleData::parseBannerCarrierPosition(INI* ini, void*, void* store, const void*)
{
	HordeBannerCarrierPosition banner;
	if (!HordeAttributes::parseBannerCarrierPosition(readAttributeTokens(ini), banner))
		throw INI_INVALID_DATA;
	static_cast<std::vector<HordeBannerCarrierPosition>*>(store)->push_back(std::move(banner));
}

void HordeContainModuleData::parseSplitHorde(INI* ini, void*, void* store, const void*)
{
	HordeSplit split;
	if (!HordeAttributes::parseSplit(readAttributeTokens(ini), split))
		throw INI_INVALID_DATA;
	static_cast<std::vector<HordeSplit>*>(store)->push_back(std::move(split));
}

void HordeContainModuleData::parseComboHorde(INI* ini, void*, void* store, const void*)
{
	HordeCombo combo;
	if (!HordeAttributes::parseCombo(readAttributeTokens(ini), combo))
		throw INI_INVALID_DATA;
	static_cast<std::vector<HordeCombo>*>(store)->push_back(std::move(combo));
}

void HordeContainModuleData::parseRankList(INI* ini, void*, void* store, const void*)
{
	std::vector<Int>* ranks = static_cast<std::vector<Int>*>(store);
	ranks->clear();
	while (const char* token = ini->getNextTokenOrNull())
		ranks->push_back(atoi(token));
}

void HordeContainModuleData::parseMeleeBehavior(INI* ini, void*, void* store, const void*)
{
	static const FieldParse fields[] =
	{
		{ "FacingBonus", INI::parseReal, nullptr, offsetof( MeleeBehavior, facingBonus ) },
		{ "AngleLimitCos", INI::parseReal, nullptr, offsetof( MeleeBehavior, angleLimitCos ) },
		{ "InnerRange", INI::parseReal, nullptr, offsetof( MeleeBehavior, innerRange ) },
		{ "OuterRange", INI::parseReal, nullptr, offsetof( MeleeBehavior, outerRange ) },
		{ "OuterRangeBuildings", INI::parseReal, nullptr, offsetof( MeleeBehavior, outerRangeBuildings ) },
		{ "IdleModelConditions", ModelConditionFlags::parseFromINI, nullptr, offsetof( MeleeBehavior, idleModelConditions ) },
		{ "DelayUntilIdle", INI::parseDurationUnsignedInt, nullptr, offsetof( MeleeBehavior, delayUntilIdle ) },
		{ "DelayRandomActivateMin", INI::parseDurationUnsignedInt, nullptr, offsetof( MeleeBehavior, delayRandomActivateMin ) },
		{ "DelayRandomActivateMax", INI::parseDurationUnsignedInt, nullptr, offsetof( MeleeBehavior, delayRandomActivateMax ) },
		{ nullptr, nullptr, nullptr, 0 }
	};

	MeleeBehavior behavior;
	behavior.name = ini->getNextAsciiString();
	ini->initFromINI(&behavior, fields);
	*static_cast<MeleeBehavior*>(store) = std::move(behavior);
}

void AODHordeContainModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse fields[] =
	{
		{ "FrequencyScale", INI::parseReal, nullptr, offsetof(AODHordeContainModuleData, m_frequencyScale) },
		{ "FrequencyRandomness", INI::parseReal, nullptr, offsetof(AODHordeContainModuleData, m_frequencyRandomness) },
		{ "AmplitudeScale", INI::parseReal, nullptr, offsetof(AODHordeContainModuleData, m_amplitudeScale) },
		{ "AmplitudeRandomness", INI::parseReal, nullptr, offsetof(AODHordeContainModuleData, m_amplitudeRandomness) },
		{ "StillAmplitude", INI::parseReal, nullptr, offsetof(AODHordeContainModuleData, m_stillAmplitude) },
		{ "FrequencyScaleZ", INI::parseReal, nullptr, offsetof(AODHordeContainModuleData, m_frequencyScaleZ) },
		{ "FrequencyRandomnessZ", INI::parseReal, nullptr, offsetof(AODHordeContainModuleData, m_frequencyRandomnessZ) },
		{ "AmplitudeScaleZ", INI::parseReal, nullptr, offsetof(AODHordeContainModuleData, m_amplitudeScaleZ) },
		{ "AmplitudeRandomnessZ", INI::parseReal, nullptr, offsetof(AODHordeContainModuleData, m_amplitudeRandomnessZ) },
		{ "StillAmplitudeZ", INI::parseReal, nullptr, offsetof(AODHordeContainModuleData, m_stillAmplitudeZ) },
		{ "OathFulfilledZFactor", INI::parseReal, nullptr, offsetof(AODHordeContainModuleData, m_oathFulfilledZFactor) },
		{ "LargeUnitHeightFactor", INI::parseReal, nullptr, offsetof(AODHordeContainModuleData, m_largeUnitHeightFactor) },
		{ "LargeUnitMinHeight", INI::parseReal, nullptr, offsetof(AODHordeContainModuleData, m_largeUnitMinHeight) },
		{ "LargeUnitMaxHeight", INI::parseReal, nullptr, offsetof(AODHordeContainModuleData, m_largeUnitMaxHeight) },
		{ "LargeUnitTimeout", INI::parseDurationUnsignedInt, nullptr, offsetof(AODHordeContainModuleData, m_largeUnitTimeout) },
		{ "LargeUnitTailOff", INI::parseReal, nullptr, offsetof(AODHordeContainModuleData, m_largeUnitTailOff) },
		{ "ScatterSpeedFactor", INI::parseReal, nullptr, offsetof(AODHordeContainModuleData, m_scatterSpeedFactor) },
		{ "ScatterRandomness", INI::parseReal, nullptr, offsetof(AODHordeContainModuleData, m_scatterRandomness) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
	HordeContainModuleData::buildFieldParse(p);
}

HorseHordeContain::~HorseHordeContain() = default;
void HorseHordeContain::crc(Xfer* xfer) { HordeContain::crc(xfer); }
void HorseHordeContain::xfer(Xfer* xfer)
{
	XferVersion version = 2;
	xfer->xferVersion(&version, 2);
	m_legacySnapshot = version == 1;
	if (m_legacySnapshot)
		ObjectModule::xfer(xfer);
	else
		HordeContain::xfer(xfer);
}

void HorseHordeContain::loadPostProcess()
{
	if (m_legacySnapshot)
		m_payloadCreated = TRUE;
	HordeContain::loadPostProcess();
}

AODHordeContain::~AODHordeContain() = default;
void AODHordeContain::crc(Xfer* xfer) { BehaviorModule::crc(xfer); }
void AODHordeContain::xfer(Xfer* xfer) { BehaviorModule::xfer(xfer); }
void AODHordeContain::loadPostProcess() { BehaviorModule::loadPostProcess(); }

HordeContain::HordeContain( Thing *thing, const ModuleData *moduleData ) : TransportContain( thing, moduleData )
{
	m_payloadCreated = FALSE;
}

HordeContain::~HordeContain()
{
}

Bool HordeContain::isValidContainerFor( const Object* rider, Bool checkCapacity ) const
{
	if (rider == nullptr || rider == getObject())
		return FALSE;
	if (rider->getControllingPlayer() != getObject()->getControllingPlayer())
		return FALSE;
	if (!OpenContain::isValidContainerFor(rider, checkCapacity))
		return FALSE;
	if (checkCapacity && static_cast<Int>(getContainCount()) - (getBannerCarrier() ? 1 : 0) >= getContainMax())
		return FALSE;

	const SpecialPowerObjectFilter& filter = getHordeContainModuleData()->m_passengerFilter;
	const auto matchesRider = [rider](const AsciiString& name)
	{
		return stricmp(rider->getTemplate()->getName().str(), name.str()) == 0;
	};
	if (rider->isAnyKindOf(filter.m_excludeKinds) ||
		std::any_of(filter.m_excludeObjects.begin(), filter.m_excludeObjects.end(), matchesRider))
		return FALSE;
	if (filter.m_includeKinds.any() || !filter.m_includeObjects.empty())
		return rider->isAnyKindOf(filter.m_includeKinds) ||
			std::any_of(filter.m_includeObjects.begin(), filter.m_includeObjects.end(), matchesRider);
	return (filter.m_rules & SpecialPowerObjectFilter::RULE_NONE) == 0;
}

void HordeContain::onObjectCreated()
{
	TransportContain::onObjectCreated();
	if (getHordeContainModuleData()->m_alternateFormation.isNotEmpty())
		updateFormationModelConditions(getObject());
}

void HordeContain::onDrawableBoundToObject()
{
	if (getObject()->getDrawable() && getHordeContainModuleData()->m_alternateFormation.isNotEmpty())
		updateFormationModelConditions(getObject());
}

void HordeContain::createPayload()
{
	const HordeContainModuleData* data = getHordeContainModuleData();
	const HordeLayout& layout = data->m_layout;
	Object* horde = getObject();
#if !RTS_GENERALS
	RotwkModifierModules::applyHordeFormationModifiers( horde );
#endif
	const Int availableSlots = std::max(0, getContainMax()) - static_cast<Int>(getContainCount());

	enableLoadSounds( FALSE );
	for (const HordeMemberSlot& entry : layout.initialMembers(availableSlots))
	{
		const HordePayload& payload = layout.payloads()[entry.payload];
		const ThingTemplate* memberTemplate = TheThingFactory->findTemplate( payload.name.c_str() );
		if (memberTemplate == nullptr)
		{
			DEBUG_CRASH(( "HordeContain: '%s' has an unknown payload '%s'", horde->getTemplate()->getName().str(), payload.name.c_str() ));
			continue;
		}
		Object* member = TheThingFactory->newObject( memberTemplate, horde->getTeam() );
#if RTS_GENERALS
		OpenContain::addToContain(member);
#else
		OpenContain::addToContain(member, FALSE);
#endif
		const Coord3D position = getMemberPosition(member->getID(), FALSE);
		member->setPosition(&position);
		member->setOrientation(horde->getOrientation());
	}
	enableLoadSounds( TRUE );

	m_payloadCreated = TRUE;
	if (RetailFileLayers::enabled())
	{
		onRankAdvanced(horde->getExperienceTracker()->getRank());
#if !RTS_GENERALS
		if (const Drawable* drawable = horde->getDrawable())
			for (Object* member : getContainList())
				if (Drawable* memberDrawable = member->getDrawable())
					memberDrawable->inheritRetailOclFadeIn(*drawable);
#endif
	}
}

void HordeContain::suppressInitialPayload()
{
#if !RTS_GENERALS
	RotwkModifierModules::applyHordeFormationModifiers(getObject());
#endif
	m_payloadCreated = TRUE;
	m_initialPayloadPending = TRUE;
}

void HordeContain::finishInitialPayload()
{
	m_initialPayloadPending = FALSE;
}

Int HordeContain::getInitialPayloadCount() const
{
	const Int availableSlots = std::max(0, getContainMax()) - static_cast<Int>(getContainCount());
	return static_cast<Int>(getActiveFormation()->m_layout.initialMembers(availableSlots).size());
}

const ThingTemplate* HordeContain::getInitialPayloadTemplate(Int index) const
{
	if (index < 0)
		return nullptr;
	const HordeLayout& layout = getActiveFormation()->m_layout;
	const auto members = layout.initialMembers(std::max(0, getContainMax()));
	if (static_cast<std::size_t>(index) >= members.size())
		return nullptr;
	return TheThingFactory->findTemplate(layout.payloads()[members[index].payload].name.c_str());
}

void HordeContain::addProducedMember(Object* member)
{
	if (!member)
		return;
	enableLoadSounds(FALSE);
#if RTS_GENERALS
	OpenContain::addToContain(member);
#else
	OpenContain::addToContain(member, FALSE);
#endif
	enableLoadSounds(TRUE);
}

void HordeContain::addToContainList( Object *rider )
{
	TransportContain::addToContainList( rider );
	if (isBannerCarrier(rider) || getMemberSlot( rider->getID() ) >= 0)
		return;

	const HordeContainModuleData* formation = getActiveFormation();
	const HordeLayout& layout = formation->m_layout;
	m_hadMembers = TRUE;
	m_slotMembers.resize( layout.slots().size(), INVALID_ID );
	m_slotOffsets.resize(layout.slots().size(), Coord2D{0.0f, 0.0f});
	std::vector<bool> occupied( m_slotMembers.size(), false );
	for (size_t index = 0; index < m_slotMembers.size(); ++index)
		occupied[index] = m_slotMembers[index] != INVALID_ID;
	const Int slot = layout.findSlot( rider->getTemplate()->getName().str(), occupied );
	if (slot >= 0)
	{
		m_slotMembers[slot] = rider->getID();
		m_memberSlots.emplace(rider->getID(), slot);
		if (!TheGameState || !TheGameState->isInLoadGame())
		{
			const Coord2D& extent = formation->m_randomOffset;
			m_slotOffsets[slot].x = extent.x > 0.0f ? GameLogicRandomValueReal(-extent.x, extent.x) : 0.0f;
			m_slotOffsets[slot].y = extent.y > 0.0f ? GameLogicRandomValueReal(-extent.y, extent.y) : 0.0f;
		}
	}
}

#if RTS_GENERALS
void HordeContain::onContaining(Object* rider)
{
	OpenContain::onContaining(rider);
#else
void HordeContain::onContaining(Object* rider, Bool wasSelected)
{
	OpenContain::onContaining(rider, wasSelected);
#endif
#if !RTS_GENERALS
	if( AttributeModifierHelper* helper = rider->getAttributeModifierHelper() )
		helper->refreshDerived();
#endif
	rider->setStatus( getHordeContainModuleData()->m_objectStatusOfContained );
	if (RetailFileLayers::enabled())
	{
		if (Object* outer = getObject()->getContainedBy())
		{
			if (ContainModuleInterface* outerContain = outer->getContain())
			{
				if (const OpenContain* outerOpen = outerContain->asOpenContain())
				{
					const ObjectStatusMaskType& outerStatus = outerOpen->getRetailOpenContainData()->m_objectStatusOfContained;
					if (outerStatus.any())
						rider->setStatus(outerStatus);
				}
				if (getObject()->isDisabledByType(DISABLED_HELD))
					rider->setDisabled(DISABLED_HELD);
				if (outerContain->isGarrisonable())
					rider->setWeaponBonusCondition(WEAPONBONUSCONDITION_GARRISONED);
				if (outerContain->isEnclosingContainerFor(getObject()))
				{
					if (rider->getDrawable())
						rider->getDrawable()->setDrawableHidden(true);
					ThePartitionManager->unRegisterObject(rider);
					TheAI->pathfinder()->removeObjectFromPathfindMap(rider);
				}
				rider->setPosition(outer->getPosition());
			}
		}
	}
	if (isBannerCarrier(rider))
	{
		m_replenishmentBanner = rider->getTemplate()->getName();
		m_bannerRespawnFrame = 0;
		m_unitSpawnFrame = 0;
		if (auto* banner = BannerCarrierUpdate::find(rider))
			banner->applyMorph(getBannerUnitType());
	}
	if (getHordeContainModuleData()->m_alternateFormation.isNotEmpty())
		updateFormationModelConditions(rider);
	WeaponSetFlags toggles;
	for (const WeaponSetType flag : { WEAPONSET_TOGGLE_1, WEAPONSET_TOGGLE_2, WEAPONSET_TOGGLE_3 })
		toggles.set(flag, getObject()->testWeaponSetFlag(flag));
	if (toggles.any())
		rider->setWeaponSetFlags(toggles, TRUE);
	updateRankWeaponConditions(rider);
	if (RetailFileLayers::enabled() && (!TheGameState || !TheGameState->isInLoadGame()))
		rider->getExperienceTracker()->setMinRetailRank(getObject()->getExperienceTracker()->getRank());
	const UpgradeMaskType hordeUpgrades = getObject()->getObjectCompletedUpgradeMask();
	if (hordeUpgrades.any() && TheUpgradeCenter)
	{
		for (const UpgradeTemplate* upgrade = TheUpgradeCenter->firstUpgradeTemplate(); upgrade; upgrade = upgrade->friend_getNext())
		{
			if (upgrade->getUpgradeType() == UPGRADE_TYPE_OBJECT &&
				hordeUpgrades.testForAny(upgrade->getUpgradeMask()) &&
				!rider->hasUpgrade(upgrade))
				rider->giveUpgrade(upgrade);
		}
	}
}

#if RTS_GENERALS
Bool HordeContain::isPassengerAllowedToFire() const
#else
Bool HordeContain::isPassengerAllowedToFire(ObjectID) const
#endif
{
	const Object* outer = RetailFileLayers::enabled() ? getObject()->getContainedBy() : nullptr;
	return !outer || outer->getContain()->isPassengerAllowedToFire();
}

void HordeContain::onRemoving( Object *rider )
{
	if (isBannerCarrier(rider))
	{
		const auto* data = findBannerData(rider->getTemplate());
		const auto* formation = getHordeContainModuleData();
		if (rider->isEffectivelyDead() && formation->m_bannerCarrierDestroyHordeOnDeath)
			m_bannerRespawnFrame = TheGameLogic->getFrame();
		else if (rider->isEffectivelyDead() && data && data->m_diedRespawnTime)
			m_bannerRespawnFrame = TheGameLogic->getFrame() + data->m_diedRespawnTime;
		else
		{
			m_replenishmentBanner.clear();
			m_bannerRespawnFrame = 0;
		}
		m_unitSpawnFrame = 0;
		if (auto* banner = BannerCarrierUpdate::find(rider))
			banner->applyMorph(AsciiString());
	}
	const auto slot = m_memberSlots.find(rider->getID());
	if (slot != m_memberSlots.end())
	{
		m_slotMembers[slot->second] = INVALID_ID;
		m_memberSlots.erase(slot);
	}
	if (m_backingMoves.erase(rider->getID()))
	{
		rider->clearModelConditionState(MODELCONDITION_BACKING_UP);
		if (m_backingMoves.empty())
			m_backingSource = INVALID_ID;
	}
	updateRankWeaponConditions(rider);
	rider->setStatus( getHordeContainModuleData()->m_objectStatusOfContained, false );
	OpenContain::onRemoving( rider );
}

void HordeContain::onCapture( Player *oldOwner, Player *newOwner )
{
	if (newOwner == nullptr || oldOwner == newOwner)
		return;
	for (Object* member : getContainList())
		member->setTeam( newOwner->getDefaultTeam() );
}

Int HordeContain::getMemberSlot( ObjectID id ) const
{
	const auto slot = m_memberSlots.find(id);
	return slot == m_memberSlots.end() ? -1 : slot->second;
}

void HordeContain::rebuildMemberSlots()
{
	m_memberSlots.clear();
	for (std::size_t slot = 0; slot < m_slotMembers.size(); ++slot)
	{
		if (m_slotMembers[slot] != INVALID_ID)
			m_memberSlots.emplace(m_slotMembers[slot], static_cast<Int>(slot));
	}
}

void HordeContain::crc( Xfer *xfer )
{
	TransportContain::crc( xfer );
	xfer->xferBool(&m_hadMembers);
	Bool switched = m_formationOverride != nullptr;
	xfer->xferBool(&switched);
	for (size_t index = 0; index < m_slotMembers.size(); ++index)
	{
		xfer->xferObjectID(&m_slotMembers[index]);
		xfer->xferCoord2D(&m_slotOffsets[index]);
	}
	if (RetailFileLayers::enabled())
	{
		xfer->xferAsciiString(&m_replenishmentBanner);
		xfer->xferUnsignedInt(&m_bannerRespawnFrame);
		xfer->xferUnsignedInt(&m_unitSpawnFrame);
		xfer->xferUnsignedInt(&m_lastMeleeFrame);
		xfer->xferUnsignedInt(&m_lastMemberDamageFrame);
		xferRetailFormationCombat(xfer);
		xferRetailBlocks(xfer, TRUE);
	}
}

void HordeContain::xferRetailBlocks(Xfer* xfer, Bool present)
{
	RetailSaveBlocks::transferSection<Xfer>(xfer, xfer->getXferMode() == XFER_LOAD, present, {
		{"initial-payload-pending", [this](Xfer* x) { x->xferBool(&m_initialPayloadPending); },
			[this] { m_initialPayloadPending = FALSE; }}
	});
}

void HordeContain::xferRetailFormationCombat(Xfer* xfer, Bool includeBackingSource)
{
	const Bool loading = xfer->getXferMode() == XFER_LOAD;
	UnsignedInt count = static_cast<UnsignedInt>(m_flankHeadings.size());
	xfer->xferUnsignedInt(&count);
	if (loading)
		m_flankHeadings.resize(count);
	for (Real& heading : m_flankHeadings)
		xfer->xferReal(&heading);

	count = static_cast<UnsignedInt>(m_flankedUntil.size());
	xfer->xferUnsignedInt(&count);
	if (loading)
	{
		m_flankedUntil.clear();
		for (UnsignedInt index = 0; index < count; ++index)
		{
			ObjectID attacker = INVALID_ID;
			UnsignedInt until = 0;
			xfer->xferObjectID(&attacker);
			xfer->xferUnsignedInt(&until);
			m_flankedUntil.emplace(attacker, until);
		}
	}
	else
	{
		for (auto [attacker, until] : m_flankedUntil)
		{
			ObjectID id = attacker;
			xfer->xferObjectID(&id);
			xfer->xferUnsignedInt(&until);
		}
	}

	count = static_cast<UnsignedInt>(m_backingMoves.size());
	xfer->xferUnsignedInt(&count);
	if (loading)
	{
		m_backingMoves.clear();
		for (UnsignedInt index = 0; index < count; ++index)
		{
			ObjectID member = INVALID_ID;
			BackingMove move{};
			xfer->xferObjectID(&member);
			xfer->xferCoord3D(&move.goal);
			xfer->xferUnsignedInt(&move.start);
			xfer->xferUnsignedInt(&move.deadline);
			m_backingMoves.emplace(member, move);
		}
	}
	else
	{
		for (auto [member, move] : m_backingMoves)
		{
			ObjectID id = member;
			xfer->xferObjectID(&id);
			xfer->xferCoord3D(&move.goal);
			xfer->xferUnsignedInt(&move.start);
			xfer->xferUnsignedInt(&move.deadline);
		}
	}
	if (includeBackingSource)
		xfer->xferObjectID(&m_backingSource);
}

void HordeContain::xfer( Xfer *xfer )
{
	XferVersion currentVersion = RetailFileLayers::enabled() ? 8 : 3;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	TransportContain::xfer( xfer );

	xfer->xferSTLObjectIDVector( &m_slotMembers );
	m_slotOffsets.resize(m_slotMembers.size(), Coord2D{0.0f, 0.0f});
	if (version >= 2)
	{
		for (Coord2D& offset : m_slotOffsets)
			xfer->xferCoord2D(&offset);
		xfer->xferBool(&m_hadMembers);
	}
	else
		m_restoreHadMembers = TRUE;
	Bool switched = m_formationOverride != nullptr;
	if (version >= 3)
		xfer->xferBool(&switched);
	if (version >= 4)
	{
		xfer->xferAsciiString(&m_replenishmentBanner);
		xfer->xferUnsignedInt(&m_bannerRespawnFrame);
		xfer->xferUnsignedInt(&m_unitSpawnFrame);
		xfer->xferUnsignedInt(&m_lastMeleeFrame);
	}
	else if (xfer->getXferMode() == XFER_LOAD)
	{
		m_replenishmentBanner.clear();
		m_bannerRespawnFrame = 0;
		m_unitSpawnFrame = 0;
		m_lastMeleeFrame = 0;
	}
	if (version >= 5)
		xfer->xferUnsignedInt(&m_lastMemberDamageFrame);
	else if (xfer->getXferMode() == XFER_LOAD)
		m_lastMemberDamageFrame = 0;
	if (version >= 6)
		xferRetailFormationCombat(xfer, version >= 7);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, version >= 8);
	if (xfer->getXferMode() == XFER_LOAD)
	{
		m_formationOverride = switched ? findAlternateFormation() : nullptr;
		m_restoreBaseFormation = switched && !m_formationOverride;
		const auto slotCount = getActiveFormation()->m_layout.slots().size();
		if (m_restoreBaseFormation || (!m_slotMembers.empty() && m_slotMembers.size() != slotCount))
		{
			m_slotMembers.assign(slotCount, INVALID_ID);
			m_slotOffsets.assign(slotCount, Coord2D{0.0f, 0.0f});
		}
		rebuildMemberSlots();
	}
}

void HordeContain::loadPostProcess()
{
	TransportContain::loadPostProcess();
	for (Object* member : getContainList())
		updateRankWeaponConditions(member);
	for (Object* member : getContainList())
		if (isBannerCarrier(member))
		{
			if (m_replenishmentBanner.isEmpty() && !member->isEffectivelyDead())
				m_replenishmentBanner = member->getTemplate()->getName();
			if (auto* banner = BannerCarrierUpdate::find(member))
				banner->applyMorph(getBannerUnitType());
		}
#if !RTS_GENERALS
	if (m_restoreBaseFormation)
	{
		Object* horde = getObject();
		if (AttributeModifierHelper* helper = horde->getAttributeModifierHelper())
		{
			const auto owner = AttributeModifiers::ownerOf(horde, getModuleTagNameKey());
			std::vector<Int> formationLists;
			for (const AsciiString& name : getFormationModifiers())
				formationLists.push_back(AttributeModifiers::findList(name));

			std::vector<Int> staleLists;
			for (const auto& active : helper->getStack().active())
			{
				if (std::find(formationLists.begin(), formationLists.end(), active.list) == formationLists.end() &&
					std::any_of(active.holds.begin(), active.holds.end(),
						[owner](const auto& hold) { return hold.owner == owner; }))
					staleLists.push_back(active.list);
			}
			for (Int list : staleLists)
				helper->remove(list, owner);
			RotwkModifierModules::applyHordeFormationModifiers(horde);
		}
	}
#endif
	if (m_restoreBaseFormation || getHordeContainModuleData()->m_alternateFormation.isNotEmpty())
	{
		updateFormationModelConditions(getObject());
		for (Object* member : getContainList())
			updateFormationModelConditions(member);
	}
	m_restoreBaseFormation = FALSE;
	if (m_restoreHadMembers)
	{
		m_hadMembers = getContainCount() != 0;
		m_restoreHadMembers = FALSE;
	}
}

const HordeContain* HordeContain::get(const Object* object)
{
	const ContainModuleInterface* contain = object ? object->getContain() : nullptr;
	return contain && contain->isHordeContain() ? static_cast<const HordeContain*>(contain) : nullptr;
}

Object* HordeContain::getAnchor(Object* member)
{
	Object* anchor = member ? member->getContainedBy() : nullptr;
	return get(anchor) ? anchor : nullptr;
}

Bool HordeContain::isFlanked(const Object* victim, const Object* attacker, Bool record)
{
	if (!RetailFileLayers::enabled() || !victim || !attacker)
		return FALSE;
	const Object* defender = get(victim) ? victim : victim->getContainedBy();
	const Object* source = get(attacker) ? attacker : attacker->getContainedBy();
	auto* contain = const_cast<HordeContain*>(get(defender));
	if (!contain || !get(source) || defender->getRelationship(source) != ENEMIES)
		return FALSE;

	const auto& data = *contain->getActiveFormation();
	if (data.m_frontAngle >= 360.0f)
		return FALSE;
	const UnsignedInt frame = TheGameLogic->getFrame();
	const ObjectID id = source->getID();
	const auto active = contain->m_flankedUntil.find(id);
	if (active != contain->m_flankedUntil.end() && frame < active->second)
		return TRUE;

	const Coord3D& origin = *defender->getPosition();
	const Coord3D& from = *source->getPosition();
	const Real dx = from.x - origin.x;
	const Real dy = from.y - origin.y;
	const Real length = WWMath::SqrtfOrigin(dx * dx + dy * dy);
	if (length <= 0.0f)
		return FALSE;
	const Real limit = length * Cos(std::max(0.0f, data.m_frontAngle) * PI / 360.0f);
	const auto outsideFront = [&](Real facing) {
		return dx * Cos(facing) + dy * Sin(facing) < limit;
	};
	const Bool flanked = outsideFront(defender->getOrientation()) ||
		std::any_of(contain->m_flankHeadings.begin(), contain->m_flankHeadings.end(), outsideFront);
	if (flanked && record && data.m_flankedDuration > 0)
		contain->m_flankedUntil[id] = frame + data.m_flankedDuration;
	return flanked;
}

void HordeContain::updateFlankState()
{
	const auto& data = *getActiveFormation();
	if (data.m_frontAngle >= 360.0f)
	{
		m_flankHeadings.clear();
		m_flankedUntil.clear();
		return;
	}
	if (data.m_flankedDelay > 0)
		m_flankHeadings.push_back(getObject()->getOrientation());
	while (m_flankHeadings.size() > data.m_flankedDelay)
		m_flankHeadings.pop_front();

	const UnsignedInt frame = TheGameLogic->getFrame();
	for (auto it = m_flankedUntil.begin(); it != m_flankedUntil.end(); )
	{
		if (frame >= it->second)
			it = m_flankedUntil.erase(it);
		else
			++it;
	}
}

Bool HordeContain::beginBacking(const Object* source, Bool uncontrollable)
{
	const auto& data = *getActiveFormation();
	const Real minDistance = std::max(0.0f, data.m_backUpMinDistance);
	const Real maxDistance = std::max(minDistance, data.m_backUpMaxDistance);
	if (maxDistance == 0.0f || !source)
		return FALSE;
	if (!m_backingMoves.empty())
		return TRUE;

	const UnsignedInt frame = TheGameLogic->getFrame();
	const UnsignedInt minDelay = data.m_backUpMinDelayTime;
	const UnsignedInt maxDelay = std::max(minDelay, data.m_backUpMaxDelayTime);
	for (Object* member : getContainList())
	{
		AIUpdateInterface* ai = member->getAI();
		if (!ai || !member->isMobile() || member->isEffectivelyDead() || !ai->isDoingGroundMovement())
			continue;
		const Bool guaranteed = uncontrollable || member->testStatus(OBJECT_STATUS_UNCONTROLLABLY_SCARED) ||
			getObject()->testStatus(OBJECT_STATUS_UNCONTROLLABLY_SCARED) || data.m_cowerRadius > 0.0f;
		if (!guaranteed && (data.m_backupPercentage <= 0.0f ||
			(data.m_backupPercentage < 1.0f && GameLogicRandomValueReal(0.0f, 1.0f) > data.m_backupPercentage)))
			continue;

		const Real distance = PATHFIND_CELL_SIZE_F * (minDistance == maxDistance ? minDistance :
			GameLogicRandomValueReal(minDistance, maxDistance));
		const UnsignedInt delay = minDelay == maxDelay ? minDelay : GameLogicRandomValue(minDelay, maxDelay);
		const Coord3D& position = *member->getPosition();
		const Coord3D& threat = *source->getPosition();
		const Real dx = position.x - threat.x;
		const Real dy = position.y - threat.y;
		const Real length = WWMath::SqrtfOrigin(dx * dx + dy * dy);
		if (length <= 0.0f || distance <= 0.0f)
			continue;
		const Real scale = distance / length;
		Coord3D goal{position.x + dx * scale, position.y + dy * scale, position.z};
		goal.z = TheTerrainLogic->getLayerHeight(goal.x, goal.y, member->getLayer());
		const UnsignedInt start = frame + delay;
#if RTS_GENERALS
		const Real speed = std::max(0.1f, ai->getCurLocomotorSpeed());
#else
		const Locomotor* locomotor = ai->getCurLocomotor();
		const Real speed = locomotor ? locomotor->getBackingUpSpeed() : 0.0f;
		const Real largestComponent = (std::max(fabsf(dx), fabsf(dy)) / length) * speed;
		if (!(largestComponent >= PhysicsBehavior::MIN_VELOCITY_COMPONENT))
			continue;
#endif
		const double maxDeadline = std::numeric_limits<UnsignedInt>::max();
		const double deadline = std::min(maxDeadline, static_cast<double>(start) + LOGICFRAMES_PER_SECOND + distance / speed);
		m_backingMoves.emplace(member->getID(), BackingMove{goal, start, static_cast<UnsignedInt>(deadline)});
	}
	if (!m_backingMoves.empty())
		m_backingSource = source->getID();
	return TRUE;
}

HordeContain::BackingPhase HordeContain::updateBacking(Object* member)
{
	const auto it = m_backingMoves.find(member->getID());
	const UnsignedInt frame = TheGameLogic->getFrame();
	if (it == m_backingMoves.end() || frame < it->second.start)
		return BackingPhase::Available;

	FormationGoal goal;
	if (frame >= it->second.deadline || !getFormationGoal(member, goal) ||
		goal.error < 2.0f || !canSteerToFormationGoal(member, goal))
	{
		m_backingMoves.erase(it);
		if (m_backingMoves.empty())
			m_backingSource = INVALID_ID;
		member->clearModelConditionState(MODELCONDITION_BACKING_UP);
		return BackingPhase::Recovered;
	}

	AIUpdateInterface* ai = member->getAI();
	if (!ai->isIdle())
		ai->aiIdle(CMD_FROM_SCRIPT);
	member->setModelConditionState(MODELCONDITION_BACKING_UP);
	ai->wakeForFormation();
	return BackingPhase::Backing;
}

void HordeContain::applyWeaponDamage(Object* victim, DamageInfo& damage, Bool contactHit)
{
	Object* anchor = contactHit && RetailFileLayers::enabled() ? getAnchor(victim) : nullptr;
	const ObjectID anchorID = anchor ? anchor->getID() : INVALID_ID;
	victim->attemptDamage(&damage);
	if (anchorID != INVALID_ID && damage.out.m_actualDamageClipped > 0.0f)
		if (auto* horde = const_cast<HordeContain*>(get(TheGameLogic->findObjectByID(anchorID))))
			horde->m_lastMeleeFrame = TheGameLogic->getFrame();
}

Bool HordeContain::canAddCrateBanner(const Object* anchor)
{
	const auto* contain = get(anchor);
	if (!RetailFileLayers::enabled() || !contain || anchor->isEffectivelyDead() || contain->getBannerCarrier())
		return FALSE;
	const auto* data = contain->getHordeContainModuleData();
	for (const AsciiString& name : data->m_bannerCarriersAllowed)
	{
		if (contain->m_bannerRespawnFrame && name.compareNoCase(contain->m_replenishmentBanner) != 0)
			continue;
		const ThingTemplate* type = TheThingFactory->findTemplate(name);
		if (type && type->isKindOf(KINDOF_BANNER))
			return TRUE;
	}
	return FALSE;
}

Bool HordeContain::addCrateBanner(Object* anchor)
{
	auto* contain = const_cast<HordeContain*>(get(anchor));
	return contain && contain->addBannerCarrier(TRUE);
}

Bool HordeContain::isBannerCarrier(const Object* member) const
{
	if (!RetailFileLayers::enabled() || !member || !member->isKindOf(KINDOF_BANNER))
		return FALSE;
	const auto& carriers = getHordeContainModuleData()->m_bannerCarriersAllowed;
	const AsciiString name = member->getTemplate()->getName();
	return std::any_of(carriers.begin(), carriers.end(), [&name](const AsciiString& carrier) {
		return carrier.compareNoCase(name) == 0;
	});
}

const Object* HordeContain::getBannerCarrier() const
{
	for (const Object* member : getContainList())
		if (isBannerCarrier(member) && !member->isEffectivelyDead())
			return member;
	return nullptr;
}

void HordeContain::onRankAdvanced(Int rank, Bool feedback)
{
	const auto* data = getHordeContainModuleData();
	if (RetailFileLayers::enabled() && m_payloadCreated && getContainCount() > 0 && !m_bannerRespawnFrame &&
		rank >= std::max<Int>(2, data->m_bannerCarrierMinLevel))
		addBannerCarrier(feedback);
}

Bool HordeContain::addBannerCarrier(Bool feedback)
{
	Object* anchor = getObject();
	if (!RetailFileLayers::enabled() || anchor->isEffectivelyDead() || getBannerCarrier())
		return FALSE;
#if !RTS_GENERALS
	if (!allowsBannerSpawn(anchor))
		return FALSE;
#endif
	const auto* data = getHordeContainModuleData();
	for (const AsciiString& name : data->m_bannerCarriersAllowed)
	{
		if (m_bannerRespawnFrame && name.compareNoCase(m_replenishmentBanner) != 0)
			continue;
		const ThingTemplate* type = TheThingFactory->findTemplate(name);
		if (!type || !type->isKindOf(KINDOF_BANNER))
			continue;
		Object* banner = TheThingFactory->newObject(type, anchor->getTeam());
#if RTS_GENERALS
		OpenContain::addToContain(banner);
#else
		OpenContain::addToContain(banner, FALSE);
#endif
		if (banner->getContainedBy() != anchor)
		{
			TheGameLogic->destroyObject(banner);
			return FALSE;
		}
		const Coord3D point = getMemberPosition(banner->getID(), FALSE);
		banner->setPosition(&point);
		banner->setOrientation(anchor->getOrientation());
		if (feedback)
			if (const auto* update = BannerCarrierUpdate::find(banner))
				update->playMorphFX(anchor);
		return TRUE;
	}
	return FALSE;
}

void HordeContain::updateReplenishment()
{
	if (m_replenishmentBanner.isEmpty() || !m_payloadCreated || getContainCount() == 0)
		return;
	const auto* formation = getHordeContainModuleData();
	if (m_bannerRespawnFrame && formation->m_bannerCarrierDestroyHordeOnDeath)
	{
		m_bannerRespawnFrame = 0;
		DamageInfo damage;
		damage.in.m_damageType = DAMAGE_UNRESISTABLE;
		damage.in.m_kill = TRUE;
		for (Int index = DEATH_CRUSHED; index < DEATH_NUM_TYPES; ++index)
			if (getDeathTypeFlag(formation->m_bannerCarrierHordeDeathType, static_cast<DeathType>(index)))
			{
				damage.in.m_deathType = static_cast<DeathType>(index);
				break;
			}
		const ContainedItemsList survivors = getContainList();
		for (Object* member : survivors)
			if (!member->isEffectivelyDead())
				member->attemptDamage(&damage);
		return;
	}
	const auto* type = TheThingFactory->findTemplate(m_replenishmentBanner, FALSE);
	const auto* data = findBannerData(type);
	if (!data)
		return;
#if !RTS_GENERALS
	if (!allowsBannerSpawn(getObject()))
		return;
#endif

	const auto frame = TheGameLogic->getFrame();
	if (std::any_of(getContainList().begin(), getContainList().end(), isAttackingMelee))
		m_lastMeleeFrame = frame;
	const auto* banner = getBannerCarrier();
	if (!banner)
	{
		if (m_bannerRespawnFrame && frame >= m_bannerRespawnFrame &&
			(!m_lastMeleeFrame || frame - m_lastMeleeFrame >= data->m_meleeFreeBannerRespawnTime))
			addBannerCarrier(TRUE);
		return;
	}

	const auto& layout = getActiveFormation()->m_layout;
	Int payloadCount = 0;
	for (const auto& payload : layout.payloads())
		payloadCount += payload.count;
	if (!data->m_idleSpawnRate || !bannerUpgradeComplete(banner, *data) ||
		static_cast<Int>(getContainCount()) - 1 >= std::min(getContainMax(), payloadCount))
	{
		m_unitSpawnFrame = 0;
		return;
	}
	if (!m_unitSpawnFrame)
		m_unitSpawnFrame = frame + data->m_idleSpawnRate;
	if (frame >= m_unitSpawnFrame &&
		(!m_lastMeleeFrame || frame - m_lastMeleeFrame >= data->m_meleeFreeUnitSpawnTime))
	{
		replenishMember(*data);
		m_unitSpawnFrame = frame + data->m_idleSpawnRate;
	}
}

Object* HordeContain::replenishMember(const BannerCarrierUpdateModuleData& banner)
{
	Object* horde = getObject();
	if (!m_payloadCreated || horde->isEffectivelyDead())
		return nullptr;
	const Int soldiers = std::count_if(getContainList().begin(), getContainList().end(), [this](const Object* member)
	{
		return !member->isEffectivelyDead() && !isBannerCarrier(member);
	});
	if (soldiers == 0 || soldiers >= getContainMax())
		return nullptr;

	const auto& layout = getActiveFormation()->m_layout;
	for (const auto& entry : layout.initialMembers(getContainMax()))
	{
		if (entry.slot < 0 || static_cast<size_t>(entry.slot) >= m_slotMembers.size() || m_slotMembers[entry.slot] != INVALID_ID)
			continue;
		const auto* type = TheThingFactory->findTemplate(layout.payloads()[entry.payload].name.c_str(), FALSE);
		if (!type)
			continue;
		Object* member = TheThingFactory->newObject(type, horde->getTeam());
#if RTS_GENERALS
		OpenContain::addToContain(member);
#else
		OpenContain::addToContain(member, FALSE);
#endif
		if (member->getContainedBy() != horde)
		{
			TheGameLogic->destroyObject(member);
			return nullptr;
		}
		const auto id = member->getID();
		const Int assignedSlot = getMemberSlot(id);
		if (assignedSlot != entry.slot)
		{
			if (assignedSlot >= 0)
			{
				m_slotMembers[assignedSlot] = INVALID_ID;
				m_slotOffsets[entry.slot] = m_slotOffsets[assignedSlot];
			}
			m_slotMembers[entry.slot] = id;
			m_memberSlots.insert_or_assign(id, entry.slot);
		}
		const auto position = getMemberPosition(id, FALSE);
		member->setPosition(&position);
		member->setOrientation(horde->getOrientation());
		if (TheFXListStore && banner.m_unitSpawnFX.isNotEmpty())
			FXList::doFXObj(TheFXListStore->findFXList(banner.m_unitSpawnFX.str()), member);
		return member;
	}
	return nullptr;
}

Object* HordeContain::findAttackTarget(const Object* attacker, AbleToAttackType attackType,
	CommandSourceType source, [[maybe_unused]] WeaponSlotType slot, CanAttackResult* attackResult) const
{
	if (attackResult)
		*attackResult = ATTACKRESULT_NOT_POSSIBLE;
	Object* nearest = nullptr;
	Real nearestDistance = 0.0f;
	const Coord3D& pos = *attacker->getPosition();
	for (Object* candidate : *getContainedItemsList())
	{
		if (candidate->isEffectivelyDead() || get(candidate))
			continue;
		const Coord3D& targetPos = *candidate->getPosition();
		const Real x = pos.x - targetPos.x;
		const Real y = pos.y - targetPos.y;
		const Real distance = x * x + y * y;
		if (nearest && (distance > nearestDistance ||
			(distance == nearestDistance && candidate->getID() >= nearest->getID())))
			continue;
#if RTS_ZEROHOUR
		const auto result = attacker->getAbleToAttackSpecificObject(attackType, candidate, source, slot);
#else
		const auto result = attacker->getAbleToAttackSpecificObject(attackType, candidate, source);
#endif
		if (result == ATTACKRESULT_POSSIBLE || result == ATTACKRESULT_POSSIBLE_AFTER_MOVING)
		{
			nearest = candidate;
			nearestDistance = distance;
			if (attackResult)
				*attackResult = result;
		}
	}
	return nearest;
}

Object* HordeContain::findNearestLivingMember(const Coord3D& fromPos) const
{
	Object* nearest = nullptr;
	Real nearestDistance = 0.0f;
	for (Object* candidate : *getContainedItemsList())
	{
		if (candidate->isEffectivelyDead() || get(candidate))
			continue;
		const Coord3D& targetPos = *candidate->getPosition();
		const Real x = fromPos.x - targetPos.x;
		const Real y = fromPos.y - targetPos.y;
		const Real distance = x * x + y * y;
		if (nearest && (distance > nearestDistance ||
			(distance == nearestDistance && candidate->getID() >= nearest->getID())))
			continue;
		nearest = candidate;
		nearestDistance = distance;
	}
	return nearest;
}

Drawable* HordeContain::resolveSelection(Drawable* draw)
{
	const Object* member = draw ? draw->getObject() : nullptr;
	const Object* horde = member ? member->getContainedBy() : nullptr;
	if (get(horde) && !member->isEffectivelyDead() && horde->getDrawable())
		return horde->getDrawable();
	return draw;
}

Bool HordeContain::rankCanAttack(const Object* member) const
{
	if (isBannerCarrier(member))
		return FALSE;
	const Int slot = getMemberSlot(member->getID());
	const auto* data = getActiveFormation();
	const Int rank = slot >= 0 ? data->m_layout.slots()[slot].rank : 0;
	return HordeLayout::rankCanAttack(rank, data->m_ranksToReleaseWhenAttacking, data->m_ranksToJustFreeWhenAttacking);
}

Bool HordeContain::rankStopsAdvance(const Object* member) const
{
	const Int slot = getMemberSlot(member->getID());
	const auto* data = getActiveFormation();
	if (slot < 0)
		return FALSE;
	const Int rank = data->m_layout.slots()[slot].rank;
	const auto& ranks = data->m_ranksThatStopAdvance;
	return std::find(ranks.begin(), ranks.end(), rank) != ranks.end();
}

Real HordeContain::directionalVisionScale(const Object* target) const
{
	const Object* anchor = getObject();
	const auto& origin = *anchor->getPosition();
	const Real dx = target->getPosition()->x - origin.x;
	const Real dy = target->getPosition()->y - origin.y;
	const auto& direction = *anchor->getUnitDirectionVector2D();
	const Real forward = dx * direction.x + dy * direction.y;
	const Real halfDistanceSquared = (dx * dx + dy * dy) * 0.5f;
	const Bool axial = forward * forward >= halfDistanceSquared;
	if (axial && forward >= 0.0f)
		return 1.0f;
	const auto* data = getActiveFormation();
	const auto& object = anchor->getTemplate()->getRotWKObjectData();
	const Real override = axial ? data->m_visionRearOverride : data->m_visionSideOverride;
	return std::max(0.0f, override > 0.0f ? override : (axial ? object.visionRear : object.visionSide));
}

Real HordeContain::maximumVisionScale() const
{
	const auto* data = getActiveFormation();
	const auto& object = getObject()->getTemplate()->getRotWKObjectData();
	return std::max({1.0f,
		data->m_visionRearOverride > 0.0f ? data->m_visionRearOverride : object.visionRear,
		data->m_visionSideOverride > 0.0f ? data->m_visionSideOverride : object.visionSide});
}

const HordeBannerCarrierPosition* HordeContain::getBannerFormationPosition() const
{
	const auto* formation = getActiveFormation();
	const auto& positions = formation->m_bannerCarrierPositions.empty() ?
		getHordeContainModuleData()->m_bannerCarrierPositions : formation->m_bannerCarrierPositions;
	for (const auto& payload : formation->m_layout.payloads())
		for (const auto& banner : positions)
			if (stricmp(banner.unitType.c_str(), payload.name.c_str()) == 0)
				return &banner;
	return positions.empty() ? nullptr : &positions.front();
}

HordePoint HordeContain::getBannerOffset() const
{
	const auto* position = getBannerFormationPosition();
	return position ? position->position : HordePoint{};
}

AsciiString HordeContain::getBannerUnitType() const
{
	if (const auto* position = getBannerFormationPosition())
		return position->unitType.c_str();
	const auto& payloads = getActiveFormation()->m_layout.payloads();
	return payloads.empty() ? AsciiString() : AsciiString(payloads.front().name.c_str());
}

Coord3D HordeContain::getMemberPosition(ObjectID id, Bool followLeader) const
{
	const Object* horde = getObject();
	Coord3D position = *horde->getPosition();
	const auto& layout = getActiveFormation()->m_layout;
	const Int slot = getMemberSlot(id);
	if (slot >= 0)
	{
		HordePoint offset = layout.slots()[slot].position;
		offset.x += m_slotOffsets[slot].x;
		offset.y += m_slotOffsets[slot].y;
		const Int leaderSlot = followLeader ? layout.leaderSlot(slot) : -1;
		if (leaderSlot >= 0)
		{
			const Object* leader = TheGameLogic->findObjectByID(m_slotMembers[leaderSlot]);
			if (leader && !leader->isEffectivelyDead())
			{
				position = *leader->getPosition();
				offset.x -= layout.slots()[leaderSlot].position.x + m_slotOffsets[leaderSlot].x;
				offset.y -= layout.slots()[leaderSlot].position.y + m_slotOffsets[leaderSlot].y;
			}
		}
		const Real angle = horde->getOrientation();
		const HordePoint worldOffset = HordeLayout::rotate(offset, Cos(angle), Sin(angle));
		position.x += worldOffset.x;
		position.y += worldOffset.y;
	}
	else if (RetailFileLayers::enabled())
	{
		const Object* member = TheGameLogic->findObjectByID(id);
		if (member && member->getContainedBy() == horde && isBannerCarrier(member))
		{
			const Real angle = horde->getOrientation();
			const HordePoint offset = HordeLayout::rotate(getBannerOffset(), Cos(angle), Sin(angle));
			position.x += offset.x;
			position.y += offset.y;
		}
	}
	if (TheTerrainLogic)
		position.z = TheTerrainLogic->getGroundHeight(position.x, position.y);
	return position;
}

namespace
{
constexpr Real FORMATION_LEAD_FRAMES = 8.0f;
constexpr Real FORMATION_CATCH_UP_PER_FRAME = 0.1f;
constexpr Real FORMATION_SLOW_ERROR = 25.0f;
constexpr Real FORMATION_STOP_ERROR = 65.0f;
constexpr Real FORMATION_MIN_SPEED_SCALE = 0.25f;
constexpr Real FORMATION_STRAGGLER_LIMIT = 120.0f;
constexpr Real FORMATION_STRAGGLER_WEIGHT = 0.5f;

void avoidBlockingRanks(const HordeContain& horde, const Object& anchor, const Object& banner, HordeContain::FormationGoal& goal)
{
	const Coord3D& origin = *banner.getPosition();
	const Coord3D step = goal.slot - origin;
	const Real lengthSquared = sqr(step.x) + sqr(step.y);
	if (lengthSquared == 0.0f)
		return;

	const Coord3D& forward = *anchor.getUnitDirectionVector2D();
	const Real radius = banner.getGeometryInfo().getBoundingCircleRadius();
	Real front = -FAST_AS_POSSIBLE;
	Real left = 0.0f;
	Real right = 0.0f;
	Bool blocked = FALSE;
	for (const Object* rider : *horde.getContainedItemsList())
	{
		if (rider->isEffectivelyDead() || horde.getMemberSlot(rider->getID()) < 0)
			continue;
		const Coord3D delta = *rider->getPosition() - origin;
		const Real clearance = radius + rider->getGeometryInfo().getBoundingCircleRadius();
		const Real distance = delta.x * forward.x + delta.y * forward.y;
		const Real lateral = delta.y * forward.x - delta.x * forward.y;
		front = std::max(front, distance + clearance);
		left = std::min(left, lateral - clearance);
		right = std::max(right, lateral + clearance);
		const Real fraction = std::clamp((delta.x * step.x + delta.y * step.y) / lengthSquared, 0.0f, 1.0f);
		blocked |= sqr(delta.x - fraction * step.x) + sqr(delta.y - fraction * step.y) < sqr(clearance);
	}
	if (!blocked)
		return;

	const Real side = -left < right ? left : right;
	const Real ahead = left < 0.0f && right > 0.0f ? 0.0f : std::max(front, step.x * forward.x + step.y * forward.y);
	goal.position.x = origin.x + ahead * forward.x - side * forward.y;
	goal.position.y = origin.y + ahead * forward.y + side * forward.x;
	if (TheTerrainLogic)
		goal.position.z = TheTerrainLogic->getGroundHeight(goal.position.x, goal.position.y);
	goal.distance = WWMath::SqrtfOrigin(sqr(ahead) + sqr(side));
}
}

Bool HordeContain::getFormationGoal(const Object* member, FormationGoal& goal) const
{
	const Object* horde = getObject();
	if (horde->isEffectivelyDead() || (getMemberSlot(member->getID()) < 0 && !isBannerCarrier(member)))
		return FALSE;
	if (HordeWorkerAIUpdate::find(horde))
	{
		const auto* ai = member->getAIUpdateInterface();
		const auto* worker = ai ? ai->getDozerAIInterface() : nullptr;
		if (worker && worker->getCurrentTask() == DOZER_TASK_REPAIR)
			return FALSE;
	}

	const Coord3D& position = *member->getPosition();
	if (const auto move = m_backingMoves.find(member->getID());
		move != m_backingMoves.end() && TheGameLogic->getFrame() >= move->second.start)
	{
		goal.backing = TRUE;
		goal.backingSource = m_backingSource;
		goal.position = goal.slot = move->second.goal;
		const Real dx = goal.position.x - position.x;
		const Real dy = goal.position.y - position.y;
		goal.error = WWMath::SqrtfOrigin(dx * dx + dy * dy);
		goal.distance = goal.error;
		goal.facing = member->getOrientation();
		goal.speed = FAST_AS_POSSIBLE;
		goal.hordeMoving = FALSE;
		return TRUE;
	}

	goal.backing = FALSE;
	goal.backingSource = INVALID_ID;
	goal.slot = getMemberPosition(member->getID(), FALSE);
	const Real dx = goal.slot.x - position.x;
	const Real dy = goal.slot.y - position.y;
	goal.error = sqrtf(dx * dx + dy * dy);
	goal.facing = horde->getOrientation();
	goal.position = goal.slot;

	const AIUpdateInterface* hordeAI = horde->getAI();
	const PhysicsBehavior* physics = horde->getPhysics();
	goal.hordeMoving = hordeAI && hordeAI->isMoving() && physics && !HordeAIUpdate::isReforming(hordeAI);
	if (!goal.hordeMoving)
	{
		goal.speed = FAST_AS_POSSIBLE;
		goal.distance = goal.error;
		if (RetailFileLayers::enabled() && horde->isKindOf(KINDOF_CAVALRY) && isBannerCarrier(member))
			avoidBlockingRanks(*this, *horde, *member, goal);
		return TRUE;
	}

	const Coord3D& velocity = *physics->getVelocity();
	const Real hordeSpeed = WWMath::SqrtfOrigin(velocity.x * velocity.x + velocity.y * velocity.y);
	const Coord3D& anchor = *horde->getPosition();
	const Coord3D& destination = *hordeAI->getGoalPosition();
	const Real gx = destination.x - anchor.x;
	const Real gy = destination.y - anchor.y;
	Real remaining = WWMath::SqrtfOrigin(gx * gx + gy * gy);
	Real leadX = velocity.x * FORMATION_LEAD_FRAMES;
	Real leadY = velocity.y * FORMATION_LEAD_FRAMES;
	const Path* path = hordeAI->getPath();
	if (const ClosestPointOnPathInfo* point = path ? path->getCachedPointOnPath() : nullptr)
	{
		const Real pathX = point->posOnPath.x - anchor.x;
		const Real pathY = point->posOnPath.y - anchor.y;
		const Real length = WWMath::SqrtfOrigin(pathX * pathX + pathY * pathY);
		const Real scale = length > 0.0f ? std::min(1.0f, hordeSpeed * FORMATION_LEAD_FRAMES / length) : 0.0f;
		leadX = pathX * scale;
		leadY = pathY * scale;
		remaining = std::max(0.0f, point->distAlongPath);
	}
	if (horde->isKindOf(KINDOF_CAVALRY) && RetailFileLayers::enabled() &&
		hordeAI->getModuleNameKey() == NAMEKEY("HordeAIUpdate"))
	{
		const Real turn = static_cast<const HordeAIUpdate*>(hordeAI)->getCavalryTurn();
		const Real leadFrames = std::min(FORMATION_LEAD_FRAMES,
			hordeSpeed > 0.0f ? remaining / hordeSpeed : FORMATION_LEAD_FRAMES);
		const Real angle = std::clamp(turn * leadFrames, -PI / 4.0f, PI / 4.0f);
		if (angle != 0.0f)
		{
			const Real midpoint = horde->getOrientation() + angle * 0.5f;
			leadX = hordeSpeed * leadFrames * Cos(midpoint);
			leadY = hordeSpeed * leadFrames * Sin(midpoint);
		}
		const Real offsetX = goal.slot.x - anchor.x;
		const Real offsetY = goal.slot.y - anchor.y;
		const Real cosAngle = Cos(angle);
		const Real sinAngle = Sin(angle);
		goal.position.x = anchor.x + leadX + offsetX * cosAngle - offsetY * sinAngle;
		goal.position.y = anchor.y + leadY + offsetX * sinAngle + offsetY * cosAngle;
		if (TheTerrainLogic)
			goal.position.z = TheTerrainLogic->getGroundHeight(goal.position.x, goal.position.y);

		const Real slotVX = velocity.x - turn * offsetY;
		const Real slotVY = velocity.y + turn * offsetX;
		const Real slotSpeed = WWMath::SqrtfOrigin(slotVX * slotVX + slotVY * slotVY);
		const Real correctionX = slotVX + dx * FORMATION_CATCH_UP_PER_FRAME;
		const Real correctionY = slotVY + dy * FORMATION_CATCH_UP_PER_FRAME;
		goal.speed = WWMath::SqrtfOrigin(correctionX * correctionX + correctionY * correctionY);
		const Real stepX = goal.position.x - position.x;
		const Real stepY = goal.position.y - position.y;
		const Real step = WWMath::SqrtfOrigin(stepX * stepX + stepY * stepY);
		goal.distance = step + std::max(0.0f, remaining - slotSpeed * leadFrames);
		return TRUE;
	}
	goal.position.x += leadX;
	goal.position.y += leadY;
	if (TheTerrainLogic)
		goal.position.z = TheTerrainLogic->getGroundHeight(goal.position.x, goal.position.y);
	const Real behind = hordeSpeed > 0.0f ? (dx * velocity.x + dy * velocity.y) / hordeSpeed : goal.error;
	goal.speed = std::max(0.0f, hordeSpeed + behind * FORMATION_CATCH_UP_PER_FRAME);
	goal.distance = std::max(0.0f, behind) + remaining;
	return TRUE;
}

Bool HordeContain::canSteerToFormationGoal(const Object* member, const FormationGoal& goal)
{
	const Locomotor* locomotor = member->getAI()->getCurLocomotor();
	if (!locomotor)
		return FALSE;
	const auto surfaces = locomotor->getLegalSurfaces();
	const auto layer = member->getLayer();
	const Coord3D& position = *member->getPosition();
	Pathfinder* pathfinder = TheAI->pathfinder();
	if (!goal.backing && RetailFileLayers::enabled() && member->isKindOf(KINDOF_CAVALRY) &&
		locomotor->getAppearance() == LOCO_WHEELS_FOUR &&
		pathfinder->validMovementTerrain(layer, locomotor, &position) &&
		!pathfinder->isMovementSegmentPassable(member, surfaces, layer, position, goal.position, TRUE))
		return FALSE;
	return pathfinder->validMovementTerrain(layer, locomotor, &goal.position) &&
		pathfinder->isLinePassable(member, surfaces, layer, position, goal.position, FALSE, TRUE);
}

void HordeContain::constrainGroundMovement(const Object* member, Coord3D& movement)
{
	if (!RetailFileLayers::enabled() || (movement.x == 0.0f && movement.y == 0.0f) ||
		!std::isfinite(movement.x) || !std::isfinite(movement.y) ||
		member->isEffectivelyDead() || member->isSignificantlyAboveTerrain())
		return;
	const Bool isHordeMember = get(member->getContainedBy()) != nullptr;
	if (!isHordeMember && !get(member))
		return;

	const AIUpdateInterface* ai = member->getAI();
	const Locomotor* locomotor = ai ? ai->getCurLocomotor() : nullptr;
	if (!locomotor || !ai->isDoingGroundMovement())
		return;

	const Coord3D& position = *member->getPosition();
	const auto layer = member->getLayer();
	Pathfinder* pathfinder = TheAI->pathfinder();
	const auto surfaces = locomotor->getLegalSurfaces();
	if (!pathfinder->validMovementTerrain(layer, locomotor, &position))
		return;
	const Bool terrainOnly = !isHordeMember ||
		!pathfinder->isMovementSegmentPassable(member, surfaces, layer, position, position, FALSE);
	const auto passable = [&](Real dx, Real dy) {
		const Coord3D next = {position.x + dx, position.y + dy, position.z};
		return pathfinder->isMovementSegmentPassable(member, surfaces, layer, position, next, terrainOnly);
	};
	if (passable(movement.x, movement.y))
		return;

	if (movement.y != 0.0f && passable(0.0f, movement.y))
		movement.x = 0.0f;
	else if (movement.x != 0.0f && passable(movement.x, 0.0f))
		movement.y = 0.0f;
	else
		movement.x = movement.y = 0.0f;
}

void HordeContain::separateGroundMovement(const Object* member, Coord3D& movement)
{
	if (!RetailFileLayers::enabled() || member->isEffectivelyDead() ||
		member->isSignificantlyAboveTerrain() || !std::isfinite(movement.x) || !std::isfinite(movement.y))
		return;
	const HordeContain* horde = get(member->getContainedBy());
	if (!horde)
		return;
	const Bool cavalry = horde->getObject()->isKindOf(KINDOF_CAVALRY);
	const auto hasFormationPosition = [&](const Object* rider) {
		return horde->getMemberSlot(rider->getID()) >= 0 || (cavalry && horde->isBannerCarrier(rider));
	};
	if (!hasFormationPosition(member))
		return;
	const AIUpdateInterface* ai = member->getAI();
	const Locomotor* locomotor = ai ? ai->getCurLocomotor() : nullptr;
	if (!locomotor || !ai->isDoingGroundMovement())
		return;

	const Real radius = member->getGeometryInfo().getBoundingCircleRadius();
	const AIUpdateInterface* anchorAI = horde->getObject()->getAI();
	const Bool cavalryBanner = cavalry && horde->isBannerCarrier(member);
	const Bool cavalrySettling = cavalry && (!anchorAI || !anchorAI->isMoving());
	const Bool alternate = horde->isAlternateFormation();
	const Int memberSlot = alternate ? horde->getMemberSlot(member->getID()) : -1;
	const auto& slots = horde->getActiveFormation()->m_layout.slots();
	const Coord3D& origin = *member->getPosition();
	const auto layer = member->getLayer();
	Coord3D next = {origin.x + movement.x, origin.y + movement.y, origin.z};
	const Coord3D planned = next;
	constexpr Real CLEARANCE = 0.05f;
	constexpr Real MAX_CORRECTION = 4.0f;
	const auto eligibleNeighbor = [&](const Object* other) {
		return other != member && !other->isEffectivelyDead() && other->getLayer() == layer &&
			!other->isSignificantlyAboveTerrain();
	};
	for (const Object* other : horde->getContainList())
	{
		if (!eligibleNeighbor(other) || !hasFormationPosition(other))
			continue;
		const Coord3D& position = *other->getPosition();
		Real diameter = radius + other->getGeometryInfo().getBoundingCircleRadius() + CLEARANCE;
		if (cavalrySettling && (cavalryBanner || horde->isBannerCarrier(other)))
			diameter = (diameter - CLEARANCE) * 0.75f + CLEARANCE;
		const Int otherSlot = memberSlot >= 0 ? horde->getMemberSlot(other->getID()) : -1;
		if (otherSlot >= 0)
		{
			const HordePoint& a = slots[memberSlot].position;
			const HordePoint& b = slots[otherSlot].position;
			diameter = std::min(diameter, WWMath::SqrtfOrigin(sqr(a.x - b.x) + sqr(a.y - b.y)));
		}
		const Real dx = next.x - position.x;
		const Real dy = next.y - position.y;
		const Real distanceSquared = dx * dx + dy * dy;
		if (distanceSquared >= diameter * diameter)
			continue;

		const Real oldX = origin.x - position.x;
		const Real oldY = origin.y - position.y;
		const Real oldDistance = WWMath::SqrtfOrigin(oldX * oldX + oldY * oldY);
		Real axisX = oldX;
		Real axisY = oldY;
		if (oldDistance > 0.001f)
		{
			axisX /= oldDistance;
			axisY /= oldDistance;
		}
		else
		{
			const Coord3D ourSlot = horde->getMemberPosition(member->getID(), FALSE);
			const Coord3D otherSlot = horde->getMemberPosition(other->getID(), FALSE);
			axisX = ourSlot.x - otherSlot.x;
			axisY = ourSlot.y - otherSlot.y;
			const Real length = WWMath::SqrtfOrigin(axisX * axisX + axisY * axisY);
			if (length > 0.001f)
			{
				axisX /= length;
				axisY /= length;
			}
			else
			{
				axisX = member->getID() < other->getID() ? -1.0f : 1.0f;
				axisY = 0.0f;
			}
		}
		const Real target = std::min(diameter, oldDistance + MAX_CORRECTION);
		const Real correction = target - (dx * axisX + dy * axisY);
		if (correction > 0.0f)
		{
			next.x += axisX * correction;
			next.y += axisY * correction;
		}
	}
	if (next.x == planned.x && next.y == planned.y)
		return;

	if (locomotor->getAppearance() == LOCO_LEGS_TWO || (cavalryBanner && !cavalrySettling))
	{
		const Real stepSquared = movement.x * movement.x + movement.y * movement.y;
		const Real dx = next.x - origin.x;
		const Real dy = next.y - origin.y;
		const Real correctedSquared = dx * dx + dy * dy;
		if (correctedSquared > stepSquared)
		{
			const Real scale = WWMath::SqrtfOrigin(stepSquared / correctedSquared);
			next.x = origin.x + dx * scale;
			next.y = origin.y + dy * scale;
		}
	}

	Pathfinder* pathfinder = TheAI->pathfinder();
	const auto surfaces = locomotor->getLegalSurfaces();
	if (!pathfinder->isMovementSegmentPassable(member, surfaces, layer, origin, next, FALSE) ||
		!pathfinder->isLinePassable(member, surfaces, layer, origin, next, FALSE, TRUE))
		return;

	for (const Object* other : horde->getContainList())
	{
		if (!eligibleNeighbor(other))
			continue;
		const Coord3D& position = *other->getPosition();
		const Real pileRadius = (radius + other->getGeometryInfo().getBoundingCircleRadius()) * 0.5f;
		const Real dx = next.x - position.x;
		const Real dy = next.y - position.y;
		if (dx * dx + dy * dy >= pileRadius * pileRadius)
			continue;
		const Real plannedX = planned.x - position.x;
		const Real plannedY = planned.y - position.y;
		if (plannedX * plannedX + plannedY * plannedY >= pileRadius * pileRadius && hasFormationPosition(other))
			return;
	}

	movement.x = next.x - origin.x;
	movement.y = next.y - origin.y;
}

Real HordeContain::getFormationSpeedScale() const
{
	if (HordeAIUpdate::isReforming(getObject()->getAI()))
		return 0.0f;

	Real total = 0.0f;
	Real worst = 0.0f;
	Int count = 0;
	for (const Object* member : getContainList())
	{
		const AIUpdateInterface* ai = member->getAI();
		if (member->isEffectivelyDead() || getMemberSlot(member->getID()) < 0 || (ai && ai->isAttacking()))
			continue;
		const Coord3D slot = getMemberPosition(member->getID(), FALSE);
		const Real dx = slot.x - member->getPosition()->x;
		const Real dy = slot.y - member->getPosition()->y;
		const Real error = sqrtf(dx * dx + dy * dy);
		total += error;
		++count;
		if (error < FORMATION_STRAGGLER_LIMIT)
			worst = std::max(worst, error);
	}
	if (count == 0)
		return 1.0f;
	const Real lag = std::max(total / count, worst * FORMATION_STRAGGLER_WEIGHT);
	if (lag <= FORMATION_SLOW_ERROR)
		return 1.0f;
	const Real scale = 1.0f - (lag - FORMATION_SLOW_ERROR) / (FORMATION_STOP_ERROR - FORMATION_SLOW_ERROR);
	return std::max(FORMATION_MIN_SPEED_SCALE, scale);
}

#if RTS_ZEROHOUR
Real HordeContain::getFormationWheelRate(Real radius) const
{
	const Coord3D& anchor = *getObject()->getPosition();
	Real limit = FAST_AS_POSSIBLE;
	for (const Object* member : getContainList())
	{
		const AIUpdateInterface* ai = member->getAI();
		const Locomotor* locomotor = ai ? ai->getCurLocomotor() : nullptr;
		if (!locomotor || member->isEffectivelyDead() || ai->isAttacking() ||
			(getMemberSlot(member->getID()) < 0 && !isBannerCarrier(member)))
			continue;
		const Coord3D slot = getMemberPosition(member->getID(), FALSE);
		const Real reach = WWMath::SqrtfOrigin(sqr(slot.x - anchor.x) + sqr(slot.y - anchor.y));
		const auto damage = member->getBodyModule()->getDamageState();
		const Real speed = locomotor->getMaxSpeedForCondition(damage, member);
		if (speed > 0.0f && radius + reach > 0.0f)
			limit = std::min(limit, speed / (radius + reach));
	}
	return limit;
}

UnsignedInt HordeContain::getFormationReformTravelFrames() const
{
	Real frames = 0.0f;
	for (const Object* member : getContainList())
	{
		const AIUpdateInterface* ai = member->getAI();
		const Locomotor* locomotor = ai ? ai->getCurLocomotor() : nullptr;
		if (!locomotor || member->isEffectivelyDead() || ai->isAttacking() || getMemberSlot(member->getID()) < 0)
			continue;
		const Coord3D slot = getMemberPosition(member->getID(), FALSE);
		const Coord3D& position = *member->getPosition();
		const Real errorSquared = sqr(position.x - slot.x) + sqr(position.y - slot.y);
		const Real radius = member->getGeometryInfo().getBoundingCircleRadius();
		const Real speed = locomotor->getMaxSpeedForCondition(member->getBodyModule()->getDamageState(), member);
		if (speed > 0.0f && errorSquared > sqr(radius))
			frames = std::max(frames, (WWMath::SqrtfOrigin(errorSquared) - radius) / speed);
	}
	return static_cast<UnsignedInt>(WWMath::Ceil(frames));
}
#endif

void HordeContain::reassignSlotsByPosition()
{
	const Object* horde = getObject();
	const HordeLayout& layout = getActiveFormation()->m_layout;
	const Real angle = horde->getOrientation();
	const Real cosAngle = Cos(-angle);
	const Real sinAngle = Sin(-angle);
	std::vector<ObjectID> ids;
	std::vector<int> current;
	std::vector<HordePoint> positions;
	std::vector<bool> reserved(m_slotMembers.size(), false);
	for (std::size_t slot = 0; slot < m_slotMembers.size(); ++slot)
		reserved[slot] = m_slotMembers[slot] != INVALID_ID;
	for (const Object* member : getContainList())
	{
		const Int slot = getMemberSlot(member->getID());
		if (member->isEffectivelyDead() || slot < 0)
			continue;
		HordePoint offset;
		offset.x = member->getPosition()->x - horde->getPosition()->x;
		offset.y = member->getPosition()->y - horde->getPosition()->y;
		HordePoint local = HordeLayout::rotate(offset, cosAngle, sinAngle);
		local.x -= m_slotOffsets[slot].x;
		local.y -= m_slotOffsets[slot].y;
		ids.push_back(member->getID());
		current.push_back(slot);
		positions.push_back(local);
		reserved[slot] = false;
	}

	const std::vector<int> slots = layout.nearestSlots(current, positions, reserved);
	std::vector<ObjectID> members(m_slotMembers.size(), INVALID_ID);
	std::vector<Coord2D> offsets(m_slotOffsets.size(), Coord2D{0.0f, 0.0f});
	for (std::size_t index = 0; index < m_slotMembers.size(); ++index)
	{
		if (reserved[index])
		{
			members[index] = m_slotMembers[index];
			offsets[index] = m_slotOffsets[index];
		}
	}
	for (std::size_t index = 0; index < ids.size(); ++index)
	{
		const Int slot = slots[index];
		if (slot < 0 || members[slot] != INVALID_ID)
			return;
		members[slot] = ids[index];
		offsets[slot] = m_slotOffsets[current[index]];
	}
	m_slotMembers.swap(members);
	m_slotOffsets.swap(offsets);
	rebuildMemberSlots();
	for (Object* member : getContainList())
		updateRankWeaponConditions(member);
}

const HordeContainModuleData* HordeContain::findAlternateFormation() const
{
	const AsciiString& name = getHordeContainModuleData()->m_alternateFormation;
	const ThingTemplate* alternate = name.isEmpty() ? nullptr : TheThingFactory->findTemplate(name, FALSE);
	if (!alternate || alternate == getObject()->getTemplate())
		return nullptr;

	const ModuleInfo& modules = alternate->getBehaviorModuleInfo();
	for (Int i = 0; i < modules.getCount(); ++i)
	{
		const AsciiString module = modules.getNthName(i);
		if (module.compareNoCase("HordeContain") == 0 || module.compareNoCase("HorseHordeContain") == 0)
		{
			const auto* data = static_cast<const HordeContainModuleData*>(modules.getNthData(i));
			if (!data->m_layout.slots().empty())
				return data;
		}
	}
	return nullptr;
}

const HordeContainModuleData* HordeContain::getActiveFormation() const
{
	return m_formationOverride ? m_formationOverride : getHordeContainModuleData();
}

Bool HordeContain::isAlternateFormation() const
{
	return getHordeContainModuleData()->m_alternateFormation.isNotEmpty() &&
		!getActiveFormation()->m_thisFormationIsTheMainFormation;
}

Bool HordeContain::canSetAlternateFormation(Bool alternate) const
{
	const auto* other = m_formationOverride ? getHordeContainModuleData() : findAlternateFormation();
	if (!other || getContainCount() == 0)
		return FALSE;
	if (alternate == isAlternateFormation())
		return TRUE;
	const auto living = std::count_if(getContainList().begin(), getContainList().end(),
		[this](const Object* member) { return !member->isEffectivelyDead() && !isBannerCarrier(member); });
	return other->m_thisFormationIsTheMainFormation != alternate && living >= other->m_minimumHordeSize &&
		static_cast<std::size_t>(living) <= other->m_layout.slots().size();
}

void HordeContain::updateFormationModelConditions(Object* object) const
{
	if (isAlternateFormation())
	{
		if (BodyModuleInterface* body = object->getBodyModule())
			body->setArmorSetFlag(ARMORSET_ALTERNATE_FORMATION);
		object->clearModelConditionState(MODELCONDITION_PRIMARY_FORMATION);
		object->setModelConditionState(MODELCONDITION_ALTERNATE_FORMATION);
	}
	else
	{
		if (BodyModuleInterface* body = object->getBodyModule())
			body->clearArmorSetFlag(ARMORSET_ALTERNATE_FORMATION);
		object->clearModelConditionState(MODELCONDITION_ALTERNATE_FORMATION);
		object->setModelConditionState(MODELCONDITION_PRIMARY_FORMATION);
	}
}

void HordeContain::setAlternateFormation(Bool alternate)
{
	if (alternate == isAlternateFormation() || !canSetAlternateFormation(alternate))
		return;

#if !RTS_GENERALS
	RotwkModifierModules::removeHordeFormationModifiers( getObject() );
#endif
	m_formationOverride = m_formationOverride ? nullptr : findAlternateFormation();
#if !RTS_GENERALS
	RotwkModifierModules::applyHordeFormationModifiers( getObject() );
#endif
	const HordeLayout& layout = getActiveFormation()->m_layout;
	std::vector<ObjectID> members(layout.slots().size(), INVALID_ID);
	std::vector<Coord2D> offsets(members.size(), Coord2D{0.0f, 0.0f});
	std::vector<bool> occupied(members.size(), false);
	updateFormationModelConditions(getObject());
	for (Object* member : getContainList())
	{
		if (member->isEffectivelyDead())
			continue;
		const Int slot = isBannerCarrier(member) ? -1 : layout.findSlot(member->getTemplate()->getName().str(), occupied);
		if (slot >= 0)
		{
			members[slot] = member->getID();
			occupied[slot] = true;
			const Int previous = getMemberSlot(member->getID());
			if (previous >= 0)
				offsets[slot] = m_slotOffsets[previous];
		}
		updateFormationModelConditions(member);
		if (isBannerCarrier(member))
			if (auto* banner = BannerCarrierUpdate::find(member))
				banner->applyMorph(getBannerUnitType());
	}
	m_slotMembers.swap(members);
	m_slotOffsets.swap(offsets);
	rebuildMemberSlots();
	if (RetailFileLayers::enabled() && alternate)
		reassignSlotsByPosition();
	else
		for (Object* member : getContainList())
			updateRankWeaponConditions(member);
	if (Object* anchor = getObject())
	{
		const Coord3D* pos = anchor->getPosition();
		if (pos->x || pos->y || pos->z)
			anchor->handlePartitionCellMaintenance();
	}
}

void HordeContain::updateRankWeaponConditions(Object* member) const
{
	if (!RetailFileLayers::enabled() || isBannerCarrier(member) || member->isEffectivelyDead())
		return;
	const auto collect = [](const HordeContainModuleData* formation, WeaponSetFlags& flags)
	{
		for (const auto& info : formation->m_rankInfos)
		{
			if (info.grantedWeaponCondition >= 0)
				flags.set(static_cast<WeaponSetType>(info.grantedWeaponCondition));
			if (info.revokedWeaponCondition >= 0)
				flags.set(static_cast<WeaponSetType>(info.revokedWeaponCondition));
		}
	};
	WeaponSetFlags controlled;
	collect(getHordeContainModuleData(), controlled);
	if (const auto* alternate = findAlternateFormation())
		collect(alternate, controlled);
	if (!controlled.any())
		return;

	WeaponSetFlags absent = getObject()->getWeaponSetFlags();
	absent.flip();
	WeaponSetFlags desired = controlled;
	desired.clear(absent);
	const Int slot = getMemberSlot(member->getID());
	if (slot >= 0)
	{
		const auto* formation = getActiveFormation();
		const auto& rank = formation->m_rankInfos[formation->m_layout.slots()[slot].rankInfoIndex];
		if (rank.revokedWeaponCondition >= 0)
			desired.clear(static_cast<WeaponSetType>(rank.revokedWeaponCondition));
		if (rank.grantedWeaponCondition >= 0)
			desired.set(static_cast<WeaponSetType>(rank.grantedWeaponCondition));
	}
	controlled.clear(desired);
	member->setWeaponSetFlags(controlled, FALSE);
	member->setWeaponSetFlags(desired, TRUE);
}

void HordeContain::toggleWeaponSet(const WeaponSetFlags& flags, Bool enabled)
{
	getObject()->setWeaponSetFlags(flags, enabled);
	for (Object* member : getContainList())
		if (!member->isEffectivelyDead())
		{
			member->setWeaponSetFlags(flags, enabled);
			updateRankWeaponConditions(member);
		}
}

#if !RTS_GENERALS
Bool HordeContain::splitHorde()
{
	Object* anchor = getObject();
	const auto* formation = getActiveFormation();
	const auto& definitions = formation->m_splitHordes.empty() ?
		getHordeContainModuleData()->m_splitHordes : formation->m_splitHordes;
	if (!RetailFileLayers::enabled() || anchor->isEffectivelyDead() || !m_payloadCreated || definitions.empty() ||
		anchor->getContainedBy())
		return FALSE;

	struct SplitGroup
	{
		const ThingTemplate* type = nullptr;
		std::vector<Object*> members;
		Object* result = nullptr;
		HordeContain* contain = nullptr;
	};
	struct SplitCohort
	{
		const ThingTemplate* type;
		Int rank;
		std::vector<Object*> members;
	};
	const auto matchesUnit = [](const ThingTemplate* member, const ThingTemplate* unit) -> Bool
	{
		if (!unit)
			return FALSE;
		const auto hasAlias = [](const ThingTemplate* type, const AsciiString& name)
		{
			const auto& aliases = type->getRotWKObjectData().equivalentTo;
			return std::any_of(aliases.begin(), aliases.end(), [&](const AsciiString& alias) { return alias.compareNoCase(name) == 0; });
		};
		const auto& aliases = member->getRotWKObjectData().equivalentTo;
		return member->isEquivalentTo(unit) || hasAlias(member, unit->getName()) || hasAlias(unit, member->getName()) ||
			std::any_of(aliases.begin(), aliases.end(), [&](const AsciiString& alias) { return hasAlias(unit, alias); });
	};
	Object* banner = nullptr;
	std::vector<SplitCohort> cohorts;
	for (Object* member : getContainList())
	{
		if (member->isEffectivelyDead())
			continue;
		if (isBannerCarrier(member))
		{
			banner = member;
			continue;
		}
		const Int slot = getMemberSlot(member->getID());
		const Int rank = formation->m_rankSplit && slot >= 0 ? formation->m_layout.slots()[slot].rank : 0;
		const ThingTemplate* memberType = member->getTemplate();
		const auto cohort = std::find_if(cohorts.begin(), cohorts.end(), [&](const SplitCohort& candidate)
		{
			return candidate.rank == rank && matchesUnit(candidate.type, memberType);
		});
		if (cohort != cohorts.end())
			cohort->members.push_back(member);
		else
			cohorts.push_back({ memberType, rank, { member } });
	}
	std::vector<SplitGroup> groups(definitions.size());
	for (std::size_t index = 0; index < definitions.size(); ++index)
	{
		const auto& def = definitions[index];
		auto& group = groups[index];
		group.type = TheThingFactory->findTemplate(def.result.c_str(), FALSE);
		if (!group.type || !group.type->isKindOf(KINDOF_HORDE))
			return FALSE;
		const ThingTemplate* unitType = TheThingFactory->findTemplate(def.unitType.c_str(), FALSE);
		const Int rank = formation->m_rankSplit ? def.rankNumber : 0;
		const auto cohort = std::find_if(cohorts.begin(), cohorts.end(), [&](const SplitCohort& candidate)
		{
			return !candidate.members.empty() && candidate.rank == rank && matchesUnit(candidate.type, unitType);
		});
		if (cohort != cohorts.end())
			group.members.swap(cohort->members);
	}
	for (auto& group : groups)
	{
		group.result = TheThingFactory->newObject(group.type, anchor->getTeam());
		group.contain = const_cast<HordeContain*>(get(group.result));
		if (!group.contain || group.members.size() > static_cast<std::size_t>(group.contain->getContainMax()))
		{
			for (auto& created : groups)
				if (created.result)
					TheGameLogic->destroyObject(created.result);
			return FALSE;
		}
		group.contain->m_payloadCreated = TRUE;
	}

	const Real experience = anchor->getExperienceTracker()->getRetailExperience();
	const AsciiString recoveringBanner = m_replenishmentBanner;
	const UnsignedInt bannerRespawnFrame = m_bannerRespawnFrame;
	const UnsignedInt unitSpawnFrame = m_unitSpawnFrame;
	for (const auto& cohort : cohorts)
	{
		for (Object* member : cohort.members)
		{
			if (auto* ai = member->getAI())
				ai->aiIdle(CMD_FROM_SCRIPT);
			removeFromContain(member);
			auto* tracker = member->getExperienceTracker();
			if (tracker->getExperienceSink() == anchor->getID())
				tracker->setExperienceSink(INVALID_ID);
		}
	}
	for (auto& group : groups)
	{
		if (group.members.empty())
		{
			TheGameLogic->destroyObject(group.result);
			continue;
		}
		Coord3D center{};
		for (const Object* member : group.members)
		{
			center.x += member->getPosition()->x;
			center.y += member->getPosition()->y;
		}
		center.x /= group.members.size();
		center.y /= group.members.size();
		center.z = TheTerrainLogic->getGroundHeight(center.x, center.y);
		group.result->setPosition(&center);
		group.result->setOrientation(anchor->getOrientation());
		auto* tracker = group.result->getExperienceTracker();
		tracker->advanceRetailLevel(experience, FALSE);
		if (const auto* sourceStance = StancesBehavior::find(anchor))
			if (auto* stance = StancesBehavior::find(group.result))
				stance->setStance(sourceStance->getStance());
		RotwkModifierModules::applyHordeFormationModifiers(group.result);

		group.contain->enableLoadSounds(FALSE);
		for (Object* member : group.members)
		{
			if (auto* ai = member->getAI())
				ai->aiIdle(CMD_FROM_SCRIPT);
			removeFromContain(member);
			group.contain->OpenContain::addToContain(member, FALSE);
			if (member->getExperienceTracker()->getExperienceSink() == anchor->getID())
				member->getExperienceTracker()->setExperienceSink(group.result->getID());
		}
		const auto& allowed = group.contain->getHordeContainModuleData()->m_bannerCarriersAllowed;
		const auto acceptsBanner = [&](const AsciiString& name)
		{
			return std::any_of(allowed.begin(), allowed.end(), [&](const AsciiString& candidate)
				{ return candidate.compareNoCase(name) == 0; });
		};
		group.contain->m_lastMeleeFrame = m_lastMeleeFrame;
		if (banner && acceptsBanner(banner->getTemplate()->getName()))
		{
			removeFromContain(banner);
			group.contain->OpenContain::addToContain(banner, FALSE);
			group.contain->m_unitSpawnFrame = unitSpawnFrame;
			banner = nullptr;
		}
		else if (bannerRespawnFrame && acceptsBanner(recoveringBanner))
		{
			group.contain->m_replenishmentBanner = recoveringBanner;
			group.contain->m_bannerRespawnFrame = bannerRespawnFrame;
		}
		group.contain->onRankAdvanced(tracker->getRank());
		group.contain->enableLoadSounds(TRUE);
	}
	if (banner)
	{
		removeFromContain(banner);
		TheGameLogic->destroyObject(banner);
	}
	TheGameLogic->destroyObject(anchor);
	return TRUE;
}
#endif

DisabledMaskType HordeContain::getDisabledTypesToProcess() const
{
	return RetailFileLayers::enabled() ? MAKE_DISABLED_MASK(DISABLED_HELD) : TransportContain::getDisabledTypesToProcess();
}

UpdateSleepTime HordeContain::update()
{
	const auto sleep = TransportContain::update();
	if (getObject()->isEffectivelyDead())
		return sleep;

	for (auto it = getContainList().begin(); it != getContainList().end(); )
	{
		Object* member = *it++;
		if (member->isEffectivelyDead())
			removeFromContain(member);
	}
	if (m_initialPayloadPending)
		return sleep;
	if (m_hadMembers && getContainCount() == 0)
	{
		Object* horde = getObject();
		horde->setModelConditionState(MODELCONDITION_HORDE_EMPTY);
		horde->setEffectivelyDead(TRUE);
		DamageInfo damage;
		horde->onDie(&damage);
		TheGameLogic->destroyObject(horde);
	}
	else if (RetailFileLayers::enabled())
	{
		updateFlankState();
		updateReplenishment();
	}
	return sleep;
}

void HordeContain::clientVisibleContainedFlashAsSelected()
{
	for (Object* member : getContainList())
	{
		if (Drawable* draw = member->getDrawable())
			draw->flashAsSelected();
	}
}

void BannerCarrierUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	static const FieldParse fields[] =
	{
		{ "MorphCondition", parseMorph, nullptr, offsetof(BannerCarrierUpdateModuleData, m_morphs) },
		{ "BannerMorphFX", INI::parseAsciiString, nullptr, offsetof(BannerCarrierUpdateModuleData, m_morphFX) },
		{ "UnitSpawnFX", INI::parseAsciiString, nullptr, offsetof(BannerCarrierUpdateModuleData, m_unitSpawnFX) },
		{ "UpgradeRequired", INI::parseAsciiString, nullptr, offsetof(BannerCarrierUpdateModuleData, m_upgradeRequired) },
		{ "IdleSpawnRate", INI::parseDurationUnsignedInt, nullptr, offsetof(BannerCarrierUpdateModuleData, m_idleSpawnRate) },
		{ "MeleeFreeUnitSpawnTime", INI::parseDurationUnsignedInt, nullptr, offsetof(BannerCarrierUpdateModuleData, m_meleeFreeUnitSpawnTime) },
		{ "DiedRespawnTime", INI::parseDurationUnsignedInt, nullptr, offsetof(BannerCarrierUpdateModuleData, m_diedRespawnTime) },
		{ "MeleeFreeBannerReSpawnTime", INI::parseDurationUnsignedInt, nullptr, offsetof(BannerCarrierUpdateModuleData, m_meleeFreeBannerRespawnTime) },
		{ "ScanHordeDistance", INI::parseReal, nullptr, offsetof(BannerCarrierUpdateModuleData, m_scanHordeDistance) },
		{ "ReplenishNearbyHorde", INI::parseBool, nullptr, offsetof(BannerCarrierUpdateModuleData, m_replenishNearbyHorde) },
		{ "ReplenishAllNearbyHordes", INI::parseBool, nullptr, offsetof(BannerCarrierUpdateModuleData, m_replenishAllNearbyHordes) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	UpdateModuleData::buildFieldParse(p);
	p.add(fields);
}

void BannerCarrierUpdateModuleData::parseMorph(INI* ini, void*, void* store, const void*)
{
	Morph morph;
	while (const char* field = ini->getNextTokenOrNull(INI::getSepsColon()))
	{
		if (stricmp(field, "UnitType") == 0)
			morph.unitType = ini->getNextToken(INI::getSepsColon());
		else if (stricmp(field, "Locomotor") == 0)
			morph.locomotor = AIUpdateModuleData::findLocomotorSetType(ini->getNextToken(INI::getSepsColon()));
		else if (stricmp(field, "ModelState") == 0)
		{
			std::istringstream conditions(ini->getNextQuotedAsciiString().str());
			for (std::string name; conditions >> name; )
				morph.conditions.set(static_cast<ModelConditionFlagType>(INI::scanIndexList(name.c_str(), ModelConditionFlags::getBitNames())));
		}
		else
			throw INI_INVALID_DATA;
	}
	if (morph.unitType.isEmpty())
		throw INI_INVALID_DATA;
	static_cast<std::vector<Morph>*>(store)->push_back(std::move(morph));
}

BannerCarrierUpdate::BannerCarrierUpdate(Thing* thing, const ModuleData* data) : UpdateModule(thing, data)
{
}

BannerCarrierUpdate::~BannerCarrierUpdate() = default;

void BannerCarrierUpdate::crc(Xfer* xfer) { UpdateModule::crc(xfer); }
void BannerCarrierUpdate::xfer(Xfer* xfer) { UpdateModule::xfer(xfer); }
void BannerCarrierUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
	const auto* data = getBannerCarrierUpdateModuleData();
	if (RetailFileLayers::enabled() && data->m_replenishNearbyHorde && data->m_idleSpawnRate &&
		!getObject()->isEffectivelyDead() && friend_getNextCallFrame() == UPDATE_SLEEP_FOREVER)
		friend_setNextCallFrame(TheGameLogic->getFrame());
}

BannerCarrierUpdate* BannerCarrierUpdate::find(Object* carrier)
{
	static const NameKeyType key = NAMEKEY("BannerCarrierUpdate");
	return static_cast<BannerCarrierUpdate*>(carrier->findUpdateModule(key));
}

void BannerCarrierUpdate::applyMorph(const AsciiString& unitType)
{
	const auto& morphs = getBannerCarrierUpdateModuleData()->m_morphs;
	const auto found = std::find_if(morphs.begin(), morphs.end(), [&unitType](const auto& morph) {
		return morph.unitType.compareNoCase(unitType) == 0;
	});
	const auto* previous = m_morph;
	m_morph = found == morphs.end() ? nullptr : &*found;
	if (previous == m_morph)
		return;
	Object* carrier = getObject();
	if (previous)
		carrier->clearModelConditionFlags(previous->conditions);
	if (m_morph)
		carrier->setModelConditionFlags(m_morph->conditions);
	const Int locomotor = m_morph ? m_morph->locomotor : LOCOMOTORSET_INVALID;
	if (AIUpdateInterface* ai = carrier->getAI())
	{
		if (locomotor != LOCOMOTORSET_INVALID)
			ai->chooseLocomotorSet(static_cast<LocomotorSetType>(locomotor));
		else if (previous && previous->locomotor != LOCOMOTORSET_INVALID)
			ai->chooseLocomotorSet(LOCOMOTORSET_NORMAL);
	}
}

UpdateSleepTime BannerCarrierUpdate::update()
{
	Object* source = getObject();
	if (const auto* horde = HordeContain::get(source->getContainedBy()))
		applyMorph(horde->getBannerUnitType());

	const auto* data = getBannerCarrierUpdateModuleData();
	if (!data->m_replenishNearbyHorde || !data->m_idleSpawnRate || source->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;
	if (source->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) || !bannerUpgradeComplete(source, *data))
		return UPDATE_SLEEP(data->m_idleSpawnRate);

	std::vector<ObjectID> eligible;
	{
		SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(source, data->m_scanHordeDistance, FROM_CENTER_2D);
		MemoryPoolObjectHolder hold(iter);
		const Player* owner = source->getControllingPlayer();
		for (Object* horde = iter->first(); horde; horde = iter->next())
			if (horde != source && HordeContain::get(horde) &&
				(source->getRelationship(horde) == ALLIES || owner == horde->getControllingPlayer()))
				eligible.push_back(horde->getID());
	}
	std::sort(eligible.begin(), eligible.end());

	const UnsignedInt frame = TheGameLogic->getFrame();
	for (ObjectID id : eligible)
	{
		auto* horde = const_cast<HordeContain*>(HordeContain::get(TheGameLogic->findObjectByID(id)));
		if (!horde || (horde->m_lastMeleeFrame && frame - horde->m_lastMeleeFrame < data->m_meleeFreeUnitSpawnTime))
			continue;
		if (Object* member = horde->replenishMember(*data))
		{
			if (RetailTestCommands::tracesCombat())
				std::fprintf(stderr, "TEST replenish frame=%u source=%s horde=%u member=%u\n", frame,
					source->getTemplate()->getName().str(), static_cast<unsigned>(id), static_cast<unsigned>(member->getID()));
			if (!data->m_replenishAllNearbyHordes)
				break;
		}
	}
	return UPDATE_SLEEP(data->m_idleSpawnRate);
}

void BannerCarrierUpdate::playMorphFX(const Object* horde) const
{
	const auto& name = getBannerCarrierUpdateModuleData()->m_morphFX;
	if (!TheFXListStore || name.isEmpty())
		return;
	const auto* fx = TheFXListStore->findFXList(name.str());
	const auto* formation = HordeContain::get(horde);
	Coord3D center = {};
	Int count = 0;
	if (formation)
		for (const Object* member : *formation->getContainedItemsList())
			if (!member->isEffectivelyDead() && !formation->isBannerCarrier(member))
			{
				center += *member->getPosition();
				++count;
			}
	if (count == 0)
	{
		FXList::doFXObj(fx, horde);
		return;
	}
	center.scale(1.0f / count);
	Matrix3D transform = *horde->getTransformMatrix();
	transform.Set_Translation(Vector3(center.x, center.y, center.z));
	FXList::doFXPos(fx, &center, &transform);
}
