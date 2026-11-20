#include "PreRTS.h"

#define DEFINE_LOCOMOTORSET_NAMES

#include "Common/BitFlagsIO.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/LogicRandomValue.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/WeaponSetFlags.h"
#include "Lib/trig.h"

#include <algorithm>
#include <string>
#include <utility>

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
}

HordeContainModuleData::HordeContainModuleData()
{
	SET_ALL_KINDOFMASK_BITS(m_allowInsideKindOf);
	m_randomOffset.x = 0.0f;
	m_randomOffset.y = 0.0f;
	m_leaderPosition.zero();
	m_bannerCarrierHordeDeathType = DEATH_TYPE_FLAGS_NONE;
	m_frontAngle = 0.0f;
	m_meleeAttackLeashDistance = 0.0f;
	m_backUpMinDistance = 0.0f;
	m_backUpMaxDistance = 0.0f;
	m_backupPercentage = 0.0f;
	m_cowerRadius = 0.0f;
	m_visionRearOverride = 0.0f;
	m_visionSideOverride = 0.0f;
	m_backUpMinDelayTime = 0;
	m_backUpMaxDelayTime = 0;
	m_flankedDelay = 0;
	m_flankedDuration = 0;
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
		{ "PassengerFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof( HordeContainModuleData, m_passengerFilter ) },
		{ "ManualPickUpFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof( HordeContainModuleData, m_manualPickUpFilter ) },
		{ "ShowPips", INI::parseBool, nullptr, offsetof( HordeContainModuleData, m_showPips ) },
		{ "ObjectStatusOfContained", ObjectStatusMaskType::parseFromINI, nullptr, offsetof( HordeContainModuleData, m_objectStatusOfContained ) },
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
void HorseHordeContain::crc(Xfer* xfer) { BehaviorModule::crc(xfer); }
void HorseHordeContain::xfer(Xfer* xfer) { BehaviorModule::xfer(xfer); }
void HorseHordeContain::loadPostProcess() { BehaviorModule::loadPostProcess(); }

AODHordeContain::~AODHordeContain() = default;
void AODHordeContain::crc(Xfer* xfer) { BehaviorModule::crc(xfer); }
void AODHordeContain::xfer(Xfer* xfer) { BehaviorModule::xfer(xfer); }
void AODHordeContain::loadPostProcess() { BehaviorModule::loadPostProcess(); }

HordeContain::HordeContain( Thing *thing, const ModuleData *moduleData ) : TransportContain( thing, moduleData )
{
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
	if (checkCapacity && (Int)getContainCount() >= getContainMax())
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

void HordeContain::createPayload()
{
	const HordeContainModuleData* data = getHordeContainModuleData();
	const HordeLayout& layout = data->m_layout;
	Object* horde = getObject();
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
}

void HordeContain::addToContainList( Object *rider )
{
	TransportContain::addToContainList( rider );
	if (getMemberSlot( rider->getID() ) >= 0)
		return;

	const HordeLayout& layout = getHordeContainModuleData()->m_layout;
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
		const Coord2D& extent = getHordeContainModuleData()->m_randomOffset;
		m_slotOffsets[slot].x = extent.x > 0.0f ? GameLogicRandomValueReal(-extent.x, extent.x) : 0.0f;
		m_slotOffsets[slot].y = extent.y > 0.0f ? GameLogicRandomValueReal(-extent.y, extent.y) : 0.0f;
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
	rider->setStatus( getHordeContainModuleData()->m_objectStatusOfContained );
}

void HordeContain::onRemoving( Object *rider )
{
	for (ObjectID& member : m_slotMembers)
	{
		if (member == rider->getID())
			member = INVALID_ID;
	}
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
	for (size_t index = 0; index < m_slotMembers.size(); ++index)
	{
		if (m_slotMembers[index] == id)
			return (Int)index;
	}
	return -1;
}

void HordeContain::crc( Xfer *xfer )
{
	TransportContain::crc( xfer );
	xfer->xferBool(&m_hadMembers);
	for (size_t index = 0; index < m_slotMembers.size(); ++index)
	{
		xfer->xferObjectID(&m_slotMembers[index]);
		xfer->xferCoord2D(&m_slotOffsets[index]);
	}
}

void HordeContain::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 2;
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
		m_hadMembers = getContainCount() != 0;
}

void HordeContain::loadPostProcess()
{
	TransportContain::loadPostProcess();
}

const HordeContain* HordeContain::get(const Object* object)
{
	const ContainModuleInterface* contain = object ? object->getContain() : nullptr;
	return contain && contain->isHordeContain() ? static_cast<const HordeContain*>(contain) : nullptr;
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
	const Int slot = getMemberSlot(member->getID());
	const auto* data = getHordeContainModuleData();
	const Int rank = slot >= 0 ? data->m_layout.slots()[slot].rank : 0;
	return HordeLayout::rankCanAttack(rank, data->m_ranksToReleaseWhenAttacking, data->m_ranksToJustFreeWhenAttacking);
}

Coord3D HordeContain::getMemberPosition(ObjectID id, Bool followLeader) const
{
	const Object* horde = getObject();
	Coord3D position = *horde->getPosition();
	const auto& layout = getHordeContainModuleData()->m_layout;
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
	if (TheTerrainLogic)
		position.z = TheTerrainLogic->getGroundHeight(position.x, position.y);
	return position;
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
	if (m_hadMembers && getContainCount() == 0)
	{
		Object* horde = getObject();
		horde->setModelConditionState(MODELCONDITION_HORDE_EMPTY);
		horde->setEffectivelyDead(TRUE);
		DamageInfo damage;
		horde->onDie(&damage);
		TheGameLogic->destroyObject(horde);
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
