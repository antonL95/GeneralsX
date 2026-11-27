#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include "Common/Dict.h"
#include "Common/NameKeyGenerator.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"

namespace
{
void importVeteranDetails(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject(true);
	const HordeContain* horde = HordeContain::get(object);
	if (!horde)
		return;

	Dict group;
	UnicodeString name;
	name.translate(AsciiString(ctx.argument));
	group.setUnicodeString(NAMEKEY("objectVeteranName"), name);
	object->updateObjValuesFromMapProperties(&group);

	Dict veteran;
	veteran.setInt(NAMEKEY("objectBattlesFought"), 4);
	for (Object* member : *horde->getContainedItemsList())
		member->updateObjValuesFromMapProperties(&veteran);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailVeteranHoverTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("world-hover-veteran", importVeteranDetails);
#endif
}
}
