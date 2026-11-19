#include "PreRTS.h"

#define DEFINE_LOCOMOTORSET_NAMES

#include "Common/BitFlagsIO.h"
#include "Common/Player.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/WeaponSetFlags.h"
#include "Lib/trig.h"

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
	if (checkCapacity && (Int)getContainCount() >= getContainMax())
		return FALSE;
	return TRUE;
}

void HordeContain::createPayload()
{
	const HordeContainModuleData* data = getHordeContainModuleData();
	const HordeLayout& layout = data->m_layout;
	Object* horde = getObject();

	enableLoadSounds( FALSE );
	for (const HordeMemberSlot& entry : layout.initialMembers(getContainMax()))
	{
		const HordePayload& payload = layout.payloads()[entry.payload];
		const ThingTemplate* memberTemplate = TheThingFactory->findTemplate( payload.name.c_str() );
		if (memberTemplate == nullptr)
		{
			DEBUG_CRASH(( "HordeContain: '%s' has an unknown payload '%s'", horde->getTemplate()->getName().str(), payload.name.c_str() ));
			continue;
		}
		Object* member = TheThingFactory->newObject( memberTemplate, horde->getTeam() );
		if (!isValidContainerFor( member, TRUE ))
		{
			DEBUG_CRASH(( "HordeContain: '%s' cannot hold its payload '%s'", horde->getTemplate()->getName().str(), payload.name.c_str() ));
			TheGameLogic->destroyObject( member );
			continue;
		}
		addToContain( member );
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
	m_slotMembers.resize( layout.slots().size(), INVALID_ID );
	std::vector<bool> occupied( m_slotMembers.size(), false );
	for (size_t index = 0; index < m_slotMembers.size(); ++index)
		occupied[index] = m_slotMembers[index] != INVALID_ID;
	const Int slot = layout.findSlot( rider->getTemplate()->getName().str(), occupied );
	if (slot >= 0)
		m_slotMembers[slot] = rider->getID();
}

void HordeContain::onContaining( Object *rider, Bool wasSelected )
{
	OpenContain::onContaining( rider, wasSelected );
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

void HordeContain::redeployOccupants()
{
	for (Object* member : getContainList())
		placeMember( member, getMemberSlot( member->getID() ) );
}

void HordeContain::placeMember( Object *member, Int slot ) const
{
	const Object* horde = getObject();
	const Real angle = horde->getOrientation();
	Coord3D position = *horde->getPosition();
	const std::vector<HordeSlot>& slots = getHordeContainModuleData()->m_layout.slots();
	if (slot >= 0 && slot < (Int)slots.size())
	{
		const HordePoint offset = HordeLayout::rotate( slots[slot].position, Cos( angle ), Sin( angle ) );
		position.x += offset.x;
		position.y += offset.y;
	}
	if (TheTerrainLogic)
		position.z = TheTerrainLogic->getGroundHeight( position.x, position.y );
	member->setPosition( &position );
	member->setOrientation( angle );
}

void HordeContain::crc( Xfer *xfer )
{
	TransportContain::crc( xfer );
}

void HordeContain::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	TransportContain::xfer( xfer );

	xfer->xferSTLObjectIDVector( &m_slotMembers );
}

void HordeContain::loadPostProcess()
{
	TransportContain::loadPostProcess();
}
