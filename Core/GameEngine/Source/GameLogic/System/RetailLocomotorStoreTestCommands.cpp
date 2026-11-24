#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/PolygonTrigger.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
const Locomotor* activeLocomotor(const Object* object)
{
	const AIUpdateInterface* ai = object ? object->getAI() : nullptr;
	return ai ? ai->getCurLocomotor() : nullptr;
}

void sampleLocomotor(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	if (const Locomotor* locomotor = activeLocomotor(object))
	{
		const Coord3D& pos = *object->getPosition();
		std::fprintf(stderr, "TEST locomotor-store %u %s %d %.3f %.3f\n",
			ctx.frame, object->getTemplate()->getName().str(),
			locomotor->participatesInGroupFormation(), pos.x, pos.y);
	}
}

void sampleAuthoredFields(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	if (const Locomotor* locomotor = activeLocomotor(object))
	{
		const Real degrees = locomotor->getTurnThresholdHighSpeed() * 360.0f / RotwkLocomotorRules::FULL_TURN;
		std::fprintf(stderr, "TEST locomotor-store-fields %u %s turnHS=%.1f overlap=%.6g scales=%d smoothing=%d\n",
			ctx.frame, object->getTemplate()->getName().str(), degrees, locomotor->getMaxOverlappedHeight(),
			locomotor->scalesWalls(), locomotor->usesTerrainSmoothing());
	}
}

void sampleRiver(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	if (!object)
		return;
	const Coord3D& pos = *object->getPosition();
	const WaterHandle* water = TheTerrainLogic->getWaterHandle(pos.x, pos.y);
	const Bool river = water && water->m_polygon && water->m_polygon->isRiver();
	const Locomotor* locomotor = activeLocomotor(object);
	const Real cap = locomotor ? locomotor->getMaxSpeedForCondition(object->getBodyModule()->getDamageState(), object) : -1.0f;
	std::fprintf(stderr, "TEST locomotor-store-water %u %s river=%d cap=%.4f\n",
		ctx.frame, object->getTemplate()->getName().str(), river, cap);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailLocomotorStoreTestCommands()
{
#if !RTS_GENERALS
	registerAction("locomotor-store-sample", sampleLocomotor);
	registerAction("locomotor-store-fields", sampleAuthoredFields);
	registerAction("locomotor-store-water", sampleRiver);
#endif
}
}
