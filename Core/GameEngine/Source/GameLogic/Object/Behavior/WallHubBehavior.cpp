#include "PreRTS.h"

#include "GameLogic/Module/WallHubBehavior.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

#include "Common/AudioEventRTS.h"
#include "Common/GameAudio.h"
#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/RetailSaveBlocks.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/HordeWorkerAIUpdate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/HeroRoster.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Module/FoundationAIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "WWMath/wwmath.h"

namespace
{
	constexpr UnsignedInt WALL_OPTIONS = OPTION_ONE | OPTION_TWO | OPTION_THREE;

	Real halfLength(const GeometryInfo& geometry, Real angle)
	{
		if (geometry.getGeomType() != GEOMETRY_BOX)
			return geometry.getMajorRadius();
		return WWMath::Fabs(WWMath::CosTrig(angle)) * geometry.getMajorRadius() +
			WWMath::Fabs(WWMath::SinTrig(angle)) * geometry.getMinorRadius();
	}

	const ThingTemplate* wallTemplate(const AsciiString& name)
	{
		const ThingTemplate* definition = name.isEmpty() ? nullptr : TheThingFactory->findTemplate(name, FALSE);
		const auto* result = definition ? static_cast<const ThingTemplate*>(definition->getFinalOverride()) : nullptr;
		return result && result->isKindOf(KINDOF_STRUCTURE) && !result->isKindOf(KINDOF_DRAWABLE_ONLY) ? result : nullptr;
	}
}

void WallHubBehaviorModuleData::parseOption(INI* ini, void*, void* store, const void*)
{
	const char* token = ini->getNextToken();
	static const char* const names[] = {"OPTION_ONE", "OPTION_TWO", "OPTION_THREE", nullptr};
	static constexpr UnsignedInt values[] = {OPTION_ONE, OPTION_TWO, OPTION_THREE};
	const Int option = CommandButtonRules::nameIndex(names, token);
	if (option < 0)
		throw INI_INVALID_NAME_LIST;
	*static_cast<UnsignedInt*>(store) = values[option];
}

void WallHubBehaviorModuleData::parseSegment(INI* ini, void*, void* store, const void*)
{
	static_cast<std::vector<AsciiString>*>(store)->push_back(ini->getNextAsciiString());
}

void WallHubBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "Options", parseOption, nullptr, offsetof(WallHubBehaviorModuleData, m_option) },
		{ "MaxBuildoutDistance", INI::parseReal, nullptr, offsetof(WallHubBehaviorModuleData, m_maxDistance) },
		{ "StaggeredBuildFactor", INI::parseInt, nullptr, offsetof(WallHubBehaviorModuleData, m_staggerFactor) },
		{ "SegmentTemplateName", parseSegment, nullptr, offsetof(WallHubBehaviorModuleData, m_segments) },
		{ "DefaultSegmentTemplateName", INI::parseAsciiString, nullptr, offsetof(WallHubBehaviorModuleData, m_defaultSegment) },
		{ "HubCapTemplateName", INI::parseAsciiString, nullptr, offsetof(WallHubBehaviorModuleData, m_hubCap) },
		{ "CliffCapTemplateName", INI::parseAsciiString, nullptr, offsetof(WallHubBehaviorModuleData, m_cliffCap) },
		{ "ShoreCapTemplateName", INI::parseAsciiString, nullptr, offsetof(WallHubBehaviorModuleData, m_shoreCap) },
		{ "BorderCapTemplateName", INI::parseAsciiString, nullptr, offsetof(WallHubBehaviorModuleData, m_borderCap) },
		{ "ElevatedSegmentTemplateName", INI::parseAsciiString, nullptr, offsetof(WallHubBehaviorModuleData, m_elevatedSegment) },
		{ "BuilderRadius", INI::parseReal, nullptr, offsetof(WallHubBehaviorModuleData, m_builderRadius) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields, 0);
}

WallHubBehavior::WallHubBehavior(Thing* thing, const ModuleData* data) : UpdateModule(thing, data)
{
	setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
}

WallHubBehavior::~WallHubBehavior() = default;

WallHubBehavior* WallHubBehavior::find(const Object* object, UnsignedInt options)
{
	options &= WALL_OPTIONS;
	if (!object || !options || !RetailFileLayers::enabled())
		return nullptr;

	const NameKeyType key = NAMEKEY("WallHubBehavior");
	for (BehaviorModule* const* module = object->getBehaviorModules(); *module; ++module)
	{
		if ((*module)->getModuleNameKey() == key)
		{
			auto* hub = static_cast<WallHubBehavior*>(*module);
			if (hub->getWallHubBehaviorModuleData()->m_option == options)
				return hub;
		}
	}
	return nullptr;
}

WallHubBehavior::Span WallHubBehavior::plan(const ThingTemplate* requested, const Coord3D& target) const
{
	return plan(requested, target, BuildAssistant::IGNORE_HIDDEN_OBJECTS | BuildAssistant::BLOCK_REMEMBERED_OBJECTS);
}

