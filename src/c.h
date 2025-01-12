/* $Id$ */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>


// `NEW`指向一个尚未初始化的空间的指针
#define NEW(p,a) ((p) = allocate(sizeof *(p), (a)))
#define NEW0(p,a) memset(NEW((p),(a)), 0, sizeof *(p))
#define isaddrop(op) (specific(op)==ADDRG+P || specific(op)==ADDRL+P \
	|| specific(op)==ADDRF+P)

#define	MAXLINE  512
#define	BUFSIZE 4096

#define istypename(t,tsym) (kind[t] == CHAR \
	|| t == ID && tsym && tsym->sclass == TYPEDEF)
#define sizeop(n) ((n)<<10)
#define generic(op)  ((op)&0x3F0)
#define specific(op) ((op)&0x3FF)
#define opindex(op) (((op)>>4)&0x3F)
#define opkind(op)  ((op)&~0x3F0)
#define opsize(op)  ((op)>>10)
#define optype(op)  ((op)&0xF)

#ifdef __LCC__
#ifndef __STDC__
#define __STDC__
#endif
#endif

#define NELEMS(a) ((int)(sizeof (a)/sizeof ((a)[0])))
#undef  roundup
#define roundup(x,n) (((x)+((n)-1))&(~((n)-1)))
#define mkop(op,ty) (specific((op) + ttob(ty)))

#define extend(x,ty) ((x)&(1<<(8*(ty)->size-1)) ? (x)|((~0UL)<<(8*(ty)->size-1)) : (x)&ones(8*(ty)->size))
#define ones(n) ((n)>=8*sizeof (unsigned long) ? ~0UL : ~((~0UL)<<(n)))


// 类型断言
#define isqual(t)     ((t)->op >= CONST)
#define unqual(t)     (isqual(t) ? (t)->type : (t))

#define isvolatile(t) ((t)->op == VOLATILE \
                    || (t)->op == CONST+VOLATILE)
#define isconst(t)    ((t)->op == CONST \
                    || (t)->op == CONST+VOLATILE)
#define isarray(t)    (unqual(t)->op == ARRAY)
#define isstruct(t)   (unqual(t)->op == STRUCT \
                    || unqual(t)->op == UNION)
#define isunion(t)    (unqual(t)->op == UNION)
#define isfunc(t)     (unqual(t)->op == FUNCTION)
#define isptr(t)      (unqual(t)->op == POINTER)
#define ischar(t)     ((t)->size == 1 && isint(t))
#define isint(t)      (unqual(t)->op == INT \
                    || unqual(t)->op == UNSIGNED)
#define isfloat(t)    (unqual(t)->op == FLOAT)
#define isarith(t)    (unqual(t)->op <= UNSIGNED)
#define isunsigned(t) (unqual(t)->op == UNSIGNED)
#define isscalar(t)   (unqual(t)->op <= POINTER \
                    || unqual(t)->op == ENUM)
#define isenum(t)     (unqual(t)->op == ENUM)

#define fieldsize(p)  (p)->bitsize      // `bitsize`存放位域以位为单位的长度
#define fieldright(p) ((p)->lsb - 1)    // 返回位域最右位的位号，可用于对位域进行移位
// 返回位域最左位的位号，可用于符号位域的符号扩展操作
#define fieldleft(p)  (8*(p)->type->size - \
                        fieldsize(p) - fieldright(p))
// `fieldmask`是一个掩码，由`bitsize`个 1 组成，用于抽取位域时清除无用的位
#define fieldmask(p)  (~(fieldsize(p) < 8 * unsignedtype->size ? ~0u << fieldsize(p) : 0u))

typedef struct node *Node;

typedef struct list *List;

typedef struct code *Code;

typedef struct swtch *Swtch;

typedef struct symbol *Symbol;


// `src`域: 指明了该符号在源代码中定义的位置
typedef struct coord {
	char *file;
	unsigned x, y;
} Coordinate;


typedef struct table *Table;


typedef union value {
	long i;
	unsigned long u;
	long double d;
	void *p;
	void (*g)(void);
} Value;


typedef struct tree *Tree;

typedef struct type *Type;

typedef struct field *Field;

typedef struct {
	unsigned printed:1;
	unsigned marked;
	unsigned short typeno;
	void *xt;
} Xtype;


#include "config.h"


// 类型度量（type metric）指定基本类型的大小和对齐字节数
typedef struct metrics {
    // `outofline`标记控制相关类型的常量的放置
    // 若`outofline`为 1，则该类型的常量不能出现在 dag 中，而是存放在一个匿名的静态变量中
	unsigned char size, align, outofline;
} Metrics;


