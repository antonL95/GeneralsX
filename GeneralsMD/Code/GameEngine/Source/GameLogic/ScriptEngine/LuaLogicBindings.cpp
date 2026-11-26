#include "Precompiled/PreRTS.h"

#include "GameLogic/LuaLogicBindings.h"

#include "Common/AudioEventRTS.h"
#include "Common/GameAudio.h"
#include "Common/GameMemory.h"
#include "Common/Player.h"
#include "Common/RandomValue.h"
#include "Common/SpecialPower.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Drawable.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/LuaScriptEngine.h"
#include "GameLogic/LuaState.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/RotwkModifierModules.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Object.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/ScriptActions.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"
#include "GameLogic/Weapon.h"

#include <algorithm>
#include <limits>
#include <lua.hpp>

namespace
{
Object* getObject(lua_State* state, int index)
{
	if (lua_type(state, index) != LUA_TUSERDATA || lua_rawlen(state, index) != sizeof(ObjectID))
		return nullptr;
	const auto id = *static_cast<const ObjectID*>(lua_touserdata(state, index));
	Object* object = TheGameLogic->findObjectByID(id);
	return object && !object->isDestroyed() ? object : nullptr;
}

const char* getName(lua_State* state, int index)
{
	return lua_type(state, index) == LUA_TSTRING ? lua_tostring(state, index) : nullptr;
}

Player* getPlayer(Object* object)
{
	return object ? object->getControllingPlayer() : nullptr;
}

const UpgradeTemplate* getUpgrade(lua_State* state)
{
	const char* name = getName(state, 2);
	return name && TheUpgradeCenter ? TheUpgradeCenter->findUpgrade(name) : nullptr;
}

int getRandomNumber(lua_State* state)
{
	lua_pushnumber(state, GameLogicRandomValueReal(0.0f, 1.0f));
	return 1;
}

int hideSubObjectPermanently(lua_State* state)
{
	Object* object = getObject(state, 1);
	const char* name = getName(state, 2);
	if (object && name && lua_isboolean(state, 3))
		if (auto* draw = object->getDrawable())
		{
			draw->showSubObject(name, !lua_toboolean(state, 3));
			draw->updateSubObjects();
		}
	return 0;
}

int hideSubObject(lua_State* state)
{
	Object* object = getObject(state, 1);
	const char* name = getName(state, 2);
	if (object && name && lua_isboolean(state, 3))
		if (auto* draw = object->getDrawable())
			draw->showSubObjectForCurrentModel(name, !lua_toboolean(state, 3));
	return 0;
}

template<bool grant, bool restoring = false>
int changeUpgrade(lua_State* state)
{
	const Object* object = getObject(state, 1);
	const auto* upgrade = getUpgrade(state);
	if (object && upgrade)
	{
		const LuaUpgradeChange change{object->getID(), upgrade, grant};
		if constexpr (restoring)
		{
			auto& changes = *static_cast<std::vector<LuaUpgradeChange>*>(lua_touserdata(state, lua_upvalueindex(1)));
			changes.push_back(change);
		}
		else
			applyLuaUpgrade(change);
	}
	return 0;
}

int hasUpgrade(lua_State* state)
{
	Object* object = getObject(state, 1);
	const auto* upgrade = getUpgrade(state);
	bool owned = false;
	if (object && upgrade)
	{
		if (upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER)
		{
			const auto* player = getPlayer(object);
			owned = player && player->hasUpgradeComplete(upgrade);
		}
		else
			owned = object->hasUpgrade(upgrade);
	}
	lua_pushnumber(state, owned ? 1.0 : 0.0);
	return 1;
}

int doSpecialPower(lua_State* state)
{
	Object* object = getObject(state, 1);
	const char* name = getName(state, 2);
	if (!object || !name || object->isEffectivelyDead() || !TheSpecialPowerStore)
		return 0;
	const auto* power = TheSpecialPowerStore->findSpecialPowerTemplate(name);
	if (!power)
		return 0;
	if (const auto* module = object->getSpecialPowerModule(power))
	{
		if (module->isReady())
			object->doSpecialPower(power, COMMAND_OPTION_INTERNAL_AUTO, TRUE);
		return 0;
	}
	for (BehaviorModule** module = object->getBehaviorModules(); *module; ++module)
	{
		const auto* caster = (*module)->getSpecialPower();
		const auto* existing = caster ? caster->getSpecialPowerTemplate() : nullptr;
		if (existing && existing->getSpecialPowerType() == power->getSpecialPowerType() && caster->isReady())
		{
			object->doSpecialPower(existing, COMMAND_OPTION_INTERNAL_AUTO, TRUE);
			break;
		}
	}
	return 0;
}

int setGeometryActive(lua_State* state)
{
	Object* object = getObject(state, 1);
	const char* name = getName(state, 2);
	if (object && name && lua_isboolean(state, 3))
		object->setLuaGeometryActive(name, lua_toboolean(state, 3));
	return 0;
}

int testModelCondition(lua_State* state)
{
	Object* object = getObject(state, 1);
	const char* name = getName(state, 2);
	const Int bit = name ? ModelConditionFlags::getSingleBitFromName(name) : -1;
	lua_pushboolean(state, object && bit >= 0 && object->getLogicModelConditions().test(bit));
	return 1;
}

int playSound(lua_State* state)
{
	Object* object = getObject(state, 1);
	const char* name = getName(state, 2);
	if (object && name && TheAudio)
	{
		AudioEventRTS sound(name);
		sound.setObjectID(object->getID());
		TheAudio->addAudioEvent(&sound);
	}
	return 0;
}

int playerSide(lua_State* state)
{
	const auto* player = getPlayer(getObject(state, 1));
	if (player)
		lua_pushstring(state, player->getSide().str());
	else
		lua_pushnil(state);
	return 1;
}

enum class Recipients { Enemies, Allies, Units };

template<Recipients recipients>
int broadcast(lua_State* state)
{
	const Object* source = getObject(state, 1);
	const char* event = getName(state, 2);
	const lua_Number radius = lua_tonumber(state, 3);
	auto* engine = LuaScriptEngine::active();
	if (!source || !event || !engine || !(radius > 0 && radius <= std::numeric_limits<Real>::max()))
		return 0;
	const ObjectID sourceId = source->getID();
	std::vector<ObjectID> targets;
	{
		auto* iterator = ThePartitionManager->iterateObjectsInRange(source->getPosition(), static_cast<Real>(radius), FROM_CENTER_2D);
		MemoryPoolObjectHolder holder(iterator);
		for (Object* target = iterator->first(); target; target = iterator->next())
		{
			if (source == target || target->isEffectivelyDead() || target->isDestroyed())
				continue;
			if constexpr (recipients == Recipients::Enemies)
				if (source->getRelationship(target) != ENEMIES)
					continue;
			if constexpr (recipients == Recipients::Allies)
				if (source->getRelationship(target) != ALLIES)
					continue;
			if constexpr (recipients == Recipients::Units)
				if (!target->isMobile())
					continue;
			targets.push_back(target->getID());
		}
	}
	std::sort(targets.begin(), targets.end());
	targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
	const char* data = getName(state, 4);
	const auto payload = data ? LuaScriptEngine::Argument(std::string_view(data)) : LuaScriptEngine::Argument();
	for (const auto id : targets)
	{
		source = TheGameLogic->findObjectByID(sourceId);
		if (auto* target = TheGameLogic->findObjectByID(id); target && !target->isEffectivelyDead())
			engine->dispatchScripted(*target, event, source, payload);
	}
	return 0;
}

int createAndFireTempWeapon(lua_State* state)
{
	Object* object = getObject(state, 1);
	const char* name = getName(state, 2);
	if (object && name && TheWeaponStore && !object->isEffectivelyDead())
		if (const auto* weapon = TheWeaponStore->findWeaponTemplate(name))
			TheWeaponStore->createAndFireTempWeapon(weapon, object, object->getPosition());
	return 0;
}

int forbidPlayerCommands(lua_State* state)
{
	Object* object = getObject(state, 1);
	if (object && lua_isboolean(state, 2))
		if (auto* ai = object->getAIUpdateInterface())
			ai->setLuaForbidPlayerCommands(lua_toboolean(state, 2));
	return 0;
}

int changeAllegianceFromNonPlayablePlayer(lua_State* state)
{
	Object* object = getObject(state, 1);
	const auto* current = getPlayer(object);
	auto* owner = getPlayer(getObject(state, 2));
	if (current && owner && !current->isPlayableSide() && owner->isPlayableSide())
		object->setTeam(owner->getDefaultTeam());
	return 0;
}

int executeAction(lua_State* state)
{
	const char* name = getName(state, 1);
	if (!name || !TheScriptEngine || !TheScriptActions)
		return 0;
	for (const auto type : { ScriptAction::NAMED_KILL, ScriptAction::CAMERA_LETTERBOX_BEGIN, ScriptAction::CAMERA_LETTERBOX_END })
	{
		const auto* actionTemplate = TheScriptEngine->getActionTemplate(type);
		if (!actionTemplate || actionTemplate->m_internalName != name)
			continue;
		if (type == ScriptAction::NAMED_KILL)
		{
			if (auto* target = getObject(state, 2); target && !target->isEffectivelyDead())
				target->kill();
			return 0;
		}
		auto* action = newInstance(ScriptAction)(type);
		TheScriptActions->executeAction(action);
		deleteInstance(action);
		return 0;
	}
	return luaL_error(state, "unsupported ExecuteAction %s", name);
}

template<EmotionType type>
int enterFearState(lua_State* state)
{
	Object* object = getObject(state, 1);
	const Object* source = getObject(state, 2);
	if (object)
		EmotionTrackerUpdate::signalWeaponEmotion(object, type, source, 0);
	return 0;
}

int enterRampageState(lua_State* state)
{
	Object* object = getObject(state, 1);
	if (object)
		if (auto* ai = object->getAIUpdateInterface())
			ai->enterRetailRampage();
	return 0;
}

int setEnragedState(lua_State* state)
{
	Object* object = getObject(state, 1);
	if (object && lua_isboolean(state, 2))
		EnragedBehavior::setScriptEnraged(*object, lua_toboolean(state, 2));
	return 0;
}

int testCanSufferFear(lua_State* state)
{
	const Object* object = getObject(state, 1);
	lua_pushboolean(state, object && EmotionTrackerUpdate::canSufferFear(object));
	return 1;
}

int setChanting(lua_State* state)
{
	return luaL_error(state, "ObjectSetChanting has no corresponding AI state");
}
}

