/*
** $Id: lstate.h $
** Global State
** See Copyright Notice in lua.h
*/

#ifndef lstate_h
#define lstate_h

#include "lua.h"


/* Some header files included here need this definition */
typedef struct CallInfo CallInfo;


#include "lobject.h"
#include "ltm.h"
#include "lzio.h"


/*
** Some notes about garbage-collected objects: All objects in Lua must
** be kept somehow accessible until being freed, so all objects always
** belong to one (and only one) of these lists, using field 'next' of
** the 'CommonHeader' for the link:
**
** 'allgc': all objects not marked for finalization;
** 'finobj': all objects marked for finalization;
** 'tobefnz': all objects ready to be finalized;
** 'fixedgc': all objects that are not to be collected (currently
** only small strings, such as reserved words).
**
** For the generational collector, some of these lists have marks for
** generations. Each mark points to the first element in the list for
** that particular generation; that generation goes until the next mark.
**
** 'allgc' -> 'survival': new objects;
** 'survival' -> 'old': objects that survived one collection;
** 'old1' -> 'reallyold': objects that became old in last collection;
** 'reallyold' -> NULL: objects old for more than one cycle.
**
** 'finobj' -> 'finobjsur': new objects marked for finalization;
** 'finobjsur' -> 'finobjold1': survived   """";
** 'finobjold1' -> 'finobjrold': just old  """";
** 'finobjrold' -> NULL: really old       """".
**
** All lists can contain elements older than their main ages, due
** to 'luaC_checkfinalizer' and 'udata2finalize', which move
** objects between the normal lists and the "marked for finalization"
** lists. Moreover, barriers can age young objects in young lists as
** OLD0, which then become OLD1. However, a list never contains
** elements younger than their main ages.
**
** The generational collector also uses a pointer 'firstold1', which
** points to the first OLD1 object in the list. It is used to optimize
** 'markold'. (Potentially OLD1 objects can be anywhere between 'allgc'
** and 'reallyold', but often the list has no OLD1 objects or they are
** after 'old1'.) Note the difference between it and 'old1':
** 'firstold1': no OLD1 objects before this point; there can be all
**   ages after it.
** 'old1': no objects younger than OLD1 after this point.
*/

/*
** Moreover, there is another set of lists that control gray objects.
** These lists are linked by fields 'gclist'. (All objects that
** can become gray have such a field. The field is not the same
** in all objects, but it always has this name.)  Any gray object
** must belong to one of these lists, and all objects in these lists
** must be gray (with two exceptions explained below):
**
** 'gray': regular gray objects, still waiting to be visited.
** 'grayagain': objects that must be revisited at the atomic phase.
**   That includes
**   - black objects got in a write barrier;
**   - all kinds of weak tables during propagation phase;
**   - all threads.
** 'weak': tables with weak values to be cleared;
** 'ephemeron': ephemeron tables with white->white entries;
** 'allweak': tables with weak keys and/or weak values to be cleared.
**
** The exceptions to that "gray rule" are:
** - TOUCHED2 objects in generational mode stay in a gray list (because
** they must be visited again at the end of the cycle), but they are
** marked black because assignments to them must activate barriers (to
** move them back to TOUCHED1).
** - Open upvales are kept gray to avoid barriers, but they stay out
** of gray lists. (They don't even have a 'gclist' field.)
*/



/*
** About 'nCcalls':  This count has two parts: the lower 16 bits counts
** the number of recursive invocations in the C stack; the higher
** 16 bits counts the number of non-yieldable calls in the stack.
** (They are together so that we can change and save both with one
** instruction.)
*/


/* true if this thread does not have non-yieldable calls in the stack */
#define yieldable(L)		(((L)->nCcalls & 0xffff0000) == 0)

/* real number of C calls */
#define getCcalls(L)	((L)->nCcalls & 0xffff)


/* Increment the number of non-yieldable calls */
#define incnny(L)	((L)->nCcalls += 0x10000)

/* Decrement the number of non-yieldable calls */
#define decnny(L)	((L)->nCcalls -= 0x10000)

/* Non-yieldable call increment */
#define nyci	(0x10000 | 1)




struct lua_longjmp;  /* defined in ldo.c */


