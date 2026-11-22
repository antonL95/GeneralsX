#include "PreRTS.h"

#include "GameLogic/Module/AISpecialPowerUpdate.h"

#include "Common/ActionManager.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/RetailFileLayers.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#if RTS_ZEROHOUR
#include "GameLogic/Module/AutoPickUpUpdate.h"
#endif
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/HordeAIUpdate.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Module/SpecialPowerModule.h"

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

#if RTS_ZEROHOUR
void scoutWithPower(Object& source, const CommandButton& button, const SpecialPowerTemplate& power, Real range)
{
	constexpr Int samplesPerAxis = 16;
	Player* player = source.getControllingPlayer();
	const Coord3D& origin = *source.getPosition();
	const Real castRange = power.getMaxCastRange();
	if (power.hasRetailFlag(SpecialPowerTemplate::RETAIL_LIMIT_DISTANCE) && castRange > 0.0f)
		range = range > 0.0f ? std::min(range, castRange) : castRange;

	Region3D search;
	TheTerrainLogic->getExtent(&search);
	if (range > 0.0f)
	{
		search.lo.x = std::max(search.lo.x, origin.x - range);
		search.lo.y = std::max(search.lo.y, origin.y - range);
		search.hi.x = std::min(search.hi.x, origin.x + range);
		search.hi.y = std::min(search.hi.y, origin.y + range);
	}
	if (search.width() <= 0.0f || search.height() <= 0.0f)
		return;

	const auto startFor = [](Player& slot) -> const Waypoint* {
		const Int index = slot.getMpStartIndex();
		if (index < 0 || index >= MAX_PLAYER_COUNT)
			return nullptr;
		AsciiString name;
		name.format("Player_%d_Start", index + 1);
		return TheTerrainLogic->getWaypointByName(name);
	};
	const Waypoint* ownStart = startFor(*player);
	const Coord3D& from = ownStart && range <= 0.0f ? *ownStart->getLocation() : origin;

	for (const auto desired : {CELLSHROUD_SHROUDED, CELLSHROUD_FOGGED})
	{
		const auto tryPosition = [&](Coord3D position) {
			if (!search.containsWithin(position.asCoord2D()) ||
				(range > 0.0f && ThePartitionManager->getDistanceSquared(&source, &position, FROM_CENTER_2D) > range * range) ||
				ThePartitionManager->getShroudStatusForPlayer(player->getPlayerIndex(), &position) != desired)
				return FALSE;
			position.z = TheTerrainLogic->getGroundHeight(position.x, position.y);
			if (!button.isValidToUseOn(&source, nullptr, &position, CMD_FROM_AI) ||
				!power.isValidRetailTarget(&source, nullptr, &position, CMD_FROM_AI))
				return FALSE;

			static const Bool trace = std::getenv("GENERALSX_SKIRMISH_AI_TRACE") != nullptr;
			if (trace)
			{
				std::fprintf(stderr, "AI_SCOUT\t%u\t%d\t%s\t%a\t%a\t%d\n", TheGameLogic->getFrame(),
					player->getPlayerIndex(), power.getName().str(), position.x, position.y, desired);
				std::fflush(stderr);
			}
			source.doSpecialPowerAtLocation(&power, &position, INVALID_ANGLE, button.getOptions());
			return TRUE;
		};

		for (Int i = 0; i < ThePlayerList->getPlayerCount(); ++i)
		{
			Player* opponent = ThePlayerList->getNthPlayer(i);
			if (!opponent || !opponent->isPlayableSide() || !opponent->isPlayerActive() ||
				player->getRelationship(opponent->getDefaultTeam()) != ENEMIES)
				continue;
			const Waypoint* start = startFor(*opponent);
			if (!start)
				continue;
			const Coord3D direction = *start->getLocation() - from;
			for (Int step = samplesPerAxis; step > 0; --step)
			{
				const Real fraction = INT_TO_REAL(step) / samplesPerAxis;
				if (tryPosition({from.x + direction.x * fraction, from.y + direction.y * fraction, 0.0f}))
					return;
			}
		}

		for (Int y = 0; y < samplesPerAxis; ++y)
			for (Int x = 0; x < samplesPerAxis; ++x)
				if (tryPosition({search.lo.x + search.width() * (INT_TO_REAL(x) + 0.5f) / samplesPerAxis,
					search.lo.y + search.height() * (INT_TO_REAL(y) + 0.5f) / samplesPerAxis, 0.0f}))
					return;
	}
}
#endif
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