typedef struct interface {
    // 每个基本类型都有相关的类型度量
	Metrics charmetric;
	Metrics shortmetric;
	Metrics intmetric;
	Metrics longmetric;
	Metrics longlongmetric;
	Metrics floatmetric;
	Metrics doublemetric;
	Metrics longdoublemetric;
	Metrics ptrmetric;
	Metrics structmetric;

	unsigned little_endian:1;
	unsigned mulops_calls:1;
	unsigned wants_callb:1;
	unsigned wants_argb:1;
	unsigned left_to_right:1;
	unsigned wants_dag:1;
	unsigned unsigned_char:1;

void (*address)(Symbol p, Symbol q, long n);
void (*blockbeg)(Env *);
void (*blockend)(Env *);
void (*defaddress)(Symbol);
void (*defconst)  (int suffix, int size, Value v);
void (*defstring)(int n, char *s);
void (*defsymbol)(Symbol);
void (*emit)    (Node);
void (*export)(Symbol);
void (*function)(Symbol, Symbol[], Symbol[], int);
Node (*gen)     (Node);
void (*global)(Symbol);
void (*import)(Symbol);
void (*local)(Symbol);
void (*progbeg)(int argc, char *argv[]);
void (*progend)(void);
void (*segment)(int);
void (*space)(int);
void (*stabblock)(int, int, Symbol*);
void (*stabend)  (Coordinate *, Symbol, Coordinate **, Symbol *, Symbol *);
void (*stabfend) (Symbol, int);
void (*stabinit) (char *, int, char *[]);
void (*stabline) (Coordinate *);
void (*stabsym)  (Symbol);
void (*stabtype) (Symbol);

	Xinterface x;   // `interface`的扩展
} Interface;


typedef struct binding {
	char *name;
	Interface *ir;
} Binding;

extern Binding bindings[];
extern Interface *IR;

typedef struct {
	List blockentry;
	List blockexit;
	List entry;
	List exit;
	List returns;
	List points;
	List calls;
	List end;
} Events;

enum {
#define xx(a,b,c,d,e,f,g) a=b,
#define yy(a,b,c,d,e,f,g)
#include "token.h"
	LAST
};


// dag 节点
struct node {
	short op;       //存放 dag 操作符
	short count;    //记录节点的值被使用或被其它节点引用的次数
 	Symbol syms[3]; // 一些 dag 操作也使用一个或两个符号表指针作为操作数，这些操作数存放在 syms 中
	Node kids[2];   // 指向操作数节点
	Node link;      //指向森林中下一个 dag 的根
	Xnode x;        //后端对节点的扩展
};


enum {
	F=FLOAT,
	I=INT,
	U=UNSIGNED,
	P=POINTER,
	V=VOID,
	B=STRUCT
};


#define gop(name,value) name=value<<4,
#define op(name,type,sizes)

enum {
#include "ops.h"
	LASTOP
};


#undef gop
#undef op

enum { CODE=1, BSS, DATA, LIT };
enum { PERM=0, FUNC, STMT };
struct list {
	void *x;
	List link;
};


struct code {
	enum { Blockbeg, Blockend, Local, Address, Defpoint,
	       Label,    Start,    Gen,   Jump,    Switch
	} kind;
	Code prev, next;
	union {
		struct {
			int level;
			Symbol *locals;
			Table identifiers, types;
			Env x;
		} block;
		Code begin;
		Symbol var;

		struct {
			Symbol sym;
			Symbol base;
			long offset;
		} addr;
		struct {
			Coordinate src;
			int point;
		} point; 
		Node forest;
		struct {
			Symbol sym;
			Symbol table;
			Symbol deflab;
			int size;
			long *values;
			Symbol *labels;
		} swtch;

	} u;
};


struct swtch {
	Symbol sym;
	int lab;
	Symbol deflab;
	int ncases;
	int size;
	long *values;
	Symbol *labels;
};


struct symbol {
	char *name;
	int scope;
	Coordinate src;
	Symbol up;
	List uses;
	int sclass;
	unsigned structarg:1;

	unsigned addressed:1;
	unsigned computed:1;
	unsigned temporary:1;
	unsigned generated:1;
	unsigned defined:1;
	Type type;
	float ref;
 

    // 枚举类型和结构与联合类型相似，只是它没有域，并且其`type`域给出了相关联的整数类型
	union {
		struct {
			int label;
			Symbol equatedto;
        } l;

        struct {
            // 若结构或联合类型的任意域带有`const`或`volatile`限定符，那么`cfields`和`vfields`都为 1
            unsigned cfields:1;
			unsigned vfields:1;
			Table ftab;		/* omit */
            // `flist`指向用`link`域连接起来的`field`结构
			Field flist;
        } s;

        int value;
        // 对于枚举类型包含的枚举常量，`idlist`指向以空结尾的`Symbol`数组
		Symbol *idlist;

