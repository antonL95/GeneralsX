#include "PreRTS.h"

#if !RTS_GENERALS
#include <cstdio>
#include <string_view>

#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/RunOffMapBehavior.h"
#include "GameLogic/Module/SlavedUpdate.h"
#include "GameLogic/Object.h"

void runRetailPackage15TestCommand(std::string_view action, Object* object, UnsignedInt frame)
{
	if (!object)
		return;
	auto* ai = object->getAI();
	if (action == "pkg15-rampage" && ai)
	{
		ai->enterRetailRampage();
		return;
	}
	if (action != "pkg15-state")
		return;
	const auto* slave = static_cast<const SlavedUpdate*>(object->findUpdateModule(NAMEKEY("SlavedUpdate")));
	const auto* runOff = RunOffMapBehavior::find(object);
	const auto* draw = object->getDrawable();
	const auto* goal = ai ? ai->getGoalPosition() : nullptr;
	const auto* goalObject = ai ? ai->getGoalObject() : nullptr;
	const auto* victim = ai ? ai->getCurrentVictim() : nullptr;
	const auto* control = SlavedUpdate::getEvaSightedControl(object);
	const auto* pos = object->getPosition();
	std::fprintf(stderr,
		"TEST pkg15 state frame=%u object=%s player=%d id=%u x=%.1f y=%.1f goal=%.1f,%.1f target=%u victim=%u master=%u eva=%u skirmish=%d dead=%d unselectable=%d returning=%d deployed=%d runoff=%d opacity=%.2f\n",
		frame, object->getTemplate()->getName().str(), object->getControllingPlayer()->getPlayerIndex(),
		unsigned(object->getID()), pos->x, pos->y, goal ? goal->x : 0.0f, goal ? goal->y : 0.0f,
		goalObject ? unsigned(goalObject->getID()) : 0, victim ? unsigned(victim->getID()) : 0,
		slave ? unsigned(slave->getSlaverID()) : 0, unsigned(control->getID()),
		object->getControllingPlayer()->isSkirmishAIPlayer(), object->isEffectivelyDead(),
		object->testStatus(OBJECT_STATUS_UNSELECTABLE), object->testStatus(OBJECT_STATUS_LEASHED_RETURNING),
		object->testStatus(OBJECT_STATUS_DEPLOYED), runOff && runOff->isRunning(), draw ? draw->getEffectiveOpacity() : -1.0f);
}
#endif
