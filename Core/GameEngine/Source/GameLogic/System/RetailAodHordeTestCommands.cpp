#include "PreRTS.h"
#include "GameLogic/RetailTestCommands.h"

#if !RTS_GENERALS
#include <algorithm>
#include <cstdio>

#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object.h"
#include "GameLogic/TerrainLogic.h"
#include "WWMath/wwmath.h"

namespace
{
void handleAodHordeProbe(const RetailTestCommands::Context& ctx)
{
	const Object* anchor = ctx.requireObject();
	const HordeContain* horde = HordeContain::get(anchor);
	if (!anchor || !horde)
		return;

	const Coord3D& anchorPos = *anchor->getPosition();
	const Real anchorAngle = anchor->getOrientation();
	const Real cosAngle = WWMath::CosTrig(anchorAngle);
	const Real sinAngle = WWMath::SinTrig(anchorAngle);
	const auto& slots = horde->getActiveFormation()->m_layout.slots();

	unsigned int count = 0;
	Real minOff = 999999.0f;
	Real maxOff = 0.0f;
	Real minAlt = 999999.0f;
	Real maxAlt = -999999.0f;
	Coord3D firstPos{};
	Real firstRelY = 0.0f;
	Bool hasFirst = FALSE;
	ObjectID firstId = INVALID_ID;

	for (const Object* member : *horde->getContainedItemsList())
	{
		if (member->isEffectivelyDead())
			continue;
		const Int slot = horde->getMemberSlot(member->getID());
		if (slot < 0 || static_cast<std::size_t>(slot) >= slots.size())
			continue;

		const HordePoint& pt = slots[slot].position;
		const Real baseX = anchorPos.x + pt.x * cosAngle - pt.y * sinAngle;
		const Real baseY = anchorPos.y + pt.x * sinAngle + pt.y * cosAngle;
		const Coord3D& pos = *member->getPosition();
		const Real dx = pos.x - baseX;
		const Real dy = pos.y - baseY;
		const Real off = WWMath::SqrtfOrigin(dx * dx + dy * dy);
		const Real ground = TheTerrainLogic ? TheTerrainLogic->getLayerHeight(pos.x, pos.y, member->getLayer()) : 0.0f;
		const Real alt = pos.z - ground;
		const Real relY = -(pos.x - anchorPos.x) * sinAngle + (pos.y - anchorPos.y) * cosAngle - pt.y;

		++count;
		minOff = std::min(minOff, off);
		maxOff = std::max(maxOff, off);
		minAlt = std::min(minAlt, alt);
		maxAlt = std::max(maxAlt, alt);
		if (!hasFirst || member->getID() < firstId)
		{
			hasFirst = TRUE;
			firstId = member->getID();
			firstPos = pos;
			firstRelY = relY;
		}
	}

	if (count == 0)
		minOff = minAlt = maxAlt = 0.0f;

	std::fprintf(stderr, "TEST aod-horde probe frame=%u label=%s count=%u ax=%.2f ay=%.2f az=%.2f minOff=%.4f maxOff=%.4f minAlt=%.4f maxAlt=%.4f m0x=%.4f m0y=%.4f m0z=%.4f m0ry=%.4f\n",
		ctx.frame, ctx.argument && *ctx.argument ? ctx.argument : "-", count,
		anchorPos.x, anchorPos.y, anchorPos.z,
		minOff, maxOff, minAlt, maxAlt,
		firstPos.x, firstPos.y, firstPos.z, firstRelY);
}
}
#endif

namespace RetailTestCommands
{
void registerRetailAodHordeTestCommands()
{
#if !RTS_GENERALS
	registerAction("aod-horde-probe", handleAodHordeProbe);
#endif
}
}
