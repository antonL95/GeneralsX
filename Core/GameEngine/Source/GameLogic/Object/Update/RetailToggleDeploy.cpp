#include "PreRTS.h"
#include "GameLogic/Module/RetailToggleDeploy.h"

#include "Common/AudioEventRTS.h"
#include "Common/GameAudio.h"
#include "Common/RetailFileLayers.h"
#include "GameLogic/Module/DeployStyleAIUpdate.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"

void ToggleDeploySpecialAbilityUpdateModuleData::buildFieldParse(MultiIniFieldParse& fields)
{
	SpecialAbilityUpdateModuleData::buildFieldParse(fields);
	static const FieldParse table[] = {
		{ "SoundDeploy", INI::parseAsciiString, nullptr, offsetof(ToggleDeploySpecialAbilityUpdateModuleData, m_soundDeploy) },
		{ "SoundUndeploy", INI::parseAsciiString, nullptr, offsetof(ToggleDeploySpecialAbilityUpdateModuleData, m_soundUndeploy) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	fields.add(table);
}

ToggleDeploySpecialAbilityUpdate::ToggleDeploySpecialAbilityUpdate(Thing* thing, const ModuleData* data)
	: SpecialAbilityUpdate(thing, data)
{
}

ToggleDeploySpecialAbilityUpdate::~ToggleDeploySpecialAbilityUpdate() = default;

Bool ToggleDeploySpecialAbilityUpdate::initiateIntentToDoSpecialPower(const SpecialPowerTemplate* power,
	const Object*, const Coord3D*, const Waypoint*, UnsignedInt)
{
	if (!RetailFileLayers::enabled() || !power || power != getTemplate())
		return FALSE;

	Object* self = getObject();
	auto* ai = static_cast<DeployStyleAIUpdate*>(self->findUpdateModule(NAMEKEY("DeployStyleAIUpdate")));
	if (!ai || self->isEffectivelyDead())
		return FALSE;
	if (ai->isDeployingOrPacking())
		return FALSE;

	const Bool deployed = self->testStatus(OBJECT_STATUS_DEPLOYED);
	ai->aiIdle(CMD_FROM_AI);
	ai->setMyState(deployed ? UNDEPLOY : DEPLOY);
	const auto* data = getToggleDeploySpecialAbilityUpdateModuleData();
	const AsciiString& sound = deployed ? data->m_soundUndeploy : data->m_soundDeploy;
	if (!sound.isEmpty())
	{
		AudioEventRTS event(sound);
		event.setObjectID(self->getID());
		TheAudio->addAudioEvent(&event);
	}
	if (auto* module = getMySPM())
		module->markSpecialPowerTriggered(nullptr);
	return TRUE;
}

void ToggleDeploySpecialAbilityUpdate::crc(Xfer* xfer)
{
	SpecialAbilityUpdate::crc(xfer);
}

void ToggleDeploySpecialAbilityUpdate::xfer(Xfer* xfer)
{
	SpecialAbilityUpdate::xfer(xfer);
}

void ToggleDeploySpecialAbilityUpdate::loadPostProcess()
{
	SpecialAbilityUpdate::loadPostProcess();
}
