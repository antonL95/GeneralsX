/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: Locomotor.cpp ///////////////////////////////////////////////////////////////////////////////
// Author: Steven Johnson, Feb 2002
// Desc:   Locomotor descriptions
///////////////////////////////////////////////////////////////////////////////////////////////////


// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include <climits>	// For INT_MAX on Linux
#include <algorithm>
#define DEFINE_SURFACECATEGORY_NAMES
#define DEFINE_LOCO_Z_NAMES
#define DEFINE_LOCO_APPEARANCE_NAMES

#include "Common/INI.h"
#include "Common/INIAudit.h"
#include "Common/RetailFileLayers.h"
#include "Common/RotwkDuration.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Object.h"
#include "GameLogic/Module/AttributeModifierHelper.h"
#include "GameLogic/AI.h"
#include "GameLogic/AIPathfind.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/HordeAIUpdate.h"
#include "GameLogic/Module/RotwkAbilityModules.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Module/RetailSiegeContain.h"


static const Real DONUT_TIME_DELAY_SECONDS=2.5f;
static const Real DONUT_DISTANCE=4.0*PATHFIND_CELL_SIZE_F;


#define MAX_BRAKING_FACTOR 5.0f
///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC DATA ////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
LocomotorStore *TheLocomotorStore = nullptr;					///< the Locomotor store definition

const Real BIGNUM = 99999.0f;

static const char *const TheLocomotorPriorityNames[] =
{
	"MOVES_BACK",
	"MOVES_MIDDLE",
	"MOVES_FRONT",

	nullptr
};
static_assert(ARRAY_SIZE(TheLocomotorPriorityNames) == LOCOMOTOR_PRIORITY_COUNT + 1, "Array size");

static const char *const TheFormationPriorityNames[] =
{
	"NO_FORMATION",
	"CAVALRY1",
	"CAVALRY2",
	"CAVALRY3",
	"MELEE1",
	"MELEE2",
	"MELEE3",
	"RANGED1",
	"RANGED2",
	"RANGED3",
	"ARTILLERY1",
	"ARTILLERY2",
	"ARTILLERY3",
	"UNUSED",
	nullptr
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE DATA ///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#if USE_RETAIL_PHYSICS_FORWARD_SPEED_AVERAGE()

// TheSuperHackers @bugfix xezon 30/07/2026 The compensation that equalizes straight and diagonal
// movement speeds at their harmonic mean.
//
// Retail measured forward speed as sqrt(sum of (vi * di)^2) rather than the true projection of the
// velocity onto the heading, sum of (vi * di). For a unit heading d that understates the speed by
// sqrt(sum of di^4), which is 1 on an axis aligned heading and falls to 1/sqrt(2) on a 2d diagonal
// and 1/sqrt(3) on a 3d body diagonal. The 2d movers compare that measurement directly against
// their goal speed, so they kept accelerating until the real speed was between 1x and sqrt(2)x the
// authored speed, decided by nothing but which way the object happened to face.
//
// getForwardSpeed2D and getForwardSpeed3D now report the true projection, so the real speed equals
// the commanded speed on every heading. The speeds commanded by the 2d movers are scaled by a single
// constant, which picks where inside the old range that now uniform speed sits.
//
// The constant is the harmonic mean of the two ends of the old range, 1 and sqrt(2). Axis aligned
// headings become exactly as much faster as diagonal headings become slower, 17.16% each, so no
// heading changes its speed by more than that, whatever mix of headings a map produces.
//
// Averaging the old factor over all headings instead would preserve the average pace of the game,
// but only for an even mix of headings, and the result depends on whether that mix is even over
// distance travelled (harmonic mean, 1.1630) or over time spent (arithmetic mean, 1.1803). The
// constant sits between the two, within 0.75% of both. The midpoint of the old range, 1.2071, lies
// above all of them and would quietly speed the whole game up.
//
// THRUST, the only mover that measures itself with getForwardSpeed3D, is deliberately not scaled.
// Its thrust is damped by the velocity times acceleration over max speed, which cancels the thrust
// at exactly the max speed on every heading, so the understated measurement never raised its speed.
// Scaling it would make missiles faster than retail.

constexpr const Real DiagonalCompensation2D = 1.17157288f; // 2*sqrt(2) / (1 + sqrt(2)) = 4 - 2*sqrt(2)

#endif

//-------------------------------------------------------------------------------------------------
static Real calcSlowDownDist(Real curSpeed, Real desiredSpeed, Real maxBraking)
{
	Real delta = curSpeed - desiredSpeed;
	if (delta <= 0)
		return 0.0f;

	Real dist = (sqr(delta) / fabs(maxBraking)) * 0.5f;

	// use a little fudge so that things can stop "on a dime" more easily...
	const Real FUDGE = 1.05f;
	return dist * FUDGE;
}

//-----------------------------------------------------------------------------
inline Bool isNearlyZero(Real a)
{
	const Real TINY_EPSILON = 0.001f;
	return fabs(a) < TINY_EPSILON;
}

//-----------------------------------------------------------------------------
inline Bool isNearly(Real a, Real val)
{
	const Real TINY_EPSILON = 0.001f;
	return fabs(a - val) < TINY_EPSILON;
}

//-----------------------------------------------------------------------------
// return the angle delta (in 3-space) we turned.
static Real tryToRotateVector3D(
	Real maxAngle,						// if negative, it's a percent (0...1) of the dist to rotate 'em
	const Vector3& inCurDir,
	const Vector3& inGoalDir,
	Vector3& actualDir
)
{
	if (isNearlyZero(maxAngle))
	{
		actualDir = inCurDir;
		return 0.0f;
	}

	Vector3 curDir = inCurDir;
	curDir.Normalize();

	Vector3 goalDir = inGoalDir;
	goalDir.Normalize();

	// dot of two unit vectors is cos of angle between them.
	Real cosine = Vector3::Dot_Product(curDir, goalDir);
	// bound it in case of numerical error
	Real angleBetween = (Real)ACos(clamp(-1.0f, cosine, 1.0f));

	if (maxAngle < 0)
	{
		maxAngle = -maxAngle * angleBetween;
		if (isNearlyZero(maxAngle))
		{
			actualDir = inCurDir;
			return 0.0f;
		}
	}

	if (fabs(angleBetween) <= maxAngle)
	{
		// close enough
		actualDir = goalDir;
	}
	else
	{
		// nah, try as much as we can in the right dir.
		// we need to rotate around the axis perpendicular to these two vecs.
		// but: cross of two vectors is the perpendicular axis!
#ifdef ALLOW_TEMPORARIES
		Vector3 objCrossGoal = Vector3::Cross_Product(curDir, goalDir);
		objCrossGoal.Normalize();
#else
		Vector3 objCrossGoal;
		Vector3::Normalized_Cross_Product(curDir, goalDir, &objCrossGoal);
#endif

		angleBetween = maxAngle;
		Matrix3D rotMtx(objCrossGoal, angleBetween);
		actualDir = rotMtx.Rotate_Vector(curDir);
	}

	return angleBetween;
}

//-------------------------------------------------------------------------------------------------
static Real tryToOrientInThisDirection3D(Object* obj, Real maxTurnRate, const Vector3& desiredDir)
{
	Vector3 actualDir;
	Real relAngle = tryToRotateVector3D(maxTurnRate, obj->getTransformMatrix()->Get_X_Vector(), desiredDir, actualDir);
	if (relAngle != 0.0f)
	{
		Vector3 objPos(obj->getPosition()->x, obj->getPosition()->y, obj->getPosition()->z);

		Matrix3D newXform;
		newXform.buildTransformMatrix( objPos, actualDir );

		obj->setTransformMatrix( &newXform );
	}
	return relAngle;
}

//-------------------------------------------------------------------------------------------------
inline Real tryToOrientInThisDirection3D(Object* obj, Real maxTurnRate, const Coord3D* dir)
{
	return tryToOrientInThisDirection3D(obj, maxTurnRate, Vector3(dir->x, dir->y, dir->z));
}

//-----------------------------------------------------------------------------
static void calcDirectionToApplyThrust(
	const Object* obj,
	const PhysicsBehavior* physics,
	const Coord3D& ingoalPos,
	Real maxAccel,
	Vector3& goalDir
)
{
	/*
		our meta-goal here is to calculate the direction we should apply our motive force
		in order to minimize the angle between (our velocity) and (direction towards goalpos).

		this is complicated by the fact that we generally have an intrinsic velocity already,
		that must be accounted for, and by the fact that we can only apply force in our
		forward-x-direction (with a thrust-angle-range), and (due to limited range) might not
		be able to apply the force in the optimal direction!
	*/

	// convert to Vector3, to use all its handy stuff
	Vector3 objPos(obj->getPosition()->x, obj->getPosition()->y, obj->getPosition()->z);
	Vector3 goalPos(ingoalPos.x, ingoalPos.y, ingoalPos.z);

	Vector3 vecToGoal = goalPos - objPos;
	if (isNearlyZero(vecToGoal.Length2()))
	{
		// goal pos is essentially same as current pos, so just stay the same & return
		goalDir = obj->getTransformMatrix()->Get_X_Vector();
		return;
	}

	/*
		get our cur vel into a useful Vector3 form
	*/
	Vector3 curVel(physics->getVelocity()->x, physics->getVelocity()->y, physics->getVelocity()->z);

	// add gravity to our vel so that we account for it in our calcs
	curVel.Z += TheGlobalData->m_gravity;

	Bool foundSolution = false;
	Real distToGoalSqr = vecToGoal.Length2();
	Real distToGoal = WWMath::SqrtOrigin(distToGoalSqr);
	Real curVelMagSqr = curVel.Length2();
	Real curVelMag = WWMath::SqrtOrigin(curVelMagSqr);
	Real maxAccelSqr = sqr(maxAccel);

	Real denom = curVelMagSqr - maxAccelSqr;
	if (!isNearlyZero(denom))
	{
		// solve the (greatly simplified) quadratic...
		Real t = (distToGoal * (curVelMag + maxAccel)) / denom;
		Real t2 = (distToGoal * (curVelMag - maxAccel)) / denom;
		if (t >= 0 || t2 >= 0)
		{
			// choose the smallest positive t.
			if (t < 0 || (t2 >= 0 && t2 < t))
				t = t2;

			// plug it in.
			if (!isNearlyZero(t))
			{
				goalDir.X = (vecToGoal.X / t) - curVel.X;
				goalDir.Y = (vecToGoal.Y / t) - curVel.Y;
				goalDir.Z = (vecToGoal.Z / t) - curVel.Z;
				goalDir.Normalize();
				foundSolution = true;
			}
		}
	}
	if (!foundSolution)
	{
		// Doh... no (useful) solution. revert to dumb.
		goalDir = vecToGoal;
		goalDir.Normalize();
	}

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
LocomotorTemplate::LocomotorTemplate()
{
	// these values mean "make the same as undamaged if not explicitly specified"
	m_maxSpeedDamaged = -1.0f;
	m_maxTurnRateDamaged = -1.0f;
	m_accelerationDamaged = -1.0f;
	m_liftDamaged = -1.0f;

	m_surfaces = 0;
	m_maxSpeed = 0.0f;
	m_maxTurnRate = 0.0f;
	m_acceleration = 0.0f;
	m_lift = 0.0f;
	m_braking = BIGNUM;
	m_minSpeed = 0.0f;
	m_minTurnSpeed = BIGNUM;
#if USE_RETAIL_PHYSICS_FORWARD_SPEED_AVERAGE()
	m_speedScale = DiagonalCompensation2D;
#endif
	m_behaviorZ = Z_NO_Z_MOTIVE_FORCE;
	m_appearance = LOCO_OTHER;
	m_movePriority = LOCO_MOVES_MIDDLE;
	m_preferredHeight = 0;
	m_preferredHeightDamping = 1.0f;
	m_circlingRadius = 0;

	m_maxThrustAngle = 0;
	m_speedLimitZ = 999999.0f;
	m_extra2DFriction = 0.0f;

	m_accelPitchLimit = 0;
	m_decelPitchLimit = 0;
	m_bounceKick = 0;

//	m_pitchStiffness = 0;
//	m_rollStiffness = 0;
//	m_pitchDamping = 0;
//	m_rollDamping = 0;
// it's highly unlikely you want zero for the defaults for stiffness and damping... (srj)
// for stiffness: stiffness of the "springs" in the suspension 0 = no stiffness, 1 = totally stiff (huh huh, he said "stiff")
// for damping: 0=perfect spring, bounces forever.  1=glued to terrain.
	m_pitchStiffness = 0.1f;
	m_rollStiffness = 0.1f;
	m_pitchDamping = 0.9f;
	m_rollDamping = 0.9f;
	m_forwardVelCoef = 0;
	m_pitchByZVelCoef = 0;
	m_thrustRoll = 0.0f;
	m_wobbleRate = 0.0f;
	m_minWobble = 0.0f;
	m_maxWobble = 0.0f;
	m_lateralVelCoef = 0;
	m_forwardAccelCoef = 0;
	m_lateralAccelCoef = 0;
	m_uniformAxialDamping = 1.0f;
	m_turnPivotOffset = 0;
	m_apply2DFrictionWhenAirborne = false;
	m_downhillOnly = false;
	m_allowMotiveForceWhileAirborne = false;
	m_locomotorWorksWhenDead = false;
	m_airborneTargetingHeight = INT_MAX;
	m_stickToGround = false;
	m_canMoveBackward = false;
	m_hasSuspension = false;
	m_wheelTurnAngle = 0;
	m_maximumWheelExtension = 0;
	m_maximumWheelCompression = 0;
	m_closeEnoughDist = 1.0f;
	m_isCloseEnoughDist3D = FALSE;
	m_ultraAccurateSlideIntoPlaceFactor = 0.0f;

	m_wanderWidthFactor = 0.0f;
	m_wanderLengthFactor = 1.0f;
	m_wanderAboutPointRadius = 0.0f;

	m_rudderCorrectionDegree    = 0.0f;
	m_rudderCorrectionRate      = 0.0f;
	m_elevatorCorrectionDegree  = 0.0f;
	m_elevatorCorrectionRate    = 0.0f;

	m_rotwk = RotwkLocomotorRules::Authored();
	m_waitForFormation = FALSE;
	m_maxTurnWithoutReform = 2.0f * PI;
	m_turnWhileMoving = TRUE;

}

//-------------------------------------------------------------------------------------------------
LocomotorTemplate::~LocomotorTemplate()
{

}

//-------------------------------------------------------------------------------------------------
void LocomotorTemplate::validate()
{
	// this is ok; parachutes need it!
	//DEBUG_ASSERTCRASH(m_lift == 0.0f || m_lift > fabs(TheGlobalData->m_gravity), ("Lift is too low to counteract gravity!"));
	//DEBUG_ASSERTCRASH(m_liftDamaged == 0.0f || m_liftDamaged > fabs(TheGlobalData->m_gravity), ("LiftDamaged is too low to counteract gravity!"));
	//DEBUG_ASSERTCRASH(m_preferredHeight == 0.0f || (m_behaviorZ == Z_SURFACE_RELATIVE_HEIGHT || m_behaviorZ == Z_ABSOLUTE_HEIGHT || m_appearance == LOCO_THRUST),
	//	("You must use Z_SURFACE_RELATIVE_HEIGHT or Z_ABSOLUTE_HEIGHT (or THRUST) to use preferredHeight"));

	// for 'damaged' stuff that was omitted, set 'em to be the same as 'undamaged'...
	if (m_maxSpeedDamaged < 0.0f)
		m_maxSpeedDamaged = m_maxSpeed;

	if (m_maxTurnRateDamaged < 0.0f)
		m_maxTurnRateDamaged = m_maxTurnRate;

	if (m_accelerationDamaged < 0.0f)
		m_accelerationDamaged = m_acceleration;

	if (m_liftDamaged < 0.0f)
		m_liftDamaged = m_lift;

	if (m_appearance == LOCO_WINGS)
	{
		if (m_minSpeed <= 0.0f)
		{
			DEBUG_CRASH(("WINGS should always have positive minSpeeds (otherwise, they hover)"));
			m_minSpeed = 0.01f;
		}
		if (m_minTurnSpeed <= 0.0f)
		{
			DEBUG_CRASH(("WINGS should always have positive minTurnSpeed"));
			m_minTurnSpeed = 0.01f;
		}
	}

	if (m_appearance == LOCO_THRUST)
	{
		if (m_behaviorZ != Z_NO_Z_MOTIVE_FORCE ||
				m_lift != 0.0f ||
				m_liftDamaged != 0.0f)
		{
			DEBUG_CRASH(("THRUST locos may not use ZAxisBehavior or lift!"));
			throw INI_INVALID_DATA;
		}
		if (m_maxSpeed <= 0.0f)
		{
			// if one of these was omitted, it defaults to zero... just quietly heal it here, rather than crashing
			DEBUG_LOG(("THRUST locos may not have zero m_maxSpeed; healing..."));
			m_maxSpeed = 0.01f;
		}
		if (m_maxSpeedDamaged <= 0.0f)
		{
			// if one of these was omitted, it defaults to zero... just quietly heal it here, rather than crashing
			DEBUG_LOG(("THRUST locos may not have zero m_maxSpeedDamaged; healing..."));
			m_maxSpeedDamaged = 0.01f;
		}
		if (m_minSpeed <= 0.0f)
		{
			// if one of these was omitted, it defaults to zero... just quietly heal it here, rather than crashing
			DEBUG_LOG(("THRUST locos may not have zero m_minSpeed; healing..."));
			m_minSpeed = 0.01f;
		}
	}

#if RTS_GENERALS
	// TheSuperHackers @info DecelerationPitchLimit was just added in Zero Hour and is therefore defaulted to
	// AccelerationPitchLimit when zero to preserve the original behavior of Generals.
	if (m_decelPitchLimit == 0.0f)
		m_decelPitchLimit = m_accelPitchLimit;
#endif

#if USE_RETAIL_PHYSICS_FORWARD_SPEED_AVERAGE()
	// TheSuperHackers @info THRUST is not scaled, see DiagonalCompensation2D.
	m_speedScale = (m_appearance == LOCO_THRUST) ? 1.0f : DiagonalCompensation2D;
#endif
}

//-------------------------------------------------------------------------------------------------
Real LocomotorTemplate::getActualMaxSpeed() const
{
	return scaleSpeed(m_maxSpeed);
}

//-------------------------------------------------------------------------------------------------
Real LocomotorTemplate::getActualMaxSpeedDamaged() const
{
	return scaleSpeed(m_maxSpeedDamaged);
}

//-------------------------------------------------------------------------------------------------
Real LocomotorTemplate::getActualMinSpeed() const
{
	return scaleSpeed(m_minSpeed);
}

//-------------------------------------------------------------------------------------------------
Real LocomotorTemplate::getActualMinTurnSpeed() const
{
	return scaleSpeed(m_minTurnSpeed);
}

//-------------------------------------------------------------------------------------------------
Real LocomotorTemplate::scaleSpeed(Real speed) const
{
#if USE_RETAIL_PHYSICS_FORWARD_SPEED_AVERAGE()
	if (!PhysicsBehavior::useLegacyForwardSpeed())
		return speed * m_speedScale;
#endif
	return speed;
}

//-------------------------------------------------------------------------------------------------
static void parseFrictionPerSec( INI* ini, void * /*instance*/, void *store, const void* /*userData*/ )
{
	Real fricPerSec = INI::scanReal(ini->getNextToken());
	Real fricPerFrame = fricPerSec * SECONDS_PER_LOGICFRAME_REAL;
	*(Real *)store = fricPerFrame;
}

static Bool usesRotwkLocomotorFields()
{
	return RetailFileLayers::enabled() || INIAudit::isEnabled();
}

static void skipNonRotwkField(INI* ini)
{
	DEBUG_CRASH(("[LINE: %d - FILE: '%s'] Unknown field in Locomotor", ini->getLineNum(), ini->getFilename().str()));
	ini->getNextTokenOrNull();
}

template <INIFieldParseProc parse>
static void parseRotwkField(INI* ini, void* instance, void* store, const void* userData)
{
	if (usesRotwkLocomotorFields())
		parse(ini, instance, store, userData);
	else
		skipNonRotwkField(ini);
}

static UnsignedInt scanRotwkDurationFrames(INI* ini)
{
	UnsignedInt frames;
	INI::parseDurationUnsignedInt(ini, nullptr, &frames, nullptr);
	return frames;
}

void LocomotorTemplate::parseRotwkSpeedDamaged(INI* ini, void* instance, void* store, const void* userData)
{
	if (usesRotwkLocomotorFields())
		static_cast<LocomotorTemplate*>(instance)->m_rotwk.speedDamagedFraction = INI::scanPercentToReal(ini->getNextToken());
	else
		INI::parseVelocityReal(ini, instance, store, userData);
}

void LocomotorTemplate::parseRotwkMinSpeed(INI* ini, void* instance, void* store, const void* userData)
{
	if (usesRotwkLocomotorFields())
		static_cast<LocomotorTemplate*>(instance)->m_rotwk.minSpeedFraction = INI::scanPercentToReal(ini->getNextToken());
	else
		INI::parseVelocityReal(ini, instance, store, userData);
}

void LocomotorTemplate::parseRotwkMinTurnSpeed(INI* ini, void* instance, void* store, const void* userData)
{
	if (usesRotwkLocomotorFields())
		static_cast<LocomotorTemplate*>(instance)->m_rotwk.minTurnSpeedFraction = INI::scanPercentToReal(ini->getNextToken());
	else
		INI::parseVelocityReal(ini, instance, store, userData);
}

void LocomotorTemplate::parseRotwkLift(INI* ini, void* instance, void* store, const void* userData)
{
	if (usesRotwkLocomotorFields())
		static_cast<LocomotorTemplate*>(instance)->m_rotwk.liftFraction = INI::scanPercentToReal(ini->getNextToken());
	else
		INI::parseAccelerationReal(ini, instance, store, userData);
}

void LocomotorTemplate::parseRotwkLiftDamaged(INI* ini, void* instance, void* store, const void* userData)
{
	if (usesRotwkLocomotorFields())
		static_cast<LocomotorTemplate*>(instance)->m_rotwk.liftDamagedFraction = INI::scanPercentToReal(ini->getNextToken());
	else
		INI::parseAccelerationReal(ini, instance, store, userData);
}

void LocomotorTemplate::parseRotwkAcceleration(INI* ini, void* instance, void* store, const void* userData)
{
	if (usesRotwkLocomotorFields())
		static_cast<LocomotorTemplate*>(instance)->m_rotwk.accelerationFrames = scanRotwkDurationFrames(ini);
	else
		INI::parseAccelerationReal(ini, instance, store, userData);
}

void LocomotorTemplate::parseRotwkBraking(INI* ini, void* instance, void* store, const void* userData)
{
	if (usesRotwkLocomotorFields())
		static_cast<LocomotorTemplate*>(instance)->m_rotwk.brakingFrames = scanRotwkDurationFrames(ini);
	else
		INI::parseAccelerationReal(ini, instance, store, userData);
}

void LocomotorTemplate::parseRotwkTurnTime(INI* ini, void* instance, void* store, const void* userData)
{
	if (usesRotwkLocomotorFields())
		static_cast<LocomotorTemplate*>(instance)->m_rotwk.turnFrames = scanRotwkDurationFrames(ini);
	else
		skipNonRotwkField(ini);
}

void LocomotorTemplate::parseRotwkTurnTimeDamaged(INI* ini, void* instance, void* store, const void* userData)
{
	if (usesRotwkLocomotorFields())
		static_cast<LocomotorTemplate*>(instance)->m_rotwk.turnFramesDamaged = scanRotwkDurationFrames(ini);
	else
		skipNonRotwkField(ini);
}

void LocomotorTemplate::applyRotwkSetSpeed(Real speedPerSecond)
{
	const Real speed = speedPerSecond > 0.0f ? speedPerSecond * SECONDS_PER_LOGICFRAME_REAL : m_maxSpeed;
	RotwkLocomotorRules::Resolved current;
	current.maxSpeed = m_maxSpeed;
	current.maxSpeedDamaged = m_maxSpeedDamaged;
	current.minSpeed = m_minSpeed;
	current.minTurnSpeed = m_minTurnSpeed;
	current.acceleration = m_acceleration;
	current.accelerationDamaged = m_accelerationDamaged;
	current.braking = m_braking;
	current.lift = m_lift;
	current.liftDamaged = m_liftDamaged;
	current.turnRate = m_maxTurnRate;
	current.turnRateDamaged = m_maxTurnRateDamaged;

	const RotwkLocomotorRules::Resolved resolved = RotwkLocomotorRules::resolve(m_rotwk, speed, current);
	m_maxSpeed = resolved.maxSpeed;
	m_maxSpeedDamaged = resolved.maxSpeedDamaged;
	m_minSpeed = resolved.minSpeed;
	m_minTurnSpeed = resolved.minTurnSpeed;
	m_acceleration = resolved.acceleration;
	m_accelerationDamaged = resolved.accelerationDamaged;
	m_braking = resolved.braking;
	m_lift = resolved.lift;
	m_liftDamaged = resolved.liftDamaged;
	m_maxTurnRate = resolved.turnRate;
	m_maxTurnRateDamaged = resolved.turnRateDamaged;

	validate();
}

//-------------------------------------------------------------------------------------------------
const FieldParse* LocomotorTemplate::getFieldParse() const
{
	static const FieldParse TheFieldParse[] =
	{
		{ "Surfaces", INI::parseBitString32, TheLocomotorSurfaceTypeNames, offsetof(LocomotorTemplate, m_surfaces) },
		{ "Speed", INI::parseVelocityReal, nullptr, offsetof(LocomotorTemplate, m_maxSpeed) },
		{ "SpeedDamaged", parseRotwkSpeedDamaged, nullptr, offsetof( LocomotorTemplate, m_maxSpeedDamaged ) },
		{ "TurnRate", INI::parseAngularVelocityReal, nullptr, offsetof(LocomotorTemplate, m_maxTurnRate) },
		{ "TurnRateDamaged", INI::parseAngularVelocityReal, nullptr, offsetof( LocomotorTemplate, m_maxTurnRateDamaged ) },
		{ "Acceleration", parseRotwkAcceleration, nullptr, offsetof(LocomotorTemplate, m_acceleration) },
		{ "AccelerationDamaged", INI::parseAccelerationReal, nullptr, offsetof( LocomotorTemplate, m_accelerationDamaged ) },
		{ "Lift", parseRotwkLift, nullptr, offsetof(LocomotorTemplate, m_lift) },
		{ "LiftDamaged", parseRotwkLiftDamaged, nullptr, offsetof( LocomotorTemplate, m_liftDamaged ) },
		{ "Braking", parseRotwkBraking, nullptr, offsetof(LocomotorTemplate, m_braking) },
		{ "MinSpeed", parseRotwkMinSpeed, nullptr, offsetof(LocomotorTemplate, m_minSpeed) },
		{ "MinTurnSpeed", parseRotwkMinTurnSpeed, nullptr, offsetof(LocomotorTemplate, m_minTurnSpeed) },
		{ "TurnTime", parseRotwkTurnTime, nullptr, offsetof(LocomotorTemplate, m_rotwk) },
		{ "TurnTimeDamaged", parseRotwkTurnTimeDamaged, nullptr, offsetof(LocomotorTemplate, m_rotwk) },
		{ "PreferredHeight", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_preferredHeight) },
		{ "PreferredAttackHeight", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_preferredAttackHeight) },
		{ "PreferredHeightDamping", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_preferredHeightDamping) },
		{ "AeleronCorrectionDegree", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_aeleronCorrectionDegree) },
		{ "AeleronCorrectionRate", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_aeleronCorrectionRate) },
		{ "SwoopStandoffRadius", parseRotwkField<INI::parseReal>, nullptr, offsetof(LocomotorTemplate, m_swoopStandoffRadius) },
		{ "SwoopStandoffHeight", parseRotwkField<INI::parseReal>, nullptr, offsetof(LocomotorTemplate, m_swoopStandoffHeight) },
		{ "SwoopTerminalVelocity", parseRotwkField<INI::parseReal>, nullptr, offsetof(LocomotorTemplate, m_swoopTerminalVelocity) },
		{ "SwoopAccelerationRate", parseRotwkField<INI::parseReal>, nullptr, offsetof(LocomotorTemplate, m_swoopAccelerationRate) },
		{ "CirclingRadius", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_circlingRadius) },
		{ "Extra2DFriction", parseFrictionPerSec, nullptr, offsetof(LocomotorTemplate, m_extra2DFriction) },
		{ "SpeedLimitZ", INI::parseVelocityReal, nullptr, offsetof(LocomotorTemplate, m_speedLimitZ) },
		{ "MaxThrustAngle", INI::parseAngleReal, nullptr, offsetof(LocomotorTemplate, m_maxThrustAngle) },		// yes, angle, not angular-vel
		{ "ZAxisBehavior", INI::parseIndexList, TheLocomotorBehaviorZNames, offsetof(LocomotorTemplate, m_behaviorZ) },
		{ "Appearance", INI::parseIndexList, TheLocomotorAppearanceNames, offsetof(LocomotorTemplate, m_appearance) },		\
		{ "GroupMovementPriority", INI::parseIndexList, TheLocomotorPriorityNames, offsetof(LocomotorTemplate, m_movePriority) },		\

		{ "AccelerationPitchLimit", INI::parseAngleReal, nullptr, offsetof(LocomotorTemplate, m_accelPitchLimit) },
		{ "DecelerationPitchLimit", INI::parseAngleReal, nullptr, offsetof(LocomotorTemplate, m_decelPitchLimit) }, // Added in Zero Hour
		{ "BounceAmount", INI::parseAngularVelocityReal, nullptr, offsetof(LocomotorTemplate, m_bounceKick) },
		{ "PitchStiffness", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_pitchStiffness) },
		{ "RollStiffness", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_rollStiffness) },
		{ "PitchDamping", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_pitchDamping) },
		{ "RollDamping", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_rollDamping) },
		{ "ThrustRoll", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_thrustRoll) },
		{ "ThrustWobbleRate",	INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_wobbleRate) },
		{ "ThrustMinWobble",	INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_minWobble) },
		{ "ThrustMaxWobble",	INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_maxWobble) },
		{ "PitchInDirectionOfZVelFactor", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_pitchByZVelCoef) },
		{ "ForwardVelocityPitchFactor", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_forwardVelCoef) },
		{ "LateralVelocityRollFactor", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_lateralVelCoef) },
		{ "ForwardAccelerationPitchFactor", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_forwardAccelCoef) },
		{ "LateralAccelerationRollFactor", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_lateralAccelCoef) },
		{ "UniformAxialDamping", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_uniformAxialDamping) },
		{ "TurnPivotOffset", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_turnPivotOffset) },
		{ "Apply2DFrictionWhenAirborne", INI::parseBool, nullptr, offsetof(LocomotorTemplate, m_apply2DFrictionWhenAirborne) },
		{ "DownhillOnly", INI::parseBool, nullptr, offsetof(LocomotorTemplate, m_downhillOnly) },
		{ "AllowAirborneMotiveForce", INI::parseBool, nullptr, offsetof(LocomotorTemplate, m_allowMotiveForceWhileAirborne) },
		{ "LocomotorWorksWhenDead", INI::parseBool, nullptr, offsetof(LocomotorTemplate, m_locomotorWorksWhenDead) },
		{ "AirborneTargetingHeight", INI::parseInt, nullptr, offsetof( LocomotorTemplate, m_airborneTargetingHeight ) },
		{ "StickToGround",				INI::parseBool,			nullptr,	offsetof(LocomotorTemplate, m_stickToGround) },
		{ "CanMoveBackwards",				INI::parseBool,			nullptr,	offsetof(LocomotorTemplate, m_canMoveBackward) },
		{ "HasSuspension",				INI::parseBool,			nullptr,	offsetof(LocomotorTemplate, m_hasSuspension) },
		{ "FrontWheelTurnAngle", INI::parseAngleReal, nullptr, offsetof(LocomotorTemplate, m_wheelTurnAngle) },
		{ "MaximumWheelExtension", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_maximumWheelExtension) },
		{ "MaximumWheelCompression", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_maximumWheelCompression) },
		{ "CloseEnoughDist",				 INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_closeEnoughDist) },
		{ "CloseEnoughDist3D",			 INI::parseBool, nullptr, offsetof(LocomotorTemplate, m_isCloseEnoughDist3D) },
		{ "SlideIntoPlaceTime",		INI::parseDurationReal, nullptr, offsetof(LocomotorTemplate, m_ultraAccurateSlideIntoPlaceFactor) },

		{ "WanderWidthFactor", INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_wanderWidthFactor) },
		{ "WanderLengthFactor",				 INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_wanderLengthFactor) },
		{ "WanderAboutPointRadius",				 INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_wanderAboutPointRadius) },
		{ "BurningDeathRadius", parseRotwkField<INI::parseReal>, nullptr, offsetof(LocomotorTemplate, m_rotwk.burningDeathRadius) },
		{ "BurningDeathIsCavalry", parseRotwkField<INI::parseBool>, nullptr, offsetof(LocomotorTemplate, m_rotwk.burningDeathIsCavalry) },
		{ "ChargeAvailable", parseRotwkField<INI::parseBool>, nullptr, offsetof(LocomotorTemplate, m_rotwk.chargeAvailable) },
		{ "ChargeIgnoresCondition", parseRotwkField<INI::parseBool>, nullptr, offsetof(LocomotorTemplate, m_rotwk.chargeIgnoresCondition) },
		{ "ChargeSpeed", parseRotwkField<INI::parsePercentToReal>, nullptr, offsetof(LocomotorTemplate, m_rotwk.chargeSpeedFraction) },
		{ "BackingUpSpeed", parseRotwkField<INI::parsePercentToReal>, nullptr, offsetof(LocomotorTemplate, m_rotwk.backingUpSpeedFraction) },
		{ "FormationPriority", parseRotwkField<INI::parseIndexList>, TheFormationPriorityNames, offsetof(LocomotorTemplate, m_rotwk.formationPriority) },
		{ "WalkDistance", parseRotwkField<INI::parseReal>, nullptr, offsetof(LocomotorTemplate, m_rotwk.walkDistance) },
		{ "AccDecTrigger", parseRotwkField<INI::parseReal>, nullptr, offsetof(LocomotorTemplate, m_rotwk.accelDecelTrigger) },
		{ "TurnThreshold", parseRotwkField<INI::parseAngleReal>, nullptr, offsetof(LocomotorTemplate, m_rotwk.turnThreshold) },
		{ "EnableHighSpeedTurnModelconditions", parseRotwkField<INI::parseBool>, nullptr, offsetof(LocomotorTemplate, m_rotwk.enableHighSpeedTurnModelConditions) },

		{ "RudderCorrectionDegree",		 INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_rudderCorrectionDegree) }, // Added in Zero Hour
		{ "RudderCorrectionRate",			 INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_rudderCorrectionRate) }, // Added in Zero Hour
		{ "ElevatorCorrectionDegree",	 INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_elevatorCorrectionDegree) }, // Added in Zero Hour
		{ "ElevatorCorrectionRate",		 INI::parseReal, nullptr, offsetof(LocomotorTemplate, m_elevatorCorrectionRate) }, // Added in Zero Hour
		{ "WaitForFormation", parseRotwkField<INI::parseBool>, nullptr, offsetof(LocomotorTemplate, m_waitForFormation) },
		{ "SlowTurnRadius", parseRotwkField<INI::parseReal>, nullptr, offsetof(LocomotorTemplate, m_slowTurnRadius) },
		{ "FastTurnRadius", parseRotwkField<INI::parseReal>, nullptr, offsetof(LocomotorTemplate, m_fastTurnRadius) },
		{ "MaxTurnWithoutReform", parseRotwkField<INI::parseAngleReal>, nullptr, offsetof(LocomotorTemplate, m_maxTurnWithoutReform) },
		{ "TurnWhileMoving", parseRotwkField<INI::parseBool>, nullptr, offsetof(LocomotorTemplate, m_turnWhileMoving) },
		{ nullptr, nullptr, nullptr, 0 }

	};
	return TheFieldParse;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
