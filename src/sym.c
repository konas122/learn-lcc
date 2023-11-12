#include "c.h"
#include <stdio.h>

static char rcsid[] = "$Id$";

#define equalp(x) v.x == p->sym.u.c.v.x


// 该结构将某作用域内的符号保存在一个哈希表中
struct table {
	int level;          // 指明作用域
	Table previous;     // 指向外层作用域对应的`table`
	struct entry {
		struct symbol sym;
		struct entry *link;
	} *buckets[256];    // 指针数组，每个指针指向一个哈希链
    /*
    若要查找一个符号，先根据关键字计算出哈希值，找到相应的哈希链，
    然后通过遍历链表找到相应的符号； 若未找到该符号，
    则通过`previous`域在外层作用域的入口中进行查找
    */
	Symbol all;         // 指向由当前及外层作用域中所有符号组成的组成的列表的头
};


#define HASHSIZE NELEMS(((Table)0)->buckets)

static struct table
	cns = { CONSTANTS },
	ext = { GLOBAL },
	ids = { GLOBAL },
	tys = { GLOBAL };

// Tabel: pointer to struct table
Table constants   = &cns;
Table externals   = &ext;   // 声明为`extern`的标识符，用于警告外部标识符声明冲突
Table identifiers = &ids;   // 一般标识符
Table globals     = &ids;   // 保存具有文件作用域的标识符
Table types       = &tys;   // 编译器定义的类型标记
Table labels;               // 编译器定义的内部标号

int level = GLOBAL;
static int tempid;
List loci, symbols;


Table newtable(int arena) {
	Table new;

	NEW0(new, arena);
	return new;
}


// 动态分配内层嵌套的作用域表
Table table(Table tp, int level) {
	Table new = newtable(FUNC);
	new->previous = tp;
	new->level = level;
	if (tp)
		new->all = tp->all;
	return new;
}


/* 
`all`域初始化成指向外层表的`list`。因此，从`table`的`all`域开始，
就可以访问所有作用域的所有符号。`foreach`的作用就是扫描一个表   
*/
void foreach(Table tp, int lev, void (*apply)(Symbol, void *), void *cl) {
	assert(tp);

    // 循环查找与作用域对应的表
	while (tp && tp->level > lev)
		tp = tp->previous;

	if (tp && tp->level == lev) {
		Symbol p;
		Coordinate sav;
		sav = src;  // 临时存放`src`值
		for (p = tp->all; p && p->scope == lev; p = p->up) {
            // 全局变量`src`存放符号定义的位置
			src = p->src;
            // 并为该符号应用`apply`函数
            // `cl`是一个指针，指向与调用相关的数据`closure`
			(*apply)(p, cl);
            // `src`中保存的细腻些使得`apply`引发的诊断程序能够指向正确的源程序坐标
		}
		src = sav;
	}
}


// 进入新的作用域时，全局变量`level`会递增
void enterscope(void) {
	if (++level == LOCAL)
		tempid = 0;
}


// 退出作用域时，全局变量`level`将递减，相应的`identifiers`和`types`表也会随之撤销
void exitscope(void) {
    // 在撤销作用域中定义的带标记类型，`rmtypes`将从其类型缓冲中删除
	rmtypes(level);

	if (types->level == level)
		types = types->previous;

	if (identifiers->level == level) {
		if (Aflag >= 2) {
			int n = 0;
			Symbol p;
			for (p = identifiers->all; p && p->scope == level; p = p->up)
				if (++n > 127) {
					warning("more than 127 identifiers declared in a block\n");
					break;
				}
		}
		identifiers = identifiers->previous;
	}

	assert(level >= GLOBAL);
	--level;
}