WallHubBehavior::Span WallHubBehavior::plan(const ThingTemplate* requested, const Coord3D& target,
	UnsignedInt overlapChecks) const
{
	Span span;
	const Object* hub = getObject();
	Player* player = hub->getControllingPlayer();
	const auto* data = getWallHubBehaviorModuleData();
	const auto reject = [&span](LegalBuildCode code)
	{
		if (span.legal == LBC_OK)
			span.legal = code;
	};

	if (!requested || !player || hub->isEffectivelyDead() || hub->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) ||
		!std::isfinite(target.x) || !std::isfinite(target.y))
	{
		span.legal = LBC_RESTRICTED_TERRAIN;
		return span;
	}

	const ThingTemplate* cap = wallTemplate(data->m_hubCap);
	if (!cap)
		cap = static_cast<const ThingTemplate*>(requested->getFinalOverride());
	const ThingTemplate* filler = wallTemplate(data->m_defaultSegment);
	std::vector<const ThingTemplate*> pattern;
	for (const AsciiString& name : data->m_segments)
	{
		const ThingTemplate* tile = wallTemplate(name);
		if (tile || filler)
			pattern.push_back(tile ? tile : filler);
	}
	if (pattern.empty() && filler)
		pattern.push_back(filler);
	if (pattern.empty() || !cap->isKindOf(KINDOF_STRUCTURE))
	{
		span.legal = LBC_RESTRICTED_TERRAIN;
		return span;
	}

	const Coord3D origin = *hub->getPosition();
	const Real snapRange = cap->getTemplateGeometryInfo().getBoundingCircleRadius();
	const Real requestedDx = target.x - origin.x;
	const Real requestedDy = target.y - origin.y;
	const Real requestedDistance = WWMath::SqrtOrigin(requestedDx * requestedDx + requestedDy * requestedDy);
	if (!(requestedDistance <= data->m_maxDistance + snapRange))
	{
		span.legal = LBC_RESTRICTED_TERRAIN;
		return span;
	}

	Coord3D endpoint = target;
	const Object* endHub = nullptr;
	Real bestDistanceSquared = snapRange * snapRange;
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(&target, snapRange, FROM_CENTER_2D,
		nullptr, ITER_FASTEST);
	for (Object* object = iter->first(); object; object = iter->next())
	{
		if (object != hub && object->getControllingPlayer() == player && object->isKindOf(KINDOF_WALL_HUB) &&
			!object->isEffectivelyDead() && !object->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
		{
			const Coord3D* position = object->getPosition();
			const Real dx = position->x - target.x;
			const Real dy = position->y - target.y;
			const Real distanceSquared = dx * dx + dy * dy;
			if (distanceSquared < bestDistanceSquared ||
				(distanceSquared == bestDistanceSquared && (!endHub || object->getID() < endHub->getID())))
			{
				bestDistanceSquared = distanceSquared;
				endHub = object;
				endpoint = *position;
			}
		}
	}
	deleteInstance(iter);

	const Real dx = endpoint.x - origin.x;
	const Real dy = endpoint.y - origin.y;
	const Real distance = WWMath::SqrtOrigin(dx * dx + dy * dy);
	const Real limit = data->m_maxDistance;
	if (!(distance > 0.0f && distance <= limit))
	{
		span.legal = LBC_RESTRICTED_TERRAIN;
		return span;
	}

	const Real angle = WWMath::Atan2Origin(dy, dx);
	const Real wallAngle = normalizeAngle(angle - static_cast<Real>(PI) * 0.5f);
	const Real endRadius = endHub ? halfLength(endHub->getGeometryInfo(), angle - endHub->getOrientation()) :
		cap->getTemplateGeometryInfo().getMinorRadius();
	Real seam = data->m_builderRadius > 0.0f ? data->m_builderRadius :
		halfLength(hub->getGeometryInfo(), angle - hub->getOrientation());

	const Int maxPieces = std::max(0, TheGlobalData->m_maxLineBuildObjects);
	const auto append = [&](const ThingTemplate* tile, Real offset)
	{
		const Coord3D pos = {origin.x + dx * offset / distance, origin.y + dy * offset / distance, 0.0f};
		Coord3D ground = pos;
		ground.z = TheTerrainLogic->getGroundHeight(pos.x, pos.y);
		const UnsignedInt checks = BuildAssistant::TERRAIN_RESTRICTIONS | BuildAssistant::NO_OBJECT_OVERLAP | overlapChecks;
		const LegalBuildCode legal = TheBuildAssistant->isLocationLegalToBuild(&ground, tile, wallAngle,
			checks, hub, player, hub);
		reject(legal);
		const Int cost = std::max(0, tile->calcCostToBuild(player));
		span.pieces.push_back({tile, ground, wallAngle, cost});
		span.cost += std::min(static_cast<UnsignedInt>(cost), std::numeric_limits<UnsignedInt>::max() - span.cost);
	};

	const Int maxSegments = maxPieces - (endHub == nullptr);
	for (Int count = 0; count < maxSegments; ++count)
	{
		const ThingTemplate* tile = pattern[static_cast<std::size_t>(count) % pattern.size()];
		const Real radius = tile->getTemplateGeometryInfo().getMinorRadius();
		if (!(radius > 0.0f) || seam + radius + endRadius >= distance)
			break;
		const Real nextSeam = seam + 2.0f * radius;
		const Bool reachesExistingHub = endHub && nextSeam + endRadius >= distance;
		append(tile, reachesExistingHub ? distance - endRadius - radius : seam + radius);
		seam = nextSeam;
		if (reachesExistingHub)
			break;
	}

	if (span.pieces.empty() || (endHub && seam + endRadius < distance))
		reject(LBC_RESTRICTED_TERRAIN);
	else if (!endHub)
		append(cap, seam + endRadius);

	span.affordable = span.cost <= player->getMoney()->countMoney();
	return span;
}

