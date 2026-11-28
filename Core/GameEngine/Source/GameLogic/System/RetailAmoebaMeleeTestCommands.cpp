#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "Common/ThingTemplate.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HordeAIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "WWMath/wwmath.h"

namespace
{
void logAmoebaMeleeProbe(const RetailTestCommands::Context& ctx)
{
	const Object* anchor = ctx.requireObject();
	const HordeContain* horde = HordeContain::get(anchor);
	if (!anchor || !horde)
		return;

	const Object* target = ctx.findTarget();
	const AIUpdateInterface* anchorAI = anchor->getAI();
	const Bool engaging = anchorAI && anchorAI->getModuleNameKey() == NAMEKEY("HordeAIUpdate") &&
		static_cast<const HordeAIUpdate*>(anchorAI)->isEngaging();
	const Real facing = anchor->getOrientation();
	Real targetAngle = facing;
	Real angleErr = 0.0f;
	if (target)
	{
		const Coord3D* aPos = anchor->getPosition();
		const Coord3D* tPos = target->getPosition();
		targetAngle = WWMath::Atan2(tPos->y - aPos->y, tPos->x - aPos->x);
		angleErr = fabs(stdAngleDiff(targetAngle, facing));
	}

	Real maxAnchorDistSq = 0.0f;
	unsigned int living = 0;
	unsigned int attacking = 0;
	std::vector<std::pair<ObjectID, Int>> slots;
	const Coord3D& anchorPos = *anchor->getPosition();
	for (const Object* member : *horde->getContainedItemsList())
	{
		if (member->isEffectivelyDead())
			continue;
		++living;
		const AIUpdateInterface* ai = member->getAI();
		if (ai && ai->isAttacking())
			++attacking;
		const Real dx = member->getPosition()->x - anchorPos.x;
		const Real dy = member->getPosition()->y - anchorPos.y;
		maxAnchorDistSq = std::max(maxAnchorDistSq, dx * dx + dy * dy);
		const Int slot = horde->getMemberSlot(member->getID());
		if (slot >= 0)
			slots.emplace_back(member->getID(), slot);
	}
	std::sort(slots.begin(), slots.end());

	std::fprintf(stderr,
		"TEST amoeba-probe %u %s engaging=%d facing=%.5f targetAngle=%.5f angleErr=%.5f living=%u attacking=%u maxDist=%.3f slots=",
		ctx.frame, anchor->getTemplate()->getName().str(), engaging ? 1 : 0,
		facing, targetAngle, angleErr, living, attacking, WWMath::SqrtfOrigin(maxAnchorDistSq));
	for (std::size_t i = 0; i < slots.size(); ++i)
		std::fprintf(stderr, "%s%d", i ? "," : "", slots[i].second);
	std::fprintf(stderr, "\n");
	std::fflush(stderr);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailAmoebaMeleeTestCommands()
{
#if !RTS_GENERALS
	registerAction("amoeba-probe", logAmoebaMeleeProbe);
#endif
}
}