		struct {
			Value min, max;
		} limits;

		struct {
			Value v;
			Symbol loc;
		} c;

		struct {
			Coordinate pt;
			int label;
			int ncalls;
			Symbol *callee;
		} f;

		int seg;
		Symbol alias;

		struct {
			Node cse;
			int replace;
			Symbol next;
		} t;
	} u;
	Xsymbol x;
};


// `scope`域
// 第`k`曾中声明的局部变量，其`scope`域等于`LOCAL+k`
enum { CONSTANTS = 1, LABELS, GLOBAL, PARAM, LOCAL };


struct tree {
	int op;
	Type type;
	Tree kids[2];
	Node node;
	union {
		Value v;
		Symbol sym;

		Field field;
	} u;
};


enum
{
    AND = 38 << 4,
    NOT = 39 << 4,
    OR = 40 << 4,
    COND = 41 << 4,
    RIGHT = 42 << 4,
    FIELD = 43 << 4
};


struct type {
	int op;
	Type type;
	int align;
	int size;
    // 该`union`表示函数各个参数
	union {
		Symbol sym;
		struct {
            // `oldstyle`标记两种函数类型
            // - 1 表示省略参数的类型
            // - 0 表示包含参数的类型
			unsigned oldstyle:1;
            // 指向以空指针`NULL`结尾的`Type`数组
			Type *proto;    // `f.proto[i]`是第`i+1`个参数的类型
		} f;
	} u;
	Xtype x;
};


struct field {
    // 域的名字
	char *name;
    // 域的类型
	Type type;
    // `offset`是该域在结构实例中以字节为单位的偏移量
	int offset;
	short bitsize;
	short lsb;
	Field link;
};


extern int assignargs;
extern int prunetemps;
extern int nodecount;
extern Symbol cfunc;
extern Symbol retv;
extern Tree (*optree[])(int, Tree, Tree);

extern char kind[];
extern int errcnt;
extern int errlimit;
extern int wflag;
extern Events events;
extern float refinc;

extern unsigned char *cp;
extern unsigned char *limit;
extern char *firstfile;
extern char *file;
extern char *line;
extern int lineno;
extern int t;
extern char *token;
extern Symbol tsym;
extern Coordinate src;
extern int Aflag;
extern int Pflag;
extern Symbol YYnull;
extern Symbol YYcheck;
extern int glevel;
extern int xref;

extern int ncalled;
extern int npoints;

extern int needconst;
extern int explicitCast;
extern struct code codehead;
extern Code codelist;
extern Table stmtlabs;
extern float density;
extern Table constants;
extern Table externals;
extern Table globals;
extern Table identifiers;
extern Table labels;
extern Table types;
extern int level;

extern List loci, symbols;

extern List symbols;

extern int where;
extern Type chartype;
extern Type doubletype;
extern Type floattype;
extern Type inttype;
extern Type longdouble;
extern Type longtype;
extern Type longlong;
extern Type shorttype;
extern Type signedchar;
extern Type unsignedchar;
extern Type unsignedlonglong;
extern Type unsignedlong;
extern Type unsignedshort;
extern Type unsignedtype;
extern Type charptype;
extern Type funcptype;
extern Type voidptype;
extern Type voidtype;
extern Type unsignedptr;
extern Type signedptr;
extern Type widechar;
extern void  *allocate(unsigned long n, unsigned a);
extern void deallocate(unsigned a);
extern void *newarray(unsigned long m, unsigned long n, unsigned a);
extern void walk(Tree e, int tlab, int flab);
extern Node listnodes(Tree e, int tlab, int flab);
extern Node newnode(int op, Node left, Node right, Symbol p);
extern Tree cvtconst(Tree);
extern void printdag(Node, int);
extern void compound(int, Swtch, int);
extern void defglobal(Symbol, int);
extern void finalize(void);
extern void program(void);

extern Tree vcall(Symbol func, Type ty, ...);
extern Tree addrof(Tree);
extern Tree asgn(Symbol, Tree);
extern Tree asgntree(int, Tree, Tree);
extern Type assign(Type, Tree);
extern Tree bittree(int, Tree, Tree);
extern Tree call(Tree, Type, Coordinate);
extern Tree calltree(Tree, Type, Tree, Symbol);
extern Tree condtree(Tree, Tree, Tree);
extern Tree cnsttree(Type, ...);
extern Tree consttree(int, Type);
extern Tree eqtree(int, Tree, Tree);
extern int iscallb(Tree);
extern Tree shtree(int, Tree, Tree);
extern void typeerror(int, Tree, Tree);

extern void test(int tok, char set[]);
extern void expect(int tok);
extern void skipto(int tok, char set[]);
extern void error(const char *, ...);
extern int fatal(const char *, const char *, int);
extern void warning(const char *, ...);

