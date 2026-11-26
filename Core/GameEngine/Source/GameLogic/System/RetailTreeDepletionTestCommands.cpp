#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>
#include <vector>

#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameLogic/ExperienceTracker.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"

namespace
{
constexpr Real SAME_TREE_RADIUS = 5.0f;

void survey()
{
	std::vector<Coord3D> found;
	const Coord3D origin{0.0f, 0.0f, 0.0f};
	Coord3D tree;
	while (TheTerrainLogic->findNearestHarvestTree(origin, 100000.0f, found, tree))
	{
		found.push_back(tree);
		std::fprintf(stderr, "TEST tree-survey %.3f %.3f\n", tree.x, tree.y);
	}
}

void castDevastation(const RetailTestCommands::Context& ctx)
{
	Object* caster = ctx.requireObject();
	const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate("SpellBookDevastation");
	if (!caster || !power)
		return;
	SpecialPowerModuleInterface* module = caster->getSpecialPowerModule(power);
	const UnsignedInt before = caster->getControllingPlayer()->getMoney()->countMoney();
	const Coord3D point = ctx.groundLocation();
	if (module)
		caster->doSpecialPowerAtLocation(power, &point, INVALID_ANGLE, 0, TRUE);
	std::fprintf(stderr, "TEST tree-depletion devastate frame=%u module=%d payout=%u\n", ctx.frame, module != nullptr,
		caster->getControllingPlayer()->getMoney()->countMoney() - before);
}

void handleTreeDepletion(const RetailTestCommands::Context& ctx)
{
	if (ctx.action == "tree-depletion-survey")
	{
		survey();
		return;
	}
	if (ctx.action == "tree-depletion-devastate")
	{
		castDevastation(ctx);
		return;
	}
	if (ctx.action == "tree-depletion-selectable")
	{
		if (const Object* object = ctx.requireObject())
			std::fprintf(stderr, "TEST tree-depletion selectable frame=%u %s object=%d drawable=%d\n", ctx.frame,
				object->getTemplate()->getName().str(), object->isSelectable(),
				object->getDrawable() && object->getDrawable()->isSelectable());
		return;
	}
	if (ctx.action == "tree-depletion-prime" || ctx.action == "tree-depletion-level")
	{
		Object* mill = ctx.requireObject();
		if (!mill)
			return;
		ExperienceTracker* experience = mill->getExperienceTracker();
		if (ctx.action == "tree-depletion-prime")
			experience->addRetailExperience(static_cast<Real>(atoi(ctx.argument)) - experience->getRetailExperience(), FALSE);
		std::fprintf(stderr, "TEST tree-depletion mill frame=%u rank=%d experience=%.0f\n", ctx.frame,
			experience->getRank(), experience->getRetailExperience());
		return;
	}
	Coord3D tree;
	const Bool listed = TheTerrainLogic->findNearestHarvestTree(ctx.position, SAME_TREE_RADIUS, {}, tree);
	if (ctx.action == "tree-depletion-drain")
	{
		if (listed)
			TheTerrainLogic->takeMapTreeBoxes(tree, TheTerrainLogic->getMapTreeBoxes(tree) - ctx.ordinal);
		return;
	}
	std::fprintf(stderr, "TEST tree-depletion frame=%u tree=%d boxes=%d toppled=%d\n", ctx.frame, atoi(ctx.argument),
		listed ? TheTerrainLogic->getMapTreeBoxes(tree) : 0, listed && TheTerrainLogic->isMapTreeToppled(tree));
}
}
#endif

namespace RetailTestCommands
{
void registerRetailTreeDepletionTestCommands()
{
#if !RTS_GENERALS
	registerPrefix("tree-depletion-", handleTreeDepletion);
#endif
}
}
