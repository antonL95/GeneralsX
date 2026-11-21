#include "PreRTS.h"

#include "GameLogic/Module/ProductionSpeedBonus.h"

#include <algorithm>
#include <cmath>

#include "Common/INI.h"
#include "Common/INIException.h"
#include "Common/Player.h"
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

void ProductionSpeedBonusModuleData::buildFieldParse(MultiIniFieldParse& fields)
{
	SpecialPowerModuleData::buildFieldParse(fields);
	static const FieldParse table[] =
	{
		{ "NumberOfFrames", INI::parseUnsignedInt, nullptr, offsetof(ProductionSpeedBonusModuleData, m_numberOfFrames) },
		{ "SpeedMulitplier", INI::parseReal, nullptr, offsetof(ProductionSpeedBonusModuleData, m_speedMultiplier) },
		{ "Type", INI::parseAsciiStringVector, nullptr, offsetof(ProductionSpeedBonusModuleData, m_types) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	fields.add(table);
}

ModuleData* ProductionSpeedBonus::friend_newModuleData(INI* ini)
{
	auto data = std::unique_ptr<ProductionSpeedBonusModuleData>(MSGNEW("AllModuleData") ProductionSpeedBonusModuleData);
	if (ini)
	{
		ini->initFromINIMultiProc(data.get(), ProductionSpeedBonusModuleData::buildFieldParse);
		if (!data->m_specialPowerTemplate)
			throw INIException("ProductionSpeedBonus requires a defined SpecialPowerTemplate");
	}
	return data.release();
}

ProductionSpeedBonus::~ProductionSpeedBonus() = default;

void ProductionSpeedBonus::applyPowerEffect(const Coord3D*)
{
	const auto* data = static_cast<const ProductionSpeedBonusModuleData*>(getModuleData());
	Player* player = getObject()->getControllingPlayer();
	if (player && data->m_numberOfFrames > 0 && data->m_speedMultiplier > 0.0f && std::isfinite(data->m_speedMultiplier))
		player->setPowerProductionBonus(data->m_types, data->m_speedMultiplier, data->m_numberOfFrames);
}

void Player::setPowerProductionBonus(const std::vector<AsciiString>& types, Real speed, UnsignedInt duration)
{
	const UnsignedInt now = TheGameLogic->getFrame();
	m_powerProductionBonuses.erase(std::remove_if(m_powerProductionBonuses.begin(), m_powerProductionBonuses.end(),
		[now](const PowerProductionBonus& bonus) { return bonus.endFrame <= now; }), m_powerProductionBonuses.end());
	for (const AsciiString& type : types)
	{
		const auto match = std::find_if(m_powerProductionBonuses.begin(), m_powerProductionBonuses.end(),
			[&type](const PowerProductionBonus& bonus) { return stricmp(type.str(), bonus.type.str()) == 0; });
		const PowerProductionBonus effect{type, speed, now + duration};
		if (match == m_powerProductionBonuses.end())
			m_powerProductionBonuses.push_back(effect);
		else
			*match = effect;
	}
}

Real Player::getPowerProductionSpeed(const AsciiString& type) const
{
	const UnsignedInt now = TheGameLogic->getFrame();
	for (const PowerProductionBonus& bonus : m_powerProductionBonuses)
	{
		if (bonus.endFrame > now && stricmp(type.str(), bonus.type.str()) == 0)
			return bonus.speed;
	}
	return 1.0f;
}

void Player::xferPowerProductionBonuses(Xfer* xfer)
{
	UnsignedInt count = m_powerProductionBonuses.size();
	xfer->xferUnsignedInt(&count);
	if (xfer->getXferMode() == XFER_LOAD)
		m_powerProductionBonuses.resize(count);
	for (PowerProductionBonus& bonus : m_powerProductionBonuses)
	{
		xfer->xferAsciiString(&bonus.type);
		xfer->xferReal(&bonus.speed);
		xfer->xferUnsignedInt(&bonus.endFrame);
	}
}
