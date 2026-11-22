#pragma once

#include "GameLogic/Module/DumbProjectileBehavior.h"

class FXList;
class WeaponTemplate;

class BezierProjectileBehaviorModuleData : public DumbProjectileBehaviorModuleData
{
public:
	const FXList* m_groundHitFX = nullptr;
	const FXList* m_groundBounceFX = nullptr;
	const WeaponTemplate* m_groundHitWeapon = nullptr;
	const WeaponTemplate* m_groundBounceWeapon = nullptr;
	Int m_bounceCount = 0;
	Real m_bounceDistance = 0.0f;
	Real m_bounceFirstHeight = 0.0f;
	Real m_bounceSecondHeight = 0.0f;
	Real m_bounceFirstPercentIndent = 0.0f;
	Real m_bounceSecondPercentIndent = 0.0f;
	Bool m_crushStyle = FALSE;
	Bool m_dieOnImpact = FALSE;
	Bool m_ignoreTerrainHeight = FALSE;
	Real m_curveFlattenMinDist = 0.0f;
	UnsignedInt m_preLandingStateTime = 0;
	AsciiString m_preLandingEmotion;
	Real m_preLandingEmotionRadius = 0.0f;
	Int m_invisibleFrames = 0;
	UnsignedInt m_finalStuckTime = 0;
	UnsignedInt m_fadeInTime = 0;
	Real m_firstPercentHeight = 0.0f;
	Real m_secondPercentHeight = 0.0f;

	static void buildFieldParse(MultiIniFieldParse& p);
};

class BezierProjectileBehavior : public DumbProjectileBehavior
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( BezierProjectileBehavior, "BezierProjectileBehavior" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( BezierProjectileBehavior, BezierProjectileBehaviorModuleData )

public:
	BezierProjectileBehavior(Thing* thing, const ModuleData* moduleData) : DumbProjectileBehavior(thing, moduleData) {}

	virtual UpdateSleepTime update() override;
	virtual void projectileLaunchAtObjectOrPosition(const Object* victim, const Coord3D* victimPos, const Object* launcher,
		WeaponSlotType wslot, Int specificBarrelToUse, const WeaponTemplate* detWeap, const ParticleSystemTemplate* exhaustSysOverride) override;

protected:
	Bool killsPassengerOnImpact() const override { return getBezierProjectileBehaviorModuleData()->m_dieOnImpact; }
};
