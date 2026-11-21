#include "PreRTS.h"

#include "GameLogic/Module/InvisibilitySpecialPower.h"

#include "Common/INI.h"
#include "Common/INIException.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"

namespace
{
enum InvisibilityType
{
	CAMOUFLAGE,
	STEALTH
};

const char* const s_invisibilityTypes[] = {"CAMOUFLAGE", "STEALTH", nullptr};

void parseInvisibilityConditions(INI* ini, void*, void* store, const void*)
{
	auto& mask = *static_cast<ModelConditionFlags*>(store);
	mask.clear();
	for (const char* name = ini->getNextToken(); name; name = ini->getNextTokenOrNull())
	{
		if (stricmp(name, "FIRING_ANY") == 0)
		{
			mask.set(MODELCONDITION_FIRING_A);
			mask.set(MODELCONDITION_FIRING_B);
			mask.set(MODELCONDITION_FIRING_C);
			mask.set(MODELCONDITION_FIRING_D);
			mask.set(MODELCONDITION_FIRING_E);
		}
		else
		{
			mask.set(INI::scanIndexList(name, ModelConditionFlags::getBitNames()));
		}
	}
}

void parseInvisibilityNugget(INI* ini, void*, void* store, const void*)
{
	using Nugget = InvisibilitySpecialPowerModuleData::Nugget;
	static const FieldParse fields[] =
	{
		{ "InvisibilityType", INI::parseIndexList, s_invisibilityTypes, offsetof(Nugget, type) },
		{ "DetectionRange", INI::parseReal, nullptr, offsetof(Nugget, detectionRange) },
		{ "ForbiddenConditions", parseInvisibilityConditions, nullptr, offsetof(Nugget, forbidden) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(store, fields);
}

void grantInvisibility(Object* target, const InvisibilitySpecialPowerModuleData& data)
{
	const Real radius = data.m_nugget.type == CAMOUFLAGE ? data.m_nugget.detectionRange : 0.0f;
	target->grantPowerInvisibility(data.m_duration, radius, data.m_nugget.forbidden);

	const ContainModuleInterface* contain = target->getContain();
	if (!target->isKindOf(KINDOF_HORDE) || !contain)
		return;
	if (const ContainedItemsList* members = contain->getContainedItemsList())
	{
		for (Object* member : *members)
			member->grantPowerInvisibility(data.m_duration, radius, data.m_nugget.forbidden);
	}
}
}

void InvisibilitySpecialPowerModuleData::buildFieldParse(MultiIniFieldParse& fields)
{
	SpecialPowerModuleData::buildFieldParse(fields);
	static const FieldParse table[] =
	{
		{ "InvisibilityNugget", parseInvisibilityNugget, nullptr, offsetof(InvisibilitySpecialPowerModuleData, m_nugget) },
		{ "BroadcastRadius", INI::parseReal, nullptr, offsetof(InvisibilitySpecialPowerModuleData, m_broadcastRadius) },
		{ "ObjectFilter", SpecialPowerObjectFilter::parse, nullptr, offsetof(InvisibilitySpecialPowerModuleData, m_filter) },
		{ "Duration", INI::parseDurationUnsignedInt, nullptr, offsetof(InvisibilitySpecialPowerModuleData, m_duration) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	fields.add(table);
}

ModuleData* InvisibilitySpecialPower::friend_newModuleData(INI* ini)
{
	auto data = std::unique_ptr<InvisibilitySpecialPowerModuleData>(MSGNEW("AllModuleData") InvisibilitySpecialPowerModuleData);
	if (ini)
	{
		ini->initFromINIMultiProc(data.get(), InvisibilitySpecialPowerModuleData::buildFieldParse);
		if (!data->m_specialPowerTemplate)
			throw INIException("InvisibilitySpecialPower requires a defined SpecialPowerTemplate");
	}
	return data.release();
}

InvisibilitySpecialPower::~InvisibilitySpecialPower() = default;

void InvisibilitySpecialPower::applyPowerEffect(const Coord3D* position)
{
	const auto* data = static_cast<const InvisibilitySpecialPowerModuleData*>(getModuleData());
	Object* caster = getObject();
	const Coord3D center = position ? *position : *caster->getPosition();
	SimpleObjectIterator* objects = ThePartitionManager->iterateObjectsInRange(&center, data->m_broadcastRadius,
		FROM_CENTER_2D, nullptr, ITER_FASTEST);
	MemoryPoolObjectHolder hold(objects);
	for (Object* target = objects->first(); target; target = objects->next())
	{
		if (!target->isEffectivelyDead() && !target->isContained() &&
			caster->getRelationship(target) == ALLIES && data->m_filter.matchesTemplate(*target->getTemplate()))
			grantInvisibility(target, *data);
	}
}