Bool WallHubBehavior::construct(const ThingTemplate* requested, const Coord3D& target, Player* player) const
{
	Object* hub = const_cast<Object*>(getObject());
	if (requested)
		requested = static_cast<const ThingTemplate*>(requested->getFinalOverride());
	if (!player || hub->getControllingPlayer() != player || hub->isDisabled() || !TheRetailCommandCatalog ||
		!requested || !player->canBuild(requested))
		return FALSE;

	const CommandSet* commands = TheRetailCommandCatalog->findCommandSet(hub->getCommandSetString());
	const CommandButton* spanCommand = nullptr;
	if (commands)
	{
		for (Int slot = 0; slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
		{
			const CommandButton* button = commands->getCommandButton(slot);
			if (button && button->getCommandType() == GUI_COMMAND_DOZER_CONSTRUCT && button->getThingTemplate() &&
				button->getThingTemplate()->isEquivalentTo(requested) &&
				(button->getOptions() & WALL_OPTIONS) == getWallHubBehaviorModuleData()->m_option &&
				button->getRequiredRank() <= hub->getExperienceTracker()->getRank() && HeroRoster::buttonUpgradesMet(button, player, hub))
			{
				spanCommand = button;
				break;
			}
		}
	}
	if (!spanCommand)
		return FALSE;

	const Span span = plan(requested, target, BuildAssistant::SUPPRESS_BUILD_BIBS);
	if (span.pieces.empty() || span.legal != LBC_OK || !span.affordable)
		return FALSE;

	UnsignedInt paid = player->getMoney()->withdraw(span.cost);
	const Int stagger = std::clamp(getWallHubBehaviorModuleData()->m_staggerFactor, 0, 300);
	ObjectID spanID = INVALID_ID;
	for (std::size_t i = 0; i < span.pieces.size(); ++i)
	{
		const Piece& piece = span.pieces[i];
		const UnsignedInt piecePaid = std::min(paid, static_cast<UnsignedInt>(piece.cost));
		paid -= piecePaid;
		const Object* structure = GettingBuiltBehavior::start(*piece.type, *hub, piece.position, piece.facing,
			static_cast<UnsignedInt>(stagger * i), static_cast<Int>(piecePaid), spanID);
		if (structure && spanID == INVALID_ID)
			spanID = structure->getID();
	}
	if (TheControlBar)
		TheControlBar->markUIDirty();
	return TRUE;
}

namespace
{
ObjectID placedSpan(const Object& hub)
{
	const auto* source = GettingBuiltBehavior::find(hub);
	return source ? source->getSpan() : INVALID_ID;
}

const GettingBuiltBehavior* spanPiece(const Object& piece, const Object& hub, ObjectID sourceSpan)
{
	if (piece.getControllingPlayer() != hub.getControllingPlayer() || piece.isDestroyed() || piece.isEffectivelyDead())
		return nullptr;
	const Bool builtFromHub = piece.getBuilderID() == hub.getID();
	if (!builtFromHub && sourceSpan == INVALID_ID)
		return nullptr;
	const auto* behavior = GettingBuiltBehavior::find(piece);
	return behavior && (builtFromHub || sourceSpan == behavior->getSpan()) ? behavior : nullptr;
}
}

Bool WallHubBehavior::canCancelSpan(const Object* hub)
{
	if (!RetailFileLayers::enabled() || !hub || hub->isDestroyed() || hub->isEffectivelyDead() ||
		!hub->isKindOf(KINDOF_WALL_HUB))
		return FALSE;
	const ObjectID sourceSpan = placedSpan(*hub);
	for (const Object* piece = TheGameLogic->getFirstObject(); piece; piece = piece->getNextObject())
		if (piece->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) && spanPiece(*piece, *hub, sourceSpan))
			return TRUE;
	return FALSE;
}

Bool WallHubBehavior::cancelSpan(const Object* hub, Player* player)
{
	if (!player || !hub || hub->getControllingPlayer() != player || !canCancelSpan(hub) || !TheRetailCommandCatalog)
		return FALSE;

	const CommandSet* commands = TheRetailCommandCatalog->findCommandSet(hub->getCommandSetString());
	if (!commands)
		return FALSE;
	Bool offered = FALSE;
	for (Int slot = 0; slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
	{
		const CommandButton* command = commands->getCommandButton(slot);
		if (command && command->getCommandType() == GUI_COMMAND_CANCEL_NEIGHBORHOOD)
		{
			offered = TRUE;
			break;
		}
	}
	if (!offered)
		return FALSE;

	const ObjectID sourceSpan = placedSpan(*hub);
	std::set<ObjectID> activeSpans;
	for (const Object* piece = TheGameLogic->getFirstObject(); piece; piece = piece->getNextObject())
		if (const auto* behavior = spanPiece(*piece, *hub, sourceSpan); behavior && piece->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
			activeSpans.insert(behavior->getSpan());
	std::vector<Object*> pieces;
	for (Object* piece = TheGameLogic->getFirstObject(); piece; piece = piece->getNextObject())
		if (const auto* behavior = spanPiece(*piece, *hub, sourceSpan); behavior && activeSpans.contains(behavior->getSpan()) &&
			(behavior->getSpan() != INVALID_ID || piece->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION)))
			pieces.push_back(piece);
	std::sort(pieces.begin(), pieces.end(), [](const Object* a, const Object* b) { return a->getID() < b->getID(); });
	for (Object* piece : pieces)
	{
		player->getMoney()->deposit(GettingBuiltBehavior::constructionRefund(*piece, *player), TRUE, FALSE);
		if (piece->getConstructionPercent() > 7.5f)
			piece->kill();
		else
			TheGameLogic->destroyObject(piece);
	}
	if (TheControlBar)
		TheControlBar->markUIDirty();
	return TRUE;
}

void WallHubBehavior::crc(Xfer* xfer) { UpdateModule::crc(xfer); }

void WallHubBehavior::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
}

void WallHubBehavior::loadPostProcess() { UpdateModule::loadPostProcess(); }

void GettingBuiltBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "WorkerName", INI::parseAsciiString, nullptr, offsetof(GettingBuiltBehaviorModuleData, m_worker) },
		{ "EvilWorkerName", INI::parseAsciiString, nullptr, offsetof(GettingBuiltBehaviorModuleData, m_evilWorker) },
		{ "SelfBuildingLoop", INI::parseAsciiString, nullptr, offsetof(GettingBuiltBehaviorModuleData, m_buildLoop) },
		{ "SelfRepairFromDamageLoop", INI::parseAsciiString, nullptr, offsetof(GettingBuiltBehaviorModuleData, m_repairLoop) },
		{ "SelfRepairFromRubbleLoop", INI::parseAsciiString, nullptr, offsetof(GettingBuiltBehaviorModuleData, m_rebuildLoop) },
		{ "SpawnTimer", INI::parseReal, nullptr, offsetof(GettingBuiltBehaviorModuleData, m_spawnTimer) },
		{ "RebuildTimeSeconds", INI::parseReal, nullptr, offsetof(GettingBuiltBehaviorModuleData, m_rebuildTime) },
		{ "UseSpawnTimerWithoutWorker", INI::parseBool, nullptr, offsetof(GettingBuiltBehaviorModuleData, m_timerWithoutWorker) },
		{ "RebuildWhenDead", INI::parseBool, nullptr, offsetof(GettingBuiltBehaviorModuleData, m_rebuildWhenDead) },
		{ "TestFaction", INI::parseBool, nullptr, offsetof(GettingBuiltBehaviorModuleData, m_testFaction) },
		{ "DisallowRebuildRange", INI::parseReal, nullptr, offsetof(GettingBuiltBehaviorModuleData, m_disallowRebuildRange) },
		{ "DisallowRebuildFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(GettingBuiltBehaviorModuleData, m_disallowRebuildFilter) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields, 0);
}

GettingBuiltBehavior::GettingBuiltBehavior(Thing* thing, const ModuleData* data) : UpdateModule(thing, data)
{
	setWakeFrame(getObject(), UPDATE_SLEEP_FOREVER);
}

GettingBuiltBehavior::~GettingBuiltBehavior() = default;

Object* GettingBuiltBehavior::start(const ThingTemplate& type, Object& builder, const Coord3D& position,
	Real facing, UnsignedInt delay, Int cost, ObjectID span)
{
	Player* owner = builder.getControllingPlayer();
	Object* structure = TheThingFactory->newObjectFromExactTemplate(&type, builder.getTeam(),
		MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNDER_CONSTRUCTION));
	structure->setProducer(&builder);
	structure->setBuilder(&builder);
	structure->setOrientation(facing);
	structure->setPosition(&position);
	TheAI->pathfinder()->addObjectToPathfindMap(structure);
	owner->onStructureCreated(&builder, structure);
	structure->setConstructionPercent(0.0f);
	if (auto* behavior = static_cast<GettingBuiltBehavior*>(structure->findUpdateModule(NAMEKEY("GettingBuiltBehavior"))))
	{
		behavior->m_startFrame = TheGameLogic->getFrame() + delay;
		behavior->m_running = TRUE;
		behavior->m_paidCost = cost;
		behavior->m_span = span == INVALID_ID ? structure->getID() : span;
		BodyModuleInterface* body = structure->getBodyModule();
		body->internalChangeHealth(1.0f - body->getHealth());
		structure->setModelConditionState(MODELCONDITION_AWAITING_CONSTRUCTION);
		behavior->setWakeFrame(structure, UPDATE_SLEEP_NONE);
	}
	else
	{
		structure->setConstructionPercent(100.0f);
		FoundationAIUpdate::advanceConstruction(*structure, &builder);
	}
	return structure;
}

const GettingBuiltBehavior* GettingBuiltBehavior::find(const Object& structure)
{
	return static_cast<const GettingBuiltBehavior*>(structure.findUpdateModule(NAMEKEY("GettingBuiltBehavior")));
}

GettingBuiltBehavior* GettingBuiltBehavior::findMutable(Object& structure)
{
	return static_cast<GettingBuiltBehavior*>(structure.findUpdateModule(NAMEKEY("GettingBuiltBehavior")));
}

UnsignedInt GettingBuiltBehavior::getRebuildFrames() const
{
	const Real seconds = getGettingBuiltBehaviorModuleData()->m_rebuildTime;
	if (!std::isfinite(seconds) || seconds <= 0.0f)
		return 0;
	return std::max(1u, static_cast<UnsignedInt>(std::min(static_cast<double>(seconds) * LOGICFRAMES_PER_SECOND,
		static_cast<double>(UPDATE_SLEEP_FOREVER))));
}

UnsignedInt GettingBuiltBehavior::getSpawnDelayFrames() const
{
	const double frames = static_cast<double>(getGettingBuiltBehaviorModuleData()->m_spawnTimer) * LOGICFRAMES_PER_SECOND;
	if (!(frames >= 0.0 && frames < UPDATE_SLEEP_FOREVER))
		return UPDATE_SLEEP_FOREVER;
	return static_cast<UnsignedInt>(frames);
}

