#include "PreRTS.h"

#include "GameClient/Module/EvaAnnounceClientCreate.h"

#include <algorithm>
#include <limits>

#include "Common/GameUtility.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameClient/RetailEva.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"

void EvaAnnounceClientCreateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ClientUpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] = {
		{ "AnnouncementEventEnemy", INI::parseAsciiString, nullptr, offsetof(EvaAnnounceClientCreateModuleData, enemy) },
		{ "AnnouncementEventAlly", INI::parseAsciiString, nullptr, offsetof(EvaAnnounceClientCreateModuleData, ally) },
		{ "AnnouncementEventOwner", INI::parseAsciiString, nullptr, offsetof(EvaAnnounceClientCreateModuleData, owner) },
		{ "OnlyIfVisible", INI::parseBool, nullptr, offsetof(EvaAnnounceClientCreateModuleData, onlyIfVisible) },
		{ "CountAsFirstSightedAnnoucement", INI::parseBool, nullptr, offsetof(EvaAnnounceClientCreateModuleData, firstSighted) },
		{ "UseObjectsPosition", INI::parseBool, nullptr, offsetof(EvaAnnounceClientCreateModuleData, usePosition) },
		{ "CreateFakeRadarEvent", INI::parseBool, nullptr, offsetof(EvaAnnounceClientCreateModuleData, fakeRadar) },
		{ "DelayBeforeAnnouncementMS", INI::parseDurationUnsignedInt, nullptr, offsetof(EvaAnnounceClientCreateModuleData, delay) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

EvaAnnounceClientCreate::EvaAnnounceClientCreate(Thing* thing, const ModuleData* data)
	: ClientUpdateModule(thing, data)
{
	if (TheGameLogic)
	{
		const UnsignedInt frame = TheGameLogic->getFrame();
		const UnsignedInt delay = std::max(getEvaAnnounceClientCreateModuleData()->delay, 2u);
		m_due = std::max(7u, frame + std::min(delay, std::numeric_limits<UnsignedInt>::max() - frame));
	}
}

EvaAnnounceClientCreate::~EvaAnnounceClientCreate() = default;

void EvaAnnounceClientCreate::clientUpdate()
{
	if (!RetailFileLayers::enabled() || !m_pending || !TheGameLogic || TheGameLogic->getFrame() < m_due)
		return;
	m_pending = FALSE;
	const auto* data = getEvaAnnounceClientCreateModuleData();
	const Object* object = getDrawable()->getObject();
	const Player* viewer = rts::getObservedOrLocalPlayer_Safe();
	if (!object || !viewer || object->isEffectivelyDead())
		return;
	const Player* owner = object->getControllingPlayer();
	const auto relationship = viewer->getRelationship(object->getTeam());
	const PartitionData* partition = object->friend_getPartitionData();
	const Bool hidden = partition && !object->isKindOf(KINDOF_ALWAYS_VISIBLE) &&
		!partition->hasClearCell(viewer->getPlayerIndex());
	if (data->onlyIfVisible && (hidden ||
		(relationship == ENEMIES && object->testStatus(OBJECT_STATUS_STEALTHED) && !object->testStatus(OBJECT_STATUS_DETECTED))))
		return;
	if (data->firstSighted)
		RetailEva::markFirstSighted(*object);
	const AsciiString& event = viewer == owner ? data->owner :
		relationship == ALLIES ? data->ally :
		relationship == ENEMIES ? data->enemy : AsciiString::TheEmptyString;
	if (event.isEmpty() || event.isNone())
		return;
	RetailEva::queue(viewer, event, data->usePosition ? object->getPosition() : nullptr,
		data->fakeRadar ? RetailEva::Ping::JumpOnly : RetailEva::Ping::None, object->getPosition());
}

void EvaAnnounceClientCreate::crc(Xfer* xfer)
{
	ClientUpdateModule::crc(xfer);
}

void EvaAnnounceClientCreate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	ClientUpdateModule::xfer(xfer);
	xfer->xferUnsignedInt(&m_due);
	xfer->xferBool(&m_pending);
}

void EvaAnnounceClientCreate::loadPostProcess()
{
	ClientUpdateModule::loadPostProcess();
}
