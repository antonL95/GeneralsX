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

// FILE: FXList.cpp ///////////////////////////////////////////////////////////////////////////////
// Author: Steven Johnson, December 2001
// Desc:   FXList descriptions
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameClient/FXList.h"

#include "Common/DrawModule.h"
#include "Common/GameAudio.h"
#include "Common/GameState.h"
#include "Common/GameUtility.h"
#include "Common/GlobalData.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/RandomValue.h"
#include "Common/RetailFileLayers.h"
#include "Common/SpecialPowerINI.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/Xfer.h"

#include "GameLogic/Object.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/TerrainLogic.h"
#include "GameClient/Display.h"
#include "GameClient/GameClient.h"
#include "GameClient/Drawable.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/RadiusDecal.h"
#include "GameClient/RetailBuffVisuals.h"
#include "GameClient/RetailEva.h"
#include "GameClient/RetailFXDecals.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/Module/HordeContain.h"
#include "WWMath/wwmath.h"

#include <optional>
#include <cmath>
#include <limits>
#include <list>

///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC DATA ////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


FXListStore *TheFXListStore = nullptr;					///< the FXList store definition

//-------------------------------------------------------------------------------------------------
static void adjustVector(Coord3D *vec, const Matrix3D* mtx)
{
	if (mtx)
	{
		Vector3 vectmp;
		vectmp.X = vec->x;
		vectmp.Y = vec->y;
		vectmp.Z = vec->z;
		vectmp = mtx->Rotate_Vector(vectmp);
		vec->x = vectmp.X;
		vec->y = vectmp.Y;
		vec->z = vectmp.Z;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE CLASSES ///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
void FXNugget::doFXObj(const Object* primary, const Object* secondary) const
{
	const Coord3D* p = primary ? primary->getPosition() : nullptr;
	const Matrix3D* mtx = primary ? primary->getTransformMatrix() : nullptr;
	const Real speed = 0.0f;	// yes, that's right -- NOT the object's speed.
	const Coord3D* s = secondary ? secondary->getPosition() : nullptr;
	doFXPos(p, mtx, speed, s);
}

void FXNugget::doFXPosWithObjects(const Coord3D* primary, const Matrix3D* primaryMtx, Real primarySpeed,
	const Coord3D* secondary, Real overrideRadius, const Object*, const Object*) const
{
	doFXPos(primary, primaryMtx, primarySpeed, secondary, overrideRadius);
}

static Bool matchesSourceConditions(const Object* source, const ModelConditionFlags& required)
{
	if (!required.any())
		return TRUE;
#if RTS_GENERALS
	return source && source->getDrawable() && source->getDrawable()->getModelConditionFlags().testForAll(required);
#else
	return source && source->getLogicModelConditions().testForAll(required);
#endif
}

static Bool traceRetailFXLists()
{
	static const Bool trace = std::getenv("GENERALSX_TRACE_RETAIL_FX_LISTS") != nullptr;
	return trace && RetailFileLayers::enabled();
}

//-------------------------------------------------------------------------------------------------
class SoundFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(SoundFXNugget, "SoundFXNugget")

public:

	virtual void doFXPos(const Coord3D *primary, const Matrix3D* /*primaryMtx*/, const Real /*primarySpeed*/, const Coord3D * /*secondary*/, const Real /*overrideRadius*/ ) const override
	{
		play(primary, nullptr);
	}

	virtual void doFXObj(const Object* primary, const Object* secondary = nullptr) const override
	{
		play(primary ? primary->getPosition() : nullptr, primary);
	}

	void doFXPosWithObjects(const Coord3D* primary, const Matrix3D*, Real, const Coord3D*, Real,
		const Object* source, const Object*) const override
	{
		play(primary, RetailFileLayers::enabled() ? source : nullptr);
	}

	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "Name",									INI::parseAsciiString,	nullptr, offsetof( SoundFXNugget, m_soundName ) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		SoundFXNugget* nugget = newInstance(SoundFXNugget);
		static const FieldParse retailFields[] = {
			{ "RequiredSourceModelConditions", ModelConditionFlags::parseFromINI, nullptr, offsetof(SoundFXNugget, m_requiredConditions) },
			{ "SourceObjectFilter", parseSourceFilter, nullptr, 0 },
			{ nullptr, nullptr, nullptr, 0 }
		};
		MultiIniFieldParse fields;
		if (RetailFileLayers::enabled())
			fields.add(retailFields);
		fields.add(myFieldParse);
		ini->initFromINIMulti(nugget, fields);
		((FXList*)instance)->addFXNugget(nugget);
	}

private:
	static void parseSourceFilter(INI* ini, void* instance, void*, const void* userData)
	{
		auto& filter = static_cast<SoundFXNugget*>(instance)->m_sourceFilter;
		SpecialPowerObjectFilter::parse(ini, instance, &filter.emplace(), userData);
	}

	void play(const Coord3D* position, const Object* source) const
	{
		if (!matchesSourceConditions(source, m_requiredConditions) ||
			(m_sourceFilter && (!source || !m_sourceFilter->matchesObject(source, source))))
			return;
		AudioEventRTS sound(m_soundName);
		if (source)
			sound.setPlayerIndex(source->getControllingPlayer()->getPlayerIndex());
		if (position)
			sound.setPosition(position);
		if (traceRetailFXLists())
			std::fprintf(stderr, "[Retail FXList] Sound %s\n", m_soundName.str());
		TheAudio->addAudioEvent(&sound);
	}

	ModelConditionFlags m_requiredConditions;
	std::optional<SpecialPowerObjectFilter> m_sourceFilter;
	AsciiString		m_soundName;
};
EMPTY_DTOR(SoundFXNugget)

class EvaEventFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(EvaEventFXNugget, "EvaEventFXNugget")

public:
	void doFXPos(const Coord3D*, const Matrix3D*, Real, const Coord3D*, Real) const override {}

	void doFXObj(const Object* source, const Object*) const override
	{
		if (source)
			play(source, source->getPosition());
	}

	void doFXPosWithObjects(const Coord3D* primary, const Matrix3D*, Real, const Coord3D*, Real,
		const Object* source, const Object*) const override
	{
		if (source)
			play(source, primary);
	}

	static void parse(INI* ini, void* instance, void*, const void*)
	{
		static const FieldParse fields[] = {
			{ "EvaEventOwner", INI::parseAsciiString, nullptr, offsetof(EvaEventFXNugget, m_ownerEvent) },
			{ "EvaEventAlly", INI::parseAsciiString, nullptr, offsetof(EvaEventFXNugget, m_allyEvent) },
			{ "RequiredSourceModelConditions", ModelConditionFlags::parseFromINI, nullptr, offsetof(EvaEventFXNugget, m_requiredConditions) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		auto* nugget = newInstance(EvaEventFXNugget);
		ini->initFromINI(nugget, fields);
		static_cast<FXList*>(instance)->addFXNugget(nugget);
	}

private:
	void play(const Object* source, const Coord3D* position) const
	{
		if (!matchesSourceConditions(source, m_requiredConditions))
			return;
		const Player* viewer = rts::getObservedOrLocalPlayer_Safe();
		if (!viewer)
			return;
		const AsciiString& event = viewer == source->getControllingPlayer() ? m_ownerEvent :
			(viewer->getRelationship(source->getTeam()) == ALLIES ? m_allyEvent : AsciiString::TheEmptyString);
		if (traceRetailFXLists() && event.isNotEmpty())
			std::fprintf(stderr, "[Retail FXList] EvaEvent %s\n", event.str());
		RetailEva::queue(viewer, event, position);
	}

	AsciiString m_ownerEvent;
	AsciiString m_allyEvent;
	ModelConditionFlags m_requiredConditions;
};
EMPTY_DTOR(EvaEventFXNugget)
//-------------------------------------------------------------------------------------------------
static Real calcDist(const Coord3D& src, const Coord3D& dst)
{
  Real dx = dst.x - src.x;
  Real dy = dst.y - src.y;
  Real dz = dst.z - src.z;
  return sqrt(dx*dx + dy*dy + dz*dz);
}

//-------------------------------------------------------------------------------------------------
class TracerFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(TracerFXNugget, "TracerFXNugget")
public:

	TracerFXNugget()
	{
		m_tracerName.set("GenericTracer");
    m_boneName.clear();
    m_speed = 0.0f; // means "use passed-in speed"
    m_decayAt = 1.0f;
		m_length = 10.0f;
		m_width = 1.0f;
		m_color.red = m_color.green = m_color.blue = 1.0f;
		m_probability = 1.0f;
	}

	virtual void doFXPos(const Coord3D *primary, const Matrix3D* primaryMtx, const Real primarySpeed, const Coord3D *secondary, const Real /*overrideRadius*/ ) const override
	{
		if (m_probability <= GameClientRandomValueReal(0, 1))
			return;

		if (primary && secondary)
		{
			Drawable *tracer = TheThingFactory->newDrawable(TheThingFactory->findTemplate(m_tracerName));
			if(!tracer)
				return;

			//Kris -- Redid this section Sept 18, 2002
			//Calculate tracer orientations to face from primary to secondary position. This
			//should be the direction that the projectile is being fired towards. It doesn't make
			//sense that the old stuff made use of the muzzle fx bone orientation (because it's a
			//subobject). It had other problems because of elevation variations the tracers would
			//stay on the ground.
			//tracer->setTransformMatrix(primaryMtx);
			Matrix3D tracerMtx;
			Vector3 pos( primary->x, primary->y, primary->z );
			Vector3 dir( secondary->x - primary->x, secondary->y - primary->y, secondary->z - primary->z );
			dir.Normalize(); //This is fantastically crucial for calling buildTransformMatrix!!!!!
			tracerMtx.buildTransformMatrix( pos, dir );
			tracer->setTransformMatrix( &tracerMtx );
			tracer->setPosition(primary);

			Real speed = m_speed;
			if (speed == 0.0f)
			{
				speed = primarySpeed;
			}

			TracerDrawInterface* tdi = nullptr;
			for (DrawModule** d = tracer->getDrawModules(); *d; ++d)
			{
				if ((tdi = (*d)->getTracerDrawInterface()) != nullptr)
				{
					tdi->setTracerParms(speed, m_length, m_width, m_color, 1.0f);
				}
			}

			// estimate how long it will take us to get to the destination
			Real dist = calcDist(*primary, *secondary) - m_length;
			Real frames = (dist >= 0.0f && speed >= 0.0f) ? (dist / speed) : 1;
			Int framesAdjusted = REAL_TO_INT_CEIL(frames * m_decayAt);
			tracer->setExpirationDate(TheGameLogic->getFrame() + framesAdjusted);
		}
		else
		{
			DEBUG_CRASH(("You must have a primary and secondary source for this effect"));
		}
	}

	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "TracerName",			INI::parseAsciiString,			nullptr, offsetof( TracerFXNugget, m_tracerName ) },
			{ "BoneName",				INI::parseAsciiString,			nullptr, offsetof( TracerFXNugget, m_boneName ) },
      { "Speed",          INI::parseVelocityReal,     nullptr, offsetof( TracerFXNugget, m_speed ) },
      { "DecayAt",        INI::parseReal,             nullptr, offsetof( TracerFXNugget, m_decayAt ) },
      { "Length",					INI::parseReal,             nullptr, offsetof( TracerFXNugget, m_length ) },
      { "Width",					INI::parseReal,             nullptr, offsetof( TracerFXNugget, m_width ) },
      { "Color",					INI::parseRGBColor,					nullptr, offsetof( TracerFXNugget, m_color ), true },
      { "Probability",		INI::parseReal,             nullptr, offsetof( TracerFXNugget, m_probability ) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		TracerFXNugget* nugget = newInstance( TracerFXNugget );
		ini->initFromINI(nugget, myFieldParse);
		((FXList*)instance)->addFXNugget(nugget);
	}

