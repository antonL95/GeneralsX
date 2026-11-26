set(lua_engine_dir ${PROJECT_SOURCE_DIR}/GeneralsMD/Code/GameEngine)
add_executable(z_lua_test
    LuaStateTest.cpp
    ${lua_engine_dir}/Include/GameLogic/LuaState.h
    ${lua_engine_dir}/Source/GameLogic/ScriptEngine/LuaState.cpp
)
target_include_directories(z_lua_test PRIVATE ${lua_engine_dir}/Include)
target_link_libraries(z_lua_test PRIVATE z_lua GTest::gtest_main)
add_test(NAME z_lua_test COMMAND z_lua_test)