/*
** Atomic type (relative to signals) to better ensure that 'lua_sethook'
** is thread safe
*/
#if !defined(l_signalT)
#include <signal.h>
#define l_signalT	sig_atomic_t
#endif


/*
** Extra stack space to handle TM calls and some other extras. This
** space is not included in 'stack_last'. It is used only to avoid stack
** checks, either because the element will be promptly popped or because
** there will be a stack check soon after the push. Function frames
** never use this extra space, so it does not need to be kept clean.
*/
#define EXTRA_STACK   5


#define BASIC_STACK_SIZE        (2*LUA_MINSTACK)

#define stacksize(th)	cast_int((th)->stack_last.p - (th)->stack.p)


/* kinds of Garbage Collection */
#define KGC_INC		0	/* incremental gc */    // 增量式标记清楚法 // 默认
#define KGC_GEN		1	/* generational gc */   // 分代式


typedef struct stringtable {                // 用来缓存短字符串
  TString **hash;                           // 指向TString二维数组
  int nuse;  /* number of elements */
  int size;                                 // 容量
} stringtable;                              // 通过判断nuse与size的比例 判断hash数组是否动态扩大容量或者缩小容量


/*
** Information about a call.
** About union 'u':
** - field 'l' is used only for Lua functions;
** - field 'c' is used only for C functions.
** About union 'u2':
** - field 'funcidx' is used only by C functions while doing a
** protected call;
** - field 'nyield' is used only while a function is "doing" an
** yield (from the yield until the next resume);
** - field 'nres' is used only while closing tbc variables when
** returning from a function;
** - field 'transferinfo' is used only during call/returnhooks,
** before the function starts or after it ends.
*/
struct CallInfo {
  StkIdRel func;  /* function index in the stack */                         // 函数地址指针 指向CallInfo对应的Lua闭包在运行栈中的位置 CallInfo初始化的时候赋值运行时不变
  StkIdRel	top;  /* top for this function */                               // 函数在栈中的顶部指针 函数在执行的时候有可能会需要继续往运行栈中插入数据
                                                                            // 插入数据的时候Lua运行栈的top指针会往上(顶部)移动 而CallInfo的top指针指向
                                                                            // 插入数据后会到达的最高的Lua运行栈的位置(运行后lua_State的top最大值) CallInfo初始化的时候赋值运行时不变。
  struct CallInfo *previous, *next;  /* dynamic call link */                // 
  union {
    struct {  /* only for Lua functions */                                  // 用于存储Lua闭包运行时的信息
      const Instruction *savedpc;                                           // 记录当前虚拟机执行器执行到当前函数的哪条指令 // PC指针 指向 Proto->code 即哪条指令处
      volatile l_signalT trap;                                              // 当前调用是否需要被捕捉 可视作布尔变量 用于表示hook函数钩子功能是否开启 开启后会根据lua_State的hookmask字段决定要钩起监听的逻辑
                                                                            // 钩子相关我们在文末再进行讲解 当trap为1时即代表该次调用hook功能开启了 告诉执行器需要在当前CallInfo指令执行的过程中 跳转并执行特定的自定义钩子函数
      int nextraargs;  /* # of extra arguments in vararg functions */       // 表示额外传入的函数参数的个数
    } l;
    struct {  /* only for C functions */
      lua_KFunction k;  /* continuation in case of yields */
      ptrdiff_t old_errfunc;
      lua_KContext ctx;  /* context info. in case of yields */
    } c;
  } u;
  union {
    int funcidx;  /* called-function index */
    int nyield;  /* number of values yielded */
    int nres;  /* number of values returned */
    struct {  /* info about transferred values (for call/return hooks) */
      unsigned short ftransfer;  /* offset of first value transferred */
      unsigned short ntransfer;  /* number of values transferred */
    } transferinfo;
  } u2;
  short nresults;  /* expected number of results from this function */      // 调用该函数期待的返回值个数
  unsigned short callstatus;                                                // 当前函数的调用状态 // CIST
};


