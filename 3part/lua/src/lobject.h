/*
** $Id: lobject.h,v 2.20.1.2 2008/08/06 13:29:48 roberto Exp $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/


#ifndef lobject_h
#define lobject_h


#include <stdarg.h>


#include "llimits.h"
#include "lua.h"


/* tags for values visible from Lua */
#define LAST_TAG	LUA_TTHREAD

#define NUM_TAGS	(LAST_TAG+1)


/*
** Extra tags for non-values
*/
#define LUA_TPROTO	(LAST_TAG+1)
#define LUA_TUPVAL	(LAST_TAG+2)
#define LUA_TDEADKEY	(LAST_TAG+3)


/*
** Union of all collectable objects
*/
typedef union GCObject GCObject;


/*
** Common Header for all collectable objects (in macro form, to be
** included in other objects)
*/
#define CommonHeader	GCObject *next; lu_byte tt; lu_byte marked  // 所有可以被GC垃圾回收的对象结构 都会在头部加这个宏 // 称为CommonHeader三剑客


/*
** Common header in struct form
*/
typedef struct GCheader {
  CommonHeader;
} GCheader;




/*
** Union of all Lua values
*/
typedef union {
  GCObject *gc;
  void *p;
  lua_Number n;
  int b;
} Value;


/*
** Tagged Values
*/

#define TValuefields	Value value; int tt

typedef struct lua_TValue {
  TValuefields;
} TValue;

//typedef struct TValue {
//  union Value {
//    struct GCObject *gc;  /* collectable objects */ // 存储复合类型(string/table...)时使用 // lstate.h中定义
//    void *p;         		/* light userdata */
//    lua_CFunction f; 		/* light C functions */
//    lua_Integer i;   		/* integer numbers */
//    lua_Number n;    		/* float numbers */
//  } value_; 
//  lu_byte tt_;		// 即typetag 即type 
//} TValue;

/*
Lua5.3:
lu_byte tt_字段  tt在源码中英文为type tag 即类型标记：
该字段占用一个字节8位 lua中充分利用了每一个位 各个位都有其不同的含义
    bit0~3: 16个数值 用于存储变量的基本类型 变量的基本类型即为本文开始时图1中类型枚举中0到8分别代表的nil到thread
    bit4~5: 4个数值 用于存放类型变体 类型变体也属于它0到3位所对应的的基本类型
    bit6: 是否可以垃圾回收 BIT_ISCOLLECTABLE

#define makevariant(t,v)  ((t) | ((v) << 4))
#define LUA_VFALSE  makevariant(LUA_TBOOLEAN, 0)    // 第5位0代表false 1代表true 即0000 0001为false 0001 0001为true
#define LUA_VTRUE  makevariant(LUA_TBOOLEAN, 1)

Lua5.3之前所有数字都是浮点数没有整数的概念 所以即使整数运算也会产生误差 
数字类型也是通过类型变体来区分整型和浮点
*/


/* Macros to test type */
#define ttisnil(o)	(ttype(o) == LUA_TNIL)
#define ttisnumber(o)	(ttype(o) == LUA_TNUMBER)
#define ttisstring(o)	(ttype(o) == LUA_TSTRING)
#define ttistable(o)	(ttype(o) == LUA_TTABLE)
#define ttisfunction(o)	(ttype(o) == LUA_TFUNCTION)
#define ttisboolean(o)	(ttype(o) == LUA_TBOOLEAN)
#define ttisuserdata(o)	(ttype(o) == LUA_TUSERDATA)
#define ttisthread(o)	(ttype(o) == LUA_TTHREAD)
#define ttislightuserdata(o)	(ttype(o) == LUA_TLIGHTUSERDATA)