private:
	AsciiString			m_tracerName;
  AsciiString     m_boneName;
  Real            m_speed;
  Real            m_decayAt;
	Real						m_length;
	Real						m_width;
	RGBColor				m_color;
	Real						m_probability;
};
EMPTY_DTOR(TracerFXNugget)

//-------------------------------------------------------------------------------------------------
class RayEffectFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(RayEffectFXNugget, "RayEffectFXNugget")
public:

	RayEffectFXNugget()
	{
		m_templateName.clear();
		m_primaryOffset.x = m_primaryOffset.y = m_primaryOffset.z = 0;
		m_secondaryOffset.x = m_secondaryOffset.y = m_secondaryOffset.z = 0;
	}

	virtual void doFXPos(const Coord3D *primary, const Matrix3D* /*primaryMtx*/, const Real /*primarySpeed*/, const Coord3D * secondary, const Real /*overrideRadius*/ ) const override
	{
		const ThingTemplate* tmpl = TheThingFactory->findTemplate(m_templateName);
		DEBUG_ASSERTCRASH(tmpl, ("RayEffect %s not found",m_templateName.str()));
		if (primary && secondary && tmpl)
		{
			Coord3D sourcePos = *primary;
			sourcePos.x += m_primaryOffset.x;
			sourcePos.y += m_primaryOffset.y;
			sourcePos.z += m_primaryOffset.z;

			Coord3D targetPos = *secondary;
			targetPos.x += m_secondaryOffset.x;
			targetPos.y += m_secondaryOffset.y;
			targetPos.z += m_secondaryOffset.z;

			TheGameClient->createRayEffectByTemplate(&sourcePos, &targetPos, tmpl);
		}
		else
		{
			DEBUG_CRASH(("You must have a primary AND secondary source for this effect"));
		}
	}

	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "Name",									INI::parseAsciiString,			nullptr, offsetof( RayEffectFXNugget, m_templateName ) },
			{ "PrimaryOffset",				INI::parseCoord3D,					nullptr, offsetof( RayEffectFXNugget, m_primaryOffset ) },
			{ "SecondaryOffset",			INI::parseCoord3D,					nullptr, offsetof( RayEffectFXNugget, m_secondaryOffset ) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		RayEffectFXNugget* nugget = newInstance( RayEffectFXNugget );
		ini->initFromINI(nugget, myFieldParse);
		((FXList*)instance)->addFXNugget(nugget);
	}

private:
	AsciiString			m_templateName;
	Coord3D					m_primaryOffset;
	Coord3D					m_secondaryOffset;
};
EMPTY_DTOR(RayEffectFXNugget)

//-------------------------------------------------------------------------------------------------
class LightPulseFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(LightPulseFXNugget, "LightPulseFXNugget")
public:

	LightPulseFXNugget() : m_radius(0), m_increaseFrames(0), m_decreaseFrames(0), m_boundingCirclePct(0)
	{
		m_color.red = m_color.green = m_color.blue = 0;
	}

	virtual void doFXObj(const Object* primary, const Object* /*secondary*/) const override
	{
		// GeneralsX @feature fbraz3 08/10/2026 Explosion dynamic light toggle via INI/Options
		if (TheGlobalData && !TheGlobalData->isExplosionDynamicLights())
			return;

		if (primary)
		{
			Real radius = m_radius;

			if (m_boundingCirclePct > 0)
				radius = (primary->getGeometryInfo().getBoundingCircleRadius() * m_boundingCirclePct);

			TheDisplay->createLightPulse(primary->getPosition(), &m_color, 1, radius, m_increaseFrames, m_decreaseFrames);
		}
		else
		{
			DEBUG_CRASH(("You must have a primary source for this effect"));
		}
	}

	virtual void doFXPos(const Coord3D *primary, const Matrix3D* /*primaryMtx*/, const Real /*primarySpeed*/, const Coord3D * /*secondary*/, const Real /*overrideRadius*/ ) const override
	{
		// GeneralsX @feature fbraz3 08/10/2026 Explosion dynamic light toggle via INI/Options
		if (TheGlobalData && !TheGlobalData->isExplosionDynamicLights())
			return;

		if (primary)
		{
			TheDisplay->createLightPulse(primary, &m_color, 1, m_radius, m_increaseFrames, m_decreaseFrames);
		}
		else
		{
			DEBUG_CRASH(("You must have a primary source for this effect"));
		}
	}

	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "Color",						INI::parseRGBColor,								nullptr, offsetof( LightPulseFXNugget, m_color ), true },
			{ "Radius",						INI::parseReal,										nullptr, offsetof( LightPulseFXNugget, m_radius ) },
			{ "RadiusAsPercentOfObjectSize",		INI::parsePercentToReal,	nullptr, offsetof( LightPulseFXNugget, m_boundingCirclePct ) },
			{ "IncreaseTime",			INI::parseDurationUnsignedInt,	nullptr, offsetof( LightPulseFXNugget, m_increaseFrames ) },
			{ "DecreaseTime",			INI::parseDurationUnsignedInt,	nullptr, offsetof( LightPulseFXNugget, m_decreaseFrames ) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		LightPulseFXNugget* nugget = newInstance( LightPulseFXNugget );
		ini->initFromINI(nugget, myFieldParse);
		((FXList*)instance)->addFXNugget(nugget);
	}

private:
	RGBColor			m_color;
	Real					m_radius;
	Real					m_boundingCirclePct;
	UnsignedInt		m_increaseFrames;
	UnsignedInt		m_decreaseFrames;
};
EMPTY_DTOR(LightPulseFXNugget)

