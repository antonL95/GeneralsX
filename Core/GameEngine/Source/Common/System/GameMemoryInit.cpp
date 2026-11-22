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

// FILE: MemoryInit.cpp
//-----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:   RTS3
//
// File name: MemoryInit.cpp
//
// Created:   Steven Johnson, August 2001
//
// Desc:      Memory manager
//
// ----------------------------------------------------------------------------
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#ifndef DISABLE_GAMEMEMORY

// SYSTEM INCLUDES

// USER INCLUDES
#include "Common/GameMemory.h"
#include "Common/RetailFileLayers.h"

struct PoolSizeRec
{
	const char* name;
	Int initial;
	Int overflow;
};

#if RTS_GENERALS
#include "GameMemoryInitDMA_Generals.inl"
#include "GameMemoryInitPools_Generals.inl"
#elif RTS_ZEROHOUR
#include "GameMemoryInitDMA_GeneralsMD.inl"
#include "GameMemoryInitPools_GeneralsMD.inl"
#endif

static const PoolSizeRec RetailPoolSizes[] =
{
#if RTS_ZEROHOUR
	{ "ParticleSystemTemplatePool", 4500, 64 },
#endif
	{ "W3DScriptedModelDraw", 12288, 1024 },
	{ "SpecialPowerModule", 768, 128 },
	{ "AISpecialPowerUpdate", 1024, 128 },
	{ "W3DHordeModelDraw", 1024, 256 },
	{ "HordeContain", 256, 64 },
	{ "HordeTransportContain", 64, 32 },
	{ "SiegeEngineContain", 64, 32 },
	{ "HordeSiegeEngineContain", 64, 32 },
	{ "ToggleDeploySpecialAbilityUpdate", 64, 32 },
	{ "SiegeDockingBehavior", 64, 32 },
	{ "SiegeAIUpdate", 64, 32 },
	{ "SiegeDeploySpecialPower", 64, 32 },
	{ "StopSpecialPower", 64, 32 },
	{ "SiegeDeployHordeSpecialPower", 64, 32 },
	{ "ManTheWallsSpecialPower", 64, 32 },
	{ "HordeGarrisonContain", 128, 32 },
	{ "EvacuateDamage", 128, 32 },
	{ "HordeTransportContainDamage", 32, 16 },
	{ "W3DFloorDraw", 512, 64 },
	{ "StancesBehavior", 256, 64 },
	{ "LargeGroupAudioUpdate", 256, 64 },
	{ "PickupStuffUpdate", 256, 64 },
	{ "HitReactionBehavior", 256, 64 },
	{ "HordeMemberCollide", 512, 128 },
	{ "CastleMemberBehavior", 128, 32 },
	{ "CastleBehavior", 32, 16 },
	{ "FoundationAIUpdate", 128, 32 },
	{ "GateOpenAndCloseBehavior", 32, 16 },
	{ "GateProxyBehavior", 16, 16 },
	{ "AIGateUpdate", 32, 16 },
	{ "WallHubBehavior", 64, 32 },
	{ "WallUpgradeUpdate", 32, 16 },
	{ "GettingBuiltBehavior", 256, 64 },
	{ "ReplaceSelfUpgrade", 64, 32 },
	{ "FakePathfindPortalBehaviour", 32, 16 },
	{ "DynamicPortalBehaviour", 32, 16 },
	{ "HorseHordeContain", 64, 32 },
	{ "AODHordeContain", 16, 16 },
	{ "BezierProjectileBehavior", 512, 128 },
	{ "W3DStreakDraw", 512, 128 },
	{ "RespawnBody", 64, 32 },
	{ "RespawnUpdate", 64, 32 },
	{ "DelayedDeathBody", 64, 32 },
	{ "PorcupineFormationBodyModule", 256, 64 },
	{ "SymbioticStructuresBody", 16, 16 },
	{ "DetachableRiderBody", 16, 16 },
	{ "WeaponFireSpecialAbilityUpdate", 256, 64 },
	{ "ToggleMountedSpecialAbilityUpdate", 64, 32 },
	{ "ActivateModuleSpecialPower", 64, 32 },
	{ "HeroModeSpecialAbilityUpdate", 32, 16 },
	{ "WeaponModeSpecialPowerUpdate", 64, 32 },
	{ "MonitorConditionUpdate", 64, 32 },
	{ "AutoAbilityBehavior", 512, 128 },
	{ "PlayerHealSpecialPower", 32, 16 },
	{ "AttributeModifierHelper", 8192, 1024 },
	{ "AttributeModifierAuraUpdate", 1024, 256 },
	{ "PassiveAreaEffectBehavior", 128, 32 },
	{ "BannerCarrierUpdate", 128, 32 },
	{ "AttributeModifierUpgrade", 512, 128 },
	{ "HordeDispatchSpecialPower", 64, 32 },
	{ "SpecialPowerTimerRefreshSpecialPower", 32, 16 },
	{ "ProductionSpeedBonus", 32, 16 },
	{ "InvisibilitySpecialPower", 32, 16 },
	{ "WeaponChangeSpecialPowerModule", 64, 32 },
	{ "PlayerUpgradeSpecialPower", 32, 16 },
	{ "CloudBreakSpecialPower", 32, 16 },
	{ "TaintSpecialPower", 32, 16 },
	{ "UntamedAllegianceSpecialPower", 32, 16 },
	{ "GrabPassengerSpecialPower", 32, 16 }
};