void applyLuaUpgrade(const LuaUpgradeChange& change)
{
	Object* object = TheGameLogic->findObjectByID(change.object);
	if (!object || object->isDestroyed() || (change.grant && object->isEffectivelyDead()))
		return;
	const auto* upgrade = change.upgrade;
	if (upgrade->getUpgradeType() == UPGRADE_TYPE_PLAYER)
	{
		auto* player = getPlayer(object);
		if (!player || player->hasUpgradeComplete(upgrade) == change.grant)
			return;
		if (change.grant)
			player->addUpgrade(upgrade, UPGRADE_STATUS_COMPLETE);
		else
			player->removeUpgrade(upgrade);
	}
	else if (object->hasUpgrade(upgrade) != change.grant)
	{
		if (change.grant)
			object->giveUpgrade(upgrade);
		else
			object->removeUpgrade(upgrade);
	}
}

void bindRestoringLua(LuaState& state, std::vector<LuaUpgradeChange>& changes)
{
	state.bind("ObjectGrantUpgrade", changeUpgrade<true, true>, &changes);
	state.bind("ObjectRemoveUpgrade", changeUpgrade<false, true>, &changes);
}

void bindLogicLua(LuaState& state)
{
	const luaL_Reg bindings[] = {
		{ "GetRandomNumber", getRandomNumber },
		{ "ObjectHideSubObjectPermanently", hideSubObjectPermanently },
		{ "ObjectHideSubObject", hideSubObject },
		{ "ObjectGrantUpgrade", changeUpgrade<true> },
		{ "ObjectRemoveUpgrade", changeUpgrade<false> },
		{ "ObjectHasUpgrade", hasUpgrade },
		{ "ObjectDoSpecialPower", doSpecialPower },
		{ "ObjectSetGeometryActive", setGeometryActive },
		{ "ObjectTestModelCondition", testModelCondition },
		{ "ObjectPlaySound", playSound },
		{ "ObjectPlayerSide", playerSide },
		{ "ObjectCapturingObjectPlayerSide", playerSide },
		{ "ObjectBroadcastEventToEnemies", broadcast<Recipients::Enemies> },
		{ "ObjectBroadcastEventToAllies", broadcast<Recipients::Allies> },
		{ "ObjectBroadcastEventToUnits", broadcast<Recipients::Units> },
		{ "ObjectCreateAndFireTempWeapon", createAndFireTempWeapon },
		{ "ObjectForbidPlayerCommands", forbidPlayerCommands },
		{ "ObjectChangeAllegianceFromNonPlayablePlayer", changeAllegianceFromNonPlayablePlayer },
		{ "ExecuteAction", executeAction },
		{ "ObjectEnterCowerState", enterFearState<EMOTION_FEAR> },
		{ "ObjectEnterRunAwayPanicState", enterFearState<EMOTION_TERROR> },
		{ "ObjectEnterUncontrollableCowerState", enterFearState<EMOTION_UNCONTROLLABLE_FEAR> },
		{ "ObjectEnterRampageState", enterRampageState },
		{ "ObjectSetEnragedState", setEnragedState },
		{ "ObjectSetChanting", setChanting },
		{ "ObjectTestCanSufferFear", testCanSufferFear },
	};
	for (const auto& binding : bindings)
		state.bind(binding.name, binding.func);
}
