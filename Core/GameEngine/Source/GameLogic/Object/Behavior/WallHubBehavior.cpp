#include "PreRTS.h"

#include "GameLogic/Module/WallHubBehavior.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/HeroRoster.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Module/CreateModule.h"
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
		span.pieces.push_back({tile, ground, wallAngle});
		const UnsignedInt cost = static_cast<UnsignedInt>(std::max(0, tile->calcCostToBuild(player)));
		span.cost += std::min(cost, std::numeric_limits<UnsignedInt>::max() - span.cost);
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

	player->getMoney()->withdraw(span.cost);
	const Int stagger = std::clamp(getWallHubBehaviorModuleData()->m_staggerFactor, 0, 300);
	for (std::size_t i = 0; i < span.pieces.size(); ++i)
	{
		const Piece& piece = span.pieces[i];
		GettingBuiltBehavior::start(*piece.type, *hub, piece.position, piece.facing,
			static_cast<UnsignedInt>(stagger * i));
	}
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

void ReplaceSelfUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "ReplaceWith", INI::parseAsciiString, nullptr, offsetof(ReplaceSelfUpgradeModuleData, m_replacement) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields, 0);
}

ReplaceSelfUpgrade::ReplaceSelfUpgrade(Thing* thing, const ModuleData* data) : UpgradeModule(thing, data) {}
ReplaceSelfUpgrade::~ReplaceSelfUpgrade() = default;

void ReplaceSelfUpgrade::upgradeImplementation()
{
	Object* object = getObject();
	const AsciiString& name = getReplaceSelfUpgradeModuleData()->m_replacement;
	const ThingTemplate* definition = name.isEmpty() ? nullptr : TheThingFactory->findTemplate(name, FALSE);
	const auto* type = definition ? static_cast<const ThingTemplate*>(definition->getFinalOverride()) : nullptr;
	if (!type || type->isKindOf(KINDOF_DRAWABLE_ONLY) || object->isEffectivelyDead())
		return;

	const Matrix3D transform = *object->getTransformMatrix();
	Player* owner = object->getControllingPlayer();
	Team* team = object->getTeam();
	const Bool structure = type->isKindOf(KINDOF_STRUCTURE);
	const auto construction = MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_UNDER_CONSTRUCTION);
	TheAI->pathfinder()->removeObjectFromPathfindMap(object);
	TheGameLogic->destroyObject(object);
	Object* replacement = TheThingFactory->newObjectFromExactTemplate(type, team,
		structure ? construction : OBJECT_STATUS_MASK_NONE);
	replacement->setTransformMatrix(&transform);
	if (structure)
		replacement->clearStatus(construction);
	for (BehaviorModule** module = replacement->getBehaviorModules(); *module; ++module)
	{
		CreateModuleInterface* create = (*module)->getCreate();
		if (create && create->shouldDoOnBuildComplete())
			create->onBuildComplete();
	}
	if (owner && structure)
		owner->onStructureConstructionComplete(nullptr, replacement, TRUE);
	else
		TheAI->pathfinder()->addObjectToPathfindMap(replacement);
}

void ReplaceSelfUpgrade::crc(Xfer* xfer) { UpgradeModule::crc(xfer); }

void ReplaceSelfUpgrade::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpgradeModule::xfer(xfer);
}

void ReplaceSelfUpgrade::loadPostProcess() { UpgradeModule::loadPostProcess(); }

void GettingBuiltBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "WorkerName", INI::parseAsciiString, nullptr, offsetof(GettingBuiltBehaviorModuleData, m_worker) },
		{ "SelfBuildingLoop", INI::parseAsciiString, nullptr, offsetof(GettingBuiltBehaviorModuleData, m_buildLoop) },
		{ "SelfRepairFromDamageLoop", INI::parseAsciiString, nullptr, offsetof(GettingBuiltBehaviorModuleData, m_repairLoop) },
		{ "SelfRepairFromRubbleLoop", INI::parseAsciiString, nullptr, offsetof(GettingBuiltBehaviorModuleData, m_rebuildLoop) },
		{ "SpawnTimer", INI::parseDurationUnsignedInt, nullptr, offsetof(GettingBuiltBehaviorModuleData, m_spawnTimer) },
		{ "RebuildTimeSeconds", INI::parseReal, nullptr, offsetof(GettingBuiltBehaviorModuleData, m_rebuildTime) },
		{ "UseSpawnTimerWithoutWorker", INI::parseBool, nullptr, offsetof(GettingBuiltBehaviorModuleData, m_timerWithoutWorker) },
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
	Real facing, UnsignedInt delay)
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

UpdateSleepTime GettingBuiltBehavior::update()
{
	Object* structure = getObject();
	if (!m_running || structure->isEffectivelyDead() || !structure->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
		return UPDATE_SLEEP_FOREVER;

	const UnsignedInt frame = TheGameLogic->getFrame();
	if (frame < m_startFrame)
		return UPDATE_SLEEP(m_startFrame - frame);
	structure->clearAndSetModelConditionFlags(
		MAKE_MODELCONDITION_MASK(MODELCONDITION_AWAITING_CONSTRUCTION),
		MAKE_MODELCONDITION_MASK2(MODELCONDITION_PARTIALLY_CONSTRUCTED, MODELCONDITION_ACTIVELY_BEING_CONSTRUCTED));
	Object* builder = TheGameLogic->findObjectByID(structure->getBuilderID());
	FoundationAIUpdate::advanceConstruction(*structure, builder);
	if (!structure->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))
	{
		m_running = FALSE;
		return UPDATE_SLEEP_FOREVER;
	}
	return UPDATE_SLEEP_NONE;
}

void GettingBuiltBehavior::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	xfer->xferUnsignedInt(&m_startFrame);
	xfer->xferBool(&m_running);
}

void GettingBuiltBehavior::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	UpdateModule::xfer(xfer);
	xfer->xferUnsignedInt(&m_startFrame);
	xfer->xferBool(&m_running);
}

void GettingBuiltBehavior::loadPostProcess() { UpdateModule::loadPostProcess(); }