UpdateSleepTime AISpecialPowerUpdate::update()
{
#if RTS_ZEROHOUR
	if (!RetailFileLayers::enabled())
		return UPDATE_SLEEP_FOREVER;
	Object* source = getObject();
	const UpdateSleepTime checkAgain = UPDATE_SLEEP(LOGICFRAMES_PER_SECOND * 2);
	Player* player = source->getControllingPlayer();
	if (!player || !player->isSkirmishAIPlayer() || !player->isPlayerActive() || !TheRetailCommandCatalog ||
		source->isDestroyed() || source->isDisabled() ||
		source->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION) || source->testStatus(OBJECT_STATUS_IS_USING_ABILITY))
		return checkAgain;
	const Bool book = source->isKindOf(KINDOF_SPELL_BOOK) && source == player->getRetailSpellBook();
	if (source->isEffectivelyDead() && !book)
		return checkAgain;

	const auto* data = getAISpecialPowerUpdateModuleData();
	const CommandButton* button = TheRetailCommandCatalog->findCommandButton(data->m_commandButtonName);
	if (button && button->getCommandType() == GUI_COMMAND_WAKE_AUTO_PICKUP)
	{
		if (data->m_specialPowerAIType < 0 ||
			std::string_view(specialPowerAITypes[data->m_specialPowerAIType]) != "AI_SPECIAL_POWER_BASIC_SELF_BUFF" ||
			!AutoPickUpUpdate::canScanFromButton(source, button))
			return checkAgain;
		const CommandSet* commands = TheRetailCommandCatalog->findCommandSet(source->getCommandSetString());
		for (Int slot = 0; commands && slot < CommandButtonRules::RETAIL_COMMANDS_PER_SET; ++slot)
			if (commands->getCommandButton(slot) == button)
			{
				source->doCommandButton(button, CMD_FROM_AI);
				break;
			}
		return checkAgain;
	}
	const SpecialPowerTemplate* power = button ? button->getSpecialPowerTemplate() : nullptr;
	if (!power || data->m_specialPowerAIType < 0 ||
		TheRetailCommandCatalog->findPowerButton(source, power) != button)
		return checkAgain;
	const auto* module = source->getSpecialPowerModule(power);
	if (!module || !module->isReady() || !module->isRetailCastAllowed(button->getOptions()) ||
		!TheSpecialPowerStore->canUseSpecialPower(source, power))
		return checkAgain;

	const std::string_view policy(specialPowerAITypes[data->m_specialPowerAIType]);
	const Bool always = policy == "AI_SPELLBOOK_ALWAYS_FIRE";
	const Bool allyBattle = policy == "AI_SPELLBOOK_ASSIST_BATTLE_BUFF" ||
		policy == "AI_SPELLBOOK_BUFFTERRAIN" || policy == "AI_SPECIAL_POWER_GIVEXP_AOE";
	const Bool repair = policy == "AI_SPELLBOOK_REBUILD";
	const Bool heal = policy == "AI_SPELLBOOK_HEAL" || policy == "AI_SPECIAL_POWER_HEAL_AOE";
	const Bool allyEconomy = policy == "AI_SPELLBOOK_BUFFECONOMYBUILDING";
	const Bool enemyStructure = policy == "AI_SPELLBOOK_STRUCTURE_BREAKER" ||
		policy == "AI_SPELLBOOK_STRUCTURE_BREAKER_PREF_WALLS" || policy == "AI_SPELLBOOK_STRUCTURE_BASEKILL" ||
		policy == "AI_SPELLBOOK_DEBUFFECONOMYBUILDING" || policy == "AI_SPELLBOOK_DEBUFFPRODUCTIONBUILDING" ||
		policy == "AI_SPECIAL_POWER_ENEMY_TYPE_KILLER_STRUCTURES";
	const Bool enemyArmy = policy == "AI_SPELLBOOK_ASSIST_BATTLE_DEBUFF" || policy == "AI_SPELLBOOK_ARMY_BREAKER" ||
		policy == "AI_SPECIAL_POWER_RANGED_AOE_ATTACK" || policy == "AI_SPECIAL_POWER_RANGED_AOE_ATTACK_UNITS" ||
		policy == "AI_SPECIAL_POWER_ENEMY_TYPE_KILLER" || policy == "AI_SPECIAL_POWER_ENEMY_TYPE_KILLER_RANGED";
	const Bool reveal = policy == "AI_SPELLBOOK_SHROUD_REVEAL";
	const Bool selfBattle = policy == "AI_SPECIAL_POWER_BASIC_SELF_BUFF" || policy == "AI_SPECIAL_POWER_BASIC_SELF_DEBUFF";
	if (!(always || allyBattle || repair || heal || allyEconomy || enemyStructure || enemyArmy || reveal || selfBattle))
		return checkAgain;

	const UnsignedInt options = button->getOptions();
	const Bool objectTarget = (options & COMMAND_OPTION_NEED_OBJECT_TARGET) != 0;
	const Bool positionTarget = (options & NEED_TARGET_POS) != 0;
	if (!objectTarget && !positionTarget && (always ||
		(selfBattle && source->getAI() && source->getAI()->isAttacking())))
	{
		if (button->isValidToUseOn(source, nullptr, nullptr, CMD_FROM_AI))
			source->doSpecialPower(power, options);
		return checkAgain;
	}

	const Real range = data->m_specialPowerRange > 0.0f ? data->m_specialPowerRange : data->m_specialPowerRadius;
	if (reveal && positionTarget && !objectTarget)
	{
		scoutWithPower(*source, *button, *power, book ? 0.0f : range);
		return checkAgain;
	}

	Object* best = nullptr;
	ObjectShroudStatus bestShroud = OBJECTSHROUD_INVALID;
	Real bestScore = 0.0f;
	for (Object* target = TheGameLogic->getFirstObject(); target; target = target->getNextObject())
	{
		if (target->isEffectivelyDead() || target->isKindOf(KINDOF_INERT) ||
			(target->getContainedBy() && !(heal && HordeContain::get(target->getContainedBy()))))
			continue;
		const Relationship relationship = source->getRelationship(target);
		const auto shroud = target->getShroudedStatus(player->getPlayerIndex());
		if (shroud != OBJECTSHROUD_CLEAR && shroud != OBJECTSHROUD_PARTIAL_CLEAR)
			continue;
		if (relationship == ENEMIES && target->testStatus(OBJECT_STATUS_STEALTHED) && !target->testStatus(OBJECT_STATUS_DETECTED))
			continue;
		const Bool building = target->isKindOf(KINDOF_STRUCTURE);
		AIUpdateInterface* ai = target->getAI();
		Real score = 0.0f;
		if (relationship == ALLIES)
		{
			if ((heal && !building) || (repair && building))
			{
				const auto* body = target->getBodyModule();
				if (body->getMaxHealth() > 0.0f)
					score = 1.0f - body->getHealth() / body->getMaxHealth() - 0.25f;
			}
			else if (allyBattle && !building && ai)
			{
				const auto* horde = static_cast<const HordeAIUpdate*>(target->findUpdateModule(NAMEKEY("HordeAIUpdate")));
				if (ai->isAttacking() || (horde && horde->isEngaging()))
					score = 1.0f + target->getTemplate()->getThreatValue();
			}
			else if (allyEconomy && target->isKindOf(KINDOF_ECONOMY_STRUCTURE))
				score = 1.0f;
		}
		else if (relationship == ENEMIES &&
			((building && (enemyStructure || reveal)) || (!building && enemyArmy)))
		{
			score = 1.0f + target->getTemplate()->getThreatValue();
		}
		if (score <= 0.0f || (!book && range > 0.0f && target != source &&
			ThePartitionManager->getDistanceSquared(source, target, FROM_CENTER_2D) > range * range))
			continue;
		if (best && (score < bestScore || (score == bestScore && target->getID() > best->getID())))
			continue;
		const Coord3D* location = positionTarget ? target->getPosition() : nullptr;
		if (!button->isValidToUseOn(source, objectTarget ? target : nullptr, location, CMD_FROM_AI) ||
			!power->isValidRetailTarget(source, objectTarget ? target : nullptr, location, CMD_FROM_AI))
			continue;
		if (data->m_spellMakesAStructure && positionTarget && target->isKindOf(KINDOF_IMMOBILE))
			continue;
		best = target;
		bestShroud = shroud;
		bestScore = score;
	}
	if (best)
	{
		static const Bool trace = std::getenv("GENERALSX_SKIRMISH_AI_TRACE") != nullptr;
		if (trace && (objectTarget || positionTarget))
		{
			std::fprintf(stderr, "AI_TARGET\t%u\t%d\t%s\t%d\t%d\t%d\n", TheGameLogic->getFrame(),
				player->getPlayerIndex(), power->getName().str(), source->getID(), best->getID(), bestShroud);
			std::fflush(stderr);
		}
		if (objectTarget)
			source->doSpecialPowerAtObject(power, best, options);
		else if (positionTarget)
			source->doSpecialPowerAtLocation(power, best->getPosition(), INVALID_ANGLE, options);
		else
			source->doSpecialPower(power, options);
	}
	return checkAgain;
#else
	return UPDATE_SLEEP_FOREVER;
#endif
}

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
