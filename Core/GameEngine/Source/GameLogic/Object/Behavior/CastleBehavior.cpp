#include "PreRTS.h"

#include "GameLogic/Module/CastleBehavior.h"

#include <algorithm>
#include <cstdio>

#include "Common/Errors.h"
#include "Common/INI.h"
#include "Common/INIAudit.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/CastleRules.h"
#include "GameLogic/CastleTemplateStore.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/FoundationAIUpdate.h"

CastleBehaviorModuleData::CastleBehaviorModuleData()
{
	for (KindOfType kind : {KINDOF_STRUCTURE, KINDOF_WALK_ON_TOP_OF_WALL, KINDOF_BASE_FOUNDATION,
		KINDOF_BASE_DEFENSE_FOUNDATION, KINDOF_TACTICAL_MARKER})
		m_ownedFilter.m_includeKinds.set(kind);
}

void CastleBehaviorModuleData::parseCampEntry(INI* ini, void*, void* store, const void*)
{
	CampEntry entry;
	entry.faction = ini->getNextAsciiString();
	entry.camp = ini->getNextAsciiString();
	if (const char* cost = ini->getNextTokenOrNull())
	{
		entry.cost = INI::scanUnsignedInt(cost);
		if (const char* value = ini->getNextTokenOrNull())
			entry.extraValue = INI::scanUnsignedInt(value);
	}
	static_cast<std::vector<CampEntry>*>(store)->push_back(std::move(entry));
}

void CastleBehaviorModuleData::parseDecalEntry(INI* ini, void*, void* store, const void*)
{
	DecalEntry entry;
	entry.faction = ini->getNextAsciiString();
	entry.decal = ini->getNextAsciiString();
	entry.size = INI::scanReal(ini->getNextToken());
	static_cast<std::vector<DecalEntry>*>(store)->push_back(std::move(entry));
}

void CastleBehaviorModuleData::parsePreBuiltEntry(INI* ini, void*, void* store, const void*)
{
	PreBuiltEntry entry;
	entry.name = ini->getNextAsciiString();
	entry.count = INI::scanInt(ini->getNextToken());
	static_cast<std::vector<PreBuiltEntry>*>(store)->push_back(std::move(entry));
}

void CastleBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);

	static const FieldParse fields[] =
	{
		{ "CastleToUnpackForFaction", parseCampEntry, nullptr, offsetof(CastleBehaviorModuleData, m_castles) },
		{ "FilterValidOwnedEntries", SpecialPowerObjectFilter::parse, nullptr, offsetof(CastleBehaviorModuleData, m_ownedFilter) },
		{ "FilterCrew", SpecialPowerObjectFilter::parse, nullptr, offsetof(CastleBehaviorModuleData, m_crewFilter) },
		{ "FactionDecal", parseDecalEntry, nullptr, offsetof(CastleBehaviorModuleData, m_decals) },
		{ "PreBuiltList", parsePreBuiltEntry, nullptr, offsetof(CastleBehaviorModuleData, m_preBuiltList) },
		{ "PreBuiltPlyr", INI::parseAsciiString, nullptr, offsetof(CastleBehaviorModuleData, m_preBuiltPlayer) },
		{ "DecalName", INI::parseAsciiString, nullptr, offsetof(CastleBehaviorModuleData, m_decalName) },
		{ "DecalSize", INI::parseReal, nullptr, offsetof(CastleBehaviorModuleData, m_decalSize) },
		{ "FadeTime", INI::parseReal, nullptr, offsetof(CastleBehaviorModuleData, m_fadeTime) },
		{ "UnpackDelayTime", INI::parseReal, nullptr, offsetof(CastleBehaviorModuleData, m_unpackDelayTime) },
		{ "BuildTime", INI::parseReal, nullptr, offsetof(CastleBehaviorModuleData, m_buildTime) },
		{ "ScanDistance", INI::parseReal, nullptr, offsetof(CastleBehaviorModuleData, m_scanDistance) },
		{ "MaxCastleRadius", INI::parseReal, nullptr, offsetof(CastleBehaviorModuleData, m_maxCastleRadius) },
		{ "CrewPrepareTime", INI::parseUnsignedInt, nullptr, offsetof(CastleBehaviorModuleData, m_crewPrepareTime) },
		{ "InstantUnpack", INI::parseBool, nullptr, offsetof(CastleBehaviorModuleData, m_instantUnpack) },
		{ "KeepDeathKillsEverything", INI::parseBool, nullptr, offsetof(CastleBehaviorModuleData, m_keepDeathKillsEverything) },
		{ "Summoned", INI::parseBool, nullptr, offsetof(CastleBehaviorModuleData, m_summoned) },
		{ "CrewPrepareInterval", INI::parseUnsignedInt, nullptr, offsetof(CastleBehaviorModuleData, m_crewPrepareInterval) },
		{ "CrewPrepareFX", INI::parseAsciiString, nullptr, offsetof(CastleBehaviorModuleData, m_crewPrepareFX) },
		{ "CrewReleaseFX", INI::parseAsciiString, nullptr, offsetof(CastleBehaviorModuleData, m_crewReleaseFX) },
		{ "EvaEnemyCastleSightedEvent", INI::parseAsciiString, nullptr, offsetof(CastleBehaviorModuleData, m_enemySightedEvent) },
		{ "DisableStructureRotation", INI::parseBool, nullptr, offsetof(CastleBehaviorModuleData, m_disableStructureRotation) },
		{ "TransferFoundationHealthToCastleUponUnpack", INI::parseBool, nullptr, offsetof(CastleBehaviorModuleData, m_transferFoundationHealth) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields, 0);
}

