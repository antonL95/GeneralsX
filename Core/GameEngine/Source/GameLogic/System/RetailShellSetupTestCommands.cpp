#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/SidesList.h"
#include "GameNetwork/GameInfo.h"

#include <cstdio>

namespace
{
void logShellSetup(const RetailTestCommands::Context &ctx)
{
	if (!TheSkirmishGameInfo)
		return;
	const auto &game = *TheSkirmishGameInfo;
	std::fprintf(stderr, "TEST shell-setup frame=%u cash=%u\n", ctx.frame, game.getStartingCash().countMoney());
	for (Int index = 0; index < MAX_SLOTS; ++index)
	{
		const auto *slot = game.getConstSlot(index);
		if (!slot || !slot->isOccupied())
			continue;
		const AsciiString name = TheSidesList->getSlotPlayerName(index, slot->getStartPos(), FALSE);
		auto *player = ThePlayerList->findPlayerWithName(name);
		if (player)
			std::fprintf(stderr, "TEST shell-slot %d %s team=%d start=%d color=%d easy=%d\n",
				index, player->getPlayerTemplate()->getName().str(), slot->getTeamNumber(),
				player->getMpStartIndex(), slot->getColor(), slot->getState() == SLOT_EASY_AI);
	}
}

void logShellChecksum(const RetailTestCommands::Context &ctx)
{
	if (TheGameLogic->isInSkirmishGame())
		std::fprintf(stderr, "TEST shell-crc %u %08X\n", ctx.frame, TheGameLogic->getCRC(CRC_RECALC));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailShellSetupTestCommands()
{
#if !RTS_GENERALS
	registerAction("shell-setup", logShellSetup);
	registerAction("shell-crc", logShellChecksum);
#endif
}
}
