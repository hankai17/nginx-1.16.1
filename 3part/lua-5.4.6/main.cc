extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

int main()
{
    // 初始化Lua解释器
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    // 加载并执行Lua脚本文件
    luaL_dofile(L, "1.lua");

    // 关闭Lua解释器
    lua_close(L);

    return 0;
}
