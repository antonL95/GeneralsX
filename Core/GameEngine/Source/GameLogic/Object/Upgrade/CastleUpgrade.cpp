#include "PreRTS.h"

#include "Common/Player.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/CastleBehavior.h"
#include "GameLogic/Module/CastleUpgrade.h"

void CastleUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "Upgrade", INI::parseAsciiString, nullptr, offsetof(CastleUpgradeModuleData, m_upgrade) },
		{ "WallUpgradeRadius", INI::parseReal, nullptr, offsetof(CastleUpgradeModuleData, m_wallUpgradeRadius) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

CastleUpgrade::~CastleUpgrade() = default;

void CastleUpgrade::upgradeImplementation()
{
	const auto* data = getCastleUpgradeModuleData();
	const UpgradeTemplate* upgrade = getGrantedUpgrade();
	if (!upgrade)
		return;

	Object* source = getObject();
	setUpgradeExecuted(TRUE);

	if (CastleBehavior* castle = CastleBehavior::findForMember(*source))
		castle->grantMemberUpgrade(*source, *upgrade);

	if (!source->hasUpgrade(upgrade))
		source->giveUpgrade(upgrade);

	if (data->m_wallUpgradeRadius <= 0.0f)
		return;

	const Coord3D* center = source->getPosition();
	const Real radiusSquared = data->m_wallUpgradeRadius * data->m_wallUpgradeRadius;
	for (Object* wall = TheGameLogic->getFirstObject(); wall; wall = wall->getNextObject())
	{
		if (!(wall->isKindOf(KINDOF_WALL_SEGMENT) || wall->isKindOf(KINDOF_WALL_HUB) ||
			wall->isKindOf(KINDOF_WALL_GATE) || wall->isKindOf(KINDOF_WALL_UPGRADE)) || wall->isEffectivelyDead() ||
			wall->getControllingPlayer() != source->getControllingPlayer() ||
			wall->hasUpgrade(upgrade) || !wall->affectedByUpgrade(upgrade))
			continue;
		const Coord3D* pos = wall->getPosition();
		const Real dx = center->x - pos->x;
		const Real dy = center->y - pos->y;
		if (dx * dx + dy * dy <= radiusSquared)
			wall->giveUpgrade(upgrade);
	}
}

const UpgradeTemplate* CastleUpgrade::getGrantedUpgrade() const
{
	return TheUpgradeCenter->findUpgrade(getCastleUpgradeModuleData()->m_upgrade);
}

void CastleUpgrade::crc(Xfer* xfer)
{
	UpgradeModule::crc(xfer);
}

void CastleUpgrade::xfer(Xfer* xfer)
{
	UpgradeModule::xfer(xfer);
}

void CastleUpgrade::loadPostProcess()
{
	UpgradeModule::loadPostProcess();
}
