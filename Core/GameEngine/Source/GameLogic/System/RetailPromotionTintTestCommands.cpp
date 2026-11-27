#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if RTS_ZEROHOUR
#include "Common/INI.h"
#include "GameClient/Drawable.h"
#include "GameLogic/ExperienceLevelStore.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace
{
void loadLevels(const RetailTestCommands::Context& ctx)
{
	std::ifstream input(ctx.argument);
	if (!input)
		return;
	std::ostringstream text;
	text << input.rdbuf();
	INI ini;
	ini.loadText(ctx.argument, text.str().c_str(), INI_LOAD_OVERWRITE, "ExperienceLevel", ExperienceLevelStore::parseDefinition);
}

void awardExperience(const RetailTestCommands::Context& ctx)
{
	if (Object* object = ctx.requireObject(true))
		object->getExperienceTracker()->addRetailExperience(10.0f, FALSE);
}

void probeTint(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject(true);
	if (!object)
		return;
	const Drawable* drawable = object->getDrawable();
	if (const HordeContain* horde = HordeContain::get(object))
	{
		const auto* members = horde->getContainedItemsList();
		drawable = members->empty() ? nullptr : members->front()->getDrawable();
	}
	if (!drawable)
		return;
	Vector3 combined;
	const Vector3* color = drawable->getTintColor(combined);
	std::fprintf(stderr, "TEST promotion-tint frame=%u %s rank=%d rgb=%.3f,%.3f,%.3f\n",
		ctx.frame, ctx.targetName, object->getExperienceTracker()->getRank(),
		color ? color->X : 0.0f, color ? color->Y : 0.0f, color ? color->Z : 0.0f);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailPromotionTintTestCommands()
{
#if RTS_ZEROHOUR
	registerAction("promotion-tint-load-levels", loadLevels);
	registerAction("promotion-tint-award", awardExperience);
	registerAction("promotion-tint-probe", probeTint);
#endif
}
}