CastleBehavior::CastleBehavior(Thing* thing, const ModuleData* data) : UpdateModule(thing, data)
{
	setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
}

CastleBehavior::~CastleBehavior() = default;

void CastleBehavior::onCreate()
{
	m_buildComplete = FALSE;
}

void CastleBehavior::onBuildComplete()
{
	if (m_buildComplete)
		return;

	m_buildComplete = TRUE;
	if (getCastleBehaviorModuleData()->m_instantUnpack)
	{
		m_pendingUnpack = TRUE;
		setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
	}
}

UpdateSleepTime CastleBehavior::update()
{
	if (m_pendingUnpack)
	{
		m_pendingUnpack = FALSE;
		unpack();
	}
	if (!m_unpacked || m_keepLost || m_keep == INVALID_ID)
		return UPDATE_SLEEP_FOREVER;

	checkKeep();
	return m_keepLost ? UPDATE_SLEEP_FOREVER : UPDATE_SLEEP_NONE;
}

void CastleBehavior::checkKeep()
{
	const Object* keep = TheGameLogic->findObjectByID(m_keep);
	switch (CastleRules::keepAction(keep != nullptr, keep && keep->isEffectivelyDead(),
		getCastleBehaviorModuleData()->m_keepDeathKillsEverything))
	{
	case CastleRules::KeepAction::None:
		return;
	case CastleRules::KeepAction::Lost:
		m_keepLost = TRUE;
		return;
	case CastleRules::KeepAction::KillEverything:
		m_keepLost = TRUE;
		killEverything();
		return;
	}
}

void CastleBehavior::killEverything()
{
	std::vector<ObjectID> pieces;
	pieces.reserve(m_members.size() + 1);
	pieces.push_back(getObject()->getID());
	pieces.insert(pieces.end(), m_members.begin(), m_members.end());

	for (ObjectID id : pieces)
	{
		Object* piece = TheGameLogic->findObjectByID(id);
		if (!piece)
			continue;
		if (piece->isKindOf(KINDOF_BASE_FOUNDATION))
		{
			const FoundationAIUpdate* foundation = FoundationAIUpdate::find(*piece);
			Object* structure = foundation ? foundation->getStructure() : nullptr;
			if (structure && !structure->isEffectivelyDead())
				structure->kill();
		}
		else if (!piece->isEffectivelyDead())
		{
			piece->kill();
		}
	}

	for (ObjectID id : pieces)
	{
		Object* piece = TheGameLogic->findObjectByID(id);
		if (piece && piece->isKindOf(KINDOF_BASE_FOUNDATION))
			TheGameLogic->destroyObject(piece);
	}
}

