#include "PreRTS.h"

#include "Common/GameCommon.h"
#include "Common/GlobalData.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Module/RetailConstruction.h"
#include "GameLogic/Object.h"

void BuildingBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);
	static const FieldParse fields[] = {
		{ "NightWindowName", INI::parseAsciiStringVector, nullptr, offsetof(BuildingBehaviorModuleData, m_nightWindows) },
		{ "FireWindowName", INI::parseAsciiString, nullptr, offsetof(BuildingBehaviorModuleData, m_fireWindow) },
		{ "GlowWindowName", INI::parseAsciiString, nullptr, offsetof(BuildingBehaviorModuleData, m_glowWindow) },
		{ "FireName", INI::parseAsciiStringVectorAppend, nullptr, offsetof(BuildingBehaviorModuleData, m_fireMeshes) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

BuildingBehavior::~BuildingBehavior() = default;

UpdateSleepTime BuildingBehavior::update()
{
	Object* object = getObject();
	if (!RetailFileLayers::enabled() || object->isEffectivelyDead())
		return UPDATE_SLEEP_FOREVER;
	if (object->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) || object->testStatus(OBJECT_STATUS_SOLD))
		return UPDATE_SLEEP_NONE;
	Drawable* drawable = object->getDrawable();
	if (!drawable)
		return UPDATE_SLEEP_NONE;

	const Bool night = TheGlobalData->m_timeOfDay == TIME_OF_DAY_NIGHT;
	const Bool fire = object->testStatus(OBJECT_STATUS_AFLAME);
	const Int state = (night ? 1 : 0) | (fire ? 2 : 0);
	if (state != m_windowState)
	{
		const auto* data = getBuildingBehaviorModuleData();
		for (const AsciiString& window : data->m_nightWindows)
			drawable->showSubObject(window, night && !fire);
		drawable->showSubObject(data->m_fireWindow, fire);
		drawable->showSubObject(data->m_glowWindow, fire);
		for (const AsciiString& mesh : data->m_fireMeshes)
			drawable->showSubObject(mesh, night && fire);
		drawable->updateSubObjects();
		m_windowState = state;
	}
	return UPDATE_SLEEP_NONE;
}

void BuildingBehavior::crc(Xfer* xfer) { UpdateModule::crc(xfer); }
void BuildingBehavior::xfer(Xfer* xfer) { UpdateModule::xfer(xfer); }
void BuildingBehavior::loadPostProcess()
{
	UpdateModule::loadPostProcess();
	m_windowState = -1;
}

void BaseUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse(p);
	static const FieldParse fields[] = {
		{ "BuildingTemplateName", INI::parseAsciiString, nullptr, offsetof(BaseUpgradeModuleData, m_building) },
		{ "PlacementPrefix", INI::parseAsciiString, nullptr, offsetof(BaseUpgradeModuleData, m_prefix) },
		{ "PlacementIndex", INI::parseInt, nullptr, offsetof(BaseUpgradeModuleData, m_index) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

BaseUpgrade::~BaseUpgrade() = default;

void BaseUpgrade::upgradeImplementation()
{
	if (!RetailFileLayers::enabled())
		return;
	Object* base = getObject();
	const auto* data = getBaseUpgradeModuleData();
	const ThingTemplate* type = TheThingFactory->findTemplate(data->m_building, FALSE);
	if (!type || !base->getDrawable() || base->isEffectivelyDead())
		return;

	Coord3D position = *base->getPosition();
	if (data->m_index > 0 && !data->m_prefix.isEmpty())
	{
		Coord3D positions[32];
		const Int count = base->getMultiLogicalBonePosition(data->m_prefix.str(), 32, positions, nullptr);
		if (data->m_index < count)
			position = positions[data->m_index];
	}
	setUpgradeExecuted(TRUE);
	Object* building = TheThingFactory->newObject(type, base->getTeam(), OBJECT_STATUS_MASK_NONE);
	building->setProducer(base);
	building->setPosition(&position);
	building->setOrientation(base->getOrientation());
	TheAI->pathfinder()->addObjectToPathfindMap(building);
}

void BaseUpgrade::crc(Xfer* xfer) { UpgradeModule::crc(xfer); }
void BaseUpgrade::xfer(Xfer* xfer) { UpgradeModule::xfer(xfer); }
void BaseUpgrade::loadPostProcess() { UpgradeModule::loadPostProcess(); }