LocomotorStore::LocomotorStore()
{
}

//-------------------------------------------------------------------------------------------------
LocomotorStore::~LocomotorStore()
{
	// delete all the templates, then clear out the table.
	LocomotorTemplateMap::iterator it;
	for (it = m_locomotorTemplates.begin(); it != m_locomotorTemplates.end(); ++it) {
		deleteInstance(it->second);
	}

	m_locomotorTemplates.clear();
}

//-------------------------------------------------------------------------------------------------
LocomotorTemplate* LocomotorStore::findLocomotorTemplate(NameKeyType namekey)
{
	if (namekey == NAMEKEY_INVALID)
		return nullptr;

  LocomotorTemplateMap::iterator it = m_locomotorTemplates.find(namekey);
  if (it == m_locomotorTemplates.end())
		return nullptr;
	else
		return (*it).second;
}

//-------------------------------------------------------------------------------------------------
const LocomotorTemplate* LocomotorStore::findLocomotorTemplate(NameKeyType namekey) const
{
	if (namekey == NAMEKEY_INVALID)
		return nullptr;

  LocomotorTemplateMap::const_iterator it = m_locomotorTemplates.find(namekey);
  if (it == m_locomotorTemplates.end())
	{
		return nullptr;
	}
	else
	{
		return (*it).second;
	}
}

const LocomotorTemplate* LocomotorStore::findOrCreateRotwkVariant(const AsciiString& locomotorName, Real speedPerSecond)
{
	AsciiString variantName;
	variantName.format("%s:%.3f", locomotorName.str(), speedPerSecond);
	const NameKeyType variantKey = NAMEKEY(variantName);
	if (const LocomotorTemplate* existing = findLocomotorTemplate(variantKey))
		return existing;

	const LocomotorTemplate* base = findLocomotorTemplate(NAMEKEY(locomotorName));
	if (base == nullptr)
		return nullptr;

	LocomotorTemplate* variant = newInstance(LocomotorTemplate);
	*variant = *static_cast<const LocomotorTemplate*>(base->getFinalOverride());
	variant->setNextOverride(nullptr);
	variant->markAsOverride(false);
	variant->friend_setName(variantName);
	variant->applyRotwkSetSpeed(speedPerSecond);
	m_locomotorTemplates[variantKey] = variant;
	return variant;
}

//-------------------------------------------------------------------------------------------------
void LocomotorStore::update()
{
}

