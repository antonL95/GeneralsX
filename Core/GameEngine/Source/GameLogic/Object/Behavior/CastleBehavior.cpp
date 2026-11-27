#include "PreRTS.h"

#include "GameLogic/Module/CastleBehavior.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string_view>
#include <utility>

#include "Common/Errors.h"
#include "Common/INI.h"
#include "Common/Money.h"
#include "Common/INIAudit.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/PlayerList.h"
#include "Common/Team.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameClient/RetailEva.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/CastleRules.h"
#include "GameLogic/CastleTemplateStore.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/FoundationAIUpdate.h"
#include "GameLogic/Module/CastleUpgrade.h"
#include "GameLogic/Module/RotwkParseOnlyModules.h"

namespace
{
const CastleTemplates::Camp* readCastleCamp(const AsciiString& name)
{
	try
	{
		return CastleTemplates::find(name);
	}
	catch (ErrorCode)
	{
		std::fprintf(stderr, "Castle %s: cannot read the castle templates\n", name.str());
		return nullptr;
	}
}

const ThingTemplate* findCastleMemberTemplate(const std::string& name)
{
	if (name.empty() || name.size() >= AsciiString::MAX_LEN)
		return nullptr;
	const ThingTemplate* definition = TheThingFactory->findTemplate(name.c_str(), FALSE);
	return definition && !definition->isKindOf(KINDOF_DRAWABLE_ONLY) ? definition : nullptr;
}

AsciiString castlePerimeterPrefix(const Object& packed)
{
	AsciiString prefix;
	prefix.format("Dynamic_%s_of_id_%d_at_index_", packed.getTemplate()->getName().str(), packed.getID());
	return prefix;
}
}

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
	entry.plotIndex = INI::scanInt(ini->getNextToken());
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

void CastleBehavior::onDelete()
{
	if (m_unpacked)
		PolygonTrigger::retireCastleAreas(getObject()->getID());
}

void CastleBehavior::onCreate()
{
	m_buildComplete = FALSE;
}

void CastleBehavior::onBuildComplete()
{
	if (m_buildComplete)
		return;

	m_buildComplete = TRUE;
	const auto* data = getCastleBehaviorModuleData();
	m_pendingUnpack = data->m_instantUnpack || data->m_preBuiltPlayer.isNotEmpty();
	if (m_pendingUnpack || data->m_scanDistance > 0.0f)
		setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
}

UpdateSleepTime CastleBehavior::update()
{
	if (m_pendingUnpack)
	{
		const UnsignedInt frame = TheGameLogic->getFrame();
		if (m_orderedUnpack && frame < m_unpackFrame)
			return UPDATE_SLEEP(m_unpackFrame - frame);
		m_pendingUnpack = FALSE;
		unpack(m_orderedUnpack);
		m_orderedUnpack = FALSE;
	}
	if (!m_unpacked)
	{
		const auto* data = getCastleBehaviorModuleData();
		if (!m_buildComplete || data->m_instantUnpack || data->m_scanDistance <= 0.0f || getObject()->isEffectivelyDead())
			return UPDATE_SLEEP_FOREVER;
		scanForCapture();
		return UPDATE_SLEEP(LOGICFRAMES_PER_SECOND);
	}
	if (m_keepLost)
	{
		if (getCastleBehaviorModuleData()->m_keepDeathKillsEverything)
			return UPDATE_SLEEP_FOREVER;
		const UnsignedInt frame = TheGameLogic->getFrame();
		if (frame < m_unpackFrame)
			return UPDATE_SLEEP(m_unpackFrame - frame);
		if (!hasOccupiedPlots())
			repack();
		return UPDATE_SLEEP_NONE;
	}

	checkKeep();
	return UPDATE_SLEEP_NONE;
}

