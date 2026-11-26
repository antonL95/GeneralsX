#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>

#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/AIStateMachine.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"

namespace
{
constexpr UnsignedInt STALL_FRAMES = 150;
constexpr UnsignedInt SAMPLE_INTERVAL = 10;
constexpr Real PROGRESS_DISTANCE = 3.0f;

struct Watch
{
	Coord3D base = {0.0f, 0.0f, 0.0f};
	UnsignedInt baseFrame = 0;
	Bool reported = FALSE;
};

std::map<ObjectID, Watch> s_watches;

Bool holdsMoveOrder(const AIUpdateInterface* ai)
{
	const StateID state = ai->getCurrentStateID();
	return state == AI_MOVE_TO || state == AI_FOLLOW_PATH || state == AI_MOVE_AND_TIGHTEN;
}

const Object* nearestStructure(const Object* object, Real& distance)
{
	const Object* best = nullptr;
	distance = 0.0f;
	for (const Object* other = TheGameLogic->getFirstObject(); other; other = other->getNextObject())
	{
		if (other == object || !other->isKindOf(KINDOF_STRUCTURE) || other->isEffectivelyDead())
			continue;
		const Coord3D& a = *object->getPosition();
		const Coord3D& b = *other->getPosition();
		const Real d = std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
		if (!best || d < distance)
		{
			best = other;
			distance = d;
		}
	}
	return best;
}

void report(UnsignedInt frame, const Object* object, const Object* anchor, const Watch& watch)
{
	const AIUpdateInterface* ai = object->getAIUpdateInterface();
	const AIUpdateInterface* orderAi = anchor ? anchor->getAIUpdateInterface() : ai;
	const Coord3D& pos = *object->getPosition();
	const Coord3D* goal = orderAi->getGoalPosition();
	const Player* owner = object->getControllingPlayer();
	Real buildingDistance = 0.0f;
	const Object* building = nearestStructure(object, buildingDistance);

	std::fprintf(stderr, "STALL frame=%u since=%u id=%u tmpl=%s owner=%d/%s pos=%.1f,%.1f goal=%.1f,%.1f",
		frame, watch.baseFrame, object->getID(), object->getTemplate()->getName().str(),
		owner ? owner->getPlayerIndex() : -1, owner ? owner->getSide().str() : "-",
		pos.x, pos.y, goal ? goal->x : -1.0f, goal ? goal->y : -1.0f);
	std::fprintf(stderr, " state=%d orderState=%d waitPath=%d stuck=%d idle=%d pathAge=%u",
		static_cast<int>(ai->getCurrentStateID()), static_cast<int>(orderAi->getCurrentStateID()),
		ai->isWaitingForPath() ? 1 : 0,
		ai->isBlockedAndStuck() ? 1 : 0, ai->isIdle() ? 1 : 0, ai->getPath() ? ai->getPathAge() : 0u);
	if (anchor)
	{
		const HordeContain* horde = HordeContain::get(anchor);
		HordeContain::FormationGoal formation = {};
		const Bool hasGoal = horde && horde->getFormationGoal(object, formation);
		std::fprintf(stderr, " horde=%u hordeScale=%.2f anchorStuck=%d anchorWaitPath=%d", anchor->getID(),
			horde ? (horde->isHeadingAligned(anchor->getOrientation()) ? 1.0f : 0.1f) : -1.0f,
			orderAi->isBlockedAndStuck() ? 1 : 0,
			orderAi->isWaitingForPath() ? 1 : 0);
		if (hasGoal)
			std::fprintf(stderr, " slotErr=%.1f slotSpeed=%.2f slot=%.1f,%.1f hordeMoving=%d backing=%d",
				formation.error, formation.speed, formation.slot.x, formation.slot.y,
				formation.hordeMoving ? 1 : 0, formation.backing ? 1 : 0);
	}
	std::fprintf(stderr, " building=%s buildingDist=%.1f\n",
		building ? building->getTemplate()->getName().str() : "-", buildingDistance);
}
}

namespace RetailTestCommands
{
void registerRetailStallWatchTestCommands() {}

void runStallWatch(unsigned int frame)
{
	static const bool enabled = std::getenv("GENERALSX_STALL_WATCH") != nullptr;
	if (!enabled || frame % SAMPLE_INTERVAL != 0)
		return;

	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		AIUpdateInterface* ai = object->getAIUpdateInterface();
		if (!ai || object->isEffectivelyDead())
			continue;
		Object* anchor = HordeContain::getAnchor(object);
		const AIUpdateInterface* orderAi = anchor ? anchor->getAIUpdateInterface() : ai;
		if (!orderAi || !holdsMoveOrder(orderAi))
		{
			s_watches.erase(object->getID());
			continue;
		}

		const Coord3D& pos = *object->getPosition();
		auto [it, inserted] = s_watches.try_emplace(object->getID());
		Watch& watch = it->second;
		const Real dx = pos.x - watch.base.x;
		const Real dy = pos.y - watch.base.y;
		if (inserted || std::sqrt(dx * dx + dy * dy) >= PROGRESS_DISTANCE)
		{
			watch.base = pos;
			watch.baseFrame = frame;
			watch.reported = FALSE;
			continue;
		}
		if (!watch.reported && frame - watch.baseFrame >= STALL_FRAMES)
		{
			watch.reported = TRUE;
			report(frame, object, anchor, watch);
		}
	}
}
}
#else
namespace RetailTestCommands
{
void registerRetailStallWatchTestCommands() {}
void runStallWatch(unsigned int) {}
}
#endif