//-------------------------------------------------------------------------------------------------
class ViewShakeFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ViewShakeFXNugget, "ViewShakeFXNugget")
public:

	ViewShakeFXNugget() : m_shake(View::SHAKE_NORMAL)
	{
	}

	void doFXObj(const Object* primary, const Object* secondary) const override
	{
		if (matchesReceiver(secondary, primary))
			doShake(primary ? primary->getPosition() : nullptr);
	}

	void doFXPosWithObjects(const Coord3D* primary, const Matrix3D*, Real, const Coord3D*, Real,
		const Object* source, const Object* target) const override
	{
		if (matchesReceiver(source, target))
			doShake(m_filter ? target->getPosition() : primary);
	}

	virtual void doFXPos(const Coord3D *primary, const Matrix3D* /*primaryMtx*/, const Real /*primarySpeed*/, const Coord3D * /*secondary*/, const Real /*overrideRadius*/ ) const override
	{
		if (!m_filter)
			doShake(primary);
	}

	void doShake(const Coord3D* primary) const
	{
		if (primary)
		{
			if (traceRetailFXLists())
				std::fprintf(stderr, "[Retail FXList] ViewShake type=%d\n", static_cast<Int>(m_shake));
			if (TheTacticalView)
			{
				const ScopedVisualRandom random(RetailFileLayers::enabled());
				TheTacticalView->shake(primary, m_shake);
			}
		}
		else
		{
			DEBUG_CRASH(("You must have a primary source for this effect"));
		}
	}

	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "Type",				parseShakeType,								nullptr, offsetof( ViewShakeFXNugget, m_shake ) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		ViewShakeFXNugget* nugget = newInstance( ViewShakeFXNugget );
		static const FieldParse retailFields[] = {
			{ "ObjectFilter", parseFilter, nullptr, 0 },
			{ nullptr, nullptr, nullptr, 0 }
		};
		MultiIniFieldParse fields;
		if (RetailFileLayers::enabled())
			fields.add(retailFields);
		fields.add(myFieldParse);
		ini->initFromINIMulti(nugget, fields);
		((FXList*)instance)->addFXNugget(nugget);
	}

protected:
	static void parseShakeType( INI* ini, void *instance, void *store, const void* /*userData*/ )
	{
		static const LookupListRec shakeTypeNames[] =
		{
			{ "SUBTLE", View::SHAKE_SUBTLE },
			{ "NORMAL", View::SHAKE_NORMAL },
			{ "STRONG", View::SHAKE_STRONG },
			{ "SEVERE", View::SHAKE_SEVERE },
			{ "CINE_EXTREME", View::SHAKE_CINE_EXTREME },
			{ "CINE_INSANE",  View::SHAKE_CINE_INSANE },
			{ nullptr, 0 }
		};
		static_assert(ARRAY_SIZE(shakeTypeNames) == View::SHAKE_COUNT + 1, "Incorrect array size");

		*(Int *)store = INI::scanLookupList(ini->getNextToken(), shakeTypeNames);
	}

private:
	Bool matchesReceiver(const Object* source, const Object* receiver) const
	{
		return !m_filter || (receiver && m_filter->matchesObject(source ? source : receiver, receiver));
	}

	static void parseFilter(INI* ini, void* instance, void*, const void* userData)
	{
		auto& filter = static_cast<ViewShakeFXNugget*>(instance)->m_filter;
		SpecialPowerObjectFilter::parse(ini, instance, &filter.emplace(), userData);
	}

	View::CameraShakeType m_shake;
	std::optional<SpecialPowerObjectFilter> m_filter;

};
EMPTY_DTOR(ViewShakeFXNugget)

class CameraShakerVolumeFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(CameraShakerVolumeFXNugget, "CameraShakerVolumeFXNugget")