Bool GettingBuiltBehavior::canRebuildNow() const
{
	const auto* data = getGettingBuiltBehaviorModuleData();
	if (!(data->m_disallowRebuildRange > 0.0f))
		return TRUE;
	const Object* self = getObject();
	const auto& filter = data->m_disallowRebuildFilter;
	const Bool hasFilter = filter.m_rules != 0 || filter.m_includeKinds.any() || filter.m_excludeKinds.any() ||
		!filter.m_includeObjects.empty() || !filter.m_excludeObjects.empty();
	if (!hasFilter)
		return TRUE;
	SimpleObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(self->getPosition(),
		data->m_disallowRebuildRange, FROM_CENTER_2D);
	MemoryPoolObjectHolder holder(iter);
	for (Object* other = iter->first(); other; other = iter->next())
	{
		if (other == self || other->isDestroyed() || other->isEffectivelyDead())
			continue;
		if (filter.matchesObject(self, other))
			return FALSE;
	}
	return TRUE;
}

void GettingBuiltBehavior::onRubbleEntered(Object& structure)
{
	if (!RetailFileLayers::enabled())
		return;
	GettingBuiltBehavior* behavior = findMutable(structure);
	if (!behavior)
		return;
	behavior->m_keptAsRubble = TRUE;
	behavior->m_running = FALSE;
	behavior->m_rebuildingFromRubble = FALSE;
	behavior->m_workerSpawned = FALSE;
	behavior->m_repairFrame = 0;
	behavior->releaseBuilder();
	behavior->stopLoop();
	const auto* data = behavior->getGettingBuiltBehaviorModuleData();
	const UnsignedInt delay = behavior->getSpawnDelayFrames();
	if (!data->m_rebuildWhenDead || delay == UPDATE_SLEEP_FOREVER)
		return;
	behavior->m_waitingToRebuild = TRUE;
	behavior->m_rebuildReadyFrame = TheGameLogic->getFrame() + delay;
	behavior->setWakeFrame(&structure, UPDATE_SLEEP(delay));
}

Bool GettingBuiltBehavior::canOrderRebuild(const Object* structure, const Player* owner)
{
	if (!RetailFileLayers::enabled() || !structure || !owner || structure->getControllingPlayer() != owner ||
		structure->isDestroyed() || !structure->isEffectivelyDead() ||
		!structure->getTemplate()->getRotWKObjectData().keepSelectableWhenDead)
		return FALSE;
	const auto* behavior = find(*structure);
	return behavior && behavior->m_keptAsRubble && behavior->getRebuildFrames() > 0;
}

Bool GettingBuiltBehavior::startRebuild(Object& structure)
{
	if (!RetailFileLayers::enabled() || structure.isDestroyed() || !structure.isEffectivelyDead())
		return FALSE;
	GettingBuiltBehavior* behavior = findMutable(structure);
	if (!behavior || !behavior->m_keptAsRubble || !behavior->canRebuildNow())
		return FALSE;

	structure.setEffectivelyDead(FALSE);
	structure.setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNDER_CONSTRUCTION));
	structure.clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNSELECTABLE));
	if (!structure.isKindOf(KINDOF_NO_COLLIDE))
		structure.clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_NO_COLLISIONS));
	structure.clearAndSetModelConditionFlags(
		MAKE_MODELCONDITION_MASK4(MODELCONDITION_RUBBLE, MODELCONDITION_COLLAPSING,
			MODELCONDITION_POST_RUBBLE, MODELCONDITION_POST_COLLAPSE),
		MAKE_MODELCONDITION_MASK2(MODELCONDITION_PARTIALLY_CONSTRUCTED, MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED));
	structure.clearModelConditionState(MODELCONDITION_DYING);
	structure.setConstructionPercent(0.0f);
	if (BodyModuleInterface* body = structure.getBodyModule())
		body->internalChangeHealth(1.0f - body->getHealth());
	TheAI->pathfinder()->addObjectToPathfindMap(&structure);

	behavior->m_startFrame = TheGameLogic->getFrame();
	behavior->m_running = TRUE;
	behavior->m_rebuildingFromRubble = TRUE;
	behavior->m_waitingToRebuild = FALSE;
	behavior->m_keptAsRubble = FALSE;
	behavior->m_rebuildReadyFrame = 0;
	behavior->setWakeFrame(&structure, UPDATE_SLEEP_NONE);
	return TRUE;
}

UnsignedInt GettingBuiltBehavior::constructionRefund(const Object& structure, const Player& player)
{
	const Int paid = structure.getConstructionCostPaid();
	if (paid >= 0)
		return static_cast<UnsignedInt>(paid);
	const auto* behavior = find(structure);
	const Int cost = behavior && behavior->m_paidCost >= 0 ? behavior->m_paidCost : structure.getTemplate()->calcCostToBuild(&player);
	return static_cast<UnsignedInt>(std::max(0, cost));
}

Bool GettingBuiltBehavior::beginConstruction(Object& structure)
{
	if (!RetailFileLayers::enabled())
		return FALSE;
	auto* behavior = findMutable(structure);
	if (!behavior)
		return FALSE;
	behavior->m_startFrame = TheGameLogic->getFrame();
	behavior->m_running = TRUE;
	behavior->setWakeFrame(&structure, UPDATE_SLEEP_NONE);
	return TRUE;
}

Bool GettingBuiltBehavior::takeOverConstruction(Object& structure)
{
	if (!RetailFileLayers::enabled())
		return FALSE;
	auto* behavior = findMutable(structure);
	if (!behavior || behavior->m_workerSpawned || behavior->m_running)
		return FALSE;
	behavior->m_workerSpawned = TRUE;
	if (!behavior->spawnBuilder(FALSE) && !structure.testStatus(OBJECT_STATUS_RECONSTRUCTING))
		return FALSE;
	beginConstruction(structure);
	return TRUE;
}

