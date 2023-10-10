local a = 1
local b = 10
local sum = a + b
return sum

--[[
[root@localhost lua-5.4.6]# ./luac -l 1.lua 

main <1.lua:0,0> (7 instructions at 0x15f0cd0)
0+ params, 3 slots, 1 upvalue, 3 locals, 0 constants, 0 functions
        1       [1]     VARARGPREP      0
        2       [1]     LOADI           0 1								-- R[0] := 1
        3       [2]     LOADI           1 10							-- R[1] := 10
        4       [3]     ADD             2 0 1
        5       [3]     MMBIN           0 1 6   ; __add
        6       [4]     RETURN          2 2 1   ; 1 out
        7       [4]     RETURN          3 1 1   ; 0 out

+------+ stack_last
|      |
|      |
+------+ top
|      |
|sum=11|
|------|
| b=10 |
|------|
| a=1  |
|------|
| fun  |
+------+ stack


]]--