//-------------------------------------------------------------------------------------------------
void LocomotorStore::reset()
{
	// cleanup overrides.
	LocomotorTemplateMap::iterator it;
	for (it = m_locomotorTemplates.begin(); it != m_locomotorTemplates.end(); ) {
		Overridable *locoTemp = it->second->deleteOverrides();
		if (!locoTemp)
		{
			it = m_locomotorTemplates.erase(it);
		}
		else
		{
			++it;
		}
	}
}

//-------------------------------------------------------------------------------------------------
LocomotorTemplate *LocomotorStore::newOverride( LocomotorTemplate *locoTemplate )
{
	if (locoTemplate == nullptr)
		return nullptr;

	// allocate new template
	LocomotorTemplate *newTemplate = newInstance(LocomotorTemplate);

	// copy data from final override to 'newTemplate' as a set of initial default values
	*newTemplate = *locoTemplate;
	locoTemplate->setNextOverride(newTemplate);

	newTemplate->markAsOverride();

	// return the newly created override for us to set values with etc
	return newTemplate;

}

//-------------------------------------------------------------------------------------------------
/*static*/ void LocomotorStore::parseLocomotorTemplateDefinition(INI* ini)
{
	if (!TheLocomotorStore)
		throw INI_INVALID_DATA;

	Bool isOverride = false;
	// read the Locomotor name
	const char* token = ini->getNextToken();
	NameKeyType namekey = NAMEKEY(token);

	LocomotorTemplate *loco = TheLocomotorStore->findLocomotorTemplate(namekey);
	if (loco) {
		if (ini->getLoadType() == INI_LOAD_CREATE_OVERRIDES) {
			loco = TheLocomotorStore->newOverride((LocomotorTemplate*) loco->friend_getFinalOverride());
		}
		isOverride = true;
	} else {
		loco = newInstance(LocomotorTemplate);
		if (ini->getLoadType() == INI_LOAD_CREATE_OVERRIDES) {
			loco->markAsOverride();
		}
	}

	loco->friend_setName(token);
	ini->initFromINI(loco, loco->getFieldParse());
	loco->validate();

	// if this is an override, then we want the pointer on the existing named locomotor to point us
	// to the override, so don't add it to the map.
	if (!isOverride)
		TheLocomotorStore->m_locomotorTemplates[namekey] = loco;
}

