--[[
local t = {}
local metatable_t = {
  __newindex = {
    a = "xxxxxxxxxxxx",
    b = "xxxxxxxxxxxx"
  }
}

setmetatable(t, metatable_t)

t[1] = 2

print("t[2]:\t" .. metatable_t.a)
]]--

-------------------------------------

local clearValue = {"123"}
local t = {
  ["value_key"] = clearValue
}

-- set value to nil
clearValue = nil    --clearValue被清除 但没有清除其在t中的引用

--  手动执行gc回收操作
collectgarbage()

for k, v in pairs(t["value_key"]) do
  print(k, v)
end

-------------------------------------

local clearValue1 = {"123"}
local t1 = {
  ["value_key"] = clearValue1
}
-- 声明值为弱引用
setmetatable(t1, {__mode = "v"})

-- set value to nil
clearValue1 = nil

--  手动执行gc回收操作
collectgarbage()

print(t1["value_key"])

-------------------------------------
local tt1 = {1, 4}
local tt2 = {1, 8}
local metatable = {
    __add = function(t1, t2)
        local ret = {}
        for k, v in pairs(t1) do
            ret[k] = t1[k] + t2[k]
        end
        return ret
    end
}

setmetatable(tt1, metatable)
setmetatable(tt2, metatable)

local t3 = tt1 + tt2
for k, v in pairs(t3) do
    print(k, v)
end