Bool GettingBuiltBehavior::hasDelegatedConstruction(ObjectID builder)
{
	if (!RetailFileLayers::enabled())
		return FALSE;
	for (const Object* site = TheGameLogic->getFirstObject(); site; site = site->getNextObject())
	{
		if (site->getProducerID() == builder && site->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) &&
			!site->isEffectivelyDead())
		{
			const auto* behavior = find(*site);
			if (behavior && behavior->m_running)
				return TRUE;
		}
	}
	return FALSE;
}

Bool GettingBuiltBehavior::isAuthoredWorker(const Object& structure, const Object& worker)
{
	if (!RetailFileLayers::enabled())
		return FALSE;
	const auto* behavior = find(structure);
	return behavior && behavior->m_workerID == worker.getID();
}

Int GettingBuiltBehavior::getConstructionFrames(const Object& structure, Int fallback)
{
	if (!RetailFileLayers::enabled() || !structure.testStatus(OBJECT_STATUS_RECONSTRUCTING))
		return fallback;
	const auto* behavior = find(structure);
	const UnsignedInt frames = behavior ? behavior->getRebuildFrames() : 0;
	return frames > 0 ? static_cast<Int>(frames) : fallback;
}

void GettingBuiltBehavior::onObjectCreated()
{
	if (!RetailFileLayers::enabled())
		return;
	const auto* data = getGettingBuiltBehaviorModuleData();
	if (getSpawnDelayFrames() != UPDATE_SLEEP_FOREVER && (data->m_timerWithoutWorker || data->m_worker.isNotEmpty()))
		setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
}

Object* GettingBuiltBehavior::spawnBuilder(Bool repair)
{
	if (!RetailFileLayers::enabled())
		return nullptr;
	Object* structure = getObject();
	const AsciiString& workerName = getGettingBuiltBehaviorModuleData()->m_worker;
	if (workerName.isEmpty())
		return nullptr;
	const ThingTemplate* workerType = TheThingFactory->findTemplate(workerName, FALSE);
	if (!workerType || !workerType->isKindOf(KINDOF_DOZER) || workerType->isKindOf(KINDOF_STRUCTURE))
		return nullptr;
	Player* owner = structure->getControllingPlayer();
	if (!owner)
		return nullptr;
	Object* worker = TheThingFactory->newObject(workerType, owner->getDefaultTeam());
	if (!worker)
		return nullptr;
	auto* ai = worker->getAIUpdateInterface();
	auto* dozer = ai ? ai->getDozerAIInterface() : nullptr;
	if (!dozer)
	{
		TheGameLogic->destroyObject(worker);
		return nullptr;
	}
	worker->setOrientation(structure->getOrientation());
	worker->setPosition(structure->getPosition());
	m_workerID = worker->getID();
	dozer->newTask(repair ? DOZER_TASK_REPAIR : DOZER_TASK_BUILD, structure);
	return worker;
}

void GettingBuiltBehavior::releaseBuilder()
{
	if (m_workerID == INVALID_ID)
		return;
	Object* worker = TheGameLogic->findObjectByID(m_workerID);
	m_workerID = INVALID_ID;
	if (worker)
		TheGameLogic->destroyObject(worker);
}

void GettingBuiltBehavior::onDelete()
{
	if (RetailFileLayers::enabled())
		releaseBuilder();
	stopLoop();
}

void GettingBuiltBehavior::playLoop(const AsciiString& name)
{
	const AsciiString* event = name.isEmpty() ? nullptr : &name;
	if (m_currentLoop == event)
		return;
	stopLoop();
	m_currentLoop = event;
	if (!TheAudio || !event)
		return;
	AudioEventRTS loop(name);
	loop.setObjectID(getObject()->getID());
	m_loopHandle = TheAudio->addAudioEvent(&loop);
}

void GettingBuiltBehavior::stopLoop()
{
	m_currentLoop = nullptr;
	if (m_loopHandle == 0)
		return;
	if (TheAudio)
		TheAudio->removeAudioEvent(m_loopHandle);
	m_loopHandle = 0;
}

