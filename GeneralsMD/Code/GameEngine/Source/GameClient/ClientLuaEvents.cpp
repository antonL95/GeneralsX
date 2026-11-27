#include "Precompiled/PreRTS.h"

#include "GameClient/ClientLuaEvents.h"

#include "Common/DrawModule.h"
#include "Common/File.h"
#include "Common/FileSystem.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/LuaEventTables.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/SpecialAbilityUpdate.h"
#include "GameLogic/Object.h"

#include <cstdio>
#include <lua.hpp>
#include <memory>
#include <utility>
#include <vector>

namespace
{
constexpr char scriptPath[] = "Data\\Scripts\\Scripts.lua";
constexpr char eventsPath[] = "Data\\Scripts\\ScriptEvents.xml";

std::string readScriptData(const char* path)
{
	File* file = TheFileSystem ? TheFileSystem->openFile(path, File::READ) : nullptr;
	if (!file)
		return {};
	const Int size = file->size();
	if (size <= 0)
	{
		file->close();
		return {};
	}
	const std::unique_ptr<char[]> buffer(file->readEntireAndClose());
	return buffer ? std::string(buffer.get(), size) : std::string();
}

const Player* capturingPlayer(const Object& object)
{
	if (!TheGameLogic || !object.getLogicModelConditions().test(MODELCONDITION_START_CAPTURE))
		return nullptr;

	const Object* captor = nullptr;
	for (const Object* candidate = TheGameLogic->getFirstObject(); candidate; candidate = candidate->getNextObject())
	{
		const auto* ability = candidate->findSpecialAbilityUpdate(SPECIAL_INFANTRY_CAPTURE_BUILDING);
		if (ability && ability->getCaptureTargetID() == object.getID() &&
			(!captor || candidate->getID() < captor->getID()))
			captor = candidate;
	}
	return captor ? captor->getControllingPlayer() : nullptr;
}

int pushSide(lua_State* state, const Player* player)
{
	if (player)
		lua_pushstring(state, player->getSide().str());
	else
		lua_pushnil(state);
	return 1;
}
}

struct ClientLuaEvents::Invocation
{
	const Object& object;
	const AsciiString& event;
	std::vector<std::pair<AsciiString, Bool>> visibility;
};

ClientLuaEvents::ClientLuaEvents()
{
	const std::string source = readScriptData(scriptPath);
	const std::string xml = readScriptData(eventsPath);
	if (source.empty() || xml.empty() || !m_state.load(source, scriptPath))
		return;

	std::string error;
	auto tables = LuaEventTables::parse(xml, error);
	if (!tables)
	{
		std::fprintf(stderr, "Client Lua %s: %s\n", eventsPath, error.c_str());
		std::fflush(stderr);
		return;
	}

	for (auto& [name, handlers] : tables->lists)
	{
		const auto generic = handlers.find("OnGenericEvent");
		m_handlers.emplace(name, generic == handlers.end() ? std::string() : std::move(generic->second));
	}
	m_state.bind("ObjectPlayerSide", ownerSide, this);
	m_state.bind("ObjectCapturingObjectPlayerSide", capturingSide, this);
	m_state.bind("ObjectHideSubObjectPermanently", hideSubObject, this);
}

void ClientLuaEvents::dispatch(Drawable* drawable, const AsciiString& event)
{
	const Object* object = drawable ? drawable->getObject() : nullptr;
	if (!TheGameClient || !object || object->isEffectivelyDead() || object->isDestroyed())
		return;

	const auto* ai = object->getAIUpdateInterface();
	const AsciiString& list = ai ? ai->getRetailLuaEventList() : AsciiString::TheEmptyString;
	const Bool forced = object->getTemplate()->getRotWKObjectData().forceLuaRegistration;
	if (list.isEmpty() && !forced)
		return;

	auto& events = TheGameClient->m_clientLuaEvents;
	if (!events)
		events.reset(new ClientLuaEvents);
	auto handler = events->m_handlers.find(list.str());
	if (handler == events->m_handlers.end() && forced)
		handler = events->m_handlers.find("BaseScriptFunctions");
	if (handler == events->m_handlers.end() || handler->second.empty())
		return;

	Invocation invocation{*object, event, {}};
	events->m_invocation = &invocation;
	const bool handled = events->m_state.call(handler->second, pushArguments, &invocation);
	events->m_invocation = nullptr;
	if (!handled || invocation.visibility.empty())
		return;

	for (auto module = drawable->getDrawModulesNonDirty(); module && *module; ++module)
		if (auto* draw = (*module)->getObjectDrawInterface())
			for (const auto& [name, hidden] : invocation.visibility)
				draw->setClientLuaSubObjectHidden(name, hidden);
}

ClientLuaEvents::Invocation* ClientLuaEvents::currentInvocation(lua_State* state)
{
	auto* events = static_cast<ClientLuaEvents*>(lua_touserdata(state, lua_upvalueindex(1)));
	auto* invocation = events->m_invocation;
	if (!invocation || lua_type(state, 1) != LUA_TUSERDATA || lua_rawlen(state, 1) != sizeof(ObjectID))
		return nullptr;
	const auto id = *static_cast<const ObjectID*>(lua_touserdata(state, 1));
	return id == invocation->object.getID() ? invocation : nullptr;
}

int ClientLuaEvents::pushArguments(lua_State* state, void* context)
{
	const auto& invocation = *static_cast<const Invocation*>(context);
	auto* id = static_cast<ObjectID*>(lua_newuserdatauv(state, sizeof(ObjectID), 0));
	*id = invocation.object.getID();
	lua_pushstring(state, invocation.event.str());
	return 2;
}

int ClientLuaEvents::ownerSide(lua_State* state)
{
	const auto* invocation = currentInvocation(state);
	return pushSide(state, invocation ? invocation->object.getControllingPlayer() : nullptr);
}

int ClientLuaEvents::capturingSide(lua_State* state)
{
	const auto* invocation = currentInvocation(state);
	return pushSide(state, invocation ? capturingPlayer(invocation->object) : nullptr);
}

int ClientLuaEvents::hideSubObject(lua_State* state)
{
	auto* invocation = currentInvocation(state);
	if (invocation && lua_type(state, 2) == LUA_TSTRING && lua_isboolean(state, 3))
	{
		const char* name = lua_tostring(state, 2);
		if (*name)
			invocation->visibility.emplace_back(AsciiString(name), lua_toboolean(state, 3) != 0);
	}
	return 0;
}
