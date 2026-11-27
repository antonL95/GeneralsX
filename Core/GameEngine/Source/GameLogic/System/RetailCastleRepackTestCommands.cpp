#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/CastleBehavior.h"
#include "GameLogic/Module/FoundationAIUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

namespace
{
void unpack(const RetailTestCommands::Context& ctx)
{
	Object* packed = ctx.requireObject();
	if (!packed)
		return;
	packed->setName(packed->getTemplate()->getName());
	TheScriptEngine->addObjectToCache(packed);
	auto* action = newInstance(ScriptAction)(ScriptAction::NAMED_BASE_UNPACK_FREE);
	action->getParameter(0)->friend_setString(packed->getName());
	action->getParameter(1)->friend_setString(ctx.argument);
	TheScriptEngine->friend_executeAction(action, packed->getTeam());
	deleteInstance(action);
	const auto* castle = CastleBehavior::find(packed);
	if (!castle || !castle->isUnpacked())
		return;
	for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		if (CastleBehavior::findForMember(*object) == castle)
			object->setStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_NO_ATTACK));
	}
}

void removeKeep(const RetailTestCommands::Context& ctx)
{
	const auto* castle = CastleBehavior::find(ctx.requireObject());
	if (Object* keep = castle ? castle->getKeep() : nullptr)
		TheGameLogic->destroyObject(keep);
}

void probe(const RetailTestCommands::Context& ctx)
{
	const Object* packed = ctx.requireObject();
	const auto* castle = CastleBehavior::find(packed);
	if (!castle)
		return;
	Int occupied = 0;
	Int defensive = 0;
	for (const Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
	{
		const auto* plot = FoundationAIUpdate::find(*object);
		if (plot && plot->getStructure() && CastleBehavior::findForMember(*object) == castle)
		{
			++occupied;
			if (object->isKindOf(KINDOF_BASE_DEFENSE_FOUNDATION))
				++defensive;
		}
	}
	std::fprintf(stderr, "TEST castle-repack %s frame=%u unpacked=%d keep=%d occupied=%d defensive=%d masked=%d\n",
		ctx.targetName, ctx.frame, castle->isUnpacked(), castle->getKeep() != nullptr, occupied, defensive,
		packed->testStatus(OBJECT_STATUS_MASKED));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailCastleRepackTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("castle-repack-unpack", unpack);
	registerAction("castle-repack-remove-keep", removeKeep);
	registerAction("castle-repack-probe", probe);
#endif
}
}
