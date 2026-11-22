#include "PreRTS.h"

#include "GameLogic/Module/AnnounceBirthAndDeathBehavior.h"

#include <cstdio>
#include <cstdlib>

#include "Common/GameState.h"
#include "Common/GameUtility.h"
#include "Common/Player.h"
#include "Common/RetailFileLayers.h"
#include "Common/Xfer.h"
#include "GameClient/RetailHudExtras.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

namespace
{
void announce(const Object& object, RetailNotification::HeroEvent event, const char* label)
{
	if (!RetailFileLayers::enabled() || !object.isKindOf(KINDOF_HERO) ||
		(TheGameState && TheGameState->isInLoadGame()))
		return;
	const Player* viewer = rts::getObservedOrLocalPlayer_Safe();
	if (!viewer || viewer != object.getControllingPlayer())
		return;
	if (TheGameLogic && std::getenv("GENERALSX_TRACE_RETAIL_AUDIO"))
		std::fprintf(stderr, "RETAIL NOTICE %s frame=%u owner=%u\n", label, TheGameLogic->getFrame(),
			static_cast<UnsignedInt>(object.getID()));
	RetailNotification::showHero(event, object.getTemplate());
}
}

AnnounceBirthAndDeathBehavior::~AnnounceBirthAndDeathBehavior() = default;

void AnnounceBirthAndDeathBehavior::onCreate()
{
	announce(*getObject(), RetailNotification::HeroEvent::InitialSpawn, "birth");
}

void AnnounceBirthAndDeathBehavior::onDie(const DamageInfo*)
{
	announce(*getObject(), RetailNotification::HeroEvent::Death, "death");
}

void AnnounceBirthAndDeathBehavior::crc(Xfer* xfer)
{
	CreateModule::crc(xfer);
}

void AnnounceBirthAndDeathBehavior::xfer(Xfer* xfer)
{
	XferVersion version = 1;
	xfer->xferVersion(&version, 1);
	CreateModule::xfer(xfer);
}

void AnnounceBirthAndDeathBehavior::loadPostProcess()
{
	CreateModule::loadPostProcess();
}
