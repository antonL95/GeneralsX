#include "PreRTS.h"
#include "GameLogic/RetailRingDrop.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/CreateObjectDie.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/PartitionManager.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
	constexpr UnsignedInt DROP_FRAME = 10 * LOGICFRAMES_PER_SECOND;

	Bool requested()
	{
		static const char* const value = std::getenv("GENERALSX_RING_DROP");
		return value && *value && std::strcmp(value, "0") != 0;
	}

	const ObjectCreationList* findGollumRingDeathList()
	{
		for (const ThingTemplate* tmpl = TheThingFactory->firstTemplate(); tmpl; tmpl = tmpl->friend_getNextTemplate())
		{
			if (!tmpl->isKindOf(KINDOF_NEUTRALGOLLUM))
				continue;
			const ModuleInfo& behaviors = tmpl->getBehaviorModuleInfo();
			for (Int i = 0; i < behaviors.getCount(); ++i)
			{
				if (behaviors.getNthName(i).compare("CreateObjectDie") != 0)
					continue;
				const auto* data = static_cast<const CreateObjectDieModuleData*>(behaviors.getNthData(i));
				if (data->m_ocl && data->m_ocl->createsKindOf(KINDOF_ONE_RING))
					return data->m_ocl;
			}
		}
		return nullptr;
	}

	const Object* findHumanFortress()
	{
		const Object* fortress = nullptr;
		for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		{
			const Player* owner = object->getControllingPlayer();
			if (!owner || owner->getPlayerType() != PLAYER_HUMAN || object->isEffectivelyDead() ||
				!object->isKindOf(KINDOF_COMMANDCENTER) || !object->isKindOf(KINDOF_CASTLE_KEEP))
				continue;
			if (!fortress || object->getID() < fortress->getID())
				fortress = object;
		}
		return fortress;
	}

	Real reachOfBase(const Object& fortress)
	{
		const Real searched = fortress.getVisionRange();
		Real reach = fortress.getGeometryInfo().getBoundingCircleRadius();
		for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
		{
			if (object->getControllingPlayer() != fortress.getControllingPlayer() || !object->isKindOf(KINDOF_STRUCTURE))
				continue;
			const Real x = object->getPosition()->x - fortress.getPosition()->x;
			const Real y = object->getPosition()->y - fortress.getPosition()->y;
			const Real distance = sqrt(x * x + y * y);
			if (distance <= searched)
				reach = max(reach, distance + object->getGeometryInfo().getBoundingCircleRadius());
		}
		return reach;
	}
}

void RetailRingDrop::update(UnsignedInt frame)
{
	if (frame != DROP_FRAME || !requested())
		return;

	const ObjectCreationList* list = findGollumRingDeathList();
	const Object* fortress = findHumanFortress();
	if (!list || !fortress)
	{
		std::fprintf(stderr, "RING-DROP frame=%u skipped list=%d fortress=%d\n", frame, list != nullptr, fortress != nullptr);
		return;
	}

	FindPositionOptions options;
	options.minRadius = reachOfBase(*fortress) + fortress->getGeometryInfo().getBoundingCircleRadius();
	options.maxRadius = max(options.minRadius, fortress->getVisionRange());
	options.startAngle = 0.0f;
	options.flags = FPF_CLEAR_CELLS_ONLY;
	Coord3D position;
	if (!ThePartitionManager->findPositionAround(fortress->getPosition(), &options, &position))
	{
		std::fprintf(stderr, "RING-DROP frame=%u skipped position=0\n", frame);
		return;
	}

	const Object* ring = ObjectCreationList::create(list, nullptr, &position, &position, 0.0f);
	if (ring)
		std::fprintf(stderr, "RING-DROP frame=%u object=%s id=%u position=%.1f,%.1f fortress=%s at=%.1f,%.1f\n",
			frame, ring->getTemplate()->getName().str(), static_cast<UnsignedInt>(ring->getID()),
			ring->getPosition()->x, ring->getPosition()->y, fortress->getTemplate()->getName().str(),
			fortress->getPosition()->x, fortress->getPosition()->y);
}