/* Macros to access values */
#define ttype(o)	((o)->tt)
#define gcvalue(o)	check_exp(iscollectable(o), (o)->value.gc)
#define pvalue(o)	check_exp(ttislightuserdata(o), (o)->value.p)
#define nvalue(o)	check_exp(ttisnumber(o), (o)->value.n)
#define rawtsvalue(o)	check_exp(ttisstring(o), &(o)->value.gc->ts)
#define tsvalue(o)	(&rawtsvalue(o)->tsv)
#define rawuvalue(o)	check_exp(ttisuserdata(o), &(o)->value.gc->u)
#define uvalue(o)	(&rawuvalue(o)->uv)
#define clvalue(o)	check_exp(ttisfunction(o), &(o)->value.gc->cl)
#define hvalue(o)	check_exp(ttistable(o), &(o)->value.gc->h)
#define bvalue(o)	check_exp(ttisboolean(o), (o)->value.b)
#define thvalue(o)	check_exp(ttisthread(o), &(o)->value.gc->th)

#define l_isfalse(o)	(ttisnil(o) || (ttisboolean(o) && bvalue(o) == 0))

/*
** for internal debug only
*/
#define checkconsistency(obj) \
  lua_assert(!iscollectable(obj) || (ttype(obj) == (obj)->value.gc->gch.tt))

#define checkliveness(g,obj) \
  lua_assert(!iscollectable(obj) || \
  ((ttype(obj) == (obj)->value.gc->gch.tt) && !isdead(g, (obj)->value.gc)))


/* Macros to set values */
#define setnilvalue(obj) ((obj)->tt=LUA_TNIL)

#define setnvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.n=(x); i_o->tt=LUA_TNUMBER; }         // 老版本lua 只有一个number类型

#define setpvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.p=(x); i_o->tt=LUA_TLIGHTUSERDATA; }

#define setbvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.b=(x); i_o->tt=LUA_TBOOLEAN; }

#define setsvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TSTRING; \
    checkliveness(G(L),i_o); }                                          // 字符串赋值

#define setuvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TUSERDATA; \
    checkliveness(G(L),i_o); }

#define setthvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TTHREAD; \
    checkliveness(G(L),i_o); }                                          // gc指向lua_State

#define setclvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TFUNCTION; \
    checkliveness(G(L),i_o); }

#define sethvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TTABLE; \
    checkliveness(G(L),i_o); }                                          // gc指向真正的数据; tt赋为table

#define setptvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TPROTO; \
    checkliveness(G(L),i_o); }




#define setobj(L,obj1,obj2) \
  { const TValue *o2=(obj2); TValue *o1=(obj1); \
    o1->value = o2->value; o1->tt=o2->tt; \
    checkliveness(G(L),o1); }


/*
** different types of sets, according to destination
*/

/* from stack to (same) stack */
#define setobjs2s	setobj
/* to stack (not from same stack) */
#define setobj2s	setobj
#define setsvalue2s	setsvalue
#define sethvalue2s	sethvalue
#define setptvalue2s	setptvalue
/* from table to same table */
#define setobjt2t	setobj
/* to table */
#define setobj2t	setobj
/* to new object */
#define setobj2n	setobj
#define setsvalue2n	setsvalue

#define setttype(obj, tt) (ttype(obj) = (tt))


#define iscollectable(o)	(ttype(o) >= LUA_TSTRING)       // 字符串类型往上 都是需要动态回收的



typedef TValue *StkId;  /* index to stack elements */


/*
** String headers for string table
*/
typedef union TString {
  L_Umaxalign dummy;  /* ensures maximum alignment for strings */
  struct {
    CommonHeader;       // 有点像QT中的 QObject
    lu_byte reserved;
    unsigned int hash;  // 字符串hash
    size_t len;         // 字符串长度
  } tsv;
  // char content[1]    // Lua5.3 1数组
} TString;


#define getstr(ts)	cast(const char *, (ts) + 1)
#define svalue(o)       getstr(rawtsvalue(o))



typedef union Udata {
  L_Umaxalign dummy;  /* ensures maximum alignment for `local' udata */
  struct {
    CommonHeader;
    struct Table *metatable;
    struct Table *env;
    size_t len;
  } uv;
  // char conrtent[1]   // 存放C结构体
} Udata;

