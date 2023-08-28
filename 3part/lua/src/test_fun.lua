-------------------------c闭包
--[[
int functionIndex = 0;
public void InitClouse(lua_State *L){
  // create c closure
  CClosure* c = luaF_newCclosure(L, 2); 			// 2代表有2个upvalue // 2个参数?
  c->f = &IncreaseUpValue; 							// 声明闭包对应的函数
  
  // init two upvalues
  lua_pushinteger(L, 1); 
  lua_setupvalue(L, functionIndex, 1); //upvalue[0] = 1
  
  lua_pushinteger(L, 99); 
  lua_setupvalue(L, functionIndex, 2); //upvalue[1] = 99
}

public int AddUpValues(lua_State *L)
{
  int value1 = lua_getupvalue(L, functionIndex, 1);
  int value2 = lua_getupvalue(L, functionIndex, 2);
  int ret = value1 + value2;
  // output:
  // value1 + value2 = 100
  printf("value1 + value2 = %d", ret);
  lua_pushinteger(L, ret); 
  return 1; 										// 有一个返回值
}
]]--


-----------------------lua闭包
-- Lua闭包支持多层嵌套（闭包嵌套闭包） 并能更好地管理复用UpValue
function Main()
  local v1 = 1
  local inner_func = function()
    local v2 = 99
    print(v1 + v2)		-- upvalue中有v1
  end
  inner_func()
end

Main()