/*
** Bits in CallInfo status
*/
#define CIST_OAH	(1<<0)	/* original value of 'allowhook' */
#define CIST_C		(1<<1)	/* call is running a C function */
#define CIST_FRESH	(1<<2)	/* call is on a fresh "luaV_execute" frame */
#define CIST_HOOKED	(1<<3)	/* call is running a debug hook */
#define CIST_YPCALL	(1<<4)	/* doing a yieldable protected call */
#define CIST_TAIL	(1<<5)	/* call was tail called */
#define CIST_HOOKYIELD	(1<<6)	/* last hook called yielded */
#define CIST_FIN	(1<<7)	/* function "called" a finalizer */
#define CIST_TRAN	(1<<8)	/* 'ci' has transfer information */
#define CIST_CLSRET	(1<<9)  /* function is closing tbc variables */
/* Bits 10-12 are used for CIST_RECST (see below) */
#define CIST_RECST	10
#if defined(LUA_COMPAT_LT_LE)
#define CIST_LEQ	(1<<13)  /* using __lt for __le */
#endif


/*
** Field CIST_RECST stores the "recover status", used to keep the error
** status while closing to-be-closed variables in coroutines, so that
** Lua can correctly resume after an yield from a __close method called
** because of an error.  (Three bits are enough for error status.)
*/
#define getcistrecst(ci)     (((ci)->callstatus >> CIST_RECST) & 7)
#define setcistrecst(ci,st)  \
  check_exp(((st) & 7) == (st),   /* status must fit in three bits */  \
            ((ci)->callstatus = ((ci)->callstatus & ~(7 << CIST_RECST))  \
                                                  | ((st) << CIST_RECST)))


/* active function is a Lua function */
#define isLua(ci)	(!((ci)->callstatus & CIST_C))

/* call is running Lua code (not a hook) */
#define isLuacode(ci)	(!((ci)->callstatus & (CIST_C | CIST_HOOKED)))

/* assume that CIST_OAH has offset 0 and that 'v' is strictly 0/1 */
#define setoah(st,v)	((st) = ((st) & ~CIST_OAH) | (v))
#define getoah(st)	((st) & CIST_OAH)

/*
1）lightuserdata类型使用Value的void* p字段
2）number类型对象使用Value的lua_Integer i和lua_Number n字段
它们都不需要使用GCObject* gc字段 不需要额外申请堆空间 所以lightuserdata和number类型对象都并不是GCObject子对象
3）我们的nil对象是不需要存储任何数值的
4）然后boolean对象会把true和false作为一种子类型 以不同标识存储在类型标识tt(typetag)中
所以nil和boolean也并不需要用到gc字段 也不需要申请额外堆空间存储数据 他们也不是GCObject子对象
剩下的5种基础类型string table function userdata thread都是GCObject类的子类型 都是需要申请堆空间存储额外数据

local tvalue1 = {1,2,3}
local tvalue2 = tvalue1
t1 t2的gc指向是一样的                                                                   // 即多个Tvalue 引用同一个obj
*/

