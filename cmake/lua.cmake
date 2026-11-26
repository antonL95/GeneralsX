include(FetchContent)

FetchContent_Declare(retail_lua
    URL https://www.lua.org/ftp/lua-5.4.7.tar.gz
    URL_HASH SHA256=9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(retail_lua)

add_library(z_lua STATIC
    ${retail_lua_SOURCE_DIR}/src/lapi.c
    ${retail_lua_SOURCE_DIR}/src/lauxlib.c
    ${retail_lua_SOURCE_DIR}/src/lbaselib.c
    ${retail_lua_SOURCE_DIR}/src/lcode.c
    ${retail_lua_SOURCE_DIR}/src/lctype.c
    ${retail_lua_SOURCE_DIR}/src/ldebug.c
    ${retail_lua_SOURCE_DIR}/src/ldo.c
    ${retail_lua_SOURCE_DIR}/src/ldump.c
    ${retail_lua_SOURCE_DIR}/src/lfunc.c
    ${retail_lua_SOURCE_DIR}/src/lgc.c
    ${retail_lua_SOURCE_DIR}/src/llex.c
    ${retail_lua_SOURCE_DIR}/src/lmem.c
    ${retail_lua_SOURCE_DIR}/src/lobject.c
    ${retail_lua_SOURCE_DIR}/src/lopcodes.c
    ${retail_lua_SOURCE_DIR}/src/lparser.c
    ${retail_lua_SOURCE_DIR}/src/lstate.c
    ${retail_lua_SOURCE_DIR}/src/lstring.c
    ${retail_lua_SOURCE_DIR}/src/ltable.c
    ${retail_lua_SOURCE_DIR}/src/ltm.c
    ${retail_lua_SOURCE_DIR}/src/lundump.c
    ${retail_lua_SOURCE_DIR}/src/lvm.c
    ${retail_lua_SOURCE_DIR}/src/lzio.c
)
target_include_directories(z_lua PUBLIC ${retail_lua_SOURCE_DIR}/src)
target_compile_definitions(z_lua PRIVATE LUA_USER_H="${CMAKE_CURRENT_LIST_DIR}/lua_seed.h")
