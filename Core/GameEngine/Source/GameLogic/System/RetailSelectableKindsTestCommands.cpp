#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <cstdio>

#include "GameClient/Drawable.h"
#include "GameClient/SelectionInfo.h"
#include "GameClient/SelectionXlat.h"
#include "GameLogic/Object.h"

namespace
{
void probeSelection(const RetailTestCommands::Context& ctx)
{
	const Object* object = ctx.requireObject();
	if (!object || !object->getDrawable())
		return;

	const Drawable* draw = object->getDrawable();
	std::fprintf(stderr, "TEST selectable-kinds %u %s object=%d click=%d drag=%d\n",
		ctx.frame, ctx.targetName, object->isSelectable(), CanSelectDrawable(draw, FALSE),
		CanSelectDrawable(draw, TRUE));
}

void probeSelected(const RetailTestCommands::Context& ctx)
{
	if (const Object* object = ctx.requireObject())
		std::fprintf(stderr, "TEST selectable-kinds %u %s selected=%d\n", ctx.frame, ctx.targetName,
			object->getDrawable() && object->getDrawable()->isSelected());
}

void dragSelection(const RetailTestCommands::Context& ctx)
{
	Object* object = ctx.requireObject();
	if (!object || !object->getDrawable())
		return;

	DrawableList candidates;
	PickDrawableStruct selection;
	selection.drawableListToFill = &candidates;
	Drawable* draw = object->getDrawable();
	const Bool wasObscured = draw->getFullyObscuredByShroud();
	draw->setFullyObscuredByShroud(FALSE);
	if (addDrawableToList(draw, &selection))
		ctx.selectObject(candidates.front()->getObject());
	draw->setFullyObscuredByShroud(wasObscured);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailSelectableKindsTestCommands()
{
#if !RTS_GENERALS
	registerAction("selectable-kinds-probe", probeSelection);
	registerAction("selectable-kinds-selected", probeSelected);
	registerAction("selectable-kinds-drag", dragSelection);
#endif
}
}
