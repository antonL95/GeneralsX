#pragma once

#include "Common/ModelState.h"

#include <memory>
#include <string_view>
#include <variant>

class Object;
class Xfer;

class LuaScriptEngine
{
public:
	using Argument = std::variant<std::monostate, const Object*, std::string_view, Real>;
	enum class ModelEventState { InitialConditions, Transitions };

	virtual ~LuaScriptEngine() = default;

	static LuaScriptEngine* active();
	virtual bool load(Xfer& crc) = 0;
	virtual void reset() = 0;
	virtual void clearObjects() = 0;
	virtual void registerObject(Object& object, ModelEventState state = ModelEventState::Transitions) = 0;
	virtual void finishObjectCreation(const Object& object) = 0;
	virtual void onDestroyed(Object& object) = 0;
	virtual bool dispatch(Object& object, std::string_view event, Argument second = {}, Argument third = {}) = 0;
	virtual bool dispatchScripted(Object& object, std::string_view event, Argument second = {}, Argument third = {}) = 0;
	virtual void modelConditionsChanged(Object& object, const ModelConditionFlags& previous) = 0;
	virtual void restoreHordeConditions() = 0;
};

std::unique_ptr<LuaScriptEngine> createLuaScriptEngine();
