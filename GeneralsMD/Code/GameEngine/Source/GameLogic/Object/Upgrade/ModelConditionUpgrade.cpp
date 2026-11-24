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

// FILE: ModelConditionUpgrade.cpp /////////////////////////////////////////////////////////////////////////////
// Author: Graham Smallwood, July 2003
// Desc:	 UpgradeModule that sets a modelcondition flag
///////////////////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameLogic/Module/ModelConditionUpgrade.h"

#include <limits>

#include "Common/ModuleFactory.h"
#include "Common/ModelState.h"
#include "Common/SpecialPowerINI.h"
#include "GameLogic/Object.h"

namespace
{
void parseConditionRange(INI* ini, void*, void* store, const void*)
{
	ModelConditionFlags endpoints;
	ModelConditionFlags::parseFromINI(ini, nullptr, &endpoints, nullptr);
	if (endpoints.count() != 2)
		throw INI_INVALID_DATA;

	ModelConditionFlags range;
	Bool insideRange = FALSE;
	for (Int flag = 0; flag < MODELCONDITION_COUNT; ++flag)
	{
		const Bool endpoint = endpoints.test(flag);
		if (insideRange || endpoint)
			range.set(flag);
		if (endpoint)
			insideRange = !insideRange;
	}
	*static_cast<ModelConditionFlags*>(store) = range;
}
}
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
ModelConditionUpgradeModuleData::ModelConditionUpgradeModuleData()
{
	m_conditionFlag = MODELCONDITION_INVALID;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void ModelConditionUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
  UpgradeModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "ConditionFlag",	ModelConditionFlags::parseSingleBitFromINI,	nullptr, offsetof( ModelConditionUpgradeModuleData, m_conditionFlag ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
  p.add(dataFieldParse);
	if (ModuleFactory::registersRotwkModules())
	{
		static const FieldParse retailFields[] =
		{
			{ "AddConditionFlags", ModelConditionFlags::parseFromINI, nullptr, offsetof(ModelConditionUpgradeModuleData, m_addConditions) },
			{ "RemoveConditionFlags", ModelConditionFlags::parseFromINI, nullptr, offsetof(ModelConditionUpgradeModuleData, m_removeConditions) },
			{ "RemoveConditionFlagsInRange", parseConditionRange, nullptr, offsetof(ModelConditionUpgradeModuleData, m_removeConditions) },
			{ "AddTempConditionFlag", parseSpecialPowerModelCondition, nullptr, offsetof(ModelConditionUpgradeModuleData, m_tempCondition) },
			{ "TempConditionTime", INI::parseReal, nullptr, offsetof(ModelConditionUpgradeModuleData, m_tempConditionTime) },
			{ nullptr, nullptr, nullptr, 0 }
		};
		p.add(retailFields);
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ModelConditionUpgrade::ModelConditionUpgrade( Thing *thing, const ModuleData* moduleData ) : UpgradeModule( thing, moduleData )
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ModelConditionUpgrade::~ModelConditionUpgrade()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ModelConditionUpgrade::upgradeImplementation()
{
	const ModelConditionUpgradeModuleData *data = getModelConditionUpgradeModuleData();

	Object *me = getObject();

	if( data->m_conditionFlag != MODELCONDITION_INVALID )
		me->setModelConditionState(data->m_conditionFlag);
	if (data->m_addConditions.any() || data->m_removeConditions.any())
		me->clearAndSetModelConditionFlags(data->m_removeConditions, data->m_addConditions);
	if (data->m_tempCondition != MODELCONDITION_INVALID && data->m_tempConditionTime > 0.0f)
	{
		const Real frames = data->m_tempConditionTime * LOGICFRAMES_PER_SECONDS_REAL;
		if (frames < static_cast<Real>(std::numeric_limits<UnsignedInt>::max()))
			me->setTimedModelCondition(static_cast<ModelConditionFlagType>(data->m_tempCondition), static_cast<UnsignedInt>(frames));
	}
}

Bool ModelConditionUpgrade::resetUpgrade(const UpgradeMaskType& keyMask)
{
	if (!UpgradeModule::resetUpgrade(keyMask))
		return FALSE;

	const auto* data = getModelConditionUpgradeModuleData();
	ModelConditionFlags flags = data->m_removeConditions;
	flags.set(data->m_addConditions);
	getObject()->clearModelConditionFlags(flags);
	return TRUE;
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void ModelConditionUpgrade::crc( Xfer *xfer )
{

	// extend base class
	UpgradeModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void ModelConditionUpgrade::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpgradeModule::xfer( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void ModelConditionUpgrade::loadPostProcess()
{

	// extend base class
	UpgradeModule::loadPostProcess();

}