public:
	void doFXPos(const Coord3D* position, const Matrix3D*, Real, const Coord3D*, Real) const override
	{
		if (!position || !TheTacticalView || !(m_radius > 0.0f && m_duration > 0.0f && m_amplitude > 0.0f) ||
			!std::isfinite(m_radius) || !std::isfinite(m_duration) || !std::isfinite(m_amplitude))
			return;
		TheTacticalView->Add_Camera_Shake(*position, m_radius, m_duration, m_amplitude);
		if (traceRetailFXLists())
			std::fprintf(stderr, "[Retail FXList] CameraShakerVolume radius=%.3f duration=%.3f amplitude=%.3f\n",
				m_radius, m_duration, m_amplitude);
	}

	static void parse(INI* ini, void* instance, void*, const void*)
	{
		static const FieldParse fields[] = {
			{ "Radius", INI::parseReal, nullptr, offsetof(CameraShakerVolumeFXNugget, m_radius) },
			{ "Duration_Seconds", INI::parseReal, nullptr, offsetof(CameraShakerVolumeFXNugget, m_duration) },
			{ "Amplitude_Degrees", INI::parseReal, nullptr, offsetof(CameraShakerVolumeFXNugget, m_amplitude) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		auto* nugget = newInstance(CameraShakerVolumeFXNugget);
		ini->initFromINI(nugget, fields);
		static_cast<FXList*>(instance)->addFXNugget(nugget);
	}

private:
	Real m_radius = 0.0f;
	Real m_duration = 0.0f;
	Real m_amplitude = 0.0f;
};
EMPTY_DTOR(CameraShakerVolumeFXNugget)

class TintDrawableFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(TintDrawableFXNugget, "TintDrawableFXNugget")

public:
	void doFXPos(const Coord3D*, const Matrix3D*, Real, const Coord3D*, Real) const override {}

	void doFXObj(const Object* primary, const Object*) const override
	{
		Drawable* draw = primary ? primary->getDrawable() : nullptr;
		if (!draw || !(m_attack || m_decay || m_hold))
			return;
#if RTS_GENERALS
		draw->colorFlash(&m_color, m_decay, m_attack, m_hold);
#else
		draw->playFXTint(m_color, m_attack, m_hold, m_decay);
#endif
		if (traceRetailFXLists())
			std::fprintf(stderr, "[Retail FXList] TintDrawable attack=%u hold=%u decay=%u rgb=(%.3f,%.3f,%.3f)\n",
				m_attack, m_hold, m_decay, m_color.red, m_color.green, m_color.blue);
	}

	void doFXPosWithObjects(const Coord3D*, const Matrix3D*, Real, const Coord3D*, Real,
		const Object* subject, const Object*) const override
	{
		doFXObj(subject, nullptr);
	}

	static void parse(INI* ini, void* instance, void*, const void*)
	{
		static const FieldParse fields[] = {
			{ "Color", parseColor, nullptr, offsetof(TintDrawableFXNugget, m_color) },
			{ "PreColorTime", INI::parseUnsignedInt, nullptr, offsetof(TintDrawableFXNugget, m_attack) },
			{ "PostColorTime", INI::parseUnsignedInt, nullptr, offsetof(TintDrawableFXNugget, m_decay) },
			{ "SustainedColorTime", INI::parseUnsignedInt, nullptr, offsetof(TintDrawableFXNugget, m_hold) },
			{ "Frequency", INI::parseReal, nullptr, offsetof(TintDrawableFXNugget, m_frequency) },
			{ "Amplitude", INI::parseReal, nullptr, offsetof(TintDrawableFXNugget, m_amplitude) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		auto* nugget = newInstance(TintDrawableFXNugget);
		ini->initFromINI(nugget, fields);
		static_cast<FXList*>(instance)->addFXNugget(nugget);
	}

private:
	static void parseColor(INI* ini, void*, void* store, const void*)
	{
		const Int red = INI::scanInt(ini->getNextSubToken("R"));
		const Int green = INI::scanInt(ini->getNextSubToken("G"));
		const Int blue = INI::scanInt(ini->getNextSubToken("B"));
		if (red < -255 || red > 255 || green < -255 || green > 255 || blue < -255 || blue > 255)
			throw INI_INVALID_DATA;
		*static_cast<RGBColor*>(store) = {red / 255.0f, green / 255.0f, blue / 255.0f};
	}

	RGBColor m_color{0.0f, 0.0f, 0.0f};
	UnsignedInt m_attack = 0;
	UnsignedInt m_decay = 0;
	UnsignedInt m_hold = 0;
	Real m_frequency = 0.0f;
	Real m_amplitude = 0.0f;
};
EMPTY_DTOR(TintDrawableFXNugget)

namespace
{
struct TimedDecal
{
	AsciiString texture;
	Coord2D offset{};
	Real size = 0.0f;
	RGBColor color{};
	Int start = 0;
	Int peak = 0;
	Int end = 0;
	UnsignedInt fadeIn = 0;
	UnsignedInt hold = 0;
	UnsignedInt fadeOut = 0;
	UnsignedInt delay = 0;
	UnsignedInt lifetime = 0;
	Bool additive = FALSE;
	Bool orientToObject = TRUE;

	Bool isValid() const
	{
		return lifetime && texture.isNotEmpty() && size > 0.0f && size <= 65536.0f &&
			start >= 0 && start <= 255 && peak >= 0 && peak <= 255 && end >= 0 && end <= 255 &&
			color.red >= 0.0f && color.red <= 1.0f && color.green >= 0.0f && color.green <= 1.0f &&
			color.blue >= 0.0f && color.blue <= 1.0f;
	}

	Int opacityAt(UnsignedInt age) const
	{
		if (age < fadeIn)
			return start + static_cast<Int>((static_cast<Real>(age) / fadeIn) * (peak - start));
		age -= fadeIn;
		if (age < hold)
			return peak;
		age -= hold;
		if (age < fadeOut)
			return peak + static_cast<Int>((static_cast<Real>(age) / fadeOut) * (end - peak));
		return end;
	}
};

struct ActiveTimedDecal
{
	TimedDecal data;
	Coord3D position;
	Real angle;
	UnsignedInt issued;
	Int opacity = -1;
	WorldGroundDecal projection;

	ActiveTimedDecal(const TimedDecal& decal, const Coord3D& origin, Real yaw, UnsignedInt frame)
		: data(decal), position(origin), angle(yaw), issued(frame) {}
};

std::list<ActiveTimedDecal> retailFXDecals;
constexpr size_t MAX_RETAIL_FX_DECALS = 256;
}

void clearRetailFXDecals()
{
	retailFXDecals.clear();
}

void xferRetailFXDecals(Xfer* xfer)
{
	const Bool loading = xfer->getXferMode() == XFER_LOAD;
	UnsignedInt count = static_cast<UnsignedInt>(retailFXDecals.size());
	xfer->xferUnsignedInt(&count);
	if (count > MAX_RETAIL_FX_DECALS)
		throw SC_INVALID_DATA;

	if (loading)
	{
		clearRetailFXDecals();
		for (UnsignedInt i = 0; i < count; ++i)
			retailFXDecals.emplace_back(TimedDecal{}, Coord3D{}, 0.0f, 0);
	}

	for (auto& decal : retailFXDecals)
	{
		auto& data = decal.data;
		xfer->xferAsciiString(&data.texture);
		xfer->xferReal(&data.size);
		xfer->xferRGBColor(&data.color);
		xfer->xferInt(&data.start);
		xfer->xferInt(&data.peak);
		xfer->xferInt(&data.end);
		xfer->xferUnsignedInt(&data.fadeIn);
		xfer->xferUnsignedInt(&data.hold);
		xfer->xferUnsignedInt(&data.fadeOut);
		xfer->xferUnsignedInt(&data.delay);
		xfer->xferUnsignedInt(&data.lifetime);
		xfer->xferBool(&data.additive);
		xfer->xferCoord3D(&decal.position);
		xfer->xferReal(&decal.angle);
		xfer->xferUnsignedInt(&decal.issued);
		if (loading && (!data.isValid() || !std::isfinite(decal.position.x) ||
			!std::isfinite(decal.position.y) || !std::isfinite(decal.position.z) || !std::isfinite(decal.angle)))
			throw SC_INVALID_DATA;
	}
}

void updateRetailFXDecals()
{
	if (retailFXDecals.empty())
		return;
	const UnsignedInt frame = TheGameLogic->getFrame();
	for (auto it = retailFXDecals.begin(); it != retailFXDecals.end();)
	{
		auto& decal = *it;
		const UnsignedInt elapsed = frame - decal.issued;
		const auto& data = decal.data;
		if (frame < decal.issued || elapsed < data.delay)
		{
			++it;
			continue;
		}
		if (elapsed - data.delay >= data.lifetime)
		{
			if (traceRetailFXLists())
				std::fprintf(stderr, "[Retail FXList] DynamicDecal expired name=%s frame=%u\n", data.texture.str(), frame);
			it = retailFXDecals.erase(it);
			continue;
		}
		const Int opacity = data.opacityAt(elapsed - data.delay);
		if (!TheGlobalData->m_headless && opacity != decal.opacity)
		{
			decal.opacity = opacity;
			if (opacity == 0)
				decal.projection.clear();
			else
			{
				const Color tint = GameMakeColor(static_cast<Int>(data.color.red * 255.0f),
					static_cast<Int>(data.color.green * 255.0f), static_cast<Int>(data.color.blue * 255.0f), 255);
				decal.projection.set(data.texture, decal.position, data.size, data.size, opacity, tint, data.additive, decal.angle);
			}
			if (traceRetailFXLists())
				std::fprintf(stderr, "[Retail FXList] DynamicDecal name=%s frame=%u opacity=%d additive=%d\n",
					data.texture.str(), frame, opacity, data.additive);
		}
		++it;
	}
}

class DynamicDecalFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(DynamicDecalFXNugget, "DynamicDecalFXNugget")

public:
	void doFXPos(const Coord3D* primary, const Matrix3D* transform, Real, const Coord3D*, Real) const override
	{
		if (!primary || TheGlobalData->m_headless || !m_decal.isValid())
			return;
		Coord3D offset{m_decal.offset.x, m_decal.offset.y, 0.0f};
		Real angle = 0.0f;
		if (transform && m_decal.orientToObject)
		{
			adjustVector(&offset, transform);
			angle = transform->Get_Z_Rotation();
		}
		Coord3D position = *primary + offset;
		if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) || !std::isfinite(angle))
			return;
		if (retailFXDecals.size() == MAX_RETAIL_FX_DECALS)
			retailFXDecals.pop_front();
		retailFXDecals.emplace_back(m_decal, position, angle, TheGameLogic->getFrame());
	}

	static void parse(INI* ini, void* instance, void*, const void*)
	{
		static const FieldParse fields[] = {
			{ "DecalName", INI::parseAsciiString, nullptr, offsetof(TimedDecal, texture) },
			{ "Size", INI::parseReal, nullptr, offsetof(TimedDecal, size) },
			{ "Color", INI::parseRGBColor, nullptr, offsetof(TimedDecal, color) },
			{ "Offset", INI::parseCoord2D, nullptr, offsetof(TimedDecal, offset) },
			{ "OrientToObject", INI::parseBool, nullptr, offsetof(TimedDecal, orientToObject) },
			{ "OpacityStart", parseOpacity, nullptr, offsetof(TimedDecal, start) },
			{ "OpacityPeak", parseOpacity, nullptr, offsetof(TimedDecal, peak) },
			{ "OpacityEnd", parseOpacity, nullptr, offsetof(TimedDecal, end) },
			{ "OpacityFadeTimeOne", INI::parseDurationUnsignedInt, nullptr, offsetof(TimedDecal, fadeIn) },
			{ "OpacityPeakTime", INI::parseDurationUnsignedInt, nullptr, offsetof(TimedDecal, hold) },
			{ "OpacityFadeTimeTwo", INI::parseDurationUnsignedInt, nullptr, offsetof(TimedDecal, fadeOut) },
			{ "StartingDelay", INI::parseDurationUnsignedInt, nullptr, offsetof(TimedDecal, delay) },
			{ "Lifetime", INI::parseDurationUnsignedInt, nullptr, offsetof(TimedDecal, lifetime) },
			{ "Shader", parseShader, nullptr, offsetof(TimedDecal, additive) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		TimedDecal data;
		ini->initFromINI(&data, fields);
		auto* nugget = newInstance(DynamicDecalFXNugget);
		nugget->m_decal = data;
		static_cast<FXList*>(instance)->addFXNugget(nugget);
	}

private:
	static void parseOpacity(INI* ini, void*, void* store, const void*)
	{
		*static_cast<Int*>(store) = std::clamp(INI::scanInt(ini->getNextToken()), 0, 255);
	}

	static void parseShader(INI* ini, void*, void* store, const void*)
	{
		static const LookupListRec names[] = {
			{ "ALPHA", FALSE },
			{ "ADDITIVE", TRUE },
			{ nullptr, 0 }
		};
		*static_cast<Bool*>(store) = INI::scanLookupList(ini->getNextToken(), names) != 0;
	}

	TimedDecal m_decal;
};
EMPTY_DTOR(DynamicDecalFXNugget)

class BuffFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(BuffFXNugget, "BuffFXNugget")

public:
	Bool isBuffNugget() const override { return TRUE; }

	void doFXPos(const Coord3D*, const Matrix3D*, Real, const Coord3D*, Real) const override {}

	void doFXPosWithObjects(const Coord3D*, const Matrix3D*, Real, const Coord3D*, Real,
		const Object* source, const Object* target) const override
	{
		doFXObj(target ? target : source, nullptr);
	}

	void doFXObj(const Object* primary, const Object*) const override
	{
		if (!primary || TheGlobalData->m_headless || m_complex)
			return;
		if (const HordeContain* horde = HordeContain::get(primary))
		{
			for (const Object* member : *horde->getContainedItemsList())
				apply(*member);
		}
		else
			apply(*primary);
	}

	static void parse(INI* ini, void* instance, void*, const void*)
	{
		static const FieldParse fields[] =
		{
			{ "BuffType", INI::parseAsciiString, nullptr, offsetof(BuffFXNugget, m_type) },
			{ "BuffThingTemplate", INI::parseAsciiString, nullptr, offsetof(BuffFXNugget, m_default) },
			{ "BuffInfantryTemplate", INI::parseAsciiString, nullptr, offsetof(BuffFXNugget, m_infantry) },
			{ "BuffCavalryTemplate", INI::parseAsciiString, nullptr, offsetof(BuffFXNugget, m_cavalry) },
			{ "BuffTrollTemplate", INI::parseAsciiString, nullptr, offsetof(BuffFXNugget, m_troll) },
			{ "BuffOrcTemplate", INI::parseAsciiString, nullptr, offsetof(BuffFXNugget, m_orc) },
			{ "BuffShipTemplate", INI::parseAsciiString, nullptr, offsetof(BuffFXNugget, m_ship) },
			{ "BuffMonsterTemplate", INI::parseAsciiString, nullptr, offsetof(BuffFXNugget, m_monster) },
			{ "BuffMumakilTemplate", INI::parseAsciiString, nullptr, offsetof(BuffFXNugget, m_mumakil) },
			{ "BuffLifeTime", INI::parseDurationUnsignedInt, nullptr, offsetof(BuffFXNugget, m_duration) },
			{ "IsComplexBuff", INI::parseBool, nullptr, offsetof(BuffFXNugget, m_complex) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		auto* nugget = newInstance(BuffFXNugget);
		ini->initFromINI(nugget, fields);
		nugget->m_type.toLower();
		static_cast<FXList*>(instance)->addFXNugget(nugget);
	}

private:
	void apply(const Object& owner) const
	{
		if (owner.isEffectivelyDead())
			return;
		const struct
		{
			KindOfType kind;
			const AsciiString& art;
		} choices[] =
		{
			{ KINDOF_MUMAKIL_BUFF_NUGGET, m_mumakil },
			{ KINDOF_SHIP, m_ship },
			{ KINDOF_TROLL, m_troll },
			{ KINDOF_CAVALRY, m_cavalry },
			{ KINDOF_ORC, m_orc },
			{ KINDOF_MONSTER, m_monster },
			{ KINDOF_INFANTRY, m_infantry }
		};
		for (const auto& choice : choices)
			if (owner.isKindOf(choice.kind) && choice.art.isNotEmpty())
			{
				setRetailBuffVisual(owner, m_type, choice.art, m_duration);
				return;
			}
		setRetailBuffVisual(owner, m_type, m_default, m_duration);
	}

	AsciiString m_type, m_default, m_infantry, m_cavalry, m_troll, m_orc, m_ship, m_monster, m_mumakil;
	UnsignedInt m_duration = 0;
	Bool m_complex = FALSE;
};
EMPTY_DTOR(BuffFXNugget)

//-------------------------------------------------------------------------------------------------
class TerrainScorchFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(TerrainScorchFXNugget, "TerrainScorchFXNugget")
public:

	TerrainScorchFXNugget() : m_scorch(-1), m_radius(0)
	{
	}

	virtual void doFXPos(const Coord3D *primary, const Matrix3D* /*primaryMtx*/, const Real /*primarySpeed*/, const Coord3D * /*secondary*/, const Real /*overrideRadius*/ ) const override
	{
		if (primary)
		{
			if (m_weather >= 0 && m_weather != TheGlobalData->m_weather)
				return;
			const ScopedVisualRandom random(RetailFileLayers::enabled());
			Int scorch = m_scorch;
			if (scorch < 0)
			{
				const Int first = m_randomRange.x >= 0 ? m_randomRange.x : Int(SCORCH_1);
				const Int last = m_randomRange.x >= 0 ? m_randomRange.y : Int(SCORCH_4);
				scorch = GameClientRandomValue(first, last);
			}
			TheGameClient->addScorch(primary, m_radius, (Scorches)scorch);
			if (traceRetailFXLists())
				std::fprintf(stderr, "[Retail FXList] TerrainScorch cell=%d x=%.3f y=%.3f radius=%.3f\n",
					scorch, primary->x, primary->y, m_radius);
		}
		else
		{
			DEBUG_CRASH(("You must have a primary source for this effect"));
		}
	}

	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "Type",				parseScorchType,			nullptr, offsetof( TerrainScorchFXNugget, m_scorch ) },
			{ "Radius",			INI::parseReal,				nullptr, offsetof( TerrainScorchFXNugget, m_radius ) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		TerrainScorchFXNugget* nugget = newInstance( TerrainScorchFXNugget );
		static const FieldParse retailFields[] = {
			{ "Weather", INI::parseIndexList, WeatherNames, offsetof(TerrainScorchFXNugget, m_weather) },
			{ "RandomRange", parseRandomRange, nullptr, offsetof(TerrainScorchFXNugget, m_randomRange) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		MultiIniFieldParse fields;
		if (RetailFileLayers::enabled())
			fields.add(retailFields);
		fields.add(myFieldParse);
		ini->initFromINIMulti(nugget, fields);
		((FXList*)instance)->addFXNugget(nugget);
	}

protected:

	static void parseScorchType( INI* ini, void *instance, void *store, const void* /*userData*/ )
	{
		static const LookupListRec scorchTypeNames[] =
		{
			{ "SCORCH_1",				SCORCH_1 },
			{ "SCORCH_2",				SCORCH_2 },
			{ "SCORCH_3",				SCORCH_3 },
			{ "SCORCH_4",				SCORCH_4 },
			{ "SHADOW_SCORCH",	SHADOW_SCORCH },
			{ "RANDOM",					-1 },
			{ nullptr, 0 }
		};
		static_assert(ARRAY_SIZE(scorchTypeNames) == SCORCH_COUNT + 2, "Incorrect array size");

		*(Int *)store = INI::scanLookupList(ini->getNextToken(), scorchTypeNames);
	}

private:
	static void parseRandomRange(INI* ini, void* instance, void* store, const void* userData)
	{
		ICoord2D range;
		INI::parseICoord2D(ini, instance, &range, userData);
		if (range.x < 0 || range.y > 8 || range.x > range.y)
			throw INI_INVALID_DATA;
		*static_cast<ICoord2D*>(store) = range;
	}

	Int		m_scorch;
	Real	m_radius;
	Int m_weather = -1;
	ICoord2D m_randomRange{-1, -1};
};
EMPTY_DTOR(TerrainScorchFXNugget)

//-------------------------------------------------------------------------------------------------
class ParticleSystemFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ParticleSystemFXNugget, "ParticleSystemFXNugget")
public:

	ParticleSystemFXNugget()
	{
		m_name.clear();
		m_count = 1;
		m_radius.setRange(0, 0, GameClientRandomVariable::CONSTANT);
		m_height.setRange(0, 0, GameClientRandomVariable::CONSTANT);
		// -1 means "don't mess with it, just accept the particle-system's defaults"
		m_delay.setRange(-1, -1, GameClientRandomVariable::CONSTANT);
		m_offset.x = m_offset.y = m_offset.z = 0;
		m_orientToObject = false;
		m_attachToObject = false;
		m_ricochet = false;
		m_createAtGroundHeight = FALSE;
		m_useCallersRadius = FALSE;
		m_rotateX = m_rotateY = m_rotateZ = 0;
	}

	virtual void doFXPos(const Coord3D *primary, const Matrix3D* primaryMtx, const Real /*primarySpeed*/, const Coord3D *secondary, const Real overrideRadius ) const override
	{
		if (m_requireCreationBone)
			return;
		if (primary)
		{
			reallyDoFX(primary, primaryMtx, nullptr, overrideRadius, nullptr, nullptr, secondary);
		}
		else
		{
			DEBUG_CRASH(("You must have a primary source for this effect"));
		}
	}

	virtual void doFXObj(const Object* primary, const Object* secondary) const override
	{
		if (primary)
		{

			if (m_ricochet && secondary)
			{
				// HERE WE MUST BUILD A MATRIX WHICH WILL ORIENT THE NEW PARTICLE SYSTEM TO FACE AWAY FROM THE SECONDARY OBJECT
				// THE RESULT SHOULD LOOK LIKE THE DIRECTION OF THE "ATTACK" IS CARRIED THROUGH LIKE A RICOCHET
				Real deltaX = primary->getPosition()->x - secondary->getPosition()->x;
				Real deltaY = primary->getPosition()->y - secondary->getPosition()->y;
				Real aimingAngle = atan2(deltaY, deltaX);
				Matrix3D aimingMatrix(1);
				aimingMatrix.Rotate_Z( aimingAngle );

				doFXFromObject(primary->getPosition(), &aimingMatrix, primary, secondary, 0.0f);
			}
			else
				// if we have an object, then adjust the offset and direction by the object's transformation
				// matrix, so that (say) an offset of +10 in the z axis "follows" the orientation of the object.
				doFXFromObject(primary->getPosition(), primary->getTransformMatrix(), primary, secondary, 0.0f);
		}
		else
		{
			DEBUG_CRASH(("You must have a primary source for this effect"));
		}
	}

	void doFXPosWithObjects(const Coord3D* primary, const Matrix3D* primaryMtx, Real primarySpeed,
		const Coord3D* secondary, Real overrideRadius, const Object* source, const Object* target) const override
	{
		if (primary && source && RetailFileLayers::enabled())
			doFXFromObject(primary, primaryMtx, source, target, overrideRadius, secondary);
		else if (primary && target && !m_requireCreationBone && RetailFileLayers::enabled())
			reallyDoFX(primary, primaryMtx, nullptr, overrideRadius, nullptr, target, secondary);
		else
			doFXPos(primary, primaryMtx, primarySpeed, secondary, overrideRadius);
	}

	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "Name",									INI::parseAsciiString,			nullptr, offsetof( ParticleSystemFXNugget, m_name ) },
			{ "Count",								INI::parseInt,							nullptr, offsetof( ParticleSystemFXNugget, m_count ) },
			{ "Offset",								INI::parseCoord3D,					nullptr, offsetof( ParticleSystemFXNugget, m_offset ) },
			{ "Radius",								INI::parseGameClientRandomVariable,		nullptr, offsetof( ParticleSystemFXNugget, m_radius ) },
			{ "Height",								INI::parseGameClientRandomVariable,		nullptr, offsetof( ParticleSystemFXNugget, m_height ) },
			{ "InitialDelay",					INI::parseGameClientRandomVariable,		nullptr, offsetof( ParticleSystemFXNugget, m_delay ) },
			{ "RotateX",							INI::parseAngleReal,				nullptr, offsetof( ParticleSystemFXNugget, m_rotateX ) },
			{ "RotateY",							INI::parseAngleReal,				nullptr, offsetof( ParticleSystemFXNugget, m_rotateY ) },
			{ "RotateZ",							INI::parseAngleReal,				nullptr, offsetof( ParticleSystemFXNugget, m_rotateZ ) },
			{ "OrientToObject",				INI::parseBool,							nullptr, offsetof( ParticleSystemFXNugget, m_orientToObject ) },
			{ "Ricochet",				      INI::parseBool,							nullptr, offsetof( ParticleSystemFXNugget, m_ricochet ) },
			{ "AttachToObject",				INI::parseBool,							nullptr, offsetof( ParticleSystemFXNugget, m_attachToObject ) },
			{ "CreateAtGroundHeight",	INI::parseBool,							nullptr, offsetof( ParticleSystemFXNugget, m_createAtGroundHeight ) },
			{ "UseCallersRadius",			INI::parseBool,							nullptr, offsetof( ParticleSystemFXNugget, m_useCallersRadius ) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		static const FieldParse retailOnlyFieldParse[] =
		{
			{ "Name", parseRetailName, nullptr, 0 },
			{ "AttachToBone", INI::parseAsciiString, nullptr, offsetof(ParticleSystemFXNugget, m_retailAttachmentBone) },
			{ "CreateBoneOverride", INI::parseAsciiString, nullptr, offsetof(ParticleSystemFXNugget, m_retailCreateBone) },
			{ "TargetBoneOverride", INI::parseAsciiString, nullptr, offsetof(ParticleSystemFXNugget, m_retailTargetBone) },
			{ "UseTargetOffset", INI::parseBool, nullptr, offsetof(ParticleSystemFXNugget, m_retailUseTargetOffset) },
			{ "TargetOffset", INI::parseCoord3D, nullptr, offsetof(ParticleSystemFXNugget, m_retailTargetOffset) },
			{ "SetTargetMatrix", INI::parseBool, nullptr, offsetof(ParticleSystemFXNugget, m_retailSetTargetMatrix) },
			{ "OnlyIfOnLand", INI::parseBool, nullptr, offsetof(ParticleSystemFXNugget, m_retailOnlyIfOnLand) },
			{ "OnlyIfOnWater", INI::parseBool, nullptr, offsetof(ParticleSystemFXNugget, m_retailOnlyIfOnWater) },
			{ "Weather", INI::parseIndexList, WeatherNames, offsetof(ParticleSystemFXNugget, m_retailWeather) },
			{ "ObjectFilter", parseRetailObjectFilter, nullptr, 0 },
			{ "SystemLife", INI::parseInt, nullptr, offsetof(ParticleSystemFXNugget, m_retailSystemLife) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		ParticleSystemFXNugget* nugget = newInstance( ParticleSystemFXNugget );
		MultiIniFieldParse fields;
		if (RetailFileLayers::enabled())
			fields.add(retailOnlyFieldParse);
		fields.add(myFieldParse);
		ini->initFromINIMulti(nugget, fields);
		((FXList*)instance)->addFXNugget(nugget);
	}

	static void parseBone(INI* ini, void* instance, void*, const void*)
	{
		const AsciiString bone = ini->getNextAsciiString();
		const AsciiString name = ini->getNextAsciiString();
		Bool follow = FALSE;
		if (const char* option = ini->getNextTokenOrNull(INI::getSepsColon()))
		{
			if (stricmp(option, "FollowBone") != 0)
				throw INI_INVALID_DATA;
			follow = INI::scanBool(ini->getNextToken(INI::getSepsColon()));
		}
		auto* nugget = newInstance(ParticleSystemFXNugget);
		nugget->m_name = name;
		nugget->m_retailCreateBone = bone;
		nugget->m_retailFollowBone = follow;
		nugget->m_requireCreationBone = TRUE;
		static_cast<FXList*>(instance)->addFXNugget(nugget);
	}

protected:

	static void parseRetailName(INI* ini, void* instance, void*, const void*)
	{
		auto* nugget = static_cast<ParticleSystemFXNugget*>(instance);
		nugget->m_name = ini->getNextAsciiString();
		const char* field = ini->getNextTokenOrNull(INI::getSepsColon());
		if (field && stricmp(field, "FollowBone") == 0)
			nugget->m_retailFollowBone = INI::scanBool(ini->getNextToken(INI::getSepsColon()));
	}

	static void parseRetailObjectFilter(INI* ini, void* instance, void*, const void* userData)
	{
		auto* nugget = static_cast<ParticleSystemFXNugget*>(instance);
		if (!nugget->m_retailObjectFilter)
			nugget->m_retailObjectFilter.emplace();
		SpecialPowerObjectFilter::parse(ini, instance, &*nugget->m_retailObjectFilter, userData);
	}

	void doFXFromObject(const Coord3D* position, const Matrix3D* transform, const Object* object, const Object* target,
		Real overrideRadius, const Coord3D* secondary = nullptr) const
	{
		const Drawable* drawable = object->getDrawable();
		const AsciiString& boneName = m_retailCreateBone.isNotEmpty() ? m_retailCreateBone : m_retailAttachmentBone;
		Matrix3D creation;
		Coord3D origin;
		const Bool hasCreation = drawable && boneName.isNotEmpty() &&
			drawable->getCurrentWorldspaceClientBonePositions(boneName.str(), creation);
		if (!hasCreation && m_requireCreationBone)
			return;
		if (hasCreation)
		{
			origin = {creation.Get_X_Translation(), creation.Get_Y_Translation(), creation.Get_Z_Translation()};
			position = &origin;
			transform = &creation;
			if (traceRetailFXLists())
				std::fprintf(stderr, "[Retail FXList] ParticleOrigin name=%s bone=%s position=(%.3f,%.3f,%.3f)\n",
					m_name.str(), boneName.str(), origin.x, origin.y, origin.z);
		}

		const AsciiString& followName = m_retailAttachmentBone.isNotEmpty() ? m_retailAttachmentBone : m_retailCreateBone;
		if (drawable && followName.isNotEmpty() &&
			(m_retailAttachmentBone.isNotEmpty() || m_retailFollowBone || m_attachToObject))
		{
			if (hasCreation && followName == boneName)
			{
				reallyDoFX(position, transform, object, overrideRadius, &followName, target, secondary, &creation);
				return;
			}
			Matrix3D attachment;
			if (drawable->getCurrentWorldspaceClientBonePositions(followName.str(), attachment))
			{
				reallyDoFX(position, transform, object, overrideRadius, &followName, target, secondary, &attachment);
				return;
			}
		}
		reallyDoFX(position, transform, object, overrideRadius, nullptr, target, secondary);
	}

	void reallyDoFX(const Coord3D *primary, const Matrix3D* mtx, const Object* thingToAttachTo,
		Real overrideRadius, const AsciiString* attachmentBone, const Object* target, const Coord3D* secondary,
		const Matrix3D* attachmentTransform = nullptr) const
	{
		const ScopedVisualRandom random(RetailFileLayers::enabled());
		if (m_retailWeather >= 0 && m_retailWeather != (TheGlobalData ? TheGlobalData->m_weather : WEATHER_NORMAL))
			return;
		if (m_retailObjectFilter)
		{
			const Object* subject = target ? target : thingToAttachTo;
			if (!subject || !m_retailObjectFilter->matchesObject(thingToAttachTo, subject))
				return;
		}

		Coord3D offset = m_offset;
		if (mtx)
		{
			adjustVector(&offset, mtx);
		}

		const ParticleSystemTemplate *tmp = TheParticleSystemManager->findTemplate(m_name);
		DEBUG_ASSERTCRASH(tmp, ("ParticleSystem %s not found",m_name.str()));
		if (tmp)
		{
			const bool filterSurface = m_retailOnlyIfOnLand || m_retailOnlyIfOnWater;
			for (Int i = 0; i < m_count; i++ )
			{
				ParticleSystem* sys = filterSurface ? nullptr : TheParticleSystemManager->createParticleSystem(tmp);
				if (sys || filterSurface)
				{
					Coord3D newPos;
					Real radius = m_radius.getValue();
					Real angle = GameClientRandomValueReal( 0.0f, 2.0f * PI );

					newPos.x = primary->x + offset.x + radius * cos(angle);
					newPos.y = primary->y + offset.y + radius * sin(angle);
					newPos.z = primary->z + offset.z;
					if( m_createAtGroundHeight && TheTerrainLogic )
					{
						//old way:
						//newPos.z = TheTerrainLogic->getGrsoundHeight( newPos.x, newPos.y ) + 1;// The plus one prevents scissoring with terrain

						//new way: now we allow bridges in the GroundHeight.
						PathfindLayerEnum layer = TheTerrainLogic->getLayerForDestination(&newPos);
						newPos.z = TheTerrainLogic->getLayerHeight( newPos.x, newPos.y, layer );
					}
					else
						newPos.z += m_height.getValue();

					if (filterSurface)
					{
						if (!TheTerrainLogic || TheTerrainLogic->isUnderwater(newPos.x, newPos.y) == m_retailOnlyIfOnLand)
							continue;
						sys = TheParticleSystemManager->createParticleSystem(tmp);
						if (!sys)
							continue;
					}

					if (m_orientToObject && mtx && !attachmentBone)
					{
						const Coord3D* aimPosition = target ? target->getPosition() : secondary;
						if (m_retailSetTargetMatrix && aimPosition)
						{
							const Coord3D delta = *aimPosition - newPos;
							Matrix3D aim = *mtx;
							aim.Rotate_Y(DEG_TO_RADF(180.0f) - WWMath::Atan2Origin(delta.z + 10.0f,
								WWMath::Sqrt(delta.x * delta.x + delta.y * delta.y)));
							sys->setRetailTargetMatrix(&aim);
						}
						else
							sys->setLocalTransform(mtx);
					}
					if (m_rotateX != 0.0f)
						sys->rotateLocalTransformX(m_rotateX);
					if (m_rotateY != 0.0f)
						sys->rotateLocalTransformY(m_rotateY);
					if (m_rotateZ != 0.0f)
						sys->rotateLocalTransformZ(m_rotateZ);

					if (attachmentBone)
					{
						const Vector3 worldPosition(newPos.x, newPos.y, newPos.z);
						Vector3 bonePosition;
						Matrix3D::Inverse_Transform_Vector(*attachmentTransform, worldPosition, &bonePosition);
						const Coord3D local = {bonePosition.X, bonePosition.Y, bonePosition.Z};
						sys->setPosition(&local);
						sys->attachToObjectBone(thingToAttachTo, *attachmentBone);
						if (traceRetailFXLists())
							std::fprintf(stderr, "[Retail FXList] ParticleBone name=%s bone=%s origin=(%.3f,%.3f,%.3f)\n",
								m_name.str(), attachmentBone->str(), newPos.x, newPos.y, newPos.z);
					}
					else if (m_attachToObject && thingToAttachTo)
						sys->attachToObject(thingToAttachTo);
					else
						sys->setPosition( &newPos );

					if (m_retailUseTargetOffset)
						sys->setRetailTargetEndpoint(newPos + m_retailTargetOffset);
					else if (target && m_retailTargetBone.isNotEmpty())
					{
						const Drawable* draw = target->getDrawable();
						Matrix3D bone;
						if (draw && draw->getCurrentWorldspaceClientBonePositions(m_retailTargetBone.str(), bone))
							sys->setRetailTargetEndpoint({bone.Get_X_Translation(), bone.Get_Y_Translation(), bone.Get_Z_Translation()});
						else
							sys->setRetailTargetEndpoint(*target->getPosition());
					}

					if (m_retailSystemLife >= 0)
						sys->setRetailSystemLifetime(static_cast<UnsignedInt>(m_retailSystemLife));

					Real delayInMsec = m_delay.getValue();
					if (delayInMsec >= 0.0f)
					{
						UnsignedInt delayInFrames = REAL_TO_INT_CEIL(ConvertDurationFromMsecsToFrames(delayInMsec));
						sys->setInitialDelay(delayInFrames);
					}

					if( m_useCallersRadius  &&  overrideRadius )
					{
						ParticleSystemInfo::EmissionVolumeType type = sys->getEmisionVolumeType();

						if( type == ParticleSystemInfo::EmissionVolumeType::SPHERE )
							sys->setEmissionVolumeSphereRadius( overrideRadius );
						else if( type == ParticleSystemInfo::EmissionVolumeType::CYLINDER )
							sys->setEmissionVolumeCylinderRadius( overrideRadius );
					}
				}
			}
		}
	}


private:
	AsciiString m_retailAttachmentBone;
	AsciiString m_retailCreateBone;
	AsciiString m_retailTargetBone;
	Bool m_retailUseTargetOffset = FALSE;
	Coord3D m_retailTargetOffset{};
	Bool m_retailFollowBone = FALSE;
	Bool m_requireCreationBone = FALSE;
	Bool m_retailSetTargetMatrix = FALSE;
	Bool m_retailOnlyIfOnLand = FALSE;
	Bool m_retailOnlyIfOnWater = FALSE;
	Int m_retailWeather = -1;
	Int m_retailSystemLife = -1;
	std::optional<SpecialPowerObjectFilter> m_retailObjectFilter;
	AsciiString			m_name;
	Int							m_count;
	Coord3D					m_offset;
	GameClientRandomVariable	m_radius;
	GameClientRandomVariable	m_height;
	GameClientRandomVariable	m_delay;
	Real						m_rotateX, m_rotateY, m_rotateZ;
	Bool						m_orientToObject;
	Bool						m_attachToObject;
	Bool						m_createAtGroundHeight;
	Bool						m_useCallersRadius;
	Bool						m_ricochet;
};
EMPTY_DTOR(ParticleSystemFXNugget)

//-------------------------------------------------------------------------------------------------
class FXListAtBonePosFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(FXListAtBonePosFXNugget, "FXListAtBonePosFXNugget")
public:

	FXListAtBonePosFXNugget()
	{
		m_fx = nullptr;
    m_boneName.clear();
    m_orientToBone = true;
	}

	virtual void doFXPos(const Coord3D *primary, const Matrix3D* primaryMtx, const Real /*primarySpeed*/, const Coord3D * /*secondary*/, const Real /*overrideRadius*/ ) const override
	{
		DEBUG_CRASH(("You must use the object form for this effect"));
	}

	virtual void doFXObj(const Object* primary, const Object* /*secondary*/) const override
	{
		if (m_weather >= 0 && m_weather != TheGlobalData->m_weather)
			return;
		const ScopedVisualRandom random(RetailFileLayers::enabled());
		if (primary)
		{
      // first, try the unadorned name.
			doFxAtBones(primary, 0);

      // then, try the 01,02,03...etc names.
			doFxAtBones(primary, 1);
		}
		else
		{
			DEBUG_CRASH(("You must have a primary source for this effect"));
		}
	}

	void doFXPosWithObjects(const Coord3D*, const Matrix3D*, Real, const Coord3D*, Real,
		const Object* source, const Object*) const override
	{
		if (source && RetailFileLayers::enabled())
			doFXObj(source, nullptr);
	}

	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "FX",								  	INI::parseFXList,			    nullptr, offsetof( FXListAtBonePosFXNugget, m_fx ) },
			{ "BoneName",							INI::parseAsciiString,		nullptr, offsetof( FXListAtBonePosFXNugget, m_boneName ) },
			{ "OrientToBone",					INI::parseBool,					  nullptr, offsetof( FXListAtBonePosFXNugget, m_orientToBone ) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		FXListAtBonePosFXNugget* nugget = newInstance( FXListAtBonePosFXNugget );
		static const FieldParse retailFields[] = {
			{ "Weather", INI::parseIndexList, WeatherNames, offsetof(FXListAtBonePosFXNugget, m_weather) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		MultiIniFieldParse fields;
		if (RetailFileLayers::enabled())
			fields.add(retailFields);
		fields.add(myFieldParse);
		ini->initFromINIMulti(nugget, fields);
		((FXList*)instance)->addFXNugget(nugget);
	}

protected:

	void doFxAtBones(const Object* obj, Int start) const
	{
    Coord3D bonePos[MAX_BONE_POINTS];
    Matrix3D boneMtx[MAX_BONE_POINTS];

    Drawable* draw = obj->getDrawable();
		if (draw)
		{
			// yes, BONEPOS_CURRENT_CLIENT_ONLY -- this is client-only, so should be safe to do.
			Int count = draw->getCurrentClientBonePositions(m_boneName.str(), start, bonePos, boneMtx, MAX_BONE_POINTS);
			for (Int i = 0; i < count; ++i)
			{
				Coord3D p;
				Matrix3D m;
				obj->convertBonePosToWorldPos(&bonePos[i], &boneMtx[i], &p, &m);
				if (traceRetailFXLists())
					std::fprintf(stderr, "[Retail FXList] BoneFX bone=%s weather=%d\n", m_boneName.str(), m_weather);
				FXList::doFXPos(m_fx, &p, m_orientToBone || !RetailFileLayers::enabled() ? &m : nullptr, 0.0f, nullptr, 0.0f);
			}
		}
	}

private:

	enum { MAX_BONE_POINTS = 40 };

	const FXList*		m_fx;
  AsciiString     m_boneName;
  Bool            m_orientToBone;
	Int m_weather = -1;
};
EMPTY_DTOR(FXListAtBonePosFXNugget)

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

static const FieldParse TheFXListFieldParse[] =
{
	{ "Sound",											SoundFXNugget::parse, nullptr, 0},
	{ "RayEffect",									RayEffectFXNugget::parse, nullptr, 0},
	{ "Tracer",											TracerFXNugget::parse, nullptr, 0},
	{ "LightPulse",									LightPulseFXNugget::parse, nullptr, 0},
	{ "ViewShake",									ViewShakeFXNugget::parse, nullptr, 0},
	{ "TerrainScorch",							TerrainScorchFXNugget::parse, nullptr, 0},
	{ "ParticleSystem",							ParticleSystemFXNugget::parse, nullptr, 0},
	{ "FXListAtBonePos",						FXListAtBonePosFXNugget::parse, nullptr, 0},
	{ nullptr, nullptr, nullptr, 0 }
};

//-------------------------------------------------------------------------------------------------
FXList::FXList()
{
}

//-------------------------------------------------------------------------------------------------
FXList::~FXList()
{
	clear();
}

//-------------------------------------------------------------------------------------------------
void FXList::clear()
{
	for (FXNuggetList::iterator it = m_nuggets.begin(); it != m_nuggets.end(); ++it)
	{
		deleteInstance(*it);
	}
	m_nuggets.clear();
	m_playEvenIfShrouded = FALSE;
	m_cullingFrames = 0;
	m_startCullingAbove = 0;
	m_cullAllAbove = 0;
	resetCulling();
}

void FXList::parseCullingInfo(INI* ini, void* instance, void*, const void*)
{
	auto& fx = *static_cast<FXList*>(instance);
	const Real seconds = INI::scanReal(ini->getNextSubToken("TrackingSeconds"));
	const Int start = INI::scanInt(ini->getNextSubToken("StartCullingAbove"));
	const Int stop = INI::scanInt(ini->getNextSubToken("CullAllAbove"));
	if (!(seconds > 0.0f) || !std::isfinite(seconds) || start < 0 || stop <= start ||
		static_cast<double>(seconds) * LOGICFRAMES_PER_SECOND >= std::numeric_limits<UnsignedInt>::max())
		return;
	fx.m_cullingFrames = std::max(1u, static_cast<UnsignedInt>(static_cast<double>(seconds) * LOGICFRAMES_PER_SECOND));
	fx.m_startCullingAbove = start;
	fx.m_cullAllAbove = stop;
}

void FXList::resetCulling()
{
	m_recentPlays.clear();
}

Bool FXList::allowRetailPlayback() const
{
	if (!m_cullingFrames || !TheGameLogic)
		return TRUE;
	const UnsignedInt frame = TheGameLogic->getFrame();
	if (!m_recentPlays.empty() && frame < m_recentPlays.back())
		m_recentPlays.clear();
	while (!m_recentPlays.empty() && frame - m_recentPlays.front() >= m_cullingFrames)
		m_recentPlays.pop_front();
	const Int count = static_cast<Int>(m_recentPlays.size());
	if (count >= m_cullAllAbove)
		return FALSE;
	m_recentPlays.push_back(frame);
	if (count <= m_startCullingAbove)
		return TRUE;
	const ScopedVisualRandom random(TRUE);
	return GameClientRandomValue(m_startCullingAbove, m_cullAllAbove - 1) >= count;
}

//-------------------------------------------------------------------------------------------------
void FXList::doFXFromDrawable(const FXList* fx, const Drawable& drawable)
{
	const ScopedVisualRandom random(RetailFileLayers::enabled());
	if (const Object* object = drawable.getObject())
		doFXObj(fx, object);
	else
		doFXPos(fx, drawable.getPosition());
}

void FXList::doFXPos(const Coord3D* primary, const Matrix3D* primaryMtx, Real primarySpeed,
	const Coord3D* secondary, Real overrideRadius, const Object* source, const Object* target) const
{
	const Int playerIndex = rts::getObservedOrLocalPlayer()->getPlayerIndex();
	if (!m_playEvenIfShrouded && ThePartitionManager->getShroudStatusForPlayer(playerIndex, primary) != CELLSHROUD_CLEAR)
		return;
	if (!allowRetailPlayback())
		return;

	for (FXNuggetList::const_iterator it = m_nuggets.begin(); it != m_nuggets.end(); ++it)
	{
		if (source || target)
			(*it)->doFXPosWithObjects(primary, primaryMtx, primarySpeed, secondary, overrideRadius, source, target);
		else
			(*it)->doFXPos(primary, primaryMtx, primarySpeed, secondary, overrideRadius);
	}
}

//-------------------------------------------------------------------------------------------------
void FXList::doFXObj(const Object* primary, const Object* secondary, Bool buffsOnly) const
{
	const Int playerIndex = rts::getObservedOrLocalPlayer()->getPlayerIndex();

	if (!m_playEvenIfShrouded && primary)
	{
		if (RetailFileLayers::enabled())
		{
			const PartitionData* partition = primary->friend_getPartitionData();
			if (!primary->isKindOf(KINDOF_ALWAYS_VISIBLE) && partition && !partition->hasClearCell(playerIndex))
				return;
		}
		else if (primary->getShroudedStatus(playerIndex) > OBJECTSHROUD_PARTIAL_CLEAR)
			return;
	}
	if (!buffsOnly && !allowRetailPlayback())
		return;

	for (FXNuggetList::const_iterator it = m_nuggets.begin(); it != m_nuggets.end(); ++it)
	{

		// HERE THE PRIMARY IS THE GUY RECEIVING THE FX, AND SECONDARY MIGHT BE THE GUY DEALING IT
		if (!buffsOnly || (*it)->isBuffNugget())
			(*it)->doFXObj(primary, secondary);
	}
}



//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
FXListStore::FXListStore()
{
}

//-------------------------------------------------------------------------------------------------
FXListStore::~FXListStore()
{
	m_fxmap.clear();
}

void FXListStore::reset()
{
	for (auto& entry : m_fxmap)
		entry.second.resetCulling();
}

//-------------------------------------------------------------------------------------------------
const FXList *FXListStore::findFXList(const char* name) const
{
	if (stricmp(name, "None") == 0)
		return nullptr;

  FXListMap::const_iterator it = m_fxmap.find(NAMEKEY(name));
  if (it != m_fxmap.end())
	{
		return &(*it).second;
	}
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
/*static */ void FXListStore::parseFXListDefinition(INI *ini)
{
	// read the FXList name
	const char *c = ini->getNextToken();
	NameKeyType key = TheNameKeyGenerator->nameToKey(c);
	FXList& fxl = TheFXListStore->m_fxmap[key];
	fxl.clear();
	static const FieldParse retailFields[] = {
		{ "EvaEvent", EvaEventFXNugget::parse, nullptr, 0 },
		{ "CameraShakerVolume", CameraShakerVolumeFXNugget::parse, nullptr, 0 },
		{ "TintDrawable", TintDrawableFXNugget::parse, nullptr, 0 },
		{ "ParticleSysBone", ParticleSystemFXNugget::parseBone, nullptr, 0 },
		{ "DynamicDecal", DynamicDecalFXNugget::parse, nullptr, 0 },
		{ "BuffNugget", BuffFXNugget::parse, nullptr, 0 },
		{ "PlayEvenIfShrouded", INI::parseBool, nullptr, offsetof(FXList, m_playEvenIfShrouded) },
		{ "CullingInfo", FXList::parseCullingInfo, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	MultiIniFieldParse fields;
	if (RetailFileLayers::enabled())
		fields.add(retailFields);
	fields.add(TheFXListFieldParse);
	ini->initFromINIMulti(&fxl, fields);
}

//-------------------------------------------------------------------------------------------------
/*static*/ void INI::parseFXListDefinition(INI *ini)
{
	FXListStore::parseFXListDefinition(ini);
}
