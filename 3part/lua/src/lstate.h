/*
** $Id: lstate.h,v 2.24.1.2 2008/01/03 15:20:39 roberto Exp $
** Global State
** See Copyright Notice in lua.h
*/

#ifndef lstate_h
#define lstate_h

#include "lua.h"

#include "lobject.h"
#include "ltm.h"
#include "lzio.h"



struct lua_longjmp;  /* defined in ldo.c */


/* table of globals */
#define gt(L)	(&L->l_gt)

/* registry */
#define registry(L)	(&G(L)->l_registry)


/* extra stack space to handle TM calls and some other extras */
#define EXTRA_STACK   5


#define BASIC_CI_SIZE           8

#define BASIC_STACK_SIZE        (2*LUA_MINSTACK)



typedef struct stringtable {
  GCObject **hash;  // 指向一个TString二维数组
  lu_int32 nuse;  /* number of elements */  // 实际存储了多少个TString字符串
  int size;         // 二维数组的容量   // 判断nuse与size的比例常可用于判断hash数组是否需要动态扩大容量或者缩小容量
} stringtable;


/*
** informations about a call
*/
typedef struct CallInfo {
  StkId base;  /* base for this function */
  StkId func;  /* function index in the stack */
  StkId	top;  /* top for this function */
  const Instruction *savedpc;
  int nresults;  /* expected number of results from this function */
  int tailcalls;  /* number of tail calls lost under this entry */
} CallInfo;



#define curr_func(L)	(clvalue(L->ci->func))
#define ci_func(ci)	(clvalue((ci)->func))
#define f_isLua(ci)	(!ci_func(ci)->c.isC)
#define isLua(ci)	(ttisfunction((ci)->func) && f_isLua(ci))

/*
---------------------+    \
|                    |     |
|预充值/内存池/缓冲区|     |
|                    |     |
---------------------+     |
|                    |     + totalbytes
|                    |     |
|    allocated       |     |
|                    |     |
|                    |     |
---------------------+     /

代码实现/抽象:
    预充值一开始是一个负值 -100MB
    每当分配Value时 预充值+=sizeof(value)
    当预充值>=0时 证明缓冲区已经消费完了 此时就需要gc


*/


/*
** `global state', shared by all threads of this state
*/
/*
global_State中并不是所有对象都需要被垃圾回收处理的 
因为其中有些是长驻于内存中的数据: 
1)它们在创建的时候调用了luaC_fix移出了allgc链表
2)有些则是值类型对象
3)指针类型但指向了初始就分配好地址的C语言函数 
这些对象都是没有必要调用标记函数在标记阶段进行标记的
*/
typedef struct global_State {
  stringtable strt;  /* hash table for strings */                           // 全局TString二维数组 全局只有一个 存储了当前用到的所有短字符串 // 需要回收
  lua_Alloc frealloc;  /* function to reallocate memory */
  void *ud;         /* auxiliary data to `frealloc' */
  lu_byte currentwhite;
  lu_byte gcstate;  /* state of garbage collector */
  int sweepstrgc;  /* position of sweep in `strt' */

  GCObject *rootgc;  /* list of all collectable objects */                  // GC执行过程中需要借助的辅助数据结构字段
  GCObject **sweepgc;  /* position of sweep in `rootgc' */
  GCObject *gray;  /* list of gray objects */
  GCObject *grayagain;  /* list of objects to be traversed atomically */
  GCObject *weak;  /* list of weak tables (to be cleared) */
  GCObject *tmudata;  /* last element of list of userdata to be GC */

  Mbuffer buff;  /* temporary buffer for string concatentation */

  lu_mem GCthreshold;
  lu_mem totalbytes;  /* number of bytes currently allocated */
  lu_mem estimate;  /* an estimate of number of bytes actually in use */
  lu_mem gcdept;  /* how much GC is `behind schedule' */

  int gcpause;  /* size of pause between successive GCs */
  int gcstepmul;  /* GC `granularity' */
  lua_CFunction panic;  /* to be called in unprotected errors */
  TValue l_registry;                                                        // 全局_G表 存储全局变量 // 需要回收
  struct lua_State *mainthread;                                             // 当前运行的主线程 其中也引用着当前函数的运行堆栈 函数UpValue等信息 所以标记阶段也需要进行标记
  UpVal uvhead;  /* head of double-linked list of all open upvalues */      // 协程链表   // 需要回收
  struct Table *mt[NUM_TAGS];  /* metatables for basic types */             // 各种基础类型它们的元表(metatable)
  TString *tmname[TM_N];  /* array with tag-method names */                 // 运行时使用的常量字符串 需要长驻内存 它们在创建的时候都调用了luaC_fix函数从rootgc链表移除 所以在清除阶段不会被处理 所以它们也没有必要在标记阶段进行标记
} global_State;