typedef void (*Apply)(void *, void *, void *);
extern void attach(Apply, void *, List *);
extern void apply(List event, void *arg1, void *arg2);
extern Tree retype(Tree p, Type ty);
extern Tree rightkid(Tree p);
extern int hascall(Tree p);
extern Type binary(Type, Type);
extern Tree cast(Tree, Type);
extern Tree cond(Tree);
extern Tree expr0(int);
extern Tree expr(int);
extern Tree expr1(int);
extern Tree field(Tree, const char *);
extern char *funcname(Tree);
extern Tree idtree(Symbol);
extern Tree incr(int, Tree, Tree);
extern Tree lvalue(Tree);
extern Tree nullcall(Type, Symbol, Tree, Tree);
extern Tree pointer(Tree);
extern Tree rvalue(Tree);
extern Tree value(Tree);

extern void defpointer(Symbol);
extern Type initializer(Type, int);
extern void swtoseg(int);

extern void input_init(int, char *[]);
extern void fillbuf(void);
extern void nextline(void);

extern int getchr(void);
extern int gettok(void);

extern void emitcode(void);
extern void gencode (Symbol[], Symbol[]);
extern void fprint(FILE *f, const char *fmt, ...);
extern char *stringf(const char *, ...);
extern void check(Node);
extern void print(const char *, ...);

extern List append(void *x, List list);
extern int  length(List list);
extern void *ltov (List *list, unsigned a);
extern void init(int, char *[]);

extern Type typename(void);
extern void checklab(Symbol p, void *cl);
extern Type enumdcl(void);
extern void main_init(int, char *[]);
extern int main(int, char *[]);

extern void vfprint(FILE *, char *, const char *, va_list);

void profInit(char *);
extern int process(char *);
extern int findfunc(char *, char *);
extern int findcount(char *, int, int);

extern Tree constexpr(int);
extern int intexpr(int, int);
extern Tree simplify(int, Type, Tree, Tree);
extern int ispow2(unsigned long u);

extern int reachable(int);

extern void addlocal(Symbol);
extern void branch(int);
extern Code code(int);
extern void definelab(int);
extern void definept(Coordinate *);
extern void equatelab(Symbol, Symbol);
extern Node jump(int);
extern void retcode(Tree);
extern void statement(int, Swtch, int);
extern void swcode(Swtch, int *, int, int);
extern void swgen(Swtch);

extern char * string(const char *str);
extern char *stringn(const char *str, int len);
extern char *stringd(long n);
extern Symbol relocate(const char *name, Table src, Table dst);
extern void use(Symbol p, Coordinate src);
extern void locus(Table tp, Coordinate *cp);
extern Symbol allsymbols(Table);

extern Symbol constant(Type, Value);
extern void enterscope(void);
extern void exitscope(void);
extern Symbol findlabel(int);
extern Symbol findtype(Type);
extern void foreach(Table, int, void (*)(Symbol, void *), void *);
extern Symbol genident(int, Type, int);
extern int genlabel(int);
extern Symbol install(const char *, Table *, int, int);
extern Symbol intconst(int);
extern Symbol lookup(const char *, Table);
extern Symbol mkstr(char *);
extern Symbol mksymbol(int, const char *, Type);
extern Symbol newtemp(int, int, int);
extern Table newtable(int);
extern Table table(Table, int);
extern Symbol temporary(int, Type);
extern char *vtoa(Type, Value);

extern void traceInit(char *);
extern int nodeid(Tree);
extern char *opname(int);
extern int *printed(int);
extern void printtree(Tree, int);
extern Tree root(Tree);
extern Tree texpr(Tree (*)(int), int, int);
extern Tree tree(int, Type, Tree, Tree);

extern void type_init(int, char *[]);

extern Type signedint(Type);

extern int hasproto(Type);
extern void outtype(Type, FILE *);
extern void printdecl (Symbol p, Type ty);
extern void printproto(Symbol p, Symbol args[]);
extern char *typestring(Type ty, char *id);
extern Field fieldref(const char *name, Type ty);
extern Type array(Type, int, int);
extern Type atop(Type);
extern Type btot(int, int);
extern Type compose(Type, Type);
extern Type deref(Type);
extern int eqtype(Type, Type, int);
extern Field fieldlist(Type);
extern Type freturn(Type);
extern Type ftype(Type, ...);
extern Type func(Type, Type *, int);
extern Field newfield(char *, Type, Type);
extern Type newstruct(int, char *);
extern void printtype(Type, int);
extern Type promote(Type);
extern Type ptr(Type);
extern Type qual(int, Type);
extern void rmtypes(int);
extern int ttob(Type);
extern int variadic(Type);

