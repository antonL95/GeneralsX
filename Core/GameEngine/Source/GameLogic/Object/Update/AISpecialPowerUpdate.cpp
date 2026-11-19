#include "PreRTS.h"

#include "GameLogic/Module/AISpecialPowerUpdate.h"

namespace
{
const char* const specialPowerAITypes[] =
{
	"AI_SPECIAL_POWER_BASIC_SELF_BUFF",
	"AI_SPECIAL_POWER_CAPTURE_BUILDING",
	"AI_SPECIAL_POWER_ELENDIL",
	"AI_SPECIAL_POWER_ENEMY_TYPE_KILLER",
	"AI_SPECIAL_POWER_ENEMY_TYPE_KILLER_RANGED",
	"AI_SPECIAL_POWER_ENEMY_TYPE_KILLER_STRUCTURES",
	"AI_SPECIAL_POWER_GANDALF_WIZARD_BLAST",
	"AI_SPECIAL_POWER_GIVEXP_AOE",
	"AI_SPECIAL_POWER_RANGED_AOE_ATTACK",
	"AI_SPECIAL_POWER_TOGGLE_MOUNTED",
	"AI_SPECIAL_POWER_SELFAOEHEALHEROS",
	"AI_SPECIAL_POWER_TARGETAOE_SUMMON",
	"AI_SPECIAL_POWER_LEGOLAS_ARROWWIND",
	"AI_SPECIAL_POWER_LEGOLAS_TRAINARCHERS",
	"AI_SPECIAL_POWER_GOBLINKING_BATTLEFRENZY",
	"AI_SPECIAL_POWER_GOBLINKING_CALLOFTHEDEEP",
	"AI_SPECIAL_POWER_GOBLINKING_MOUNTED",
	"AI_SPECIAL_POWER_HEAL_AOE",
	"AI_SPECIAL_POWER_TOGGLE_MELEE_AND_RANGE",
	"AI_SPECIAL_POWER_TOGGLE_SIEGE",
	"AI_SPECIAL_POWER_CHARGE",
	"AI_SPELLBOOK_ALWAYS_FIRE",
	"AI_SPELLBOOK_ASSIST_BATTLE_BUFF",
	"AI_SPELLBOOK_ASSIST_BATTLE_DEBUFF",
	"AI_SPELLBOOK_ARMY_BREAKER",
	"AI_SPELLBOOK_CAPTURE_CREEP",
	"AI_SPELLBOOK_HEAL",
	"AI_SPELLBOOK_STRUCTURE_BREAKER",
	"AI_SPELLBOOK_STRUCTURE_BREAKER_PREF_WALLS",
	"AI_SPELLBOOK_ENSHROUDINGMIST",
	"AI_SPELLBOOK_BUFFTERRAIN",
	"AI_SPELLBOOK_REBUILD",
	"AI_SPELLBOOK_BUFFECONOMYBUILDING",
	"AI_SPELLBOOK_CALLTHEHORDE",
	"AI_SPELLBOOK_SHROUD_REVEAL",
	"AI_SPELLBOOK_TREE_KILLER",
	"AI_SPELLBOOK_STRUCTURE_BASEKILL",
	"AI_SPELLBOOK_CITADEL",
	"AI_SPECIAL_POWER_STANCEBATTLE",
	"AI_SPECIAL_POWER_STANCEAGGRESSIVE",
	"AI_SPECIAL_POWER_STANCEHOLDGROUND",
	"AI_SPELLBOOK_DEBUFFECONOMYBUILDING",
	"AI_SPELLBOOK_DEBUFFPRODUCTIONBUILDING",
	"AI_SPECIAL_POWER_AOE_AND_BUFF",
	"AI_SPECIAL_POWER_SOUL_FREEZE",
	"AI_SPECIAL_POWER_DOMINATE_ENEMY",
	"AI_SPECIAL_POWER_DOMINATE_TROLL",
	"AI_SPECIAL_POWER_TAME_THE_BEAST",
	"AI_SPECIAL_POWER_BASIC_SELF_DEBUFF",
	"AI_SPECIAL_POWER_ATTACK_HEAL_AOE",
	"AI_SPECIAL_POWER_RANGED_AOE_ATTACK_UNITS",
	"AI_SPECIAL_POWER_MORGUL_BLADE",
	"AI_SPECIAL_POWER_GOBLIN_POISON",
	nullptr
};

void parseSpecialPowerAIType(INI* ini, void*, void* store, const void*)
{
	const char* token = ini->getNextToken();
	if (stricmp(token, "AI_SPECIAL_POWER_SELFAOE_HEALHEROS") == 0)
		token = "AI_SPECIAL_POWER_SELFAOEHEALHEROS";
	*static_cast<Int*>(store) = INI::scanIndexList(token, specialPowerAITypes);
}
}

void AISpecialPowerUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpdateModuleData::buildFieldParse(p);

	static const FieldParse fields[] =
	{
		{ "CommandButtonName", INI::parseAsciiString, nullptr, offsetof( AISpecialPowerUpdateModuleData, m_commandButtonName ) },
		{ "SpecialPowerAIType", parseSpecialPowerAIType, nullptr, offsetof( AISpecialPowerUpdateModuleData, m_specialPowerAIType ) },
		{ "SpecialPowerRadius", INI::parseReal, nullptr, offsetof( AISpecialPowerUpdateModuleData, m_specialPowerRadius ) },
		{ "SpecialPowerRange", INI::parseReal, nullptr, offsetof( AISpecialPowerUpdateModuleData, m_specialPowerRange ) },
		{ "RandomizeTargetLocation", INI::parseBool, nullptr, offsetof( AISpecialPowerUpdateModuleData, m_randomizeTargetLocation ) },
		{ "SpellMakesAStructure", INI::parseBool, nullptr, offsetof( AISpecialPowerUpdateModuleData, m_spellMakesAStructure ) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

AISpecialPowerUpdate::AISpecialPowerUpdate(Thing* thing, const ModuleData* data)
	: UpdateModule(thing, data)
{
}

AISpecialPowerUpdate::~AISpecialPowerUpdate() = default;

void AISpecialPowerUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
}

void AISpecialPowerUpdate::xfer(Xfer* xfer)
{
	UpdateModule::xfer(xfer);
}

void AISpecialPowerUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
