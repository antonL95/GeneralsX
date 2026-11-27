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

// FILE: FXList.h /////////////////////////////////////////////////////////////////////////////////
// Author: Steven Johnson, December 2001
// Desc:   General Effects Descriptions
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/GameMemory.h"
#include "Common/NameKeyGenerator.h"
#include "Common/STLTypedefs.h"
#include <deque>

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class FXNugget;
class FXList;
class FXListStore;
class SoundFXNugget;
class INI;
class Object;
class Matrix3D;
class Drawable;

//-------------------------------------------------------------------------------------------------
/**
	An FXNugget encapsulates a particular type of audio/video effect. FXNuggets are virtually
	never used on their own, but rather, as a component of an FXList (see below).

	Important notes:

	-- FXNugget is an ABC; all the implementations are (currently) located in FXList.cpp,
	thought they will probably be spread out more as we add more implementations.

	-- As part of an FXList, an FXNugget is shared between multiple units. The implication is that
	an FXNugget should not require private data storage to do what it needs to do, aside from stuff
	initialized at FXNugget instantiation time (eg, parameters from an INI file). To help
	enforce this, all it's methods are declared 'const'. If you can't implement what you
	need within this framework, please *don't* simply de-const things, because it could lead to very
	strange side-effects. Instead, the system will have to be enhanced to allow for multiple instances
	of each FXNugget.

	-- an individual FXNugget is generally not directly accessible to anyone outside of the
	FXList system; in fact, it could probably be a private class, but isn't, mainly for coding convenience.

	-- Unlike most other game systems, FXNuggets can't be overridden by subsequent INI file
	loads. This isn't really a problem, because all you really need to do to "override" one is to
	specify a different one.
*/
class FXNugget : public MemoryPoolObject
{
	MEMORY_POOL_GLUE_ABC(FXNugget)
public:

	FXNugget() { }
	//virtual ~FXNugget() { }

	/**
		The main guts of the system: actually perform the sound and/or video effects
		needed. Note that primary and/or secondary can be null, so you must check for this.
	*/
	virtual void doFXPos(const Coord3D *primary, const Matrix3D* primaryMtx = nullptr, const Real primarySpeed = 0.0f, const Coord3D *secondary = nullptr, const Real overrideRadius = 0.0f) const = 0;
	virtual void doFXPosWithObjects(const Coord3D* primary, const Matrix3D* primaryMtx, Real primarySpeed,
		const Coord3D* secondary, Real overrideRadius, const Object* source, const Object* target) const;
	virtual Bool isBuffNugget() const { return FALSE; }
	virtual Bool refreshObj(const Object*) const { return FALSE; }
	virtual const SoundFXNugget* asSoundNugget() const { return nullptr; }

	/**
		the object-based version... by default, just call the location-based implementation.
		Note that primary and/or secondary can be null, so you must check for this.
	*/
	virtual void doFXObj(const Object* primary, const Object* secondary = nullptr) const;

private:

};
EMPTY_DTOR(FXNugget)

//-------------------------------------------------------------------------------------------------
/**
	An FXList is a way of encapsulating a particular set of audio/video effect(s).
	Lots of other game systems (eg, DamageFX) use FXLists to abstract AV effects into data files
	(rather than hardcoding them, which would be suboptimal).

	Important notes:

	-- an FXList is specified solely by name, and the only parameters it receives when performing
	its AV effects are a primary (and optional secondary) object position.

	-- There is no inheritance or overriding of FXLists; if you need an FXList that is nearly-but-not-quite
	identical to an existing one, you must simply make an entirely new FXList. Realistically, this shouldn't
	be a problem, since they are pretty simple to specify, and don't consume a lot of memory.

	-- an FXList is shared between multiple units. To help
	enforce this, all it's methods are declared 'const'. If you can't implement the stuff you
	need within this framework, please *don't* simply de-const things, because it could lead to very
	strange side-effects. Instead, the system will have to be enhanced to allow for multiple instances
	of each FXNugget.

	-- Unlike most other game systems, FXList can't be overridden by subsequent INI file
	loads. This isn't really a problem, because all you really need to do to "override" one is to
	specify a different one.
*/
class FXList
{
	friend class FXListStore;

public:

	FXList();
	virtual ~FXList();

	/**
		Toss the contents.
	*/
	void clear();

	/**
		add a nugget to the list. It belongs to the FXList, who is responsible for freeing it.
	*/
	void addFXNugget(FXNugget *fxn, Bool createsBoneParticles = FALSE)
	{
		m_nuggets.push_back(fxn);
		m_hasBoneParticles |= createsBoneParticles;
	}

	Bool hasBoneParticles() const { return m_hasBoneParticles; }

	/// inline convenience method to avoid having to check for null.
	inline static void doFXPos(const FXList* fx, const Coord3D *primary, const Matrix3D* primaryMtx = nullptr, const Real primarySpeed = 0.0f, const Coord3D *secondary = nullptr, const Real overrideRadius = 0.0f,
		const Object* source = nullptr, const Object* target = nullptr)
	{
		if (fx) fx->doFXPos(primary, primaryMtx, primarySpeed, secondary, overrideRadius, source, target);
	}

	Bool playsVoice() const;

	/// inline convenience method to avoid having to check for null.
	inline static void doFXObj(const FXList* fx, const Object* primary, const Object* secondary = nullptr)
	{
		if (fx)
		{
			fx->doFXObj(primary, secondary);

			//if (fx->)				// here we need to cal doFXRicochet, if fx calls for it

		}

	}

	static void doFXFromDrawable(const FXList* fx, const Drawable& drawable);
	void refreshBuffs(const Object* primary) const { doFXObj(primary, nullptr, TRUE); }


protected:


	/**
		The main guts of the system: actually perform the sound and/or video effects
		needed. Note that primary and/or secondary can be null, so you must check for this.
	*/
	void doFXPos(const Coord3D* primary, const Matrix3D* primaryMtx, Real primarySpeed,
		const Coord3D* secondary, Real overrideRadius, const Object* source, const Object* target) const;

	/**
		the object-based version... by default, just call the location-based implementation.
		Note that primary and/or secondary can be null, so you must check for this.
	*/
	void doFXObj(const Object* primary, const Object* secondary, Bool buffsOnly = FALSE) const;

private:
	static void parseCullingInfo(INI* ini, void* instance, void*, const void*);
	Bool allowRetailPlayback() const;
	void resetCulling();

	typedef std::list< FXNugget* > FXNuggetList;

	FXNuggetList m_nuggets;
	Bool m_playEvenIfShrouded = FALSE;
	Bool m_hasBoneParticles = FALSE;
	UnsignedInt m_cullingFrames = 0;
	Int m_startCullingAbove = 0;
	mutable std::deque<UnsignedInt> m_recentPlays;

};

//-------------------------------------------------------------------------------------------------
/**
	The "store" used to hold all the FXLists in existence.
*/
class FXListStore : public SubsystemInterface
{

public:

	FXListStore();
	virtual ~FXListStore() override;

	virtual void init() override { }
	void reset() override;
	virtual void update() override { }

	/**
		return the FXList with the given namekey.
		return nullptr if no such FXList exists.
	*/
	const FXList *findFXList( const char* name ) const;

	static void parseFXListDefinition(INI* ini);

private:

	// use the hashing function for Ints.
	typedef std::hash_map< NameKeyType, FXList, rts::hash<NameKeyType>, rts::equal_to<NameKeyType>/**/> FXListMap;

	FXListMap m_fxmap;

};

// EXTERNALS //////////////////////////////////////////////////////////////////////////////////////
extern FXListStore *TheFXListStore;