UpdateSleepTime GettingBuiltBehavior::update()
{
	Object* structure = getObject();
	if (!RetailFileLayers::enabled())
		return UPDATE_SLEEP_FOREVER;

	const UnsignedInt frame = TheGameLogic->getFrame();
	if (m_waitingToRebuild && structure->isEffectivelyDead())
	{
		if (frame < m_rebuildReadyFrame)
			return UPDATE_SLEEP(m_rebuildReadyFrame - frame);
		if (!startRebuild(*structure))
			return UPDATE_SLEEP(LOGICFRAMES_PER_SECOND);
		return UPDATE_SLEEP_NONE;
	}
	if (structure->isEffectivelyDead())
	{
		releaseBuilder();
		stopLoop();
		return UPDATE_SLEEP_FOREVER;
	}

	if (!structure->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
	{
		if (m_running)
		{
			m_running = FALSE;
			m_rebuildingFromRubble = FALSE;
			m_workerSpawned = FALSE;
			releaseBuilder();
			stopLoop();
		}
		return updateRepair();
	}
	const auto* data = getGettingBuiltBehaviorModuleData();
	if (!m_running)
		return getSpawnDelayFrames() != UPDATE_SLEEP_FOREVER && (data->m_timerWithoutWorker || data->m_worker.isNotEmpty())
			? UPDATE_SLEEP_NONE : UPDATE_SLEEP_FOREVER;

	if (frame < m_startFrame)
		return UPDATE_SLEEP(m_startFrame - frame);

	if (!m_workerSpawned && !m_rebuildingFromRubble)
	{
		m_workerSpawned = TRUE;
		spawnBuilder(FALSE);
	}

	if (m_workerID != INVALID_ID)
	{
		Object* worker = TheGameLogic->findObjectByID(m_workerID);
		auto* ai = worker ? worker->getAIUpdateInterface() : nullptr;
		auto* dozer = ai ? ai->getDozerAIInterface() : nullptr;
		if (dozer && !worker->isEffectivelyDead() && worker->getControllingPlayer() == structure->getControllingPlayer() &&
			dozer->getTaskTarget(DOZER_TASK_BUILD) == structure->getID())
			return UPDATE_SLEEP_NONE;
		releaseBuilder();
	}

	playLoop(m_rebuildingFromRubble || structure->testStatus(OBJECT_STATUS_RECONSTRUCTING)
		? data->m_rebuildLoop : data->m_buildLoop);
	structure->clearAndSetModelConditionFlags(
		MAKE_MODELCONDITION_MASK(MODELCONDITION_AWAITING_CONSTRUCTION),
		MAKE_MODELCONDITION_MASK2(MODELCONDITION_PARTIALLY_CONSTRUCTED, MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED));
	Object* builder = TheGameLogic->findObjectByID(structure->getBuilderID());
	const Int overrideFrames = m_rebuildingFromRubble ? static_cast<Int>(getRebuildFrames()) : 0;
	FoundationAIUpdate::advanceConstruction(*structure, builder, overrideFrames, m_rebuildingFromRubble);
	if (!structure->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
	{
		m_running = FALSE;
		m_rebuildingFromRubble = FALSE;
		m_workerSpawned = FALSE;
		stopLoop();
		return updateRepair();
	}
	return UPDATE_SLEEP_NONE;
}

UpdateSleepTime GettingBuiltBehavior::updateRepair()
{
	Object* structure = getObject();
	const auto* data = getGettingBuiltBehaviorModuleData();
	const UnsignedInt delay = getSpawnDelayFrames();
	if (delay == UPDATE_SLEEP_FOREVER || (!data->m_timerWithoutWorker && data->m_worker.isEmpty()))
		return UPDATE_SLEEP_FOREVER;

	BodyModuleInterface* body = structure->getBodyModule();
	if (body->getHealth() >= body->getMaxHealth())
	{
		m_repairFrame = 0;
		releaseBuilder();
		stopLoop();
		return UPDATE_SLEEP_NONE;
	}

	const UnsignedInt frame = TheGameLogic->getFrame();
	const UnsignedInt* damageFrame = body->getLastDamageTimestamp();
	if (damageFrame)
		m_repairFrame = *damageFrame + delay;
	else if (!m_repairFrame)
		m_repairFrame = frame + delay;
	if (frame < m_repairFrame)
	{
		releaseBuilder();
		stopLoop();
		return UPDATE_SLEEP(m_repairFrame - frame);
	}

	if (m_workerID != INVALID_ID)
	{
		Object* worker = TheGameLogic->findObjectByID(m_workerID);
		auto* ai = worker ? worker->getAIUpdateInterface() : nullptr;
		auto* dozer = ai ? ai->getDozerAIInterface() : nullptr;
		if (dozer && !worker->isEffectivelyDead() && worker->getControllingPlayer() == structure->getControllingPlayer() &&
			dozer->getTaskTarget(DOZER_TASK_REPAIR) == structure->getID())
			return UPDATE_SLEEP_NONE;
		releaseBuilder();
	}
	if (!data->m_worker.isEmpty() && spawnBuilder(TRUE))
		return UPDATE_SLEEP_NONE;
	if (data->m_timerWithoutWorker)
	{
		playLoop(data->m_repairLoop);
		const Real duration = data->m_rebuildTime * LOGICFRAMES_PER_SECOND;
		body->internalChangeHealth(body->getMaxHealth() / std::max(1.0f, duration));
	}
	return UPDATE_SLEEP_NONE;
}

void GettingBuiltBehavior::xferRetailBlocks(Xfer* xfer, Bool present)
{
	RetailSaveBlocks::transferSection(xfer, xfer->getXferMode() == XFER_LOAD, present,
	{
		{ "rebuild-clock", [this](Xfer* x)
		{
			x->xferBool(&m_rebuildingFromRubble);
			x->xferBool(&m_waitingToRebuild);
			x->xferUnsignedInt(&m_rebuildReadyFrame);
		}, [this]
		{
			m_rebuildingFromRubble = FALSE;
			m_waitingToRebuild = FALSE;
			m_rebuildReadyFrame = 0;
		} },
		{ "kept-rubble", [this](Xfer* x) { x->xferBool(&m_keptAsRubble); },
			[this] { m_keptAsRubble = m_waitingToRebuild; } },
		{ "construction-worker", [this](Xfer* x)
		{
			x->xferObjectID(&m_workerID);
			x->xferBool(&m_workerSpawned);
		}, [this]
		{
			m_workerID = INVALID_ID;
			m_workerSpawned = FALSE;
		} },
		{ "repair-timer", [this](Xfer* x) { x->xferUnsignedInt(&m_repairFrame); },
			[this] { m_repairFrame = 0; } }
	});
}

void GettingBuiltBehavior::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	xfer->xferUnsignedInt(&m_startFrame);
	xfer->xferBool(&m_running);
	xfer->xferInt(&m_paidCost);
	xfer->xferObjectID(&m_span);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, TRUE);
}

void GettingBuiltBehavior::xfer(Xfer* xfer)
{
	XferVersion version = 4;
	xfer->xferVersion(&version, 4);
	UpdateModule::xfer(xfer);
	xfer->xferUnsignedInt(&m_startFrame);
	xfer->xferBool(&m_running);
	if (version >= 2)
		xfer->xferInt(&m_paidCost);
	if (version >= 3)
		xfer->xferObjectID(&m_span);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, version >= 4);
}

