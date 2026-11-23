#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>

#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/CastleBehavior.h"
#include "GameLogic/Module/FoundationAIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

namespace
{
void unpackTestFortress(const RetailTestCommands::Context& ctx)
{
	Object* packed = ctx.requireObject();
	if (!packed)
		return;

	const AsciiString name("FortressExpansionFixturePacked");
	packed->setName(name);
	TheScriptEngine->addObjectToCache(packed);
	auto* unpack = newInstance(ScriptAction)(ScriptAction::NAMED_BASE_UNPACK_FREE);
	unpack->setNextAction(nullptr);
	unpack->getParameter(0)->friend_setString(name);
	unpack->getParameter(1)->friend_setString("FortressExpansionFixtureKeep");
	TheScriptEngine->friend_executeAction(unpack, packed->getTeam());
	deleteInstance(unpack);
	if (const CastleBehavior* castle = CastleBehavior::find(packed))
		if (Object* keep = castle->getKeep())
			keep->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_NO_ATTACK));
}

void logExpansionState(const RetailTestCommands::Context& ctx)
{
	const Object* expansion = ctx.findObject(FALSE);
	const Object* pad = expansion ? TheGameLogic->findObjectByID(expansion->getProducerID()) : nullptr;
	const auto* foundation = pad ? FoundationAIUpdate::find(*pad) : nullptr;
	std::fprintf(stderr, "TEST fortress-expansion %u pad=%d id=%u percent=%.2f under=%d dead=%d\n",
		ctx.frame, foundation && foundation->getStructure() == expansion, expansion ? static_cast<unsigned>(expansion->getID()) : 0,
		expansion ? expansion->getConstructionPercent() : 0.0f,
		expansion && expansion->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION),
		expansion && expansion->isEffectivelyDead());
}
}
#endif

namespace RetailTestCommands
{
void registerRetailFortressExpansionTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("fortress-expansion-unpack", unpackTestFortress);
	registerAction("fortress-expansion-state", logExpansionState);
#endif
}
}