void CastleBehavior::unpack()
{
	Object* packed = getObject();
	Player* player = packed->getControllingPlayer();
	const AsciiString side = player->getSide();
	const auto* data = getCastleBehaviorModuleData();
	const auto faction = std::find_if(data->m_castles.begin(), data->m_castles.end(),
		[&side](const CastleBehaviorModuleData::CampEntry& entry)
		{
			return stricmp(entry.faction.str(), side.str()) == 0;
		});
	if (faction == data->m_castles.end())
		return;

	const CastleTemplates::Members* members = nullptr;
	try
	{
		members = CastleTemplates::find(faction->camp);
	}
	catch (ErrorCode)
	{
		std::fprintf(stderr, "Castle %s: cannot read the castle templates, nothing unpacked\n", faction->camp.str());
		return;
	}
	if (!members || members->empty())
		return;

	std::vector<const ThingTemplate*> templates;
	templates.reserve(members->size());
	for (std::size_t i = 0; i < members->size(); ++i)
	{
		const auto& member = (*members)[i];
		const ThingTemplate* definition = nullptr;
		if (!member.templateName.empty() && member.templateName.size() < AsciiString::MAX_LEN)
			definition = TheThingFactory->findTemplate(member.templateName.c_str(), FALSE);
		if (!definition || definition->isKindOf(KINDOF_DRAWABLE_ONLY))
		{
			std::fprintf(stderr, "Castle %s: skipped member %u with %s template '%s'\n", faction->camp.str(),
				static_cast<unsigned>(i), member.templateName.empty() ? "an empty" : "an unknown", member.templateName.c_str());
			if (INIAudit::isEnabled())
				INIAudit::record("error", "Invalid castle member template: " + member.templateName, faction->camp.str(), 0);
			definition = nullptr;
		}
		templates.push_back(definition);
	}

	const Matrix3D transform = *packed->getTransformMatrix();
	const Real orientation = packed->getOrientation();
	m_members.reserve(members->size());
	TheAI->pathfinder()->removeObjectFromPathfindMap(packed);
	packed->setStatus(MAKE_OBJECT_STATUS_MASK3(OBJECT_STATUS_UNSELECTABLE, OBJECT_STATUS_NO_COLLISIONS, OBJECT_STATUS_MASKED));
	if (Drawable* drawable = packed->getDrawable())
		drawable->setDrawableHidden(TRUE);
	m_unpacked = TRUE;

	for (std::size_t i = 0; i < members->size(); ++i)
	{
		const ThingTemplate* definition = templates[i];
		if (!definition)
			continue;

		const auto& member = (*members)[i];
		Vector3 position;
		Matrix3D::Transform_Vector(transform, Vector3(member.offsetX, member.offsetY, member.offsetZ), &position);
		const Coord3D ground = {position.X, position.Y, TheTerrainLogic->getGroundHeight(position.X, position.Y)};
		Player* owner = data->m_ownedFilter.matchesTemplate(*definition) ? player : ThePlayerList->getNeutralPlayer();
		Team* team = owner->getDefaultTeam();
		const Bool structure = definition->isKindOf(KINDOF_STRUCTURE);
		const auto construction = MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNDER_CONSTRUCTION);
		Object* piece = TheThingFactory->newObjectFromExactTemplate(definition, team,
			structure ? construction : OBJECT_STATUS_MASK_NONE);
		piece->setOrientation(normalizeAngle(orientation + member.angle));
		piece->setPosition(&ground);
		if (structure)
		{
			owner->onStructureCreated(nullptr, piece);
			piece->clearStatus(construction);
		}
		piece->setModelConditionState(MODELCONDITION_CONSTRUCTION_COMPLETE);

		for (BehaviorModule** module = piece->getBehaviorModules(); *module; ++module)
		{
			CreateModuleInterface* create = (*module)->getCreate();
			if (create && create->shouldDoOnBuildComplete())
				create->onBuildComplete();
		}

		if (team)
			team->setActive();
		if (structure)
			owner->onStructureConstructionComplete(nullptr, piece, TRUE);
		else
			TheAI->pathfinder()->addObjectToPathfindMap(piece);

		m_members.push_back(piece->getID());
		if (piece->isKindOf(KINDOF_CASTLE_KEEP))
			m_keep = piece->getID();
	}
}

void CastleBehavior::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
}

void CastleBehavior::xfer(Xfer* xfer)
{
	XferVersion version = 2;
	xfer->xferVersion(&version, 2);
	UpdateModule::xfer(xfer);
	xfer->xferBool(&m_buildComplete);
	xfer->xferBool(&m_pendingUnpack);
	xfer->xferBool(&m_unpacked);
	xfer->xferObjectID(&m_keep);
	if (version >= 2)
		xfer->xferBool(&m_keepLost);

	UnsignedInt count = static_cast<UnsignedInt>(m_members.size());
	xfer->xferUnsignedInt(&count);
	if (xfer->getXferMode() == XFER_LOAD)
	{
		if (count > CastleTemplates::MAX_BODY_SIZE / 20)
			throw ERROR_CORRUPT_FILE_FORMAT;
		m_members.resize(count);
	}
	for (auto& member : m_members)
		xfer->xferObjectID(&member);
}

void CastleBehavior::loadPostProcess()
{
	UpdateModule::loadPostProcess();
	if (m_unpacked)
	{
		if (Drawable* drawable = getObject()->getDrawable())
			drawable->setDrawableHidden(TRUE);
	}
}
