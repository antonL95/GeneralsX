#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include "Common/Dict.h"
#include "Common/Player.h"
#include "Common/Team.h"
#include "GameLogic/Object.h"

namespace
{
void assignMountTeam(const RetailTestCommands::Context& context)
{
	Object* member = context.requireObject(true);
	if (!member)
		return;
	Player* owner = member->getControllingPlayer();
	if (!owner)
		return;
	const AsciiString name(context.argument);
	if (!TheTeamFactory->findTeamPrototype(name))
	{
		Dict teamData;
		TheTeamFactory->initTeam(name, owner->getPlayerName(), TRUE, &teamData);
	}
	Team* team = TheTeamFactory->findTeam(name);
	if (team && team->getControllingPlayer() == owner)
		member->setTeam(team);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailAIPowersTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("ai-powers-mount-team", assignMountTeam);
#endif
}
}