/*
** 'global state', shared by all threads of this state
*/
typedef struct global_State {                                                           // 标记阶段的根结点
  lua_Alloc frealloc;  /* function to reallocate memory */
  void *ud;         /* auxiliary data to 'frealloc' */
  l_mem totalbytes;  /* number of bytes currently allocated - GCdebt */                 // 当前系统分配的总内存大小
  l_mem GCdebt;  /* bytes allocated not yet compensated by the collector */             // 内存池
  lu_mem GCestimate;  /* an estimate of the non-garbage memory in use */
  lu_mem lastatomic;  /* see function 'genstep' in file 'lgc.c' */
  stringtable strt;  /* hash table for strings */                                       // 短字符串hash表                                           // 1需要回收
  TValue l_registry;                                                                    // 全局_G表 Lua声明在全局作用域的变量都存储在这个Table中    // 2需要回收
  TValue nilvalue;  /* a nil value */
  unsigned int seed;  /* randomized seed for hashes */
  lu_byte currentwhite;
  lu_byte gcstate;  /* state of garbage collector */
  lu_byte gckind;  /* kind of GC running */
  lu_byte gcstopem;  /* stops emergency collections */
  lu_byte genminormul;  /* control for minor generational collections */
  lu_byte genmajormul;  /* control for major generational collections */
  lu_byte gcstp;  /* control whether GC is running */
  lu_byte gcemergency;  /* true if this is an emergency collection */
  lu_byte gcpause;  /* size of pause between successive GCs */
  lu_byte gcstepmul;  /* GC "speed" */
  lu_byte gcstepsize;  /* (log2 of) GC granularity */
  GCObject *allgc;  /* list of all collectable objects */                               // lgc.c:luaC_newobj 将所有分配的对象挂到这里               // 
  GCObject **sweepgc;  /* current position of sweep in list */
  GCObject *finobj;  /* list of collectable objects with finalizers */                  // 析构器相关: 所有带有未触发的析构器对象
  GCObject *gray;  /* list of gray objects */
  GCObject *grayagain;  /* list of objects to be traversed atomically */
  GCObject *weak;  /* list of tables with weak values */
  GCObject *ephemeron;  /* list of ephemeron tables (weak keys) */
  GCObject *allweak;  /* list of all-weak tables */
  GCObject *tobefnz;  /* list of userdata to be GC */                                   // 析构器相关: 当finobj中的对象因为未标记将要被释放时 会在原子阶段把它们从g->finobj链表移出
                                                                                        // 并移入tobefnz中 并在原子阶段对tobefnz所有的元素标记 确保在GC最后的析构器执行阶段之前 不会因未标记而在清除阶段被清除
  GCObject *fixedgc;  /* list of objects not to be collected */
  /* fields for generational collector */
  GCObject *survival;  /* start of objects that survived one GC cycle */
  GCObject *old1;  /* start of old1 objects */
  GCObject *reallyold;  /* objects more than one cycle old ("really old") */
  GCObject *firstold1;  /* first OLD1 object in the list (if any) */
  GCObject *finobjsur;  /* list of survival objects with finalizers */
  GCObject *finobjold1;  /* list of old1 objects with finalizers */
  GCObject *finobjrold;  /* list of really old objects with finalizers */
  struct lua_State *twups;  /* list of threads with open upvalues */                    // 捕获UpValue的协程 链表                                   // 3需要对捕获的obj回收
  lua_CFunction panic;  /* to be called in unprotected errors */
  struct lua_State *mainthread;                                                         // 当前主线程 其中引用着当前函数运行堆栈 UpValue等信息      // 4需要回收
  TString *memerrmsg;  /* message for memory-allocation errors */                       // 常量字串 长驻内存 调luaC_fix从allgc链表移除 lstring.c:luaS_init
  TString *tmname[TM_N];  /* array with tag-method names */                             // 同上 ltm.c:luaT_init
  struct Table *mt[LUA_NUMTYPES];  /* metatables for basic types */                     // 各基础类型声明的元表和元方法                             // 5需要回收 用户自定义的元表及元表中引用的对象
  TString *strcache[STRCACHE_N][STRCACHE_M];  /* cache for strings in API */            //  通过地址判断字符串是否已经在缓存中                      // 6需要回收
  lua_WarnFunction warnf;  /* warning function */
  void *ud_warn;         /* auxiliary data to 'warnf' */
} global_State;

/*
lua运行时栈

+-----+ stack_last
|     |
|     |
+-----+ top
|     |
|     |
|-----|
| arg2|
|-----|
| arg1|
|-----|
| fun |
+-----+ stack
*/

/*
** 'per thread' state
*/
struct lua_State {                                                                      // 主线程或某个协程 的函数的运行与状态
  CommonHeader;
  lu_byte status;
  lu_byte allowhook;
  unsigned short nci;  /* number of items in 'ci' list */
  StkIdRel top;  /* first free slot in the stack */                                     // lua运行时堆栈
  global_State *l_G;                                                                    // 指向一个全局状态结构体 所以Lua中Thread可以共享到这些全局数据
  CallInfo *ci;  /* call info for current function */                                   // 运行函数信息相关
  StkIdRel stack_last;  /* end of stack (last element + 1) */                           // lua运行时堆栈
  StkIdRel stack;  /* stack base */                                                     // 记录堆栈的头和尾部指针
  UpVal *openupval;  /* list of open upvalues in this stack */                          // 所有open态的UpVal都链接在这个链表当中 前插法
  StkIdRel tbclist;  /* list of to-be-closed variables */                               // 函数运行完成后准备关闭的函数
  GCObject *gclist;                                                                     // gc回收算法链接到某个回收队列
  struct lua_State *twups;  /* list of threads with open upvalues */
  struct lua_longjmp *errorJmp;  /* current error recover point */                      // 常用于保护模式下运行某个函数 若发生错误的时候会调用跳转指令跳转到这个位置
  CallInfo base_ci;  /* CallInfo for first level (C calling Lua) */
  volatile lua_Hook hook;                                                               // 函数勾子
  ptrdiff_t errfunc;  /* current error handling function (stack index) */               // 若有设置此错误回调函数 则运行发生错误后会调用这个函数 通常用于输出异常信息
  l_uint32 nCcalls;  /* number of nested (non-yieldable | C)  calls */					// 复合字段
  int oldpc;  /* last pc traced */
  int basehookcount;
  int hookcount;
  volatile l_signalT hookmask;
};