//-----------------------------------------------------------------------------
void userMemoryManagerGetDmaParms(Int *numSubPools, const PoolInitRec **pParms)
{
	*numSubPools = ARRAY_SIZE(DefaultDMA);
	*pParms = DefaultDMA;
}

//-----------------------------------------------------------------------------
void userMemoryAdjustPoolSize(const char *poolName, Int& initialAllocationCount, Int& overflowAllocationCount)
{
	if (initialAllocationCount > 0)
		return;

	if (RetailFileLayers::enabled())
	{
		for (const PoolSizeRec& pool : RetailPoolSizes)
		{
			if (strcmp(pool.name, poolName) == 0)
			{
				initialAllocationCount = pool.initial;
				overflowAllocationCount = pool.overflow;
				return;
			}
		}
	}

	for (const PoolSizeRec* p = PoolSizes; p->name != nullptr; ++p)
	{
		if (strcmp(p->name, poolName) == 0)
		{
			initialAllocationCount = p->initial;
			overflowAllocationCount = p->overflow;
			return;
		}
	}

	if (RetailFileLayers::enabled())
	{
		initialAllocationCount = 32;
		overflowAllocationCount = 16;
		fprintf(stderr, "[MemoryPool] %s has no retail size entry. Using %d initial and %d overflow blocks.\n",
			poolName, initialAllocationCount, overflowAllocationCount);
		fflush(stderr);
		return;
	}

	DEBUG_CRASH(("Initial size for pool %s not found -- you should add it to MemoryInit.cpp",poolName));
}

//-----------------------------------------------------------------------------
static Int roundUpMemBound(Int i)
{
	const int MEM_BOUND_ALIGNMENT = 4;

	if (i < MEM_BOUND_ALIGNMENT)
		return MEM_BOUND_ALIGNMENT;
	else
		return (i + (MEM_BOUND_ALIGNMENT-1)) & ~(MEM_BOUND_ALIGNMENT-1);
}

//-----------------------------------------------------------------------------
void userMemoryManagerInitPools()
{
	// note that we MUST use stdio stuff here, and not the normal game file system
	// (with bigfile support, etc), because that relies on memory pools, which
	// aren't yet initialized properly! so rely ONLY on straight stdio stuff here.
	// (not even AsciiString. thanks.)

	// since we're called prior to main, the cur dir might not be what
	// we expect. so do it the hard way.
	char buf[_MAX_PATH];
	::GetModuleFileName(nullptr, buf, sizeof(buf));
	if (char* pEnd = strrchr(buf, '\\'))
	{
		*pEnd = 0;
	}
	strlcat(buf, "\\Data\\INI\\MemoryPools.ini", ARRAY_SIZE(buf));

	FILE* fp = fopen(buf, "r");
	if (fp)
	{
		char poolName[256];
		int initial, overflow;
		while (fgets(buf, _MAX_PATH, fp))
		{
			if (buf[0] == ';')
				continue;
			if (sscanf(buf, "%s %d %d", poolName, &initial, &overflow ) == 3)
			{
				for (PoolSizeRec* p = PoolSizes; p->name != nullptr; ++p)
				{
					if (stricmp(p->name, poolName) == 0)
					{
						// currently, these must be multiples of 4. so round up.
						p->initial = roundUpMemBound(initial);
						p->overflow = roundUpMemBound(overflow);
						break;	// from for-p
					}
				}
			}
		}
		fclose(fp);
	}
}

#endif // DISABLE_GAMEMEMORY