//-------------------------------------------------------------------------------------------------
/*static*/ void INI::parseLocomotorTemplateDefinition( INI* ini )
{
	LocomotorStore::parseLocomotorTemplateDefinition(ini);
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
Locomotor::Locomotor(const LocomotorTemplate* tmpl)
{
	m_template = tmpl;
	m_brakingFactor = 1.0f;
	m_maxLift = BIGNUM;
	m_maxSpeed = BIGNUM;
	m_maxAccel = BIGNUM;
	m_maxBraking = BIGNUM;
	m_maxTurnRate = BIGNUM;
	m_flags = 0;
	m_closeEnoughDist = m_template->m_closeEnoughDist;
	setFlag(IS_CLOSE_ENOUGH_DIST_3D, m_template->m_isCloseEnoughDist3D);
#ifdef CIRCLE_FOR_LANDING
	m_circleThresh = 0.0f;
#endif
	m_preferredHeight = m_template->m_preferredHeight;
	m_preferredHeightDamping = m_template->m_preferredHeightDamping;

	m_angleOffset = GameLogicRandomValueReal(-PI/6, PI/6);
	m_offsetIncrement = (PI/40) * (GameLogicRandomValueReal(0.8f, 1.2f)/m_template->m_wanderLengthFactor);
	setFlag(OFFSET_INCREASING, GameLogicRandomValue(0,1));
	m_donutTimer = TheGameLogic->getFrame()+DONUT_TIME_DELAY_SECONDS*static_cast<float>(LOGICFRAMES_PER_SECOND);
}

//-------------------------------------------------------------------------------------------------
Locomotor::Locomotor(const Locomotor& that)
{
	m_angleOffset = 0.0f;
	m_maintainPos.zero();
	m_template = that.m_template;
	m_brakingFactor = that.m_brakingFactor;
	m_maxLift = that.m_maxLift;
	m_maxSpeed = that.m_maxSpeed;
	m_maxAccel = that.m_maxAccel;
	m_maxBraking = that.m_maxBraking;
	m_maxTurnRate = that.m_maxTurnRate;
	m_flags = that.m_flags;
	m_closeEnoughDist = that.m_closeEnoughDist;
#ifdef CIRCLE_FOR_LANDING
	m_circleThresh = that.m_circleThresh;
#endif
	m_preferredHeight = that.m_preferredHeight;
	m_preferredHeightDamping = that.m_preferredHeightDamping;
	m_angleOffset = that.m_angleOffset;
	m_offsetIncrement = that.m_offsetIncrement;
}

//-------------------------------------------------------------------------------------------------
Locomotor& Locomotor::operator=(const Locomotor& that)
{
	if (this != &that)
	{
		m_template = that.m_template;
		m_brakingFactor = that.m_brakingFactor;
		m_maxLift = that.m_maxLift;
		m_maxSpeed = that.m_maxSpeed;
		m_maxAccel = that.m_maxAccel;
		m_maxBraking = that.m_maxBraking;
		m_maxTurnRate = that.m_maxTurnRate;
		m_flags = that.m_flags;
		m_closeEnoughDist = that.m_closeEnoughDist;
#ifdef CIRCLE_FOR_LANDING
		m_circleThresh = that.m_circleThresh;
#endif
		m_preferredHeight = that.m_preferredHeight;
		m_preferredHeightDamping = that.m_preferredHeightDamping;
	}
	return *this;
}

//-------------------------------------------------------------------------------------------------
Locomotor::~Locomotor()
{
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void Locomotor::crc( Xfer *xfer )
{
	if (RetailFileLayers::enabled())
		xfer->xferBool(&m_retailWasRunning);
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void Locomotor::xfer( Xfer *xfer )
{
	// version
	const XferVersion currentVersion = RetailFileLayers::enabled() ? 3 : 2;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	if (version>=2) {
		xfer->xferUnsignedInt(&m_donutTimer);
	}

	xfer->xferCoord3D(&m_maintainPos);
	xfer->xferReal(&m_brakingFactor);
	xfer->xferReal(&m_maxLift);
	xfer->xferReal(&m_maxSpeed);
	xfer->xferReal(&m_maxAccel);
	xfer->xferReal(&m_maxBraking);
	xfer->xferReal(&m_maxTurnRate);
	xfer->xferReal(&m_closeEnoughDist);
#ifdef CIRCLE_FOR_LANDING
	DEBUG_CRASH(("not supported, must fix me"));
#endif
	xfer->xferUnsignedInt(&m_flags);
	xfer->xferReal(&m_preferredHeight);
	xfer->xferReal(&m_preferredHeightDamping);
	xfer->xferReal(&m_angleOffset);
	xfer->xferReal(&m_offsetIncrement);
	if (version >= 3)
		xfer->xferBool(&m_retailWasRunning);
	else if (xfer->getXferMode() == XFER_LOAD)
		m_retailWasRunning = FALSE;

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void Locomotor::loadPostProcess()
{

}

//-------------------------------------------------------------------------------------------------
void Locomotor::startMove()
{
	// Reset the donut timer.
	m_donutTimer = TheGameLogic->getFrame()+DONUT_TIME_DELAY_SECONDS*static_cast<float>(LOGICFRAMES_PER_SECOND);
}

//-------------------------------------------------------------------------------------------------
Bool Locomotor::hasRetailChargeSpeed(BodyDamageType condition, const Object* object) const
{
	if (!object || !RetailFileLayers::enabled() ||
		(!m_template->m_rotwk.chargeIgnoresCondition &&
			!IS_CONDITION_BETTER(condition, TheGlobalData->m_movementPenaltyDamageState)))
		return FALSE;

	if (m_template->m_rotwk.chargeAvailable)
	{
		if (object->testStatus(OBJECT_STATUS_IS_ATTACKING) || object->testStatus(OBJECT_STATUS_LEASHED_RETURNING))
			return TRUE;

		const AIUpdateInterface* ai = object->getAI();
		if (ai && ai->getModuleNameKey() == NAMEKEY("HordeAIUpdate") &&
			static_cast<const HordeAIUpdate*>(ai)->isApproachingAttackTarget())
			return TRUE;
	}

	if (!object->testStatus(OBJECT_STATUS_IS_USING_ABILITY))
		return FALSE;
	if (object->getLogicModelConditions().test(MODELCONDITION_CHARGING))
		return TRUE;

	for (BehaviorModule** module = object->getBehaviorModules(); *module; ++module)
	{
		auto* special = (*module)->getSpecialPowerUpdateInterface();
		if (special && special->isSpecialAbility() && static_cast<SpecialAbilityUpdate*>(special)->isChargeAttack())
			return TRUE;
	}
	return FALSE;
}

Real Locomotor::getMaxSpeedForCondition(BodyDamageType condition, const Object* obj) const
{
	Real speed;

	if (IS_CONDITION_BETTER(condition, TheGlobalData->m_movementPenaltyDamageState) ||
		(RetailFileLayers::enabled() && m_template->m_rotwk.chargeIgnoresCondition))
		speed = m_template->getActualMaxSpeed();
	else
		speed = m_template->getActualMaxSpeedDamaged();

	if (hasRetailChargeSpeed(condition, obj))
		speed *= m_template->m_rotwk.chargeSpeedFraction;

	if (obj && obj->getAttributeModifierHelper())
		speed *= AttributeModifiers::speedFactor(obj);

	if (RetailFileLayers::enabled() && obj)
		if (const auto* siege = SiegeEngineContain::find(obj))
			speed *= siege->crewSpeedScale();

	Real maxSpeed = getMaxSpeedOverride();
	if (speed > maxSpeed)
		speed = maxSpeed;

	return speed;
}

//-------------------------------------------------------------------------------------------------
Real Locomotor::getMaxSpeedOverride() const
{
	return m_template->scaleSpeed(m_maxSpeed);
}

Real Locomotor::getBackingUpSpeed() const
{
	return std::max(0.0f, m_template->getActualMaxSpeed() * m_template->m_rotwk.backingUpSpeedFraction);
}

//-------------------------------------------------------------------------------------------------
Real Locomotor::getMinSpeed() const
{
	return m_template->getActualMinSpeed();
}

//-------------------------------------------------------------------------------------------------
Real Locomotor::getMinTurnSpeed() const
{
	return m_template->getActualMinTurnSpeed();
}

//-------------------------------------------------------------------------------------------------
Real Locomotor::getMaxTurnRate(BodyDamageType condition) const
{
	Real turn;

	if( IS_CONDITION_BETTER( condition, TheGlobalData->m_movementPenaltyDamageState ) )
		turn = m_template->m_maxTurnRate;
	else
		turn = m_template->m_maxTurnRateDamaged;

	if (turn > m_maxTurnRate)
		turn = m_maxTurnRate;

	const Real TURN_FACTOR = 2;
	if (getFlag(ULTRA_ACCURATE))
		turn *= TURN_FACTOR;	// monster turning ability

	return turn;
}

//-------------------------------------------------------------------------------------------------
Real Locomotor::getMaxAcceleration(BodyDamageType condition) const
{
	Real accel;

	if( IS_CONDITION_BETTER( condition, TheGlobalData->m_movementPenaltyDamageState ) )
		accel = m_template->m_acceleration;
	else
		accel = m_template->m_accelerationDamaged;

	if (accel > m_maxAccel)
		accel = m_maxAccel;

	return accel;
}

//-------------------------------------------------------------------------------------------------
Real Locomotor::getBraking() const
{
	Real braking = m_template->m_braking;

	if (braking > m_maxBraking)
		braking = m_maxBraking;

	return braking;
}

//-------------------------------------------------------------------------------------------------
Real Locomotor::getMaxLift(BodyDamageType condition) const
{
	Real lift;

	if( IS_CONDITION_BETTER( condition, TheGlobalData->m_movementPenaltyDamageState ) )
		lift = m_template->m_lift;
	else
		lift = m_template->m_liftDamaged;

	if (lift > m_maxLift)
		lift = m_maxLift;

	return lift;
}

//-------------------------------------------------------------------------------------------------
void Locomotor::updateRetailTurnConditions(Object* obj, Real requestedTurn) const
{
	if (!RetailFileLayers::enabled())
		return;

	static const ModelConditionFlags turnMask = MAKE_MODELCONDITION_MASK4(MODELCONDITION_TURN_LEFT,
		MODELCONDITION_TURN_RIGHT, MODELCONDITION_TURN_LEFT_HIGH_SPEED, MODELCONDITION_TURN_RIGHT_HIGH_SPEED);
	ModelConditionFlags conditions;
	if (requestedTurn != 0.0f && fabs(requestedTurn) > m_template->m_rotwk.turnThreshold)
	{
		const Real halfSpeed = 0.5f * getMaxSpeedForCondition(obj->getBodyModule()->getDamageState(), obj);
		const Coord3D& velocity = *obj->getPhysics()->getVelocity();
		const Bool highSpeed = sqr(velocity.x) + sqr(velocity.y) > sqr(halfSpeed);
		if (highSpeed && m_template->m_rotwk.enableHighSpeedTurnModelConditions)
			conditions.set(requestedTurn > 0.0f ? MODELCONDITION_TURN_LEFT_HIGH_SPEED : MODELCONDITION_TURN_RIGHT_HIGH_SPEED);
		else if (!highSpeed)
			conditions.set(requestedTurn > 0.0f ? MODELCONDITION_TURN_LEFT : MODELCONDITION_TURN_RIGHT);
	}
	obj->clearAndSetModelConditionFlags(turnMask, conditions);
}

Bool Locomotor::usesCavalrySteering(const Object* obj) const
{
	return RetailFileLayers::enabled() && obj->isKindOf(KINDOF_CAVALRY) && !m_template->m_canMoveBackward &&
		((m_template->m_appearance == LOCO_HORDE && m_template->m_turnWhileMoving && HordeContain::get(obj)) ||
			(m_template->m_appearance == LOCO_WHEELS_FOUR && HordeContain::get(obj->getContainedBy())));
}

Real Locomotor::getRetailTurnRadius(Real speed, Real maxSpeed) const
{
	const Real fraction = maxSpeed > 0.0f ? std::min(speed / maxSpeed, 1.0f) : 0.0f;
	return m_template->m_slowTurnRadius + (m_template->m_fastTurnRadius - m_template->m_slowTurnRadius) * fraction;
}

void Locomotor::locoUpdate_moveTowardsAngle(Object* obj, Real goalAngle)
{
	setFlag(MAINTAIN_POS_IS_VALID, false);

	if (obj == nullptr || m_template == nullptr)
		return;

	PhysicsBehavior *physics = obj->getPhysics();
	if (physics == nullptr)
	{
		DEBUG_CRASH(("you can only apply Locomotors to objects with Physics"));
		return;
	}

	// Skip moveTowardsAngle if physics say you're stunned
	if(physics->getIsStunned())
	{
		updateRetailTurnConditions(obj, 0.0f);
		return;
	}

#ifdef DEBUG_OBJECT_ID_EXISTS
//	DEBUG_ASSERTLOG(obj->getID() != TheObjectIDToDebug, ("locoUpdate_moveTowardsAngle %f (%f deg), spd %f (%f)",goalAngle,goalAngle*180/PI,physics->getSpeed(),physics->getForwardSpeed2D()));
#endif

	Real minSpeed = getMinSpeed();
	if (minSpeed > 0)
	{
		// can't stay in one place; move in the desired direction at min speed.
		Coord3D desiredPos = *obj->getPosition();
		desiredPos.x += Cos(goalAngle) * minSpeed * 2;
		desiredPos.y += Sin(goalAngle) * minSpeed * 2;
		// pass a huge num for "dist to goal", so that we don't think we're nearing
		// our destination and thus slow down...
		const Real onPathDistToGoal = 99999.0f;
		Bool blocked = false;
		locoUpdate_moveTowardsPosition(obj, desiredPos, onPathDistToGoal, minSpeed, &blocked);

		// don't need to call handleBehaviorZ() here, since locoUpdate_moveTowardsPosition() will do so
		return;
	}
	else
	{
		DEBUG_ASSERTCRASH(m_template->m_appearance != LOCO_THRUST, ("THRUST should always have minspeeds!"));
		Coord3D desiredPos = *obj->getPosition();
		desiredPos.x += Cos(goalAngle) * 1000.0f;
		desiredPos.y += Sin(goalAngle) * 1000.0f;
		if (usesCavalrySteering(obj))
			moveCavalryForward(obj, physics, desiredPos, 0.0f, 0.0f);
		else
			physics->setTurning(rotateTowardsPosition(obj, desiredPos));
		handleBehaviorZ(obj, physics, *obj->getPosition());
	}

}

//-------------------------------------------------------------------------------------------------
PhysicsTurningType Locomotor::rotateTowardsPosition(Object* obj, const Coord3D& goalPos, Real *relAngle)
{
	BodyDamageType bdt = obj->getBodyModule()->getDamageState();
	Real turnRate = getMaxTurnRate(bdt);

	PhysicsTurningType rotating = rotateObjAroundLocoPivot(obj, goalPos, turnRate, relAngle);
	return rotating;
}

//-------------------------------------------------------------------------------------------------
void Locomotor::setPhysicsOptions(Object* obj)
{
	PhysicsBehavior *physics = obj->getPhysics();
	if (physics == nullptr)
	{
		DEBUG_CRASH(("you can only apply Locomotors to objects with Physics"));
		return;
	}

	// crank up the friction in ultra-accurate mode to increase movement precision.
	const Real EXTRA_FRIC = 0.5f;
	Real extraExtraFriction = getFlag(ULTRA_ACCURATE) ? EXTRA_FRIC : 0.0f;
	physics->setExtraFriction(m_template->m_extra2DFriction + extraExtraFriction);
	physics->setAllowAirborneFriction(getApply2DFrictionWhenAirborne());	// you'd think we wouldn't want friction in the air, but it's needed for realistic behavior.
	physics->setStickToGround(getStickToGround()); // walking guys aren't allowed to catch huge (or even small) air.
}

//-------------------------------------------------------------------------------------------------
static constexpr Real MIN_GAIT_SPEED = 0.001f;

void Locomotor::clearRetailGait(Object* object)
{
	if (!RetailFileLayers::enabled() ||
		(m_template->m_rotwk.walkDistance <= 0.0f && m_template->m_rotwk.accelDecelTrigger < 0.0f))
		return;
	m_retailWasRunning = FALSE;
	object->clearModelConditionState(MODELCONDITION_WALKING);
	object->clearModelConditionState(MODELCONDITION_ACCELERATE);
	object->clearModelConditionState(MODELCONDITION_DECELERATE);
}

void Locomotor::updateRetailGait(Object* object, Real distance, Real speedBefore, Bool braking, Real speedAfter)
{
	const auto& data = m_template->m_rotwk;
	if (!RetailFileLayers::enabled() || (data.walkDistance <= 0.0f && data.accelDecelTrigger < 0.0f))
		return;

	const auto damage = object->getBodyModule()->getDamageState();
	const Bool authoredTrigger = data.accelDecelTrigger >= 0.0f;
	const Real threshold = getMaxSpeedForCondition(damage, object) * (authoredTrigger ? data.accelDecelTrigger : 0.5f);
	const Bool hasSteeredSpeed = speedAfter >= 0.0f;
	const Bool decelerating = m_retailWasRunning && braking &&
		(!authoredTrigger || (hasSteeredSpeed ? speedAfter : speedBefore) < threshold);
	const Bool walking = !m_retailWasRunning && data.walkDistance > 0.0f && distance < data.walkDistance && !braking;
	const Real minimumAccelerationFraction = 0.4f;
	const Bool accelerating = !walking && !braking && speedBefore < threshold &&
		(authoredTrigger && hasSteeredSpeed ? speedAfter - speedBefore > minimumAccelerationFraction * getMaxAcceleration(damage) :
			!m_retailWasRunning);
	if (!walking && !braking && speedBefore >= threshold)
		m_retailWasRunning = TRUE;

	if (walking)
		object->setModelConditionState(MODELCONDITION_WALKING);
	else
		object->clearModelConditionState(MODELCONDITION_WALKING);
	if (accelerating)
		object->setModelConditionState(MODELCONDITION_ACCELERATE);
	else
		object->clearModelConditionState(MODELCONDITION_ACCELERATE);
	if (decelerating)
		object->setModelConditionState(MODELCONDITION_DECELERATE);
	else
		object->clearModelConditionState(MODELCONDITION_DECELERATE);
}

void Locomotor::locoUpdate_moveTowardsPosition(Object* obj, const Coord3D& goalPos,
	Real onPathDistToGoal, Real desiredSpeed, Bool *blocked, Bool birdPullUp)
{
	setFlag(MAINTAIN_POS_IS_VALID, false);

	BodyDamageType bdt = obj->getBodyModule()->getDamageState();
	Real maxSpeed = getMaxSpeedForCondition(bdt, obj);

	// sanity, we cannot use desired speed that is greater than our max speed we are capable of moving at
	if( desiredSpeed > maxSpeed )
		desiredSpeed = maxSpeed;

	Real distToStopAtMaxSpeed = (maxSpeed/getBraking()) * (maxSpeed)/2.0f;
	if (onPathDistToGoal>PATHFIND_CELL_SIZE_F && onPathDistToGoal > distToStopAtMaxSpeed)
	{
		setFlag(IS_BRAKING, false);
		m_brakingFactor = 1.0f;
	}
	PhysicsBehavior *physics = obj->getPhysics();
	if (physics == nullptr)
	{
		DEBUG_CRASH(("you can only apply Locomotors to objects with Physics"));
		return;
	}

	// Skip moveTowardsPosition if physics say you're stunned
	if(physics->getIsStunned())
	{
		updateRetailTurnConditions(obj, 0.0f);
		return;
	}

#ifdef DEBUG_OBJECT_ID_EXISTS
//	DEBUG_ASSERTLOG(obj->getID() != TheObjectIDToDebug, ("locoUpdate_moveTowardsPosition %f %f %f (dtg %f, spd %f), speed %f (%f)",goalPos.x,goalPos.y,goalPos.z,onPathDistToGoal,desiredSpeed,physics->getSpeed(),physics->getForwardSpeed2D()));
#endif

	//
	// do not allow for invalid positions that the pathfinder cannot handle ... for airborne
	// objects we don't need the pathfinder so we'll ignore this
	//
	if( BitIsSet( m_template->m_surfaces, LOCOMOTORSURFACE_AIR ) == false &&
			!TheAI->pathfinder()->validMovementTerrain(obj->getLayer(), this, obj->getPosition()) &&
			!getFlag(ALLOW_INVALID_POSITION))
	{
		// Somehow, we have gotten to an invalid location.
		if (fixInvalidPosition(obj, physics))
		{
			// the we adjusted us toward a legal position, so just return.
			return;
		}
	}

	// If the actual distance is farther, then use the actual distance so we get there.
	Real dx = goalPos.x - obj->getPosition()->x;
	Real dy = goalPos.y - obj->getPosition()->y;
	Real dz = goalPos.z - obj->getPosition()->z;
	Real dist = WWMath::SqrtOrigin(dx*dx+dy*dy);
	if (dist>onPathDistToGoal)
	{
		if (!obj->isKindOf(KINDOF_PROJECTILE) && dist>2*onPathDistToGoal)
		{
			setFlag(IS_BRAKING, true);
		}
		onPathDistToGoal = dist;
	}

	Coord3D nullAccel;

	Bool treatAsAirborne = false;
	Coord3D pos = *obj->getPosition();
	Real heightAboveSurface = pos.z - TheTerrainLogic->getLayerHeight(pos.x, pos.y, obj->getLayer());

	if( obj->getStatusBits().test( OBJECT_STATUS_DECK_HEIGHT_OFFSET ) )
	{
		heightAboveSurface -= obj->getCarrierDeckHeight();
	}

	if (heightAboveSurface > -(3*3)*TheGlobalData->m_gravity)
	{
		// If we get high enough to stay up for 3 frames, then we left the ground.
		treatAsAirborne = true;
	}
	// We apply a zero acceleration to all units, as the call to
	// applyMotiveForce flags an object as being "driven" by a locomotor, rather
	// than being pushed around by objects bumping it.
	nullAccel.x = nullAccel.y = nullAccel.z = 0;
	physics->applyMotiveForce(&nullAccel);

	if (*blocked)
	{
		if (desiredSpeed > physics->getVelocityMagnitude())
		{
			*blocked = false;
		}
		if (treatAsAirborne && BitIsSet( m_template->m_surfaces, LOCOMOTORSURFACE_AIR ) )
		{
			// Airborne flying objects don't collide for now.  jba.
			*blocked = false;
		}
	}

	if (*blocked)
	{
		if (usesCavalrySteering(obj))
		{
			moveCavalryForward(obj, physics, goalPos, 0.0f, 0.0f);
			handleBehaviorZ(obj, physics, goalPos, birdPullUp);
			return;
		}
		updateRetailGait(obj, onPathDistToGoal, physics->getForwardSpeed2D(), TRUE);
		physics->scrubVelocity2D(desiredSpeed); // stop if we are about to run into the blocking object.
		Real turnRate = getMaxTurnRate(obj->getBodyModule()->getDamageState());
		if (m_template->m_wanderWidthFactor == 0.0f)
		{
			*blocked = (TURN_NONE != rotateObjAroundLocoPivot(obj, goalPos, turnRate));
		}

		// it is very important to be sure to call this in all situations, even if not moving in 2d space.
		handleBehaviorZ(obj, physics, goalPos, birdPullUp);
		return;
	}

	if (
// srj sez: I don't know why we didn't want HOVERs to allow to "brake".
// we actually really want them to, because it allows much more precise destination positioning.
//			m_template->m_appearance == LOCO_HOVER ||
				m_template->m_appearance == LOCO_WINGS)
	{
		setFlag(IS_BRAKING, false);
	}

	Bool wasBraking = obj->getStatusBits().test( OBJECT_STATUS_BRAKING );
	Bool usedRetailGroundSteering = FALSE;

	physics->setTurning(TURN_NONE);
	if (getAllowMotiveForceWhileAirborne() || !treatAsAirborne ||
		(RetailFileLayers::enabled() && getAppearance() == LOCO_HOVER &&
			getLegalSurfaces() == LOCOMOTORSURFACE_AIR && StrafeAreaUpdate::controlsAttackFlight(*obj)))
	{
		LocomotorAppearance appearance = m_template->m_appearance;
		const Bool retailIndependent = RetailFileLayers::enabled() && !obj->getContainedBy();
		const Bool retailHero = retailIndependent && obj->isKindOf(KINDOF_HERO);
		if ((appearance == LOCO_HORDE || appearance == LOCO_FOUR_LEGS_HUGE || appearance == LOCO_HUGE_TWO_LEGS) &&
			!RetailFileLayers::enabled())
			appearance = LOCO_OTHER;
		switch (appearance)
		{
			case LOCO_LEGS_TWO:
				if (retailHero && m_template->m_wanderWidthFactor == 0.0f)
				{
					moveRetailGroundTowardsPosition(obj, physics, goalPos, onPathDistToGoal, desiredSpeed, FALSE);
					usedRetailGroundSteering = TRUE;
					break;
				}
				[[fallthrough]];
			case LOCO_FOUR_LEGS_HUGE:
			case LOCO_HUGE_TWO_LEGS:
					moveTowardsPositionLegs(obj, physics, goalPos, onPathDistToGoal, desiredSpeed);
					break;
			case LOCO_CLIMBER:
					moveTowardsPositionClimb(obj, physics, goalPos, onPathDistToGoal, desiredSpeed);
					break;
			case LOCO_WHEELS_FOUR:
			{
				const Bool mountedHero = retailHero && obj->getAI() &&
					obj->getAI()->getCurLocomotorSetType() == LOCOMOTORSET_MOUNTED;
				const Bool independentCavalry = retailIndependent && !retailHero && obj->isKindOf(KINDOF_CAVALRY);
				if (!m_template->m_canMoveBackward && (mountedHero || independentCavalry))
				{
					moveRetailGroundTowardsPosition(obj, physics, goalPos, onPathDistToGoal, desiredSpeed, independentCavalry);
					usedRetailGroundSteering = TRUE;
					break;
				}
				if (usesCavalrySteering(obj))
				{
					moveCavalryForward(obj, physics, goalPos, onPathDistToGoal, desiredSpeed);
					break;
				}
				[[fallthrough]];
			}
			case LOCO_MOTORCYCLE:
					moveTowardsPositionWheels(obj, physics, goalPos, onPathDistToGoal, desiredSpeed);
					break;
			case LOCO_TREADS:
					moveTowardsPositionTreads(obj, physics, goalPos, onPathDistToGoal, desiredSpeed);
					break;
			case LOCO_HOVER:
					moveTowardsPositionHover(obj, physics, goalPos, onPathDistToGoal, desiredSpeed);
					break;
			case LOCO_WINGS:
					moveTowardsPositionWings(obj, physics, goalPos, onPathDistToGoal, desiredSpeed);
					break;
			case LOCO_THRUST:
					moveTowardsPositionThrust(obj, physics, goalPos, onPathDistToGoal, desiredSpeed);
					break;
			case LOCO_HORDE:
					moveTowardsPositionHorde(obj, physics, goalPos, onPathDistToGoal, desiredSpeed);
					break;
			case LOCO_OTHER:
			default:
					moveTowardsPositionOther(obj, physics, goalPos, onPathDistToGoal, desiredSpeed);
					break;
		}
	}

	if (!usedRetailGroundSteering && m_template->m_appearance != LOCO_HORDE && m_template->m_appearance != LOCO_WHEELS_FOUR &&
		m_template->m_appearance != LOCO_MOTORCYCLE)
		updateRetailGait(obj, onPathDistToGoal, physics->getForwardSpeed2D(), getFlag(IS_BRAKING));
	handleBehaviorZ(obj, physics, goalPos, birdPullUp);
	// Objects that are braking don't follow the normal physics, so they end up at their destination exactly.
	obj->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_BRAKING ), getFlag(IS_BRAKING) );

	if (wasBraking && !usedRetailGroundSteering && !usesCavalrySteering(obj))
	{
	#define MIN_VEL (PATHFIND_CELL_SIZE_F/(static_cast<float>(LOGICFRAMES_PER_SECOND)))

		Coord3D pos = *obj->getPosition();
		if (obj->isKindOf(KINDOF_PROJECTILE))
		{
			// Projectiles never stop braking once they start.  jba.
			obj->setStatus( MAKE_OBJECT_STATUS_MASK( OBJECT_STATUS_BRAKING ) );
			// Projectiles cheat in 3 dimensions.
			dist = WWMath::SqrtOrigin(dx*dx+dy*dy+dz*dz);
			Real vel = physics->getVelocityMagnitude();
			if (vel < MIN_VEL)
				vel = MIN_VEL;
			if (vel > dist)
				vel = dist;	// do not overcompensate!
			// Normalize.
			if (dist > 0.001f)
			{
				dist = 1.0f / dist;
				dx *= dist;
				dy *= dist;
				dz *= dist;
				pos.x += dx * vel;
				pos.y += dy * vel;
				pos.z += dz * vel;
			}
		}
		else
		{
			// not projectiles only cheat in x & y.
			// Normalize.
			if (dist > 0.001f)
			{
				Real vel = fabs(physics->getForwardSpeed2D());
				if (vel < MIN_VEL)
					vel = MIN_VEL;
				if (vel > dist)
					vel = dist;	// do not overcompensate!
				dist = 1.0f / dist;
				dx *= dist;
				dy *= dist;
				Coord3D movement = {dx * vel, dy * vel, 0.0f};
				HordeContain::constrainGroundMovement(obj, movement);
				HordeContain::separateGroundMovement(obj, movement);
				pos.x += movement.x;
				pos.y += movement.y;
			}
		}
		obj->setPosition(&pos);
	}

}

//-------------------------------------------------------------------------------------------------
void Locomotor::moveTowardsPositionTreads(Object* obj, PhysicsBehavior *physics, const Coord3D& goalPos, Real onPathDistToGoal, Real desiredSpeed)
{

	// sanity, we cannot use desired speed that is greater than our max speed we are capable of moving at
	BodyDamageType bdt = obj->getBodyModule()->getDamageState();
	Real maxSpeed = getMaxSpeedForCondition(bdt, obj);
	if( desiredSpeed > maxSpeed )
		desiredSpeed = maxSpeed;

	Real maxAcceleration = getMaxAcceleration(bdt);

	// Locomotion for treaded vehicles, ie tanks.

	//
	// Orient toward goal position
	//
//	Real angle = obj->getOrientation();
//	Real relAngle = ThePartitionManager->getRelativeAngle2D( obj, &goalPos );
//	Real desiredAngle = angle + relAngle;
	Real relAngle ;
	PhysicsTurningType rotating = rotateTowardsPosition(obj, goalPos, &relAngle);
	physics->setTurning(rotating);

	//
	// Modulate speed according to turning. The more we have to turn, the slower we go
	//
	const Real QUAETERPI = PI / 4.0f;
	Real angleCoeff = (Real)fabs( relAngle ) / QUAETERPI;
	if (angleCoeff > 1.0f)
		angleCoeff = 1.0;


	Real dx = obj->getPosition()->x - goalPos.x;
	Real dy = obj->getPosition()->y - goalPos.y;


	Real goalSpeed = (1.0f - angleCoeff) * desiredSpeed;


//	if (speed < m_minTurnSpeed)
//		speed = m_minTurnSpeed;

	Real actualSpeed = physics->getForwardSpeed2D();
	Real slowDownTime = actualSpeed / getBraking();
	Real slowDownDist = (actualSpeed/1.50f) * slowDownTime;

	if (sqr(dx)+sqr(dy)<sqr(2*PATHFIND_CELL_SIZE_F) && angleCoeff > 0.05) {
		goalSpeed = actualSpeed*0.6f;
	}

 	if (onPathDistToGoal < slowDownDist && !getFlag(IS_BRAKING) && !getFlag(NO_SLOW_DOWN_AS_APPROACHING_DEST))
	{
		setFlag(IS_BRAKING, true);
		m_brakingFactor = 1.1f;
	}

	if (onPathDistToGoal>PATHFIND_CELL_SIZE_F && onPathDistToGoal > 2.0*slowDownDist)
	{
		setFlag(IS_BRAKING, false);
	}

	if (getFlag(IS_BRAKING))
	{
		m_brakingFactor = slowDownDist/onPathDistToGoal;
		m_brakingFactor *= m_brakingFactor;
		if (m_brakingFactor>MAX_BRAKING_FACTOR) {
			m_brakingFactor = MAX_BRAKING_FACTOR;
		}
		if (slowDownDist>onPathDistToGoal) {
			goalSpeed = actualSpeed-getBraking();
			if (goalSpeed<0.0f) goalSpeed= 0.0f;
		} else if (slowDownDist>onPathDistToGoal*0.75f) {
			goalSpeed = actualSpeed-getBraking()/2.0f;
			if (goalSpeed<0.0f) goalSpeed = 0.0f;
		} else {
			goalSpeed = actualSpeed;
		}
	}


	//DEBUG_LOG(("Actual speed %f, Braking factor %f, slowDownDist %f, Pathdist %f, goalSpeed %f",
	//	actualSpeed, m_brakingFactor, slowDownDist, onPathDistToGoal, goalSpeed));

	//
	// Maintain goal speed
	//
	Real speedDelta = goalSpeed - actualSpeed;
	if (speedDelta != 0.0f)
	{
		Real mass = physics->getMass();
		Real acceleration = (speedDelta > 0.0f) ? maxAcceleration : -m_brakingFactor*getBraking();
		Real accelForce = mass * acceleration;

		/*
			don't accelerate/brake more than necessary. do a quick calc to
			see how much force we really need to achieve our goal speed...
		*/
		Real maxForceNeeded = mass * speedDelta;
		if (fabs(accelForce) > fabs(maxForceNeeded))
			accelForce = maxForceNeeded;

		const Coord3D *dir = obj->getUnitDirectionVector2D();

		Coord3D force;
		force.x = accelForce * dir->x;
		force.y = accelForce * dir->y;
		force.z = 0.0f;

		// apply forces to object
		physics->applyMotiveForce( &force );
	}
}

//-------------------------------------------------------------------------------------------------
void Locomotor::moveTowardsPositionWheels(Object* obj, PhysicsBehavior *physics, const Coord3D& goalPos, Real onPathDistToGoal, Real desiredSpeed)
{
	BodyDamageType bdt = obj->getBodyModule()->getDamageState();
	Real maxSpeed = getMaxSpeedForCondition(bdt, obj);
	Real maxTurnRate = getMaxTurnRate(bdt);
	Real maxAcceleration = getMaxAcceleration(bdt);

	// sanity, we cannot use desired speed that is greater than our max speed we are capable of moving at
	if( desiredSpeed > maxSpeed )
		desiredSpeed = maxSpeed;

	// Locomotion for wheeled vehicles, ie trucks.
	//
	// See if we are turning.  If so, use the min turn speed.
	//
	Real turnSpeed = getMinTurnSpeed();
	Real angle = obj->getOrientation();
//	Real relAngle = ThePartitionManager->getRelativeAngle2D( obj, &goalPos );
//	Real desiredAngle = angle + relAngle;
	Real desiredAngle = WWMath::Atan2Origin(goalPos.y - obj->getPosition()->y, goalPos.x - obj->getPosition()->x);
	Real relAngle = stdAngleDiff(desiredAngle, angle);

	Bool moveBackwards = false;

	// Wheeled vehicles can only turn while moving, so make sure the turn speed is reasonable.
	if (turnSpeed < maxSpeed/4.0f)
	{
		turnSpeed = maxSpeed/4.0f;
	}


	Real actualSpeed = physics->getForwardSpeed2D();
	Bool do3pointTurn = false;
#if 1
	if (actualSpeed==0.0f) {
		setFlag(MOVING_BACKWARDS, false);
		if (m_template->m_canMoveBackward && fabs(relAngle) > PI/2) {
			setFlag(MOVING_BACKWARDS, true );
			setFlag(DOING_THREE_POINT_TURN, onPathDistToGoal>5*obj->getGeometryInfo().getMajorRadius());
		}

	}
	if (getFlag(MOVING_BACKWARDS)) {
		if (fabs(relAngle) < PI/2) {
			moveBackwards = false;
			setFlag(MOVING_BACKWARDS, false);
		} else {
			moveBackwards = true;
			setFlag(DOING_THREE_POINT_TURN, onPathDistToGoal>5*obj->getGeometryInfo().getMajorRadius());
			do3pointTurn = getFlag(DOING_THREE_POINT_TURN);
			if (!do3pointTurn) {
				desiredAngle = stdAngleDiff(desiredAngle, PI);
				relAngle = stdAngleDiff(desiredAngle, angle);
			}
		}
	}
#endif

	const Real SMALL_TURN = PI / 20.0f;
	if ((Real)fabs( relAngle ) > SMALL_TURN)
	{
		if (desiredSpeed>turnSpeed)
		{
			desiredSpeed = turnSpeed;
		}
	}

	Real goalSpeed = desiredSpeed;
	if (moveBackwards) {
		actualSpeed = -actualSpeed;
	}



	Real slowDownTime = actualSpeed / getBraking() + 1.0f;
	Real slowDownDist = (actualSpeed/1.5f) * slowDownTime + actualSpeed;
	Real effectiveSlowDownDist = slowDownDist;
	if (effectiveSlowDownDist < 1*PATHFIND_CELL_SIZE) {
		effectiveSlowDownDist = 1*PATHFIND_CELL_SIZE;
	}


	const Real FIFTEEN_DEGREES = PI / 12.0f;
	const Real PROJECT_FRAMES = LOGICFRAMES_PER_SECOND/2; // Project out 1/2 second.
	if (fabs( relAngle ) > FIFTEEN_DEGREES)
	{
		// If we're turning more than 10 degrees, check & see if we're moving into "impassable territory"
		Real distance = PROJECT_FRAMES * (goalSpeed+actualSpeed)/2.0f;
		Real targetAngle = obj->getOrientation();
		Real turnFactor = ((goalSpeed+actualSpeed)/2.0f)/turnSpeed;
		if (turnFactor > 1.0f)
			turnFactor = 1.0f;
		Real turnAmount = PROJECT_FRAMES*turnFactor*maxTurnRate/4.0f;
		if (relAngle < 0)
		{
			targetAngle -= turnAmount;
		}
		else
		{
			targetAngle += turnAmount;
		}
		Coord3D offset;
		offset.x = Cos(targetAngle)*distance;
		offset.y = Sin(targetAngle)*distance;
		offset.z = 0;

		const Coord3D* pos = obj->getPosition();

		Coord3D nextPos;
		nextPos.x = pos->x+offset.x;
		nextPos.y = pos->y+offset.y;
		nextPos.z = pos->z;

		pos = obj->getPosition();

		Coord3D halfPos;
		halfPos.x = pos->x+offset.x/2;
		halfPos.y = pos->y+offset.y/2;
		halfPos.z = pos->z;

		if (!TheAI->pathfinder()->validMovementTerrain(obj->getLayer(), this, &halfPos) ||
			!TheAI->pathfinder()->validMovementTerrain(obj->getLayer(), this, &nextPos))
		{
			PhysicsTurningType rotating = rotateTowardsPosition(obj, goalPos);
			physics->setTurning(rotating);

			// apply a zero force to object so that it acts "driven"
			Coord3D force;
			force.zero();
			physics->applyMotiveForce( &force );
			return;
		}

	}

	if (onPathDistToGoal < effectiveSlowDownDist && !getFlag(IS_BRAKING) && !getFlag(NO_SLOW_DOWN_AS_APPROACHING_DEST))
	{
		setFlag(IS_BRAKING, true);
		m_brakingFactor = 1.1f;
	}


	if (onPathDistToGoal>PATHFIND_CELL_SIZE_F && onPathDistToGoal > 2.0*slowDownDist)
	{
		setFlag(IS_BRAKING, false);
	}

 	if (onPathDistToGoal > DONUT_DISTANCE) {
		m_donutTimer = TheGameLogic->getFrame()+DONUT_TIME_DELAY_SECONDS*static_cast<float>(LOGICFRAMES_PER_SECOND);
	} else {
		if (m_donutTimer < TheGameLogic->getFrame()) {
			setFlag(IS_BRAKING, true);
		}
	}

	if (getFlag(IS_BRAKING))
	{
		m_brakingFactor = slowDownDist/onPathDistToGoal;
		m_brakingFactor *= m_brakingFactor;
		if (m_brakingFactor>MAX_BRAKING_FACTOR) {
			m_brakingFactor = MAX_BRAKING_FACTOR;
		}
		m_brakingFactor = 1.0f;
		if (slowDownDist>onPathDistToGoal) {
			goalSpeed = actualSpeed-getBraking();
			if (goalSpeed<0.0f) goalSpeed= 0.0f;
		} else if (slowDownDist>onPathDistToGoal*0.75f) {
			goalSpeed = actualSpeed-getBraking()/2.0f;
			if (goalSpeed<0.0f) goalSpeed = 0.0f;
		} else {
			goalSpeed = actualSpeed;
		}
	}


	//DEBUG_LOG(("Actual speed %f, Braking factor %f, slowDownDist %f, Pathdist %f, goalSpeed %f",
	//	actualSpeed, m_brakingFactor, slowDownDist, onPathDistToGoal, goalSpeed));


	// Wheeled can only turn while moving.
	Real turnFactor = actualSpeed/turnSpeed;
	if (turnFactor<0) {
		turnFactor = -turnFactor; // in case we're sliding backwards in a 3 pt turn.
	}
	if (turnFactor > 1.0f)
		turnFactor = 1.0f;
	Real turnAmount = turnFactor*maxTurnRate;

	PhysicsTurningType rotating;
	if (moveBackwards && !do3pointTurn) {
		Coord3D backwardPos = *obj->getPosition();
		backwardPos.x += -(goalPos.x - obj->getPosition()->x);
		backwardPos.y += -(goalPos.y - obj->getPosition()->y);
		rotating = rotateObjAroundLocoPivot(obj, backwardPos, turnAmount);
	} else {
		rotating = rotateObjAroundLocoPivot(obj, goalPos, turnAmount);
	}

	physics->setTurning(rotating);

	//
	// Maintain goal speed
	//
	updateRetailGait(obj, onPathDistToGoal, actualSpeed, getFlag(IS_BRAKING) || goalSpeed + MIN_GAIT_SPEED < actualSpeed);
	Real speedDelta = goalSpeed - actualSpeed;
	if (moveBackwards) {
		speedDelta = -goalSpeed+actualSpeed;
	}
	if (speedDelta != 0.0f)
	{
		Real mass = physics->getMass();
		Real acceleration;
		if (moveBackwards) {
			acceleration = (speedDelta < 0.0f) ? -maxAcceleration : m_brakingFactor*getBraking();
		}	else {
			acceleration = (speedDelta > 0.0f) ? maxAcceleration : -m_brakingFactor*getBraking();
		}
		Real accelForce = mass * acceleration;

		/*
			don't accelerate/brake more than necessary. do a quick calc to
			see how much force we really need to achieve our goal speed...
		*/
		Real maxForceNeeded = mass * speedDelta;
		if (fabs(accelForce) > fabs(maxForceNeeded))
			accelForce = maxForceNeeded;

		//DEBUG_LOG(("Braking %d, actualSpeed %f, goalSpeed %f, delta %f, accel %f", getFlag(IS_BRAKING),
			//actualSpeed, goalSpeed, speedDelta, accelForce));

		const Coord3D *dir = obj->getUnitDirectionVector2D();

		Coord3D force;
		force.x = accelForce * dir->x;
		force.y = accelForce * dir->y;
		force.z = 0.0f;

		// apply forces to object
		physics->applyMotiveForce( &force );
	}

}
//-------------------------------------------------------------------------------------------------
Bool Locomotor::fixInvalidPosition(Object* obj, PhysicsBehavior *physics)
{
	if (obj->isKindOf(KINDOF_DOZER)) {
		// don't fix him.
		return false;
	}
#define no_IGNORE_INVALID
#ifdef IGNORE_INVALID
	// Right now we ignore invalid positions, so when units clip the edge of a building or cliff
	// they don't get stuck.  jba. 12SEPT02
	return false;
#else
	Int dx = 0;
	Int dy = 0;
	Int i, j;
	for (j=-1; j<2; j++) {
		for (i=-1; i<2; i++) {
			Coord3D thePos = *obj->getPosition();
			thePos.x += i*PATHFIND_CELL_SIZE_F;
			thePos.y += j*PATHFIND_CELL_SIZE_F;
			if (!TheAI->pathfinder()->validMovementTerrain(obj->getLayer(), this, &thePos)) {
				if (i<0) dx += 1;
				if (i>0) dx -= 1;
				if (j<0) dy += 1;
				if (j>0) dy -= 1;
			}
		}
	}
	if (dx || dy) {

		Coord3D correction;
		correction.x = dx*physics->getMass()/5;
		correction.y = dy*physics->getMass()/5;
		correction.z = 0;

		Coord3D correctionNormalized = correction;
		correctionNormalized.normalize();

		Coord3D velocity;
		// Kill current velocity	in the direction of the correction.
		velocity = *physics->getVelocity();
		Real dot = (velocity.x*correctionNormalized.x) + (velocity.y*correctionNormalized.y);
		if (dot>.25f) {
			// It was already leaving.
			return false;
		}


		// Kill current accel
		//physics->clearAcceleration();

		if (dot<0) {
			dot = WWMath::SqrtOrigin(-dot);
			correctionNormalized.x *= dot*physics->getMass();
			correctionNormalized.y *= dot*physics->getMass();
			physics->applyMotiveForce(&correctionNormalized);
		}

		// apply correction.
		physics->applyMotiveForce(&correction);
		return true;
	}
	return false;
#endif
}

//-------------------------------------------------------------------------------------------------
Real Locomotor::calcMinTurnRadius(BodyDamageType condition, Real* timeToTravelThatDist) const
{
	Real minSpeed = getMinSpeed();								// in dist/frame
	Real maxTurnRate = getMaxTurnRate(condition);	// in rads/frame

	/*
		our minimum circumference will be like so:

		Real minTurnCircum = maxSpeed * (2*PI / maxTurnRate);

		so therefore our minimum turn radius is:

		Real minTurnRadius = minTurnCircum / 2*PI;

		so we just eliminate the middleman:
	*/
	// if we can't turn, return a huge-but-finite radius rather than NAN...
	Real minTurnRadius = (maxTurnRate > 0.0f) ? minSpeed / maxTurnRate : BIGNUM;

	if (timeToTravelThatDist)
		*timeToTravelThatDist = (minSpeed > 0.0f) ? (minTurnRadius / minSpeed) : 0.0f;

	return minTurnRadius;
}


Real Locomotor::rotwkLegsTurnSlowdown(const Object* obj, const Coord3D& goalPos, Real angleCoeff, Real maxSpeed) const
{
	const Real HUGE_LEGS_MAX_SLOWDOWN = 0.9f;
	if (m_template->m_appearance == LOCO_FOUR_LEGS_HUGE || m_template->m_appearance == LOCO_HUGE_TWO_LEGS)
		return angleCoeff < HUGE_LEGS_MAX_SLOWDOWN ? angleCoeff : HUGE_LEGS_MAX_SLOWDOWN;

	const Real turnRate = getMaxTurnRate(obj->getBodyModule()->getDamageState());
	if (turnRate <= 0.0f)
		return angleCoeff;
	const Real turnRadius = maxSpeed / turnRate;
	const Real dx = goalPos.x - obj->getPosition()->x;
	const Real dy = goalPos.y - obj->getPosition()->y;
	const Real orbitDistance = 2.0f * turnRadius;
	return dx * dx + dy * dy < orbitDistance * orbitDistance ? angleCoeff : 0.0f;
}

void Locomotor::moveRetailGroundTowardsPosition(Object* obj, PhysicsBehavior* physics, const Coord3D& goalPos,
	Real onPathDistToGoal, Real desiredSpeed, Bool travelTowardGoal)
{
	const Coord3D& position = *obj->getPosition();
	if (getIsDownhillOnly() && position.z < goalPos.z)
		return;

	setFlag(IS_BRAKING, false);
	const Real dx = goalPos.x - position.x;
	const Real dy = goalPos.y - position.y;
	const Real distance = WWMath::SqrtfOrigin(sqr(dx) + sqr(dy));
	const Coord3D& velocity = *physics->getVelocity();
	const Real speed = WWMath::SqrtfOrigin(sqr(velocity.x) + sqr(velocity.y));
	const UnsignedInt framesPerRetailStep = RotwkDuration::engineFrames(1, LOGICFRAMES_PER_SECOND);
	const BodyDamageType damage = obj->getBodyModule()->getDamageState();
	const Real acceleration = getMaxAcceleration(damage);
	const Bool mounted = m_template->m_appearance == LOCO_WHEELS_FOUR;
	const Real retailTravelFloor = PATHFIND_CELL_SIZE_F / static_cast<Real>(LOGICFRAMES_PER_SECOND);
	const Real minimumTravelSpeed = mounted ? std::min(desiredSpeed, retailTravelFloor) : 0.0f;
	const Bool startingMountedTurn = mounted && speed <= acceleration * framesPerRetailStep;
	const Real facing = obj->getOrientation();
	const Real goalHeading = distance > 0.0f ? WWMath::Atan2Origin(dy, dx) : facing;
	const Real headingError = stdAngleDiff(goalHeading, facing);
	const Real angle = fabs(headingError);
	const Real maxSpeed = getMaxSpeedForCondition(damage, obj);
	const Real radius = mounted && !travelTowardGoal ? getRetailTurnRadius(speed, maxSpeed) : 0.0f;
	Real travelHeading = facing;
	if (distance > 0.0f)
	{
		Real turnRate = getMaxTurnRate(damage);
		if (radius > 0.0f && speed > 0.0f && !startingMountedTurn)
			turnRate = std::min(turnRate, speed / radius);
		physics->setTurning(rotateObjAroundLocoPivot(obj, goalPos, turnRate));

		const Real newFacing = obj->getOrientation();
		const Real previousTravel = speed > 0.0f ? WWMath::Atan2Origin(velocity.y, velocity.x) : facing;
		if (mounted)
		{
			if (speed <= 0.25f * maxSpeed)
			{
				const Real adjustedAngle = startingMountedTurn ? 2.0f * fabs(stdAngleDiff(goalHeading, newFacing)) : angle;
				desiredSpeed *= std::max(0.0f, 1.0f - adjustedAngle / (PI / 4.0f));
			}
			if (travelTowardGoal)
				travelHeading = goalHeading;
			else
			{
				const Real lateral = fabs(Sin(angle));
				if (m_template->m_fastTurnRadius > 0.0f && lateral > 0.0f)
					desiredSpeed = std::min(desiredSpeed, maxSpeed * distance / (2.0f * m_template->m_fastTurnRadius * lateral));
				const Real settleFrames = std::max(1.0f, 0.5f * framesPerRetailStep);
				travelHeading = previousTravel + stdAngleDiff(newFacing, previousTravel) / settleFrames;
			}
		}
		else
		{
			const Real retailTurn = turnRate * framesPerRetailStep;
			const Real previousLead = stdAngleDiff(previousTravel, facing);
			const Bool travelLeads = angle > retailTurn ||
				(fabs(previousLead) > turnRate && previousLead * headingError > 0.0f);
			if (travelLeads && angle > turnRate && turnRate > 0.0f && speed > 0.0f)
			{
				const Real remaining = fabs(stdAngleDiff(goalHeading, previousTravel));
				const Real travelTurn = std::min(remaining, 2.0f * turnRate);
				const Real nextTravel = previousTravel + (headingError > 0.0f ? travelTurn : -travelTurn);
				const Real lead = stdAngleDiff(nextTravel, newFacing);
				travelHeading = newFacing + std::clamp(lead, -0.5f * retailTurn, 0.5f * retailTurn);
			}
			else if (angle <= turnRate || speed == 0.0f || distance <= speed + acceleration)
				travelHeading = newFacing;
		}
	}
	else
		updateRetailTurnConditions(obj, 0.0f);

	const Real braking = getBraking();
	if (!getFlag(NO_SLOW_DOWN_AS_APPROACHING_DEST) &&
		onPathDistToGoal < calcSlowDownDist(speed, getMinSpeed(), braking))
		desiredSpeed = getMinSpeed();

	const Real steeredSpeed = speed + std::clamp(desiredSpeed - speed, -braking, acceleration);
	const Real nextSpeed = std::min(distance, std::max(minimumTravelSpeed, steeredSpeed));
	if (steeredSpeed < minimumTravelSpeed)
		travelHeading = goalHeading;
	physics->scrubVelocity2D(0.0f);
	Coord3D direction{Cos(travelHeading), Sin(travelHeading), 0.0f};
	direction.scale(physics->getMass() * nextSpeed);
	physics->applyMotiveForce(&direction);
	updateRetailGait(obj, onPathDistToGoal, speed, desiredSpeed + MIN_GAIT_SPEED < speed, nextSpeed);
}

//-------------------------------------------------------------------------------------------------
void Locomotor::moveTowardsPositionLegs(Object* obj, PhysicsBehavior *physics, const Coord3D& goalPos, Real onPathDistToGoal, Real desiredSpeed)
{
	if (getIsDownhillOnly() && obj->getPosition()->z < goalPos.z)
	{
		return;
	}

	Real maxAcceleration = getMaxAcceleration( obj->getBodyModule()->getDamageState() );

	// sanity, we cannot use desired speed that is greater than our max speed we are capable of moving at
	Real maxSpeed = getMaxSpeedForCondition( obj->getBodyModule()->getDamageState(), obj );
	if( desiredSpeed > maxSpeed )
		desiredSpeed = maxSpeed;

	// Locomotion for infantry.
	//
	// Orient toward goal position
	//
	Real actualSpeed = physics->getForwardSpeed2D();
	Real angle = obj->getOrientation();
//	Real relAngle = ThePartitionManager->getRelativeAngle2D( obj, &goalPos );
//	Real desiredAngle = angle + relAngle;
	Real desiredAngle = WWMath::Atan2Origin(goalPos.y - obj->getPosition()->y, goalPos.x - obj->getPosition()->x);

	if (m_template->m_wanderWidthFactor != 0.0f) {
		Real angleLimit = PI/8 * m_template->m_wanderWidthFactor;
		// This is the wander offline code - it forces the desired angle away from the goal, so we wander back & forth.  jba.
		if (getFlag(OFFSET_INCREASING)) {
			m_angleOffset += m_offsetIncrement*actualSpeed;
			if (m_angleOffset > angleLimit) {
				setFlag(OFFSET_INCREASING, false);
			}
		} else {
			m_angleOffset -= m_offsetIncrement*actualSpeed;
			if (m_angleOffset<-angleLimit) {
				setFlag(OFFSET_INCREASING, true);
			}
		}
		desiredAngle = normalizeAngle(desiredAngle+m_angleOffset);
	}

	Real relAngle = stdAngleDiff(desiredAngle, angle);
	locoUpdate_moveTowardsAngle(obj, desiredAngle);

	//
	// Modulate speed according to turning. The more we have to turn, the slower we go
	//
	const Real QUARTERPI = PI / 4.0f;
	Real angleCoeff = (Real)fabs( relAngle ) / (QUARTERPI);
	if (angleCoeff > 1.0f)
		angleCoeff = 1.0;
	if (RetailFileLayers::enabled())
		angleCoeff = rotwkLegsTurnSlowdown(obj, goalPos, angleCoeff, maxSpeed);

	Real goalSpeed = (1.0f - angleCoeff) * desiredSpeed;

	//Real slowDownDist = (actualSpeed - m_template->m_minSpeed) / getBraking();
	Real slowDownDist = calcSlowDownDist(actualSpeed, getMinSpeed(), getBraking());
	if (onPathDistToGoal < slowDownDist && !getFlag(NO_SLOW_DOWN_AS_APPROACHING_DEST))
	{
		goalSpeed = getMinSpeed();
	}



	//
	// Maintain goal speed
	//
	Real speedDelta = goalSpeed - actualSpeed;
	if (speedDelta != 0.0f)
	{
		Real mass = physics->getMass();
		Real acceleration = (speedDelta > 0.0f) ? maxAcceleration : -getBraking();
		Real accelForce = mass * acceleration;

		/*
			don't accelerate/brake more than necessary. do a quick calc to
			see how much force we really need to achieve our goal speed...
		*/
		Real maxForceNeeded = mass * speedDelta;
		if (fabs(accelForce) > fabs(maxForceNeeded))
			accelForce = maxForceNeeded;

		const Coord3D *dir = obj->getUnitDirectionVector2D();

		Coord3D force;
		force.x = accelForce * dir->x;
		force.y = accelForce * dir->y;
		force.z = 0.0f;



		// apply forces to object
		physics->applyMotiveForce( &force );
	}
}

//-------------------------------------------------------------------------------------------------
void Locomotor::moveTowardsPositionClimb(Object* obj, PhysicsBehavior *physics, const Coord3D& goalPos, Real onPathDistToGoal, Real desiredSpeed)
{
	Real maxAcceleration = getMaxAcceleration( obj->getBodyModule()->getDamageState() );

	// sanity, we cannot use desired speed that is greater than our max speed we are capable of moving at
	Real maxSpeed = getMaxSpeedForCondition( obj->getBodyModule()->getDamageState(), obj );
	if( desiredSpeed > maxSpeed )
		desiredSpeed = maxSpeed;

	// Locomotion for climbing infantry.


	Bool moveBackwards = false;

	Real dx, dy, dz;

	Coord3D pos = *obj->getPosition();

	dx = pos.x - goalPos.x;
	dy = pos.y - goalPos.y;
	dz = pos.z - goalPos.z;
	if (dz*dz > sqr(PATHFIND_CELL_SIZE_F)) {
		setFlag(CLIMBING, true);
	}
	if (fabs(dz)<1) {
		setFlag(CLIMBING, false);
	}


	//setFlag(CLIMBING, true);

	if (getFlag(CLIMBING)) {
		Coord3D delta = goalPos;
		delta.x -= pos.x;
		delta.y -= pos.y;
		delta.z = 0;
		delta.normalize();
		delta.x += pos.x;
		delta.y += pos.y;
		delta.z = TheTerrainLogic->getGroundHeight(delta.x, delta.y);
		if (delta.z < pos.z-0.1) {
			moveBackwards = true;
		}

		Real groundSlope = fabs(delta.z - pos.z);
		if (groundSlope<1.0f) groundSlope = 1.0f;

		if (groundSlope>1.0f) {
			desiredSpeed /= groundSlope*4;
		}
	}
	setFlag(MOVING_BACKWARDS, moveBackwards);

	//
	// Orient toward goal position
	//
	Real angle = obj->getOrientation();
//	Real relAngle = ThePartitionManager->getRelativeAngle2D( obj, &goalPos );
//	Real desiredAngle = angle + relAngle;
	Real desiredAngle = WWMath::Atan2Origin(goalPos.y - obj->getPosition()->y, goalPos.x - obj->getPosition()->x);
	Real relAngle = stdAngleDiff(desiredAngle, angle);

	if (moveBackwards) {
		desiredAngle = stdAngleDiff(desiredAngle, PI);
		relAngle = stdAngleDiff(desiredAngle, angle);
	}

	locoUpdate_moveTowardsAngle(obj, desiredAngle);

	//
	// Modulate speed according to turning. The more we have to turn, the slower we go
	//
	const Real QUARTERPI = PI / 4.0f;
	Real angleCoeff = (Real)fabs( relAngle ) / (QUARTERPI);
	if (angleCoeff > 1.0f)
		angleCoeff = 1.0;

	Real goalSpeed = (1.0f - angleCoeff) * desiredSpeed;

	Real actualSpeed = physics->getForwardSpeed2D();

	if (moveBackwards) {
		actualSpeed = -actualSpeed;
	}

	//Real slowDownDist = (actualSpeed - m_template->m_minSpeed) / getBraking();
	Real slowDownDist = calcSlowDownDist(actualSpeed, getMinSpeed(), getBraking());
	if (onPathDistToGoal < slowDownDist && !getFlag(NO_SLOW_DOWN_AS_APPROACHING_DEST))
	{
		goalSpeed = getMinSpeed();
	}

	//
	// Maintain goal speed
	//
	Real speedDelta = goalSpeed - actualSpeed;
	if (moveBackwards) {
		speedDelta = -goalSpeed+actualSpeed;
	}
	if (speedDelta != 0.0f)
	{
		Real mass = physics->getMass();
		Real acceleration;
		if (moveBackwards) {
			acceleration = (speedDelta < 0.0f) ? -maxAcceleration : getBraking();
		}	else {
			acceleration = (speedDelta > 0.0f) ? maxAcceleration : -getBraking();
		}
		Real accelForce = mass * acceleration;

		/*
			don't accelerate/brake more than necessary. do a quick calc to
			see how much force we really need to achieve our goal speed...
		*/
		Real maxForceNeeded = mass * speedDelta;
		if (fabs(accelForce) > fabs(maxForceNeeded))
			accelForce = maxForceNeeded;

		const Coord3D *dir = obj->getUnitDirectionVector2D();

		Coord3D force;
		force.x = accelForce * dir->x;
		force.y = accelForce * dir->y;
		force.z = 0.0f;

		// apply forces to object
		physics->applyMotiveForce( &force );
	}
}

//-------------------------------------------------------------------------------------------------
void Locomotor::moveTowardsPositionWings(Object* obj, PhysicsBehavior *physics, const Coord3D& goalPos, Real onPathDistToGoal, Real desiredSpeed)
{
#ifdef CIRCLE_FOR_LANDING
	if (m_circleThresh > 0.0f)
	{
		// if we are going a mostly-vertical maneuver, circle in order to
		// gain/lose altitude, then resume course...
		const Coord3D* pos = obj->getPosition();
		Real dx = goalPos.x - pos->x;
		Real dy = goalPos.y - pos->y;
		Real dz = goalPos.z - pos->z;
		if (fabs(dz) > m_circleThresh)
		{
			// aim for the spot on the opposite side of the circle.

			// find the direction towards our goal pos
			Real angleTowardPos =
					(isNearlyZero(dx) && isNearlyZero(dy)) ?
					obj->getOrientation() :
					WWMath::Atan2Origin(dy, dx);

			Real aimDir = (PI - PI/8);
			angleTowardPos += aimDir;

			BodyDamageType bdt = obj->getBodyModule()->getDamageState();
			Real turnRadius = calcMinTurnRadius(bdt, nullptr) * 4;

			// project a spot "radius" dist away from it, in that dir
			Coord3D desiredPos = goalPos;
			desiredPos.x += Cos(angleTowardPos) * turnRadius;
			desiredPos.y += Sin(angleTowardPos) * turnRadius;
			moveTowardsPositionOther(obj, physics, desiredPos, 0, desiredSpeed);
			return;
		}
	}
#endif

	// handle the 2D component.
	moveTowardsPositionOther(obj, physics, goalPos, onPathDistToGoal, desiredSpeed);
}

//-------------------------------------------------------------------------------------------------
void Locomotor::moveTowardsPositionHover(Object* obj, PhysicsBehavior *physics, const Coord3D& goalPos, Real onPathDistToGoal, Real desiredSpeed)
{
	// handle the 2D component.
	moveTowardsPositionOther(obj, physics, goalPos, onPathDistToGoal, desiredSpeed);

	// Only hover locomotors care about their OverWater special effects.  (OverWater also affects speed, so this is not a client thing)
	Coord3D newPosition = *obj->getPosition();
	if( TheTerrainLogic->isUnderwater( newPosition.x, newPosition.y ) )
	{
		if( ! getFlag( OVER_WATER ) )
		{
			// Change my model condition because I used to not be over water, but now I am
			setFlag( OVER_WATER, TRUE );
			obj->setModelConditionState( MODELCONDITION_OVER_WATER );
		}
	}
	else
	{
		if( getFlag( OVER_WATER ) )
		{
			// Here, I was, but now I'm not
			setFlag( OVER_WATER, FALSE );
			obj->clearModelConditionState( MODELCONDITION_OVER_WATER );
		}
	}
}

//-------------------------------------------------------------------------------------------------
void Locomotor::moveTowardsPositionThrust(Object* obj, PhysicsBehavior *physics, const Coord3D& goalPos, Real onPathDistToGoal, Real desiredSpeed)
{
	BodyDamageType bdt = obj->getBodyModule()->getDamageState();

	Real maxForwardSpeed = getMaxSpeedForCondition(bdt, obj);
	desiredSpeed = clamp(getMinSpeed(), desiredSpeed, maxForwardSpeed);
	Real actualForwardSpeed = physics->getForwardSpeed3D();

	if (getBraking() > 0)
	{
		//Real slowDownDist = (actualForwardSpeed - m_template->m_minSpeed) / getBraking();
		Real slowDownDist = calcSlowDownDist(actualForwardSpeed, getMinSpeed(), getBraking());
		if (onPathDistToGoal < slowDownDist && !getFlag(NO_SLOW_DOWN_AS_APPROACHING_DEST))
			desiredSpeed = getMinSpeed();
	}

	Coord3D localGoalPos = goalPos;
#ifdef USE_ZDIR_DAMPING
	Real zDirDamping = 0.0f;
#endif

	//out of the handleBehaviorZ() function
	Coord3D pos = *obj->getPosition();
	if( m_preferredHeight != 0.0f && !getFlag(PRECISE_Z_POS) )
	{
		// If we have a preferred flight height, and we haven't been told explicitly to ignore it...
		Real surfaceHt =  getSurfaceHtAtPt(pos.x, pos.y);
		localGoalPos.z = m_preferredHeight + surfaceHt;
//		localGoalPos.z = goalPos.z;
		Real delta = localGoalPos.z - pos.z;
		delta *= getPreferredHeightDamping();
		localGoalPos.z = pos.z + delta;

#ifdef USE_ZDIR_DAMPING
		// closer we get to the preferred height, less we adjust z-thrust,
		// so we tend to "level out" at that height. we don't use this till
		// below, but go ahead and calc it now...
		Real MAX_VERTICAL_DAMP_RANGE = m_preferredHeight * 0.5;
		delta = fabs(delta);
		if (delta > MAX_VERTICAL_DAMP_RANGE)
			delta = MAX_VERTICAL_DAMP_RANGE;
		zDirDamping = 1.0f - (delta / MAX_VERTICAL_DAMP_RANGE);
#endif
	}

	Vector3 forwardDir = obj->getTransformMatrix()->Get_X_Vector();

	// Maintain goal speed
	Real forwardSpeedDelta = desiredSpeed - actualForwardSpeed;
	Real maxAccel = (forwardSpeedDelta > 0.0f || getBraking() == 0) ? getMaxAcceleration(bdt) : -getBraking();
	Real maxTurnRate = getMaxTurnRate(bdt);

	// what direction do we need to thrust in, in order to reach the goalpos?
	Vector3 desiredThrustDir;
	calcDirectionToApplyThrust(obj, physics, localGoalPos, maxAccel, desiredThrustDir);

	// we might not be able to thrust in that dir, so thrust as closely as we can
	Real maxThrustAngle =	(maxTurnRate > 0) ? (m_template->m_maxThrustAngle) : 0;
	Vector3 thrustDir;
	Real thrustAngle = tryToRotateVector3D(maxThrustAngle, forwardDir, desiredThrustDir, thrustDir);

	// note that we are trying to orient in the direction of our vel, not the dir of our thrust.
	if (!isNearlyZero(physics->getVelocityMagnitude()))
	{
		const Coord3D* veltmp = physics->getVelocity();
		Vector3 vel(veltmp->x, veltmp->y, veltmp->z);
		Bool adjust = true;
		if( obj->getStatusBits().test( OBJECT_STATUS_BRAKING ) )
		{
			// align to target, cause that's where we're going anyway.

			vel.Set(goalPos.x - pos.x, goalPos.y-pos.y, goalPos.z-pos.z);
			if (isNearlyZero(sqr(vel.X)+sqr(vel.Y)+sqr(vel.Z))) {
				// we are at target.
				adjust = false;
			}
			maxTurnRate = 3*maxTurnRate;
		}
#ifdef USE_ZDIR_DAMPING
		if (zDirDamping != 0.0f)
		{
			Vector3 vel2D(veltmp->x, veltmp->y, 0);
			// no need to normalize -- this call does that internally
			tryToRotateVector3D(-zDirDamping, vel, vel2D, vel);
		}
#endif
		if (adjust) {
			/*Real orient =*/ tryToOrientInThisDirection3D(obj, maxTurnRate, vel);
		}
	}

	if (forwardSpeedDelta != 0.0f || thrustAngle != 0.0f)
	{
		if (maxForwardSpeed <= 0.0f)
		{
			maxForwardSpeed = 0.01f; // In some cases, this is 0, hack for now.  jba.
		}
		Real damping = clamp(0.0f, maxAccel / maxForwardSpeed, 1.0f);
		Vector3 curVel(physics->getVelocity()->x, physics->getVelocity()->y, physics->getVelocity()->z);

		Vector3 accelVec = thrustDir * maxAccel - curVel * damping;
		//DEBUG_LOG(("accel %f (max %f) vel %f (max %f) damping %f",accelVec.Length(),maxAccel,curVel.Length(),maxForwardSpeed,damping));

		Real mass = physics->getMass();

		Coord3D force;
		force.x = mass * accelVec.X;
		force.y = mass * accelVec.Y;
		force.z = mass * accelVec.Z;

		// apply forces to object
		physics->applyMotiveForce( &force );
	}
}

//-------------------------------------------------------------------------------------------------
Real Locomotor::getSurfaceHtAtPt(Real x, Real y)
{
	Real ht = 0;

	Real z,waterZ;
	if (TheTerrainLogic->isUnderwater(x, y, &waterZ, &z)) {
		ht += waterZ;
	} else {
		ht += z;
	}

	return ht;
}

//-------------------------------------------------------------------------------------------------
Real Locomotor::calcLiftToUseAtPt(Object* obj, PhysicsBehavior *physics, Real curZ, Real surfaceAtPt, Real preferredHeight, Bool birdPullUp)
{
	/*
		take the classic equation:

			x = x0 + v*t + 0.5*a*t^2

		and solve for acceleration.
	*/
	BodyDamageType bdt = obj->getBodyModule()->getDamageState();
	Real maxGrossLift = getMaxLift(bdt);
	if (RetailFileLayers::enabled() && (getAppearance() == LOCO_WINGS || getAppearance() == LOCO_GIANT_BIRD) &&
		maxGrossLift > 0.0f && !obj->isEffectivelyDead())
	{
		const Real maxVerticalSpeed = min(m_template->m_speedLimitZ, maxGrossLift);
		Real maxDescentSpeed = maxVerticalSpeed;
		Real maxClimbSpeed = maxVerticalSpeed;
		const Coord3D& velocity = *physics->getVelocity();
		Real maxDownwardAcceleration = maxGrossLift;
		if (getAppearance() == LOCO_GIANT_BIRD)
		{
			if (obj->getLogicModelConditions().test(MODELCONDITION_SWOOPING))
			{
				const Real frameScale = static_cast<Real>(LOGICFRAMES_PER_SECOND);
				if (m_template->m_swoopTerminalVelocity > 0.0f)
					maxDescentSpeed = min(m_template->m_speedLimitZ, m_template->m_swoopTerminalVelocity * frameScale);
				if (m_template->m_swoopAccelerationRate > 0.0f)
					maxDownwardAcceleration = min(maxDownwardAcceleration, m_template->m_swoopAccelerationRate * frameScale);
			}
			else if (preferredHeight > curZ && getSwoopStandoffRadius() > 0.0f)
			{
				const Real planarSpeed = WWMath::SqrtfOrigin(velocity.x * velocity.x + velocity.y * velocity.y);
				if (planarSpeed > 0.0f)
				{
					const Real gradient = WWMath::TanTrig((birdPullUp ? 77.0f : 31.0f) * (PI / 180.0f));
					maxClimbSpeed = min(maxClimbSpeed, planarSpeed * gradient);
				}
			}
		}
		const Real desiredVerticalSpeed = clamp(-maxDescentSpeed, (preferredHeight - curZ) * 0.25f, maxClimbSpeed);
		const Real correction = clamp(-maxDownwardAcceleration, desiredVerticalSpeed - velocity.z, maxGrossLift);
		return correction - TheGlobalData->m_gravity;
	}
	Real maxNetLift = maxGrossLift + TheGlobalData->m_gravity;	// note that gravity is always negative.
	if (maxNetLift < 0)
		maxNetLift = 0;
	Real curVelZ = physics->getVelocity()->z;
	// going down, braking is limited by net lift; going up, braking is limited by gravity
	Real maxAccel;
	if (getFlag(ULTRA_ACCURATE))
		maxAccel = (curVelZ < 0) ? 2*maxNetLift : -2*maxNetLift;
	else
		maxAccel = (curVelZ < 0) ? maxNetLift : TheGlobalData->m_gravity;
	// see how far we need to slow to dead stop, given max braking
	Real desiredAccel;
	const Real TINY_ACCEL = 0.001f;
	if (fabs(maxAccel) > TINY_ACCEL)
	{
		Real deltaZ = preferredHeight - curZ;
		// calc how far it will take for us to go from cur speed to zero speed, at max accel.
		// Real brakeDist = calcSlowDownDist(curVelZ, 0, maxAccel);
		// in theory, the above is the correct calculation, but in practice,
		// doesn't work in some situations (eg, opening of USA01 map). Why, I dunno.
		// But for now I have gone back to the old, looks-incorrect-to-me-but-works calc. (srj)
		Real brakeDist = (sqr(curVelZ) / fabs(maxAccel));
		if (fabs(brakeDist) > fabs(deltaZ))
		{
			// if the dist-to-accel (or dist-to-brake) is further than the dist-to-go,
			// use the max accel.
			desiredAccel = maxAccel;
		}
		else if (fabs(curVelZ) > m_template->m_speedLimitZ)
		{
			// or, if we're going too fast, limit it here.
			desiredAccel = m_template->m_speedLimitZ - curVelZ;
		}
		else
		{
			// ok, figure out the correct accel to use to get us there at zero.
			//
			// dz = v t + 0.5 a t^2
			//	thus
			// a = 2(dz - v t)/t^2
			//	and
			// t = (-v +- WWMath::SqrtOrigin(v*v + 2*a*dz))/a
			//
			// but if we assume t=1, then
			//	a=2(dz-v)
			// then, plug it back in and see if t is really 1...
			desiredAccel = 2.0f * (deltaZ - curVelZ);
		}
	}
	else
	{
		desiredAccel = 0.0f;
	}
	Real liftToUse = desiredAccel - TheGlobalData->m_gravity;
	if (getFlag(ULTRA_ACCURATE))
	{
		// in ultra-accurate mode, we allow cheating.
		const Real UP_FACTOR = 3.0f;
		if (liftToUse > UP_FACTOR*maxGrossLift)
			liftToUse = UP_FACTOR*maxGrossLift;
		// srj sez: we used to clip lift to zero here (not allowing neg lift).
		// however, I now think that allowing neg lift in ultra-accurate mode is
		// a good and desirable thing; in particular, it enables jets to complete
		// "short" landings more accurately (previously they sometimes would "float"
		// down, which sucked.) if you need to bump this back to zero, check it carefully...
		else if (liftToUse < -maxGrossLift)
			liftToUse = -maxGrossLift;
	}
	else
	{
		if (liftToUse > maxGrossLift)
			liftToUse = maxGrossLift;
		else if (liftToUse < 0.0f)
			liftToUse = 0.0f;
	}

	return liftToUse;
}

//-------------------------------------------------------------------------------------------------
PhysicsTurningType Locomotor::rotateObjAroundLocoPivot(Object* obj, const Coord3D& goalPos,
																											 Real maxTurnRate, Real *relAngle)
{
	Real angle = obj->getOrientation();
	Real offset = getTurnPivotOffset();

	PhysicsTurningType turn = TURN_NONE;

	if (getFlag(IS_BRAKING)) offset = 0.0f; // When braking we do exact movement towards goal, instead of physics.
	//Rotating about pivot moves the object, and can make us miss our goal, so it is disabled.  jba.
	if (offset != 0.0f)
	{
		Real radius = obj->getGeometryInfo().getBoundingCircleRadius();
		Real turnPointOffset = offset * radius;

		Coord3D turnPos = *obj->getPosition();
		const Coord3D* dir = obj->getUnitDirectionVector2D();
		turnPos.x += dir->x * turnPointOffset;
		turnPos.y += dir->y * turnPointOffset;
		Real dx =goalPos.x - turnPos.x;
		Real dy = goalPos.y - turnPos.y;
		// If we are very close to the goal, we twitch due to rounding error.  So just return. jba.
		if (fabs(dx)<0.1f && fabs(dy)<0.1f)
		{
			updateRetailTurnConditions(obj, 0.0f);
			return TURN_NONE;
		}
		Real desiredAngle = WWMath::Atan2Origin(dy, dx);
		Real amount = stdAngleDiff(desiredAngle, angle);
		updateRetailTurnConditions(obj, amount);
		if (relAngle) *relAngle = amount;
		if (amount>maxTurnRate) {
			amount = maxTurnRate;
			turn = TURN_POSITIVE;
		} else if (amount < -maxTurnRate) {
			amount = -maxTurnRate;
			turn = TURN_NEGATIVE;
		} else {
			turn = TURN_NONE;
		}

#if 0
		Coord3D desiredPos = *obj->getPosition();	// well, desired Dir, anyway
		desiredPos.x += Cos(angle + amount) * radius;
		desiredPos.y += Sin(angle + amount) * radius;


		// so, the thing is, we want to rotate ourselves so that our *center* is rotated
		// by the given amount, but the rotation must be around turnPos. so do a little
		// back-calculation.
		Real angleDesiredForTurnPos = WWMath::Atan2Origin(desiredPos.y - turnPos.y, desiredPos.x - turnPos.x);
		amount = angleDesiredForTurnPos - angle;
#endif
		/// @todo srj -- there's probably a more efficient & more direct way to do this. find it.
		Matrix3D mtx;
		Matrix3D tmp(1);
		tmp.Translate(turnPos.x, turnPos.y, 0);
		tmp.In_Place_Pre_Rotate_Z(amount);
		tmp.Translate(-turnPos.x, -turnPos.y, 0);

		mtx.mul(tmp, *obj->getTransformMatrix());

		obj->setTransformMatrix(&mtx);
	}
	else
	{
		Real desiredAngle = WWMath::Atan2Origin(goalPos.y - obj->getPosition()->y, goalPos.x - obj->getPosition()->x);
		Real amount = stdAngleDiff(desiredAngle, angle);
		updateRetailTurnConditions(obj, amount);
		if (relAngle) *relAngle = amount;
		if (amount>maxTurnRate) {
			amount = maxTurnRate;
			turn = TURN_POSITIVE;
		} else if (amount < -maxTurnRate) {
			amount = -maxTurnRate;
			turn = TURN_NEGATIVE;
		} else {
			turn = TURN_NONE;
		}
		obj->setOrientation( normalizeAngle(angle + amount) );
	}
	return turn;
}

//-------------------------------------------------------------------------------------------------
/*
	return true if we can maintain the position without being called every frame (eg, we are
	resting on the ground), false if not (eg, we are hovering or circling)
*/
Bool Locomotor::handleBehaviorZ(Object* obj, PhysicsBehavior *physics, const Coord3D& goalPos, Bool birdPullUp)
{
	Bool requiresConstantCalling = TRUE;

	// keep the agent aligned on the terrain
	switch(m_template->m_behaviorZ)
	{
		case Z_FLOATING_Z:
		case Z_SCALING_WALLS:
			requiresConstantCalling = FALSE;
			break;

		case Z_NO_Z_MOTIVE_FORCE:
			// nothing to do.
			requiresConstantCalling = FALSE;
			break;

		case Z_SEA_LEVEL:
			requiresConstantCalling = TRUE;
			if( !obj->isDisabledByType( DISABLED_HELD ) )
			{
				Coord3D pos = *obj->getPosition();
				Real waterZ;
				if (TheTerrainLogic->isUnderwater(pos.x, pos.y, &waterZ)) {
					pos.z = waterZ;
				} else {
					pos.z = TheTerrainLogic->getLayerHeight(pos.x, pos.y, obj->getLayer());
				}
				obj->setPosition(&pos);
			}
			break;

		case Z_FIXED_SURFACE_RELATIVE_HEIGHT:
		case Z_FIXED_ABSOLUTE_HEIGHT:
			requiresConstantCalling = TRUE;
			{
				Coord3D pos = *obj->getPosition();
				Bool surfaceRel = (m_template->m_behaviorZ == Z_FIXED_SURFACE_RELATIVE_HEIGHT);
				Real surfaceHt = surfaceRel ? getSurfaceHtAtPt(pos.x, pos.y) : 0.0f;
				pos.z = m_preferredHeight + (surfaceRel ? surfaceHt : 0);
				obj->setPosition(&pos);
			}
			break;

		case Z_RELATIVE_TO_GROUND_AND_BUILDINGS:
			requiresConstantCalling = TRUE;
			{
				// srj sez: use getGroundOrStructureHeight(), because someday it will cache building heights...
				Coord3D pos = *obj->getPosition();
				Real surfaceHt = ThePartitionManager->getGroundOrStructureHeight(pos.x, pos.y);

					pos.z = m_preferredHeight + surfaceHt;

					obj->setPosition(&pos);

			}
			break;
		case Z_SMOOTH_RELATIVE_TO_HIGHEST_LAYER:
			requiresConstantCalling = TRUE;
			{
				if (m_preferredHeight != 0.0f || getFlag(PRECISE_Z_POS))
				{
					Coord3D pos = *obj->getPosition();

					// srj sez: if we aren't on the ground, never find the ground layer
					PathfindLayerEnum layerAtDest = obj->getLayer();
					if (layerAtDest == LAYER_GROUND)
						layerAtDest = TheTerrainLogic->getHighestLayerForDestination( &pos );

					Real surfaceHt;
					Coord3D normal;
					const Bool clip = false;	// return the height, even if off the edge of the bridge proper.
					surfaceHt = TheTerrainLogic->getLayerHeight( pos.x, pos.y, layerAtDest, &normal, clip );

					Real preferredHeight = m_preferredHeight + surfaceHt;
					if (getFlag(PRECISE_Z_POS))
						preferredHeight = goalPos.z;

					Real delta = preferredHeight - pos.z;
					delta *= getPreferredHeightDamping();
					preferredHeight = pos.z + delta;

					Real liftToUse = calcLiftToUseAtPt(obj, physics, pos.z, surfaceHt, preferredHeight, birdPullUp);

					//DEBUG_LOG(("HandleBZ %d LiftToUse %f",TheGameLogic->getFrame(),liftToUse));
					if (liftToUse != 0.0f)
					{
						Coord3D force;
						force.x = 0.0f;
						force.y = 0.0f;
						force.z = liftToUse * physics->getMass();
						physics->applyMotiveForce(&force);
					}
				}
			}
			break;

		case Z_SURFACE_RELATIVE_HEIGHT:
		case Z_ABSOLUTE_HEIGHT:
			requiresConstantCalling = TRUE;
			{
				if (m_preferredHeight != 0.0f || getFlag(PRECISE_Z_POS))
				{
					Coord3D pos = *obj->getPosition();

					Bool surfaceRel = (m_template->m_behaviorZ == Z_SURFACE_RELATIVE_HEIGHT);
					Real surfaceHt = surfaceRel ? getSurfaceHtAtPt(pos.x, pos.y) : 0.0f;
					Real preferredHeight = m_preferredHeight + (surfaceRel ? surfaceHt : 0);
					if (getFlag(PRECISE_Z_POS))
						preferredHeight = goalPos.z;

					Real delta = preferredHeight - pos.z;
					delta *= getPreferredHeightDamping();
					preferredHeight = pos.z + delta;

					Real liftToUse = calcLiftToUseAtPt(obj, physics, pos.z, surfaceHt, preferredHeight, birdPullUp);

					//DEBUG_LOG(("HandleBZ %d LiftToUse %f",TheGameLogic->getFrame(),liftToUse));
					if (liftToUse != 0.0f)
					{
						Coord3D force;
						force.x = 0.0f;
						force.y = 0.0f;
						force.z = liftToUse * physics->getMass();
						physics->applyMotiveForce(&force);
					}
				}
			}
			break;
	}

	return requiresConstantCalling;
}

//-------------------------------------------------------------------------------------------------
void Locomotor::moveTowardsPositionOther(Object* obj, PhysicsBehavior *physics, const Coord3D& goalPos, Real onPathDistToGoal, Real desiredSpeed)
{
	BodyDamageType bdt = obj->getBodyModule()->getDamageState();
	Real maxAcceleration = getMaxAcceleration(bdt);

	// sanity, we cannot use desired speed that is greater than our max speed we are capable of moving at
	Real maxSpeed = getMaxSpeedForCondition(bdt, obj);
	if( desiredSpeed > maxSpeed )
		desiredSpeed = maxSpeed;

	Real goalSpeed = desiredSpeed;
	Real actualSpeed = physics->getForwardSpeed2D();

	// Locomotion for other things, ie don't know what it is jba :)
	//
	// Orient toward goal position
	// exception: if very close (ie, we could get there in 2 frames or less),\
	// and ULTRA_ACCURATE, just slide into place
	//
	const Coord3D* pos =  obj->getPosition();
	Coord3D dirToApplyForce = *obj->getUnitDirectionVector2D();

//DEBUG_ASSERTLOG(!getFlag(ULTRA_ACCURATE),("thresh %f %f (%f %f)",
//fabs(goalPos.y - pos->y),fabs(goalPos.x - pos->x),
//fabs(goalPos.y - pos->y)/goalSpeed,fabs(goalPos.x - pos->x)/goalSpeed));
	if (getFlag(ULTRA_ACCURATE) &&
				fabs(goalPos.y - pos->y) <= goalSpeed * m_template->m_ultraAccurateSlideIntoPlaceFactor &&
				fabs(goalPos.x - pos->x) <= goalSpeed * m_template->m_ultraAccurateSlideIntoPlaceFactor)
	{
		// don't turn, just slide in the right direction
		physics->setTurning(TURN_NONE);
		dirToApplyForce.x = goalPos.x - pos->x;
		dirToApplyForce.y = goalPos.y - pos->y;
		dirToApplyForce.z = 0.0f;
		dirToApplyForce.normalize();
	}
	else
	{
		PhysicsTurningType rotating = rotateTowardsPosition(obj, goalPos);
		physics->setTurning(rotating);
		if (RetailFileLayers::enabled() && (getAppearance() == LOCO_WINGS || getAppearance() == LOCO_GIANT_BIRD))
			dirToApplyForce = *obj->getUnitDirectionVector2D();
	}

	if (!getFlag(NO_SLOW_DOWN_AS_APPROACHING_DEST))
	{
		Real slowDownDist = calcSlowDownDist(actualSpeed, getMinSpeed(), getBraking());
		if (onPathDistToGoal < slowDownDist)
		{
			goalSpeed = getMinSpeed();
		}
	}

	//
	// Maintain goal speed
	//
	Real speedDelta = goalSpeed - actualSpeed;
	if (speedDelta != 0.0f)
	{
		Real mass = physics->getMass();
		Real acceleration = (speedDelta > 0.0f) ? maxAcceleration : -getBraking();
		Real accelForce = mass * acceleration;

		/*
			don't accelerate/brake more than necessary. do a quick calc to
			see how much force we really need to achieve our goal speed...
		*/
		Real maxForceNeeded = mass * speedDelta;
		if (fabs(accelForce) > fabs(maxForceNeeded))
			accelForce = maxForceNeeded;

		Coord3D force;
		force.x = accelForce * dirToApplyForce.x;
		force.y = accelForce * dirToApplyForce.y;
		force.z = 0.0f;

		// apply forces to object
		physics->applyMotiveForce( &force );
	}

}


void Locomotor::moveTowardsPositionHorde(Object* obj, PhysicsBehavior *physics, const Coord3D& goalPos, Real onPathDistToGoal, Real desiredSpeed)
{
	const Real REFORM_MIN_DISTANCE = 20.0f;
	const Real SLIDE_IN_PLACE_DISTANCE = 0.1f;

	BodyDamageType bdt = obj->getBodyModule()->getDamageState();
	Real maxSpeed = getMaxSpeedForCondition(bdt, RetailFileLayers::enabled() ? obj : nullptr);
	if (desiredSpeed > maxSpeed)
		desiredSpeed = maxSpeed;

	if (usesCavalrySteering(obj))
	{
		moveCavalryForward(obj, physics, goalPos, onPathDistToGoal, desiredSpeed);
		return;
	}
	if (HordeAIUpdate::isReforming(obj->getAI()))
	{
		physics->setTurning(TURN_NONE);
		updateRetailTurnConditions(obj, 0.0f);
		steerVelocity(obj, physics, *obj->getUnitDirectionVector2D(), 0.0f, 0.0f);
		return;
	}

	const Coord3D* pos = obj->getPosition();
	Coord3D dirToApplyForce;
	dirToApplyForce.x = goalPos.x - pos->x;
	dirToApplyForce.y = goalPos.y - pos->y;
	dirToApplyForce.z = 0.0f;
	const Real distToGoal = dirToApplyForce.length();
	if (distToGoal < SLIDE_IN_PLACE_DISTANCE)
	{
		dirToApplyForce = *obj->getUnitDirectionVector2D();
		updateRetailTurnConditions(obj, 0.0f);
	}
	else
	{
		dirToApplyForce.x /= distToGoal;
		dirToApplyForce.y /= distToGoal;
		const Real desiredAngle = WWMath::Atan2Origin(dirToApplyForce.y, dirToApplyForce.x);
		const Real relAngle = stdAngleDiff(desiredAngle, obj->getOrientation());
		const HordeContain* formation = HordeContain::get(obj);
		if (formation && formation->isAlternateFormation() && m_template->m_waitForFormation)
		{
			const Real radius = m_template->m_fastTurnRadius;
			const Real rate = std::min(getMaxTurnRate(bdt), formation->getFormationWheelRate(radius));
			physics->setTurning(rotateObjAroundLocoPivot(obj, goalPos, rate));
			if (radius > 0.0f && fabs(relAngle) > rate)
				desiredSpeed = std::min(desiredSpeed, rate * radius);
			desiredSpeed *= std::max(0.0f, Cos(stdAngleDiff(desiredAngle, obj->getOrientation())));
		}
		else if (fabs(relAngle) > m_template->m_maxTurnWithoutReform && onPathDistToGoal > REFORM_MIN_DISTANCE)
		{
			obj->setOrientation(desiredAngle);
			physics->setTurning(TURN_NONE);
			updateRetailTurnConditions(obj, 0.0f);
			if (m_template->m_waitForFormation && formation)
				desiredSpeed = 0.0f;
		}
		else if (m_template->m_turnWhileMoving)
			physics->setTurning(rotateTowardsPosition(obj, goalPos));
		else
			updateRetailTurnConditions(obj, 0.0f);
	}

	steerVelocity(obj, physics, dirToApplyForce, onPathDistToGoal, desiredSpeed);
}

void Locomotor::moveCavalryForward(Object* obj, PhysicsBehavior* physics, const Coord3D& goal, Real distance, Real speed)
{
	if (physics->getIsStunned())
	{
		physics->setTurning(TURN_NONE);
		updateRetailTurnConditions(obj, 0.0f);
		return;
	}

	setFlag(MOVING_BACKWARDS, false);
	setFlag(IS_BRAKING, false);
	setFlag(MAINTAIN_POS_IS_VALID, false);
	obj->clearStatus(MAKE_OBJECT_STATUS_MASK(OBJECT_STATUS_BRAKING));
	const auto damage = obj->getBodyModule()->getDamageState();
	const Real maxSpeed = getMaxSpeedForCondition(damage, obj);
	const Real forwardSpeed = std::max(0.0f, physics->getForwardSpeed2D());
	const Real radius = getRetailTurnRadius(forwardSpeed, maxSpeed);
	Real turnRate = getMaxTurnRate(damage);
	if (radius > 0.0f && forwardSpeed > 0.0f)
		turnRate = std::min(turnRate, forwardSpeed / radius);

	const Coord3D& pos = *obj->getPosition();
	const Real dx = goal.x - pos.x;
	const Real dy = goal.y - pos.y;
	const Real toGoal = WWMath::SqrtfOrigin(dx * dx + dy * dy);
	const Real facing = obj->getOrientation();
	const Real angle = toGoal > 0.05f ? stdAngleDiff(WWMath::Atan2Origin(dy, dx), facing) : 0.0f;
	if (m_template->m_fastTurnRadius > 0.0f)
	{
		if (const HordeContain* formation = HordeContain::get(obj))
		{
			const Real wheelRate = formation->getFormationWheelRate(m_template->m_fastTurnRadius);
			if (fabs(angle) > wheelRate)
			{
				turnRate = std::min(turnRate, wheelRate);
				speed = std::min(speed, wheelRate * m_template->m_fastTurnRadius);
			}
		}
	}
	Real turn = std::clamp(angle, -turnRate, turnRate);
	updateRetailTurnConditions(obj, angle);
	if (m_template->m_appearance == LOCO_HORDE && distance > 20.0f && fabs(angle) > m_template->m_maxTurnWithoutReform)
	{
		physics->scrubVelocity2D(0.0f);
		turn = angle;
		speed = 0.0f;
	}
	obj->setOrientation(facing + turn);
	physics->setTurning(turn > 0.0f ? TURN_POSITIVE : turn < 0.0f ? TURN_NEGATIVE : TURN_NONE);

	const Real remainingAngle = angle - turn;
	speed = std::min(maxSpeed, speed) * std::max(0.0f, Cos(remainingAngle));
	const Real lateral = fabs(Sin(remainingAngle));
	if (m_template->m_fastTurnRadius > 0.0f && lateral > 0.0f)
	{
		speed = std::min(speed, maxSpeed * toGoal / (2.0f * m_template->m_fastTurnRadius * lateral));
		if (m_template->m_appearance == LOCO_HORDE && distance < PATHFIND_CELL_SIZE_F)
			speed = std::min(speed, turnRate * toGoal / (2.0f * lateral));
	}
	steerVelocity(obj, physics, *obj->getUnitDirectionVector2D(), distance, speed);
}

void Locomotor::locoUpdate_moveTowardsFormationSlot(Object* obj, const Coord3D& goalPos, Real onPathDistToGoal,
	Real desiredSpeed, Bool backing, ObjectID backingSource)
{
	const Real SLIDE_IN_PLACE_DISTANCE = 0.1f;
	PhysicsBehavior* physics = obj->getPhysics();
	if (!physics)
		return;
	if (usesCavalrySteering(obj))
	{
		if (!backing)
		{
			moveCavalryForward(obj, physics, goalPos, onPathDistToGoal, desiredSpeed);
			return;
		}
		physics->setTurning(TURN_NONE);
	}
	const Real maxSpeed = backing ? getBackingUpSpeed() : getMaxSpeedForCondition(
		obj->getBodyModule()->getDamageState(), RetailFileLayers::enabled() ? obj : nullptr);
	if (desiredSpeed > maxSpeed)
		desiredSpeed = maxSpeed;
	Coord3D dir;
	dir.x = goalPos.x - obj->getPosition()->x;
	dir.y = goalPos.y - obj->getPosition()->y;
	dir.z = 0.0f;
	const Real distToGoal = backing ? WWMath::SqrtfOrigin(dir.x * dir.x + dir.y * dir.y) : dir.length();
	if (distToGoal < SLIDE_IN_PLACE_DISTANCE)
	{
		dir = *obj->getUnitDirectionVector2D();
		physics->setTurning(TURN_NONE);
		updateRetailTurnConditions(obj, 0.0f);
	}
	else
	{
		dir.x /= distToGoal;
		dir.y /= distToGoal;
		if (backing)
		{
			const Object* source = backingSource != INVALID_ID ? TheGameLogic->findObjectByID(backingSource) : nullptr;
			const Coord3D& origin = *obj->getPosition();
			const Real facingDistance = 1000.0f;
			Coord3D face{origin.x - dir.x * facingDistance, origin.y - dir.y * facingDistance, origin.z};
			if (source && !source->isEffectivelyDead() && !source->isDestroyed())
			{
				const Coord3D& position = *source->getPosition();
				if (position.x != origin.x || position.y != origin.y)
					face = position;
			}
			if (!physics->getIsStunned())
				physics->setTurning(rotateTowardsPosition(obj, face));
			else
				updateRetailTurnConditions(obj, 0.0f);
		}
		else
		{
			physics->setTurning(rotateTowardsPosition(obj, goalPos));
			const auto* anchor = obj->getContainedBy();
			const Bool reforming = m_template->m_appearance == LOCO_LEGS_TWO &&
				HordeAIUpdate::isReforming(anchor ? anchor->getAI() : nullptr);
			const Real turnRate = getMaxTurnRate(obj->getBodyModule()->getDamageState());
			if (reforming || (turnRate > 0.0f && distToGoal < 2.0f * maxSpeed / turnRate))
			{
				const Coord3D* facing = obj->getUnitDirectionVector2D();
				const Real alignment = facing->x * dir.x + facing->y * dir.y;
				desiredSpeed *= alignment > 0.0f ? alignment : 0.0f;
				if (reforming)
					dir = *facing;
			}
		}
	}
	steerVelocity(obj, physics, dir, onPathDistToGoal, desiredSpeed, backing);
}

void Locomotor::steerVelocity(Object* obj, PhysicsBehavior *physics, const Coord3D& dirToApplyForce, Real onPathDistToGoal,
	Real goalSpeed, Bool backing)
{
	const BodyDamageType bdt = obj->getBodyModule()->getDamageState();
	const Coord3D* velocity = physics->getVelocity();
	const Real speedSquared = velocity->x * velocity->x + velocity->y * velocity->y;
	const Real actualSpeed = backing ? WWMath::SqrtfOrigin(speedSquared) : sqrtf(speedSquared);

	if (!getFlag(NO_SLOW_DOWN_AS_APPROACHING_DEST))
	{
		const Real minSpeed = backing ? 0.0f : getMinSpeed();
		const Real slowDownDist = calcSlowDownDist(actualSpeed, minSpeed, getBraking());
		if (onPathDistToGoal < slowDownDist)
			goalSpeed = minSpeed;
		const Real arrivalSpeedSquared = 2.0f * getBraking() * onPathDistToGoal;
		const Real arrivalSpeed = backing ? WWMath::SqrtfOrigin(arrivalSpeedSquared) : sqrtf(arrivalSpeedSquared);
		if (goalSpeed > arrivalSpeed)
			goalSpeed = std::max(arrivalSpeed, minSpeed);
	}

	Coord3D velocityDelta;
	velocityDelta.x = dirToApplyForce.x * goalSpeed - velocity->x;
	velocityDelta.y = dirToApplyForce.y * goalSpeed - velocity->y;
	velocityDelta.z = 0.0f;
	const Real deltaLength = backing ? WWMath::SqrtfOrigin(sqr(velocityDelta.x) + sqr(velocityDelta.y))
		: velocityDelta.length();
	Real speedAfter = actualSpeed;
	if (deltaLength > 0.0f)
	{
		const Real limit = goalSpeed > actualSpeed ? getMaxAcceleration(bdt) : getBraking();
		const Real impulseScale = deltaLength > limit ? limit / deltaLength : 1.0f;
		const Real scale = physics->getMass() * impulseScale;
		if (RetailFileLayers::enabled() && m_template->m_rotwk.accelDecelTrigger >= 0.0f)
		{
			const Real nextX = velocity->x + impulseScale * velocityDelta.x;
			const Real nextY = velocity->y + impulseScale * velocityDelta.y;
			speedAfter = WWMath::SqrtfOrigin(sqr(nextX) + sqr(nextY));
		}
		Coord3D force;
		force.x = velocityDelta.x * scale;
		force.y = velocityDelta.y * scale;
		force.z = 0.0f;
		physics->applyMotiveForce(&force);
	}
	if (goalSpeed <= MIN_GAIT_SPEED && actualSpeed <= MIN_GAIT_SPEED)
		clearRetailGait(obj);
	else
		updateRetailGait(obj, onPathDistToGoal, actualSpeed, backing || goalSpeed + MIN_GAIT_SPEED < actualSpeed, speedAfter);
}

//-------------------------------------------------------------------------------------------------
/*
	return true if we can maintain the position without being called every frame (eg, we are
	resting on the ground), false if not (eg, we are hovering or circling)
*/
Bool Locomotor::locoUpdate_maintainCurrentPosition(Object* obj)
{
	if (!getFlag(MAINTAIN_POS_IS_VALID))
	{
		m_maintainPos = *obj->getPosition();
		setFlag(MAINTAIN_POS_IS_VALID, true);
	}

	m_donutTimer = TheGameLogic->getFrame()+DONUT_TIME_DELAY_SECONDS*static_cast<float>(LOGICFRAMES_PER_SECOND);
	setFlag(IS_BRAKING, false);
	PhysicsBehavior *physics = obj->getPhysics();
	if (physics == nullptr)
	{
		DEBUG_CRASH(("you can only apply Locomotors to objects with Physics"));
		return TRUE;
	}

#ifdef DEBUG_OBJECT_ID_EXISTS
//	DEBUG_ASSERTLOG(obj->getID() != TheObjectIDToDebug, ("locoUpdate_maintainCurrentPosition %f %f %f, speed %f (%f)",m_maintainPos.x,m_maintainPos.y,m_maintainPos.z,physics->getSpeed(),physics->getForwardSpeed2D()));
#endif

	if (usesCavalrySteering(obj))
		physics->setTurning(TURN_NONE);
	updateRetailTurnConditions(obj, 0.0f);

	Bool requiresConstantCalling = TRUE;	// assume the worst.
	switch (m_template->m_appearance)
	{
		case LOCO_THRUST:
			maintainCurrentPositionThrust(obj, physics);
			requiresConstantCalling = TRUE;
			break;
		case LOCO_LEGS_TWO:
			maintainCurrentPositionLegs(obj, physics);
			requiresConstantCalling = FALSE;
			break;
		case LOCO_CLIMBER:
			maintainCurrentPositionLegs(obj, physics);
			requiresConstantCalling = FALSE;
			break;
		case LOCO_WHEELS_FOUR:
		case LOCO_MOTORCYCLE:
			maintainCurrentPositionWheels(obj, physics);
			requiresConstantCalling = FALSE;
			break;
		case LOCO_TREADS:
			maintainCurrentPositionTreads(obj, physics);
			requiresConstantCalling = FALSE;
			break;
		case LOCO_HOVER:
			maintainCurrentPositionHover(obj, physics);
			requiresConstantCalling = TRUE;
			break;
		case LOCO_WINGS:
			maintainCurrentPositionWings(obj, physics);
			requiresConstantCalling = TRUE;
			break;
		case LOCO_OTHER:
		default:
			maintainCurrentPositionOther(obj, physics);
			requiresConstantCalling = TRUE;
			break;
	}

	if (RetailFileLayers::enabled())
	{
		const Real speed = m_retailWasRunning ? physics->getVelocityMagnitude() : 0.0f;
		if (speed > MIN_GAIT_SPEED)
		{
			updateRetailGait(obj, 0.0f, speed, TRUE);
			obj->setModelConditionState(MODELCONDITION_MOVING);
			requiresConstantCalling = TRUE;
		}
		else
		{
			const AIUpdateInterface* ai = obj->getAI();
			if (m_retailWasRunning && (!ai || !ai->isMoving() || obj->getLogicModelConditions().test(MODELCONDITION_DECELERATE)))
				obj->clearModelConditionState(MODELCONDITION_MOVING);
			clearRetailGait(obj);
		}
	}

	// but we do need to do this even if not moving, for hovering/Thrusting things.
	if (handleBehaviorZ(obj, physics, m_maintainPos))
		requiresConstantCalling = TRUE;

	return requiresConstantCalling;
}

//-------------------------------------------------------------------------------------------------
void Locomotor::maintainCurrentPositionThrust(Object* obj, PhysicsBehavior *physics)
{
	DEBUG_ASSERTCRASH(getFlag(MAINTAIN_POS_IS_VALID), ("invalid maintain pos"));
	/// @todo srj -- should these also use the "circling radius" stuff, like wings?
	moveTowardsPositionThrust(obj, physics, m_maintainPos, 0, getMinSpeed());
}

//-------------------------------------------------------------------------------------------------
void Locomotor::maintainCurrentPositionWings(Object* obj, PhysicsBehavior *physics)
{
	DEBUG_ASSERTCRASH(getFlag(MAINTAIN_POS_IS_VALID), ("invalid maintain pos"));
	physics->setTurning(TURN_NONE);
	const Bool ambientFlight = RetailFileLayers::enabled() && obj->isKindOf(KINDOF_INERT) &&
		!obj->isEffectivelyDead() && getMinSpeed() > 0.0f && getPreferredHeight() > 0.0f;
	if (ambientFlight || (physics->isMotive() && obj->isAboveTerrain()))
	{

			// aim for the spot on the opposite side of the circle.
		BodyDamageType bdt = obj->getBodyModule()->getDamageState();
		Real turnRadius = m_template->m_circlingRadius;
		if (turnRadius == 0.0f)
			turnRadius = calcMinTurnRadius(bdt, nullptr);

		// find the direction towards our "maintain pos"
		const Coord3D* pos = obj->getPosition();
		Real dx = m_maintainPos.x - pos->x;
		Real dy = m_maintainPos.y - pos->y;
		Real angleTowardMaintainPos =
				(isNearlyZero(dx) && isNearlyZero(dy)) ?
				obj->getOrientation() :
				WWMath::Atan2Origin(dy, dx);

		Real aimDir = (PI - PI/8);
		if (turnRadius < 0)
		{
			turnRadius = -turnRadius;
			aimDir = -aimDir;
		}
		angleTowardMaintainPos += aimDir;

		// project a spot "radius" dist away from it, in that dir
		Coord3D desiredPos = m_maintainPos;
		desiredPos.x += Cos(angleTowardMaintainPos) * turnRadius;
		desiredPos.y += Sin(angleTowardMaintainPos) * turnRadius;
		moveTowardsPositionWings(obj, physics, desiredPos, 0, getMinSpeed());
	}
}

//-------------------------------------------------------------------------------------------------
void Locomotor::maintainCurrentPositionHover(Object* obj, PhysicsBehavior *physics)
{
	physics->setTurning(TURN_NONE);
	if (physics->isMotive())	// no need to stop something that isn't moving.
	{
		DEBUG_ASSERTCRASH(m_template->m_minSpeed == 0.0f, ("HOVER should always have zero minSpeeds (otherwise, they WING)"));

		BodyDamageType bdt = obj->getBodyModule()->getDamageState();
		Real maxAcceleration = getMaxAcceleration(bdt);
		Real actualSpeed = physics->getForwardSpeed2D();
		//
		// Stop
		//
		Real minSpeed = max( 1.0E-10f, getMinSpeed() );
		Real speedDelta = minSpeed - actualSpeed;
		if (fabs(speedDelta) > minSpeed)
		{
			Real mass = physics->getMass();
			Real acceleration = (speedDelta > 0.0f) ? maxAcceleration : -getBraking();
			Real accelForce = mass * acceleration;

			/*
				don't accelerate/brake more than necessary. do a quick calc to
				see how much force we really need to achieve our goal speed...
			*/
			Real maxForceNeeded = mass * speedDelta;
			if (fabs(accelForce) > fabs(maxForceNeeded))
				accelForce = maxForceNeeded;

			const Coord3D *dir = obj->getUnitDirectionVector2D();

			Coord3D force;
			force.x = accelForce * dir->x;
			force.y = accelForce * dir->y;
			force.z = 0.0f;

			// apply forces to object
			physics->applyMotiveForce( &force );
		}
	}

}

//-------------------------------------------------------------------------------------------------
void Locomotor::maintainCurrentPositionOther(Object* obj, PhysicsBehavior *physics)
{

	physics->setTurning(TURN_NONE);
	if (physics->isMotive())	// no need to stop something that isn't moving.
	{
		physics->scrubVelocity2D(0); // stop.
	}

}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
LocomotorSet::LocomotorSet()
{
	m_locomotors.clear();
	m_validLocomotorSurfaces = 0;
	m_downhillOnly = FALSE;

}

//-------------------------------------------------------------------------------------------------
LocomotorSet::LocomotorSet(const LocomotorSet& that)
{
	DEBUG_CRASH(("unimplemented"));
}

//-------------------------------------------------------------------------------------------------
LocomotorSet& LocomotorSet::operator=(const LocomotorSet& that)
{
	if (this != &that)
	{
		DEBUG_CRASH(("unimplemented"));
	}
	return *this;
}

//-------------------------------------------------------------------------------------------------
LocomotorSet::~LocomotorSet()
{
	clear();
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void LocomotorSet::crc( Xfer *xfer )
{

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void LocomotorSet::xfer( Xfer *xfer )
{
	// version
	const XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// count of vector
	UnsignedShort count = m_locomotors.size();
	xfer->xferUnsignedShort( &count );

	// data
	if (xfer->getXferMode() == XFER_SAVE)
	{
		for (LocomotorVector::iterator it = m_locomotors.begin(); it != m_locomotors.end(); ++it)
		{
			Locomotor* loco = *it;
			AsciiString name = loco->getTemplateName();
			xfer->xferAsciiString(&name);
			xfer->xferSnapshot(loco);
		}
	}
	else if (xfer->getXferMode() == XFER_LOAD)
	{
		// vector should be empty at this point
		if (m_locomotors.empty() == FALSE)
		{
			DEBUG_CRASH(( "LocomotorSet::xfer - vector is not empty, but should be" ));
			throw XFER_LIST_NOT_EMPTY;
		}

		for (UnsignedShort i = 0; i < count; ++i)
		{
			AsciiString name;
			xfer->xferAsciiString(&name);

			const LocomotorTemplate* lt = TheLocomotorStore->findLocomotorTemplate(NAMEKEY(name));
			if (lt == nullptr)
			{
				DEBUG_CRASH(( "LocomotorSet::xfer - template %s not found", name.str() ));
				throw XFER_UNKNOWN_STRING;
			}

			Locomotor* loco = TheLocomotorStore->newLocomotor(lt);
			xfer->xferSnapshot(loco);
			m_locomotors.push_back(loco);
		}
	}

	xfer->xferInt(&m_validLocomotorSurfaces);
	xfer->xferBool(&m_downhillOnly);

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void LocomotorSet::loadPostProcess()
{

}

//-------------------------------------------------------------------------------------------------
void LocomotorSet::xferSelfAndCurLocoPtr(Xfer *xfer, Locomotor** loco)
{
	xfer->xferSnapshot(this);

	if (xfer->getXferMode() == XFER_SAVE)
	{
		AsciiString name;
		if (*loco)
			name = (*loco)->getTemplateName();
		xfer->xferAsciiString(&name);
	}
	else if (xfer->getXferMode() == XFER_LOAD)
	{
		AsciiString name;
		xfer->xferAsciiString(&name);

		if (name.isEmpty())
		{
			*loco = nullptr;
		}
		else
		{
			for (size_t i = 0; i < m_locomotors.size(); ++i)
			{
				if (m_locomotors[i]->getTemplateName() == name)
				{
					*loco = m_locomotors[i];
					return;
				}
			}

			DEBUG_CRASH(( "LocomotorSet::xfer - template %s not found", name.str() ));
			throw XFER_UNKNOWN_STRING;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void LocomotorSet::clear()
{
	for (size_t i = 0; i < m_locomotors.size(); ++i)
	{
		deleteInstance(m_locomotors[i]);
	}
	m_locomotors.clear();
	m_validLocomotorSurfaces = 0;
	m_downhillOnly = FALSE;
}

//-------------------------------------------------------------------------------------------------
void LocomotorSet::addLocomotor(const LocomotorTemplate* lt)
{
	Locomotor* loco = TheLocomotorStore->newLocomotor(lt);
	if (loco)
	{
		m_locomotors.push_back(loco);
		m_validLocomotorSurfaces |= loco->getLegalSurfaces();
		if (loco->getIsDownhillOnly())
		{
			m_downhillOnly = TRUE;
		}
		else // Previous locos were gravity only, but this one isn't!
		{
			DEBUG_ASSERTCRASH(!m_downhillOnly,("LocomotorSet, YOU CAN NOT MIX DOWNHILL-ONLY LOCOMOTORS WITH NON-DOWNHILL-ONLY ONES."));
		}

	}
}

//-------------------------------------------------------------------------------------------------
Locomotor* LocomotorSet::findLocomotor(LocomotorSurfaceTypeMask t)
{
	for (LocomotorVector::iterator it = m_locomotors.begin(); it != m_locomotors.end(); ++it)
	{
		Locomotor* curLocomotor = *it;
		if (curLocomotor && (curLocomotor->getLegalSurfaces() & t))
			return curLocomotor;
	}
	return nullptr;
}


