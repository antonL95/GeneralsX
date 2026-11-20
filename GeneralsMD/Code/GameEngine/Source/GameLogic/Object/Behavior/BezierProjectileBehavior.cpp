#include "PreRTS.h"

#include "GameLogic/Module/BezierProjectileBehavior.h"

#include "Common/INI.h"
#include "Common/Xfer.h"

void BezierProjectileBehaviorModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	DumbProjectileBehaviorModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "GroundHitFX", INI::parseFXList, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_groundHitFX) },
		{ "GroundBounceFX", INI::parseFXList, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_groundBounceFX) },
		{ "GroundHitWeapon", INI::parseWeaponTemplate, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_groundHitWeapon) },
		{ "GroundBounceWeapon", INI::parseWeaponTemplate, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_groundBounceWeapon) },
		{ "BounceCount", INI::parseInt, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_bounceCount) },
		{ "BounceDistance", INI::parseReal, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_bounceDistance) },
		{ "BounceFirstHeight", INI::parseReal, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_bounceFirstHeight) },
		{ "BounceSecondHeight", INI::parseReal, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_bounceSecondHeight) },
		{ "BounceFirstPercentIndent", INI::parsePercentToReal, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_bounceFirstPercentIndent) },
		{ "BounceSecondPercentIndent", INI::parsePercentToReal, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_bounceSecondPercentIndent) },
		{ "CrushStyle", INI::parseBool, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_crushStyle) },
		{ "DieOnImpact", INI::parseBool, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_dieOnImpact) },
		{ "IgnoreTerrainHeight", INI::parseBool, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_ignoreTerrainHeight) },
		{ "CurveFlattenMinDist", INI::parseReal, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_curveFlattenMinDist) },
		{ "PreLandingStateTime", INI::parseDurationUnsignedInt, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_preLandingStateTime) },
		{ "PreLandingEmotion", INI::parseAsciiString, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_preLandingEmotion) },
		{ "PreLandingEmotionRadius", INI::parseReal, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_preLandingEmotionRadius) },
		{ "InvisibleFrames", INI::parseInt, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_invisibleFrames) },
		{ "FinalStuckTime", INI::parseDurationUnsignedInt, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_finalStuckTime) },
		{ "FadeInTime", INI::parseDurationUnsignedInt, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_fadeInTime) },
		{ "FirstPercentHeight", INI::parsePercentToReal, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_firstPercentHeight) },
		{ "SecondPercentHeight", INI::parsePercentToReal, nullptr, offsetof(BezierProjectileBehaviorModuleData, m_secondPercentHeight) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
}

BezierProjectileBehavior::~BezierProjectileBehavior() = default;

void BezierProjectileBehavior::crc(Xfer* xfer)
{
	DumbProjectileBehavior::crc(xfer);
}

void BezierProjectileBehavior::xfer(Xfer* xfer)
{
	DumbProjectileBehavior::xfer(xfer);
}

void BezierProjectileBehavior::loadPostProcess()
{
	DumbProjectileBehavior::loadPostProcess();
}

UpdateSleepTime BezierProjectileBehavior::update()
{
	if (!hasLaunched())
		return UPDATE_SLEEP_FOREVER;
	return DumbProjectileBehavior::update();
}

void BezierProjectileBehavior::projectileLaunchAtObjectOrPosition(const Object* victim, const Coord3D* victimPos,
	const Object* launcher, WeaponSlotType wslot, Int specificBarrelToUse, const WeaponTemplate* detWeap,
	const ParticleSystemTemplate* exhaustSysOverride)
{
	DumbProjectileBehavior::projectileLaunchAtObjectOrPosition(victim, victimPos, launcher, wslot, specificBarrelToUse,
		detWeap, exhaustSysOverride);
	setWakeFrame(getObject(), UPDATE_SLEEP_NONE);
}