void GettingBuiltBehavior::loadPostProcess() { UpdateModule::loadPostProcess(); }

void WallUpgradeUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
}

WallUpgradeUpdate::WallUpgradeUpdate(Thing* thing, const ModuleData* data)
	: UpdateModule(thing, data)
{
}

WallUpgradeUpdate::~WallUpgradeUpdate() = default;

WallUpgradeUpdate* WallUpgradeUpdate::find(const Object* object)
{
	if (!object || !RetailFileLayers::enabled() || !object->areModulesReady())
		return nullptr;
	return static_cast<WallUpgradeUpdate*>(object->findUpdateModule(NAMEKEY("WallUpgradeUpdate")));
}

UpdateSleepTime WallUpgradeUpdate::update()
{
	if (!RetailFileLayers::enabled() || getObject()->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;

	Object* self = getObject();
	if (m_supportingWall == INVALID_ID)
	{
		const Object* closest = nullptr;
		const Real supportRadius = 200.0f;
		Real closestDistance = supportRadius * supportRadius;
		for (const Object* candidate = TheGameLogic->getFirstObject(); candidate; candidate = candidate->getNextObject())
		{
			if (candidate == self || candidate->isDestroyed() || candidate->isKindOf(KINDOF_WALL_UPGRADE) ||
				!candidate->isKindOf(KINDOF_STRUCTURE) || !candidate->isKindOf(KINDOF_WALK_ON_TOP_OF_WALL))
				continue;
			const Real dx = candidate->getPosition()->x - self->getPosition()->x;
			const Real dy = candidate->getPosition()->y - self->getPosition()->y;
			const Real distance = dx * dx + dy * dy;
			if (distance < closestDistance || (distance == closestDistance && (!closest || candidate->getID() < closest->getID())))
			{
				closest = candidate;
				closestDistance = distance;
			}
		}
		if (!closest)
			return UPDATE_SLEEP(5);
		m_supportingWall = closest->getID();
	}

	const Object* support = TheGameLogic->findObjectByID(m_supportingWall);
	const Bool available = support && !support->isDestroyed() && !support->isEffectivelyDead() &&
		!support->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION);
	if (!available && !m_waitingForWall)
	{
		m_waitingForWall = TRUE;
		self->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNSELECTABLE));
		self->setModelConditionState(MODELCONDITION_SOLD);
		TheGameLogic->deselectObject(self, PLAYERMASK_ALL, TRUE);
	}
	else if (available && m_waitingForWall)
	{
		m_waitingForWall = FALSE;
		for (const UpgradeTemplate* upgrade = TheUpgradeCenter->firstUpgradeTemplate(); upgrade; upgrade = upgrade->friend_getNext())
		{
			if (upgrade->getUpgradeType() == UPGRADE_TYPE_OBJECT && self->hasUpgrade(upgrade))
				self->removeUpgrade(upgrade);
		}
		self->clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNSELECTABLE));
		self->clearModelConditionState(MODELCONDITION_SOLD);
		TheAI->pathfinder()->noteRetailRouteOpening();
	}
	return UPDATE_SLEEP(5);
}

void WallUpgradeUpdate::xferRetailBlocks(Xfer* xfer, Bool present)
{
	RetailSaveBlocks::transferSection(xfer, xfer->getXferMode() == XFER_LOAD, present,
	{
		{ "wall-upgrade-support", [this](Xfer* x)
		{
			x->xferObjectID(&m_supportingWall);
			x->xferBool(&m_waitingForWall);
		}, [this]
		{
			m_supportingWall = INVALID_ID;
			m_waitingForWall = FALSE;
		} }
	});
}

void WallUpgradeUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, TRUE);
}

void WallUpgradeUpdate::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
	if (RetailFileLayers::enabled())
		xferRetailBlocks(xfer, version >= 1);
}

void WallUpgradeUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}

Bool DozerAIUpdate::findRetailBuildContact(const Object& builder, const Object& target, Coord3D& position)
{
	if (!RetailFileLayers::enabled())
		return FALSE;
	const auto* ai = builder.getAIUpdateInterface();
	const auto* dozer = ai ? ai->getDozerAIInterface() : nullptr;
	UnsignedInt mask = dozer ? dozer->getSpecialContactPoints() : 0;
	if (const auto* horde = HordeWorkerAIUpdate::find(builder.getContainedBy()))
		mask |= horde->getSpecialContactPoints();
	if (!mask)
		return FALSE;

	FindPositionOptions options;
	options.maxRadius = 20.0f;
	options.maxZDelta = 10.0f;
	options.startAngle = 0.0f;
	options.sourceToPathToDest = &builder;
	if (builder.isUsingAirborneLocomotor())
		options.ignoreObject = &target;

	Real closest = 0.0f;
	Bool found = FALSE;
	for (const auto& contact : target.getTemplate()->getRotWKObjectData().geometryContactPoints)
	{
		if (!(contact.flags & mask))
			continue;
		Vector3 world;
		Matrix3D::Transform_Vector(*target.getTransformMatrix(),
			Vector3(contact.position.x, contact.position.y, contact.position.z), &world);
		const Coord3D contactPosition = {world.X, world.Y, world.Z};
		Coord3D candidate;
		if (!ThePartitionManager->findPositionAround(&contactPosition, &options, &candidate))
			continue;
		const Real distance = sqr(builder.getPosition()->x - candidate.x) + sqr(builder.getPosition()->y - candidate.y);
		if (!found || distance < closest)
		{
			found = TRUE;
			closest = distance;
			position = candidate;
		}
	}
	return found;
}