/*
场景: lua(栈)中构造了一个People结构体
// 只需要提前调用一次,把__name为"People"的元表注册到Lua
static int RegisterPeopleMetatable(lua_State *L)
{
  luaL_newmetatable(L, "People");   // lauxlib.c 创建一个__name为People的全局table
  return 1;
}

static int People(lua_State *L)
{
  struct People *pPeople = (struct People *)lua_newuserdata(L, sizeof(struct People));
  
  // 设置上面这个UserData的metatable为已经注册好的__name为"People"的元表
  //luaL_setmetatable(L, "People"); // 等价于下面两行
  luaL_getmetatable(L, "People");   // 1) 从全局找__name是People的table
  lua_setmetatable(L, -2);          // 2) 设置栈顶元素的metatable为叫指定__name的metatable // 为何是-2 ?
  
  return 1; // 新的userdata已经在栈上了
}

C语言读取并修改这个结构体
static int InitName(lua_State *L)
{
  struct People *pPeople = (struct People *)luaL_testudata(L, 1， “People”); // 第 1 步 从Lua栈顶获取一个UserData对象 
  if(pPeople != NULL) {
    pPeople->name = "MaNong"; // 第 2 步
  }
  return 1;
}
*/



/*
** Function Prototypes
*/
typedef struct Proto {
  CommonHeader;
  int sizep;  /* size of `p' */
  int sizelocvars;
  GCObject *gclist;
  lu_byte nups;  /* number of upvalues */

  lu_byte numparams;        // 函数参数的数量
  lu_byte maxstacksize;     // 函数所需要寄存器数量

  int sizeupvalues;         //  UpValue的数量与描述
  TString **upvalues;       // /* upvalue names */ // UpValue的变量名字/作用域是在本函数内部还是在外层 还有一个用于定位UpValue所在位置的指针偏移值

  int sizek;                // k代表常量 这里分别是常量数量与常量所存储在的数组 这里可以知道常量在Lua中也是像普通数据类型一样需要占用内存的 /* size of `k' */
  TValue *k;                // 常量数组 /* constants used by the function */

  struct Proto **p;         // 函数内部又定义了的其它函数 所以说Lua的函数支持嵌套定义(根源) /* functions defined inside the function */

  int sizecode;             // 该函数所调用的所有指令码数量和指令码数组 函数执行的时候就是按顺序跑这些的指令码
  Instruction *code;        // 指令码数组

  int linedefined;          // 函数的开始与结束的行
  int lastlinedefined;

  int *lineinfo;            // 存储函数内各条指令码的地址偏移量 若偏移值过大则会同时记录该指令的绝对偏移值 /* map from opcodes to source lines */
  int sizelineinfo;

  struct LocVar *locvars;   // 局部变量描述 /* information about local variables */

  TString  *source;         // 该函数的源代码的字符串表示

  lu_byte is_vararg;
} Proto;


/* masks for new-style vararg */
#define VARARG_HASARG		1
#define VARARG_ISVARARG		2
#define VARARG_NEEDSARG		4


typedef struct LocVar {
  TString *varname;
  int startpc;  /* first point where variable is active */
  int endpc;    /* first point where variable is dead */
} LocVar;



/*
** Upvalues
*/

/*
一个UpVal当它所属的那个函数返回之后（调用了return），或者Lua运行堆栈发生改变，函数已经不处于合理堆栈下标的时候，该函数所包含的UpVal即会切换到close状态。
当一个UpVal处于open状态的时候，上图结构体中的v, u.open(u.open.next, u.open.previous)字段生效。
而UpVal处于close状态的时候，结构体中的v, u.value字段生效。
*/

