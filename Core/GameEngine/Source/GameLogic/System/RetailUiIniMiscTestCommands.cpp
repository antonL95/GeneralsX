#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include <array>
#include <cstdio>

#include "Common/MultiplayerSettings.h"
#include "GameClient/ControlBar.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameWindowTransitions.h"
#include "GameClient/Image.h"
#include "GameClient/InGameUI.h"

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

void checkPlacementClaim(const RetailTestCommands::Context &ctx)
{
	const Color color = TheInGameUI->getPlacementClaimFontColor();
	std::fprintf(stderr, "TEST ui-ini-placement-claim frame=%u color=%08x\n", ctx.frame, static_cast<unsigned>(color));
}

void checkTransitionImages(const RetailTestCommands::Context &ctx)
{
	const Bool vista = TheMappedImageCollection && TheMappedImageCollection->findImageByName("WorldViewMordorStart");
	const Bool solo = TheMappedImageCollection && TheMappedImageCollection->findImageByName("SoloMordorTransition");
	std::fprintf(stderr, "TEST ui-ini-transition-images frame=%u world=%d solo=%d\n", ctx.frame, vista, solo);
}

void checkFontDefaults(const RetailTestCommands::Context &ctx)
{
	std::fprintf(stderr, "TEST ui-ini-fonts frame=%u sacha=%d fixed=%d courier=%d\n", ctx.frame,
		TheFontLibrary->isFontAntialiased("SachaWynter", 12),
		TheFontLibrary->isFontAntialiased("FixedSys", 12),
		TheFontLibrary->isFontAntialiased("Courier New", 12));
}

void checkAlteredBorder(const RetailTestCommands::Context &ctx)
{
	const CommandButton *button = TheControlBar->findCommandButton("UiIniAlteredBorderFixture");
	const Bool altered = button && button->getCommandButtonMappedBorderType() == COMMAND_BUTTON_BORDER_ALTERED;
	std::fprintf(stderr, "TEST ui-ini-button-border frame=%u altered=%d\n", ctx.frame, altered);
}

void checkPostLoadSound(const RetailTestCommands::Context &ctx)
{
	const AsciiString name = "UiIniPostLoadSoundFixture";
	TheTransitionHandler->setGroup(name, TRUE);
	const Bool active = !TheTransitionHandler->isFinished();
	TheTransitionHandler->remove(name, TRUE);
	std::fprintf(stderr, "TEST ui-ini-post-load-sound frame=%u active=%d\n", ctx.frame, active);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailUiIniMiscTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("ui-ini-credits-check", checkStartingCredits);
	registerAction("ui-ini-placement-claim-check", checkPlacementClaim);
	registerAction("ui-ini-transition-images-check", checkTransitionImages);
	registerAction("ui-ini-fonts-check", checkFontDefaults);
	registerAction("ui-ini-border-check", checkAlteredBorder);
	registerAction("ui-ini-post-load-sound-check", checkPostLoadSound);
#endif
}
}