void CastleBehavior::checkKeep()
{
	const Object* keep = m_keep == INVALID_ID ? nullptr : TheGameLogic->findObjectByID(m_keep);
	switch (CastleRules::keepAction(keep != nullptr, keep && (keep->isEffectivelyDead() || keep->isDestroyed()),
		getCastleBehaviorModuleData()->m_keepDeathKillsEverything))
	{
	case CastleRules::KeepAction::None:
		return;
	case CastleRules::KeepAction::Lost:
	{
		if (hasOccupiedPlots())
			return;
		m_keepLost = TRUE;
		RetailEva::castleDestroyed(*getObject(), keep);
		const Real time = getCastleBehaviorModuleData()->m_fadeTime;
		const Real delay = std::isfinite(time) ? std::clamp(time, 0.0f, 300.0f) : 0.0f;
		m_unpackFrame = TheGameLogic->getFrame() + static_cast<UnsignedInt>(delay * LOGICFRAMES_PER_SECOND);
		return;
	}
	case CastleRules::KeepAction::KillEverything:
		m_keepLost = TRUE;
		RetailEva::castleDestroyed(*getObject(), keep);
		killEverything();
		return;
	}
}

Bool CastleBehavior::hasOccupiedPlots() const
{
	for (ObjectID id : m_members)
	{
		const Object* member = TheGameLogic->findObjectByID(id);
		if (!member || member->isDestroyed() || member->isKindOf(KINDOF_WALL_UPGRADE) ||
			(!member->isKindOf(KINDOF_BASE_FOUNDATION) && !member->isKindOf(KINDOF_BASE_DEFENSE_FOUNDATION)))
			continue;
		const auto* plot = FoundationAIUpdate::find(*member);
		if (plot && plot->getStructure())
			return TRUE;
	}
	return FALSE;
}

CastleBehavior* CastleBehavior::find(const Object* object)
{
	return object ? static_cast<CastleBehavior*>(object->findUpdateModule(NAMEKEY("CastleBehavior"))) : nullptr;
}

Object* CastleBehavior::getKeep() const
{
	Object* keep = m_keep == INVALID_ID ? nullptr : TheGameLogic->findObjectByID(m_keep);
	return keep && !keep->isEffectivelyDead() ? keep : nullptr;
}

ObjectID CastleBehavior::getCastleObjectID() const
{
	return getObject()->getID();
}

Bool CastleBehavior::unpackFree()
{
	if (!m_unpacked && !getObject()->isEffectivelyDead())
	{
		m_pendingUnpack = FALSE;
		m_orderedUnpack = FALSE;
		unpack();
		if (m_unpacked)
			setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
	}
	return m_unpacked;
}

CastleBehavior* CastleBehavior::findForMember(const Object& member)
{
	if (const CastleMemberBehavior* cm = CastleMemberBehavior::find(&member))
	{
		if (cm->getCastleID() != INVALID_ID)
		{
			if (CastleBehavior* castle = find(TheGameLogic->findObjectByID(cm->getCastleID())))
				return castle;
		}
	}
	if (const Object* plot = FoundationAIUpdate::findPlotFor(member))
		return findForMember(*plot);
	const NameKeyType key = NAMEKEY("CastleBehavior");
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		auto* castle = static_cast<CastleBehavior*>(object->findUpdateModule(key));
		if (castle && std::find(castle->m_members.begin(), castle->m_members.end(), member.getID()) != castle->m_members.end())
			return castle;
	}
	return nullptr;
}

Bool CastleBehavior::isOwnedMemberTemplate(const ThingTemplate& type) const
{
	return getCastleBehaviorModuleData()->m_ownedFilter.matchesTemplate(type);
}

void CastleBehavior::onObjectTeamChanged(Object* object, Team* team)
{
	if (!object || !team || !RetailFileLayers::enabled() || !object->areModulesReady())
		return;
	if (CastleBehavior* castle = find(object))
	{
		castle->transferOwnership(team);
		return;
	}
	if (object->isKindOf(KINDOF_CASTLE_KEEP))
	{
		if (CastleBehavior* castle = findForMember(*object); castle && castle->getKeep() == object)
			castle->transferOwnership(team);
	}
}

void CastleBehavior::transferOwnership(Team* newTeam)
{
	static Bool transferring = FALSE;
	if (!m_unpacked || !newTeam || transferring)
		return;
	transferring = TRUE;
	Object* packed = getObject();
	if (packed && packed->getTeam() != newTeam)
		packed->setTeam(newTeam);
	for (ObjectID id : m_members)
	{
		Object* member = TheGameLogic->findObjectByID(id);
		if (!member || member->isDestroyed() || member->isEffectivelyDead())
			continue;
		const CastleMemberBehavior* cm = CastleMemberBehavior::find(member);
		const Bool owned = cm && cm->getCastleID() == packed->getID()
			? cm->isOwnedByCastle() : isOwnedMemberTemplate(*member->getTemplate());
		if (!owned)
			continue;
		if (member->getTeam() != newTeam)
			member->setTeam(newTeam);
		if (const FoundationAIUpdate* plot = FoundationAIUpdate::find(*member))
		{
			Object* structure = plot->getStructure();
			if (structure && !structure->isDestroyed() && !structure->isEffectivelyDead() && structure->getTeam() != newTeam)
				structure->setTeam(newTeam);
		}
	}
	transferring = FALSE;
}

