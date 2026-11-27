#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <array>
#include <cstdio>

#include "Common/MultiplayerSettings.h"

namespace
{
void checkStartingCredits(const RetailTestCommands::Context &ctx)
{
	const std::array<UnsignedInt, 5> expected = {500, 1000, 1500, 2000, 2500};
	const auto &choices = TheMultiplayerSettings->getStartingMoneyList();
	Bool matches = choices.size() == expected.size();
	for (size_t i = 0; matches && i < expected.size(); ++i)
		matches = choices[i].countMoney() == expected[i];
	std::fprintf(stderr, "TEST ui-ini-credits frame=%u presets=%d default=%u\n", ctx.frame, matches,
		matches ? TheMultiplayerSettings->getDefaultStartingMoney().countMoney() : 0u);
}


}
#endif

namespace RetailTestCommands
{
void registerRetailUiIniMiscTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("ui-ini-credits-check", checkStartingCredits);
#endif
}
}
