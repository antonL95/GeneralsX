#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/Player.h"
#include "Common/Upgrade.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Object.h"
#include "../../../../GameEngineDevice/Include/W3DDevice/GameClient/Module/W3DModelDraw.h"

namespace
{
void logPlayerUpgrade(const RetailTestCommands::Context& ctx)
{
	const Player* player = ctx.findPlayer();
	const auto* upgrade = TheUpgradeCenter->findUpgrade(ctx.argument);
	std::fprintf(stderr, "TEST lua player-upgrade frame=%u found=%d\n", ctx.frame,
		player && upgrade && player->hasUpgradeComplete(upgrade));
}

void logVisibility(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject(true);
	const Drawable* drawable = object ? object->getDrawable() : nullptr;
	if (!drawable)
		return;
	for (auto module = drawable->getDrawModules(); *module; ++module)
	{
		const auto* model = dynamic_cast<const W3DModelDraw*>(*module);
		Bool hidden = FALSE;
		if (model && model->getSubObjectHidden(ctx.argument, hidden))
		{
			std::fprintf(stderr, "TEST lua visibility frame=%u part=%s hidden=%d\n", ctx.frame, ctx.argument, hidden);
			return;
		}
	}
}
}
#endif

namespace RetailTestCommands
{
void registerRetailLuaInventoryTestCommands()
{
#if !RTS_GENERALS
	registerAction("lua-player-upgrade", logPlayerUpgrade);
	registerAction("lua-visibility", logVisibility);
#endif
}
}