void CastleBehavior::grantMemberUpgrade(const Object& source, const UpgradeTemplate& upgrade)
{
	const auto grant = [&](Object* member)
	{
		if (member && member != &source && !member->isEffectivelyDead() &&
			member->getControllingPlayer() == source.getControllingPlayer() && !member->hasUpgrade(&upgrade))
			member->giveUpgrade(&upgrade);
	};
	for (ObjectID id : m_members)
	{
		Object* member = TheGameLogic->findObjectByID(id);
		grant(member);
		if (member)
		{
			if (const auto* foundation = FoundationAIUpdate::find(*member))
				grant(foundation->getStructure());
		}
	}
}

void CastleBehavior::applyKeepUpgrades(Object& member) const
{
	const Object* keep = TheGameLogic->findObjectByID(m_keep);
	if (!keep || keep->getControllingPlayer() != member.getControllingPlayer())
		return;

	const NameKeyType key = NAMEKEY("CastleUpgrade");
	for (BehaviorModule* const* module = keep->getBehaviorModules(); *module; ++module)
	{
		if ((*module)->getModuleNameKey() != key)
			continue;
		const auto* castleUpgrade = static_cast<const CastleUpgrade*>(*module);
		if (castleUpgrade->isAlreadyUpgraded())
		{
			const UpgradeTemplate* upgrade = castleUpgrade->getGrantedUpgrade();
			if (upgrade && !member.hasUpgrade(upgrade))
				member.giveUpgrade(upgrade);
		}
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

void CastleBehavior::repack()
{
	Object* packed = getObject();
	PolygonTrigger::retireCastleAreas(packed->getID());
	const Object* keep = TheGameLogic->findObjectByID(m_keep);
	if (keep && keep->getName().isNotEmpty())
		TheScriptEngine->transferObjectName(keep->getName(), packed);

	for (ObjectID id : m_members)
	{
		Object* member = TheGameLogic->findObjectByID(id);
		if (!member || id == m_keep)
			continue;
		if (const auto* plot = FoundationAIUpdate::find(*member))
		{
			if (Object* structure = plot->getStructure())
				TheGameLogic->destroyObject(structure);
		}
		TheGameLogic->destroyObject(member);
	}

	m_members.clear();
	m_keep = INVALID_ID;
	m_unpacked = FALSE;
	m_keepLost = FALSE;
	packed->clearStatus(MAKE_OBJECT_STATUS_MASK3(OBJECT_STATUS_UNSELECTABLE, OBJECT_STATUS_NO_COLLISIONS, OBJECT_STATUS_MASKED));
	Player* neutral = ThePlayerList->findPlayerWithNameKey(NAMEKEY("PlyrCivilian"));
	if (!neutral)
		neutral = ThePlayerList->getNeutralPlayer();
	packed->setTeam(neutral->getDefaultTeam());
	if (Drawable* drawable = packed->getDrawable())
		drawable->setDrawableHidden(FALSE);
	TheAI->pathfinder()->addObjectToPathfindMap(packed);
}

const CastleBehaviorModuleData::CampEntry* CastleBehavior::findCamp(const Player& player) const
{
	const AsciiString side = player.getSide();
	const auto* data = getCastleBehaviorModuleData();
	const auto faction = std::find_if(data->m_castles.begin(), data->m_castles.end(),
		[&side](const CastleBehaviorModuleData::CampEntry& entry)
		{
			return stricmp(entry.faction.str(), side.str()) == 0;
		});
	return faction == data->m_castles.end() ? nullptr : &*faction;
}

const CastleTemplates::Camp* CastleBehavior::findUnpackedCamp() const
{
	const Player* player = getObject()->getControllingPlayer();
	const auto* ownerCamp = player ? findCamp(*player) : nullptr;
	const CastleTemplates::Camp* bestCamp = nullptr;
	using CampMatch = std::pair<std::size_t, std::size_t>;
	CampMatch bestMatches{};
	const Matrix3D& transform = *getObject()->getTransformMatrix();
	const auto matchingMembers = [this, &transform](const CastleTemplates::Camp& camp) -> std::optional<CampMatch>
	{
		std::size_t index = 0;
		std::size_t positionMatches = 0;
		std::size_t templateMatches = 0;
		for (const auto& authored : camp.members)
		{
			const ThingTemplate* type = findCastleMemberTemplate(authored.templateName);
			if (!type)
				continue;
			if (index == m_members.size())
				return std::nullopt;
			const Object* member = TheGameLogic->findObjectByID(m_members[index++]);
			if (!member || member->getTemplate() != type)
				continue;
			++templateMatches;
			Vector3 position;
			Matrix3D::Transform_Vector(transform, Vector3(authored.offsetX, authored.offsetY, authored.offsetZ), &position);
			if (member->getPosition()->x == position.X && member->getPosition()->y == position.Y)
				++positionMatches;
		}
		return index == m_members.size() ? std::optional(CampMatch{positionMatches, templateMatches}) : std::nullopt;
	};

	if (ownerCamp)
	{
		bestCamp = readCastleCamp(ownerCamp->camp);
		if (bestCamp)
		{
			const auto matches = matchingMembers(*bestCamp);
			if (matches && matches->first == m_members.size())
				return bestCamp;
			bestMatches = matches.value_or(CampMatch{});
		}
	}

	for (const auto& entry : getCastleBehaviorModuleData()->m_castles)
	{
		if (&entry == ownerCamp)
			continue;
		const auto* camp = readCastleCamp(entry.camp);
		if (!camp)
			continue;
		const auto matches = matchingMembers(*camp);
		if (matches && (!bestCamp || *matches > bestMatches))
		{
			bestMatches = *matches;
			bestCamp = camp;
		}
	}
	return bestCamp;
}

void CastleBehavior::registerPerimeters(const CastleTemplates::Camp& camp, const Matrix3D& transform)
{
	const Object* packed = getObject();
	const AsciiString prefix = castlePerimeterPrefix(*packed);
	Bool registeredArea = FALSE;
	for (std::size_t i = 0; i < camp.perimeters.size(); ++i)
	{
		const auto& points = camp.perimeters[i];
		if (points.size() < 3)
			continue;
		auto* area = newInstance(PolygonTrigger)(static_cast<Int>(points.size()));
		AsciiString name;
		name.format("%s%d", prefix.str(), static_cast<Int>(i));
		area->setTriggerName(name);
		area->setCastleAreaOwner(packed->getID());
		for (const auto& [x, y] : points)
		{
			Vector3 world;
			Matrix3D::Transform_Vector(transform, Vector3(x, y, 0.0f), &world);
			area->addPoint({static_cast<Int>(world.X), static_cast<Int>(world.Y), 0});
		}
		PolygonTrigger::addPolygonTrigger(area);
		registeredArea = TRUE;
	}
	if (registeredArea)
		PolygonTrigger::refreshObjectMembership();
}

void CastleBehavior::restoreMissingPerimeters()
{
	const Object* packed = getObject();
	const AsciiString prefix = castlePerimeterPrefix(*packed);
	for (const auto* polygon = PolygonTrigger::getFirstPolygonTrigger(); polygon; polygon = polygon->getNext())
	{
		if (polygon->isCastleArea() && !polygon->isRetired() &&
			std::string_view(polygon->getTriggerName().str()).starts_with(prefix.str()))
			return;
	}
	if (const auto* camp = findUnpackedCamp())
		registerPerimeters(*camp, *packed->getTransformMatrix());
}

void CastleBehavior::unpack(Bool charge)
{
	Object* packed = getObject();
	Player* player = packed->getControllingPlayer();
	const auto* data = getCastleBehaviorModuleData();
	if (!charge && data->m_preBuiltPlayer.isNotEmpty())
	{
		if (Player* prebuilt = TheScriptEngine->getPlayerFromAsciiString(data->m_preBuiltPlayer))
			player = prebuilt;
	}
	const auto* faction = player ? findCamp(*player) : nullptr;
	if (!faction || packed->isEffectivelyDead())
		return;
	const UnsignedInt cost = charge ? faction->cost : 0;
	if (player->getMoney()->countMoney() < cost)
		return;

	const auto* camp = readCastleCamp(faction->camp);
	if (!camp || camp->members.empty())
		return;

	const auto& members = camp->members;
	std::vector<const ThingTemplate*> templates;
	templates.reserve(members.size());
	for (std::size_t i = 0; i < members.size(); ++i)
	{
		const auto& member = members[i];
		const ThingTemplate* definition = findCastleMemberTemplate(member.templateName);
		if (!definition)
		{
			std::fprintf(stderr, "Castle %s: skipped member %u with %s template '%s'\n", faction->camp.str(),
				static_cast<unsigned>(i), member.templateName.empty() ? "an empty" : "an unknown", member.templateName.c_str());
			if (INIAudit::isEnabled())
				INIAudit::record("error", "Invalid castle member template: " + member.templateName, faction->camp.str(), 0);
		}
		templates.push_back(definition);
	}

	if (std::none_of(templates.begin(), templates.end(), [](const ThingTemplate* type) { return type != nullptr; }))
		return;
	if (cost)
		player->getMoney()->withdraw(cost);
	if (player != packed->getControllingPlayer())
		packed->setTeam(player->getDefaultTeam());

	const Matrix3D transform = *packed->getTransformMatrix();
	const Real orientation = packed->getOrientation();
	m_members.reserve(members.size());
	TheAI->pathfinder()->removeObjectFromPathfindMap(packed);
	packed->setStatus(MAKE_OBJECT_STATUS_MASK3(OBJECT_STATUS_UNSELECTABLE, OBJECT_STATUS_NO_COLLISIONS, OBJECT_STATUS_MASKED));
	if (Drawable* drawable = packed->getDrawable())
		drawable->setDrawableHidden(TRUE);
	m_unpacked = TRUE;

	for (std::size_t i = 0; i < members.size(); ++i)
	{
		const ThingTemplate* definition = templates[i];
		if (!definition)
			continue;

		const auto& member = members[i];
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

	for (ObjectID id : m_members)
	{
		if (Object* piece = TheGameLogic->findObjectByID(id))
		{
			const Bool owned = data->m_ownedFilter.matchesTemplate(*piece->getTemplate());
			CastleMemberBehavior::linkToCastle(*piece, packed->getID(), m_keep, id == m_keep, owned);
		}
	}
	for (ObjectID id : m_members)
	{
		Object* piece = TheGameLogic->findObjectByID(id);
		if (piece && piece->isKindOf(KINDOF_STRUCTURE))
			piece->updateUpgradeModules();
	}

	registerPerimeters(*camp, transform);

	if (data->m_maxCastleRadius > 0.0f)
	{
		std::vector<ObjectID> tainted;
		SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(packed, data->m_maxCastleRadius, FROM_CENTER_2D);
		for (Object* object = iter->first(); object; object = iter->next())
		{
			if (object->isKindOf(KINDOF_TAINT))
				tainted.push_back(object->getID());
		}
		deleteInstance(iter);
		std::sort(tainted.begin(), tainted.end());
		for (ObjectID id : tainted)
		{
			if (Object* object = TheGameLogic->findObjectByID(id))
				TheGameLogic->destroyObject(object);
		}
	}

	if (Object* keep = getKeep())
	{
		if (data->m_transferFoundationHealth)
		{
			const BodyModuleInterface* oldBody = packed->getBodyModule();
			BodyModuleInterface* body = keep->getBodyModule();
			if (oldBody->getMaxHealth() > 0.0f)
				body->internalChangeHealth(body->getMaxHealth() * oldBody->getHealth() / oldBody->getMaxHealth() - body->getHealth());
		}
		if (packed->getName().isNotEmpty())
		{
			TheScriptEngine->transferObjectName(packed->getName(), keep);
			packed->setName(AsciiString::TheEmptyString);
		}
	}
	if (!charge && !data->m_preBuiltList.empty())
	{
		std::vector<FoundationAIUpdate*> plots;
		std::vector<FoundationAIUpdate*> defensePlots;
		for (ObjectID id : m_members)
		{
			const Object* member = TheGameLogic->findObjectByID(id);
			if (auto* plot = member ? FoundationAIUpdate::find(*member) : nullptr)
			{
				auto& list = member->isKindOf(KINDOF_BASE_DEFENSE_FOUNDATION) ? defensePlots : plots;
				list.push_back(plot);
			}
		}

		for (const auto& entry : data->m_preBuiltList)
		{
			const ThingTemplate* type = TheThingFactory->findTemplate(entry.name, FALSE);
			if (!type)
				continue;
			const auto& choices = type->isKindOf(KINDOF_FS_BASE_DEFENSE) && !defensePlots.empty() ? defensePlots : plots;
			if (choices.empty())
				continue;
			if (entry.plotIndex == -2)
			{
				for (auto* plot : choices)
					if (plot->construct(type, player, TRUE))
						break;
			}
			else
			{
				const auto index = std::min(static_cast<std::size_t>(entry.plotIndex), choices.size() - 1);
				choices[index]->construct(type, player, TRUE);
			}
		}
	}
}

void CastleBehavior::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
}

void CastleBehavior::xfer(Xfer* xfer)
{
	XferVersion version = 3;
	xfer->xferVersion(&version, 3);
	UpdateModule::xfer(xfer);
	xfer->xferBool(&m_buildComplete);
	xfer->xferBool(&m_pendingUnpack);
	xfer->xferBool(&m_unpacked);
	xfer->xferObjectID(&m_keep);
	if (version >= 2)
		xfer->xferBool(&m_keepLost);
	if (version >= 3)
	{
		xfer->xferBool(&m_orderedUnpack);
		xfer->xferUnsignedInt(&m_unpackFrame);
	}

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
		if (m_keep == INVALID_ID && friend_getNextCallFrame() == UPDATE_SLEEP_FOREVER)
			friend_setNextCallFrame(TheGameLogic->getFrame());
		if (Drawable* drawable = getObject()->getDrawable())
			drawable->setDrawableHidden(TRUE);
		restoreMissingPerimeters();
	}
}

Bool CastleBehavior::orderUnpack(Player* player)
{
	Object* packed = getObject();
	const auto* camp = player ? findCamp(*player) : nullptr;
	if (!camp || player != packed->getControllingPlayer() || m_pendingUnpack || m_unpacked ||
		!m_buildComplete || packed->isEffectivelyDead() || player->getMoney()->countMoney() < camp->cost)
		return FALSE;

	const Real seconds = getCastleBehaviorModuleData()->m_unpackDelayTime;
	const Real delay = std::isfinite(seconds) ? std::clamp(seconds, 0.0f, 300.0f) : 0.0f;
	m_orderedUnpack = TRUE;
	m_pendingUnpack = TRUE;
	m_unpackFrame = TheGameLogic->getFrame() + static_cast<UnsignedInt>(delay * LOGICFRAMES_PER_SECOND);
	setWakeFrame(packed, UPDATE_SLEEP_NONE);
	return TRUE;
}

void CastleBehavior::scanForCapture()
{
	Object* packed = getObject();
	Player* neutral = ThePlayerList->findPlayerWithNameKey(NAMEKEY("PlyrCivilian"));
	if (!neutral)
		neutral = ThePlayerList->getNeutralPlayer();

	Object* candidate = nullptr;
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(packed,
		getCastleBehaviorModuleData()->m_scanDistance, FROM_CENTER_2D);
	for (Object* unit = iter->first(); unit; unit = iter->next())
	{
		if (unit == packed || unit->isEffectivelyDead() || !unit->isMobile() || !unit->getAIUpdateInterface() ||
			unit->isKindOf(KINDOF_NO_BASE_CAPTURE) || unit->isKindOf(KINDOF_PROJECTILE) ||
			unit->getContainedBy() || unit->isSignificantlyAboveTerrain())
			continue;
		const Player* owner = unit->getControllingPlayer();
		if (owner && owner != neutral && owner->isPlayerActive() && findCamp(*owner) &&
			(!candidate || unit->getID() < candidate->getID()))
			candidate = unit;
	}
	deleteInstance(iter);

	Player* owner = candidate ? candidate->getControllingPlayer() : neutral;
	if (owner != packed->getControllingPlayer())
		packed->setTeam(owner->getDefaultTeam());
}
