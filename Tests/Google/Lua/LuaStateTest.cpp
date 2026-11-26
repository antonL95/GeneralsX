#include "GameLogic/LuaState.h"

#include <gtest/gtest.h>
#include <lua.hpp>

namespace
{
int observe(lua_State* state)
{
	auto* value = static_cast<int*>(lua_touserdata(state, 1));
	*value = static_cast<int>(lua_tonumber(state, 2));
	return 0;
}

int pushObservation(lua_State* state, void* data)
{
	lua_pushlightuserdata(state, data);
	return 1;
}
}

TEST(LuaDialect, RejectsPostRetailOperatorsAndKeywords)
{
	for (const char* script : {
		"return x // y", "return x & y", "return x | y", "return ~x", "return x ~ y",
		"return x << y", "return x >> y", "::again::", "goto again" })
		EXPECT_FALSE(LuaState::isRetailDialect(script)) << script;
}

TEST(LuaDialect, IgnoresOperatorsInQuotedTextAndComments)
{
	EXPECT_TRUE(LuaState::isRetailDialect(R"lua(
		-- goto // &
		--[=[ ::label::
		      >> ]=]
		local a = "quote \" //"
		local b = 'goto |'
		local c = [==[ << & [=[ ~ ]=] ]==]
		return a ~= b and gotoxy
	)lua"));
}

TEST(LuaState, ExposesOnlyAllowedBaseFunctions)
{
	LuaState state;
	state.bind("observe", observe);
	ASSERT_TRUE(state.load(R"lua(
		function verify(out)
			if os or io or package or debug or math or require or dofile or loadfile or load
				or collectgarbage or pairs or getmetatable or _G or string or table or coroutine then
				missing()
			end
			if tostring(out) ~= "userdata" then missing() end
			observe(out, tonumber(tostring(42)))
		end
	)lua", "sandbox"));
	int observed = 0;
	EXPECT_TRUE(state.call("verify", pushObservation, &observed));
	EXPECT_EQ(observed, 42);
}

TEST(LuaState, RejectsChunkErrorsAndRecoversFromHandlerErrors)
{
	LuaState state;
	EXPECT_FALSE(state.load("return 4 // 2", "dialect"));
	EXPECT_FALSE(state.load("BeginScript\n  return end", "malformed"));
	EXPECT_FALSE(state.load("missing()", "chunk"));
	ASSERT_TRUE(state.load("function bad() missing() end function good() end", "handlers"));
	EXPECT_TRUE(state.hasFunction("good"));
	EXPECT_FALSE(state.hasFunction("missing"));
	EXPECT_FALSE(state.call("bad"));
	EXPECT_FALSE(state.call("bad"));
	EXPECT_FALSE(state.call("missing"));
	EXPECT_TRUE(state.call("good"));
}