// `install`函数为给定的`name`分配一个符号，并把该符号加入与给定作用域层数相对应的表中，
// 如果需要，还将建立一个新表。
Symbol install(const char *name, Table *tpp, int level, int arena) {
	Table tp = *tpp;    // `tpp`：指向表的指针
	struct entry *p;
    // 根据`name`计算哈希值
    unsigned h = (unsigned long)name & (HASHSIZE - 1);

    // `level`必须为零或不小于该表的作用域层数
    assert(level == 0 || level >= tp->level);

	if (level > 0 && tp->level < level)
		tp = *tpp = table(tp, level);
    
    // 新分配空间，并初始化为零
	NEW0(p, arena);
	p->sym.name = (char *)name;
	p->sym.scope = level;
	p->sym.up = tp->all;
	tp->all = &p->sym;
	p->link = tp->buckets[h];
	tp->buckets[h] = p;

	return &p->sym;
}


Symbol relocate(const char *name, Table src, Table dst) {
	struct entry *p, **q;
	Symbol *r;
	unsigned h = (unsigned long)name&(HASHSIZE-1);

	for (q = &src->buckets[h]; *q; q = &(*q)->link)
		if (name == (*q)->sym.name)
			break;
	assert(*q);
	/*
	 Remove the entry from src's hash chain
	  and from its list of all symbols.
	*/
	p = *q;
	*q = (*q)->link;
	for (r = &src->all; *r && *r != &p->sym; r = &(*r)->up)
		;
	assert(*r == &p->sym);
	*r = p->sym.up;
	/*
	 Insert the entry into dst's hash chain
	  and into its list of all symbols.
	  Return the symbol-table entry.
	*/
	p->link = dst->buckets[h];
	dst->buckets[h] = p;
	p->sym.up = dst->all;
	dst->all = &p->sym;
	return &p->sym;
}


// 在表中查找一个名字
Symbol lookup(const char *name, Table tp) {
	struct entry *p;
    // 根据`name`计算哈希值
    unsigned h = (unsigned long)name & (HASHSIZE - 1);

    assert(tp);

    // 循环扫描外层作用域
	do
        // 循环扫描哈希链
		for (p = tp->buckets[h]; p; p = p->link)
			if (name == p->sym.name)
				return &p->sym;
	while ((tp = tp->previous) != NULL);

	return NULL;
}


// 产生标号 `genlabel`也可以用于产生唯一的、匿名的名字，如产生一个临时变量的名字
int genlabel(int n) {
	static int label = 1;

	label += n;
	return label - n;
}


// 输入标号数，返回该表号对应的符号，
// 若需要，则会建立该符号、进行初始化并通知编译器后端
Symbol findlabel(int lab) {
	struct entry *p;
    // 根据`lab`计算哈希值
    unsigned h = lab & (HASHSIZE - 1);

    for (p = labels->buckets[h]; p; p = p->link)
		if (lab == p->sym.u.l.label)
			return &p->sym;

	NEW0(p, FUNC);

	p->sym.name = stringd(lab);
	p->sym.scope = LABELS;
	p->sym.up = labels->all;
	labels->all = &p->sym;
	p->link = labels->buckets[h];
	labels->buckets[h] = p;
    // `generated`是一位二进制位域(symbol flags)，表示一个产生的符号
	p->sym.generated = 1;
	p->sym.u.l.label = lab;

    (*IR->defsymbol)(&p->sym);

    return &p->sym;
}