#define G(L)	(L->l_G)

/*
** 'g->nilvalue' being a nil value flags that the state was completely
** build.
*/
#define completestate(g)	ttisnil(&g->nilvalue)


/*
** Union of all collectable objects (only for conversions)
** ISO C99, 6.5.2.3 p.5:
** "if a union contains several structures that share a common initial
** sequence [...], and if the union object currently contains one
** of these structures, it is permitted to inspect the common initial
** part of any of them anywhere that a declaration of the complete type
** of the union is visible."
*/
union GCUnion {
  GCObject gc;  /* common header */
  struct TString ts;
  struct Udata u;
  union Closure cl;
  struct Table h;
  struct Proto p;
  struct lua_State th;  /* thread */
  struct UpVal upv;
};


/*
** ISO C99, 6.7.2.1 p.14:
** "A pointer to a union object, suitably converted, points to each of
** its members [...], and vice versa."
*/
#define cast_u(o)	cast(union GCUnion *, (o))

/* macros to convert a GCObject into a specific value */
#define gco2ts(o)  \
	check_exp(novariant((o)->tt) == LUA_TSTRING, &((cast_u(o))->ts))
#define gco2u(o)  check_exp((o)->tt == LUA_VUSERDATA, &((cast_u(o))->u))
#define gco2lcl(o)  check_exp((o)->tt == LUA_VLCL, &((cast_u(o))->cl.l))
#define gco2ccl(o)  check_exp((o)->tt == LUA_VCCL, &((cast_u(o))->cl.c))
#define gco2cl(o)  \
	check_exp(novariant((o)->tt) == LUA_TFUNCTION, &((cast_u(o))->cl))
#define gco2t(o)  check_exp((o)->tt == LUA_VTABLE, &((cast_u(o))->h))
#define gco2p(o)  check_exp((o)->tt == LUA_VPROTO, &((cast_u(o))->p))
#define gco2th(o)  check_exp((o)->tt == LUA_VTHREAD, &((cast_u(o))->th))
#define gco2upv(o)	check_exp((o)->tt == LUA_VUPVAL, &((cast_u(o))->upv))


/*
** macro to convert a Lua object into a GCObject
** (The access to 'tt' tries to ensure that 'v' is actually a Lua object.)
*/
#define obj2gco(v)	check_exp((v)->tt >= LUA_TSTRING, &(cast_u(v)->gc))


/* actual number of total bytes allocated */
#define gettotalbytes(g)	cast(lu_mem, (g)->totalbytes + (g)->GCdebt)

LUAI_FUNC void luaE_setdebt (global_State *g, l_mem debt);
LUAI_FUNC void luaE_freethread (lua_State *L, lua_State *L1);
LUAI_FUNC CallInfo *luaE_extendCI (lua_State *L);
LUAI_FUNC void luaE_freeCI (lua_State *L);
LUAI_FUNC void luaE_shrinkCI (lua_State *L);
LUAI_FUNC void luaE_checkcstack (lua_State *L);
LUAI_FUNC void luaE_incCstack (lua_State *L);
LUAI_FUNC void luaE_warning (lua_State *L, const char *msg, int tocont);
LUAI_FUNC void luaE_warnerror (lua_State *L, const char *where);
LUAI_FUNC int luaE_resetthread (lua_State *L, int status);


#endif

