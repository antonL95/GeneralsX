#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>

#include "Common/Team.h"
#include "GameLogic/AI.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

namespace
{
void order(const RetailTestCommands::Context& ctx)
{
	Object* first = ctx.requireObject();
	Object* second = ctx.findTarget();
	Object* destination = ctx.findTarget(std::string_view("Player_1:AIGroupSaveTarget:0"));
	if (!first || !second || !destination)
		return;

	Team* firstTeam = first->getTeam();
	Team* secondTeam = second->getTeam();
	Team* team = newInstance(Team)(const_cast<TeamPrototype*>(firstTeam->getPrototype()), TEAM_ID_INVALID);
	first->setTemporaryTeam(team);
	second->setTemporaryTeam(team);

	const AsciiString targetName("AIGroupSaveDestination");
	destination->setName(targetName);
	TheScriptEngine->addObjectToCache(destination);
	auto* action = newInstance(ScriptAction)(ScriptAction::ATTACK_MOVE_TEAM_TO_NAMED_OBJECT);
	action->getParameter(0)->friend_setString("<This Team>");
	action->getParameter(1)->friend_setString(targetName);
	TheScriptEngine->friend_executeAction(action, team);
	deleteInstance(action);
	TheScriptEngine->setRetailUnitReference(targetName, nullptr);

	first->setTemporaryTeam(firstTeam);
	second->setTemporaryTeam(secondTeam);
	deleteInstance(team);

	const Bool ordered = first->getAI()->getCurrentStateID() == AI_ATTACK_MOVE_TO &&
		second->getAI()->getCurrentStateID() == AI_ATTACK_MOVE_TO;
	std::fprintf(stderr, "TEST aigroup-save order frame=%u attack-move=%d\n", ctx.frame, ordered);
}

void probe(const RetailTestCommands::Context& ctx)
{
	Object* unit = ctx.requireObject();
	if (!unit)
		return;

	AIGroup* group = unit->getGroup();
	std::fprintf(stderr, "TEST aigroup-save group frame=%u id=%u", ctx.frame, group ? group->getID() : 0);
	if (group)
		for (ObjectID id : group->getAllIDs())
			std::fprintf(stderr, " %u", static_cast<UnsignedInt>(id));
	std::fprintf(stderr, "\n");
}
}
#endif

namespace RetailTestCommands
{
void registerRetailAIGroupSaveTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("aigroup-save-order", order);
	registerAction("aigroup-save-probe", probe);
#endif
}
}