Symbol constant(Type ty, Value v) {
	struct entry *p;
	unsigned h = v.u&(HASHSIZE-1);
	static union { int x; char endian; } little = { 1 };

	ty = unqual(ty);
	for (p = constants->buckets[h]; p; p = p->link)
		if (eqtype(ty, p->sym.type, 1))
			switch (ty->op) {
			case INT:      if (equalp(i)) return &p->sym; break;
			case UNSIGNED: if (equalp(u)) return &p->sym; break;
			case FLOAT:
				if (v.d == 0.0) {
					float z1 = v.d, z2 = p->sym.u.c.v.d;
					char *b1 = (char *)&z1, *b2 = (char *)&z2;
					if (z1 == z2
					&& (!little.endian && b1[0] == b2[0]
					||   little.endian && b1[sizeof (z1)-1] == b2[sizeof (z2)-1]))
						return &p->sym;
				} else if (equalp(d))
					return &p->sym;
				break;
			case FUNCTION: if (equalp(g)) return &p->sym; break;
			case ARRAY:
			case POINTER:  if (equalp(p)) return &p->sym; break;
			default: assert(0);
			}
	NEW0(p, PERM);
	p->sym.name = vtoa(ty, v);
	p->sym.scope = CONSTANTS;
	p->sym.type = ty;
	p->sym.sclass = STATIC;
	p->sym.u.c.v = v;
	p->link = constants->buckets[h];
	p->sym.up = constants->all;
	constants->all = &p->sym;
	constants->buckets[h] = p;
	if (ty->u.sym && !ty->u.sym->addressed)
		(*IR->defsymbol)(&p->sym);
	p->sym.defined = 1;
	return &p->sym;
}
Symbol intconst(int n) {
	Value v;

	v.i = n;
	return constant(inttype, v);
}
Symbol genident(int scls, Type ty, int lev) {
	Symbol p;

	NEW0(p, lev >= LOCAL ? FUNC : PERM);
	p->name = stringd(genlabel(1));
	p->scope = lev;
	p->sclass = scls;
	p->type = ty;
	p->generated = 1;
	if (lev == GLOBAL)
		(*IR->defsymbol)(p);
	return p;
}

Symbol temporary(int scls, Type ty) {
	Symbol p;

	NEW0(p, FUNC);
	p->name = stringd(++tempid);
	p->scope = level < LOCAL ? LOCAL : level;
	p->sclass = scls;
	p->type = ty;
	p->temporary = 1;
	p->generated = 1;
	return p;
}
Symbol newtemp(int sclass, int tc, int size) {
	Symbol p = temporary(sclass, btot(tc, size));

	(*IR->local)(p);
	p->defined = 1;
	return p;
}

Symbol allsymbols(Table tp) {
	return tp->all;
}

void locus(Table tp, Coordinate *cp) {
	loci    = append(cp, loci);
	symbols = append(allsymbols(tp), symbols);
}

void use(Symbol p, Coordinate src) {
	Coordinate *cp;

	NEW(cp, PERM);
	*cp = src;
	p->uses = append(cp, p->uses);
}
/* findtype - find type ty in identifiers */
Symbol findtype(Type ty) {
	Table tp = identifiers;
	int i;
	struct entry *p;

	assert(tp);
	do
		for (i = 0; i < HASHSIZE; i++)
			for (p = tp->buckets[i]; p; p = p->link)
				if (p->sym.type == ty && p->sym.sclass == TYPEDEF)
					return &p->sym;
	while ((tp = tp->previous) != NULL);
	return NULL;
}

/* mkstr - make a string constant */
Symbol mkstr(char *str) {
	Value v;
	Symbol p;

	v.p = str;
	p = constant(array(chartype, strlen(v.p) + 1, 0), v);
	if (p->u.c.loc == NULL)
		p->u.c.loc = genident(STATIC, p->type, GLOBAL);
	return p;
}

/* mksymbol - make a symbol for name, install in &globals if sclass==EXTERN */
Symbol mksymbol(int sclass, const char *name, Type ty) {
	Symbol p;

	if (sclass == EXTERN)
		p = install(string(name), &globals, GLOBAL, PERM);
	else {
		NEW0(p, PERM);
		p->name = string(name);
		p->scope = GLOBAL;
	}
	p->sclass = sclass;
	p->type = ty;
	(*IR->defsymbol)(p);
	p->defined = 1;
	return p;
}

/* vtoa - return string for the constant v of type ty */
char *vtoa(Type ty, Value v) {
	ty = unqual(ty);
	switch (ty->op) {
	case INT:      return stringd(v.i);
	case UNSIGNED: return stringf((v.u&~0x7FFF) ? "0x%X" : "%U", v.u);
	case FLOAT:    return stringf("%g", (double)v.d);
	case ARRAY:
		if (ty->type == chartype || ty->type == signedchar
		||  ty->type == unsignedchar)
			return v.p;
		return stringf("%p", v.p);
	case POINTER:  return stringf("%p", v.p);
	case FUNCTION: return stringf("%p", v.g);
	}
	assert(0); return NULL;
}