/*
** `per thread' state
*/
struct lua_State {
  CommonHeader;
  lu_byte status;
  StkId top;  /* first free slot in the stack */
  StkId base;  /* base of current function */
  const Instruction *savedpc;  /* `savedpc' of current function */

  StkId stack_last;  /* last free slot in the stack */                          // 栈头和尾部指针
  StkId stack;  /* stack base */

  global_State *l_G;                                                            // 所有lua_State指向全局同一个global_State 所以Lua中Thread可以共享到这些全局数据

  CallInfo *ci;  /* call info for current function */                           // 运行函数信息相关 当前状态机运行时候的入口函数 指向当前执行函数（链表）的指针 链表中所有函数的数量 当前执行的函数的深度
  CallInfo *end_ci;  /* points after end of ci array*/
  CallInfo *base_ci;  /* array of CallInfo's */
  int stacksize;
  int size_ci;  /* size of array `base_ci' */

  unsigned short nCcalls;  /* number of nested C calls */
  unsigned short baseCcalls;  /* nested C calls when resuming coroutine */

  lu_byte hookmask;                                                             // 钩子相关 lua.h中的四个宏
  lu_byte allowhook;
  int basehookcount;
  int hookcount;
  lua_Hook hook;

  TValue l_gt;  /* table of globals */
  TValue env;  /* temporary place for environments */
  GCObject *openupval;  /* list of open upvalues in this stack */               // 闭包相关: 所有open态的UpVal链  按照各UpVal变量声明顺序 后声明的会在表头 然后根据他们在链表的深度，会依次给他们一个level值
  GCObject *gclist;                                                             // 用于GC垃圾回收算法链接到某个回收队列
  struct lua_longjmp *errorJmp;  /* current error recover point */              // 常用于保护模式下运行某个函数 若发生错误的时候会调用跳转指令跳转到这个位置
  ptrdiff_t errfunc;  /* current error handling function (stack index) */       // 若有设置此错误回调函数 则运行发生错误后会调用这个函数 通常用于输出异常信息
};


#define G(L)	(L->l_G)


/*
** Union of all collectable objects
*/
union GCObject {
  GCheader gch;
  union TString ts;
  union Udata u;
  union Closure cl;
  struct Table h;
  struct Proto p;
  struct UpVal uv;
  struct lua_State th;  /* thread */
};


/* macros to convert a GCObject into a specific value */
#define rawgco2ts(o)	check_exp((o)->gch.tt == LUA_TSTRING, &((o)->ts))
#define gco2ts(o)	(&rawgco2ts(o)->tsv)
#define rawgco2u(o)	check_exp((o)->gch.tt == LUA_TUSERDATA, &((o)->u))
#define gco2u(o)	(&rawgco2u(o)->uv)
#define gco2cl(o)	check_exp((o)->gch.tt == LUA_TFUNCTION, &((o)->cl))
#define gco2h(o)	check_exp((o)->gch.tt == LUA_TTABLE, &((o)->h))
#define gco2p(o)	check_exp((o)->gch.tt == LUA_TPROTO, &((o)->p))
#define gco2uv(o)	check_exp((o)->gch.tt == LUA_TUPVAL, &((o)->uv))
#define ngcotouv(o) \
	check_exp((o) == NULL || (o)->gch.tt == LUA_TUPVAL, &((o)->uv))
#define gco2th(o)	check_exp((o)->gch.tt == LUA_TTHREAD, &((o)->th))

/* macro to convert any Lua object into a GCObject */
#define obj2gco(v)	(cast(GCObject *, (v)))


LUAI_FUNC lua_State *luaE_newthread (lua_State *L);
LUAI_FUNC void luaE_freethread (lua_State *L, lua_State *L1);

#endif