typedef struct UpVal {      // 有两种状态 分别是open/close态
  CommonHeader;
  TValue *v;  /* points to stack or to its own value */
  union {
    TValue value;  /* the value (when closed) */
    struct {  /* double linked list (when open) */
      struct UpVal *prev;
      struct UpVal *next;
    } l;
  } u;
} UpVal;


/*
** Closures
*/

#define ClosureHeader \
	CommonHeader; lu_byte isC; lu_byte nupvalues; GCObject *gclist; \
	struct Table *env
// nupvalues代表闭包中upvalues数组长度 // gcList代表这个闭包结构体在垃圾清除的时候 要清除包括upvalues在内的一系列可回收对象

// 函数加UpValue即为闭包 // UpValue可以理解为在当前函数外声明但函数内也可以访问到的变量 类似于全局变量但有一定作用域的一种数据

typedef struct CClosure {
  ClosureHeader;
  lua_CFunction f;
  TValue upvalue[1];		// n个参数?
} CClosure;


typedef struct LClosure {
  ClosureHeader;
  struct Proto *p;          // Lua闭包的强大之处在于Proto的设计
  UpVal *upvals[1];         // Lua闭包的upvals用的是UpVal结构
} LClosure;


typedef union Closure {
  CClosure c;
  LClosure l;
} Closure;


// C闭包
#define iscfunction(o)	(ttype(o) == LUA_TFUNCTION && clvalue(o)->c.isC)

// Lua闭包
#define isLfunction(o)	(ttype(o) == LUA_TFUNCTION && !clvalue(o)->c.isC)


/*
** Tables
*/

typedef union TKey {
  struct {
    TValuefields;
    struct Node *next;  /* for chaining */  // 用于哈希冲突的时候链接向下一个位置 (开放寻址法特有的)
  } nk;
  TValue tvk;                               // value是一个值 key其实也是一个值
} TKey;


typedef struct Node {                       // node只只只用于hash表元素抽象
  TValue i_val;
  TKey i_key;
} Node;


typedef struct Table {
  CommonHeader;                                                     // 所有可回收对象的 头部都必须定义GCObject
  lu_byte flags;  /* 1<<p means tagmethod(p) is not present */      // 元表字段查询标记
  GCObject *gclist;                                                 // 垃圾回收相关

  //////////////////////////////////////////////////////////////////// 字典相关
  lu_byte lsizenode;  /* log2 of size of `node' array */            // 存储字典容量log2后的值
  Node *node;                                                       // 指向字典首个结点的指针
  Node *lastfree;  /* any free position is before this position */  // 上一次空的结点位置

  //////////////////////////////////////////////////////////////////// 数组相关
  int sizearray;  /* size of `array' array */                       // 数组容量
  TValue *array;  /* array part */                                  // 指向数组的指针

  struct Table *metatable;                                          // 元表     // 元表相关的操作专业术语叫做元方法: tag method

} Table;



/*
** `module' operation for hashing (size is always a power of 2)
*/
#define lmod(s,size) \
	(check_exp((size&(size-1))==0, (cast(int, (s) & ((size)-1)))))


#define twoto(x)	(1<<(x))
#define sizenode(t)	(twoto((t)->lsizenode))


#define luaO_nilobject		(&luaO_nilobject_)

LUAI_DATA const TValue luaO_nilobject_;

#define ceillog2(x)	(luaO_log2((x)-1) + 1)

LUAI_FUNC int luaO_log2 (unsigned int x);
LUAI_FUNC int luaO_int2fb (unsigned int x);
LUAI_FUNC int luaO_fb2int (int x);
LUAI_FUNC int luaO_rawequalObj (const TValue *t1, const TValue *t2);
LUAI_FUNC int luaO_str2d (const char *s, lua_Number *result);
LUAI_FUNC const char *luaO_pushvfstring (lua_State *L, const char *fmt,
                                                       va_list argp);
LUAI_FUNC const char *luaO_pushfstring (lua_State *L, const char *fmt, ...);
LUAI_FUNC void luaO_chunkid (char *out, const char *source, size_t len);


#endif

