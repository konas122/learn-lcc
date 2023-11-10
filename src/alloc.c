#include "c.h"

// 每个分配去都是由一组很大的内存块构成的链表
// 每个内存块的数据结构定义如下
struct block {
	struct block *next;
    // 紧接着在这些分配区结构数据之后，直到`limit`所指的指针，都是可分配区域
	char *limit;
    // `avail`指向分配区域的首地址
	char *avail;
    // 地址小于`avail`的空间都是已分配区域，从`avail`到`limit`都是可分配区域
};


// 大多数分配内存非常零散，因此应照内存边界对齐原则来确定分配空间的大小
union align {
	long l;
	char *p;
	double d;
    int (*f)(void);
};


union header {
	struct block b;
	union align a;
};


// 若`PURIFY`已定义，则采用`malloc`和`free`来分配内存
#ifdef PURIFY
union header *arena[3];


void *allocate(unsigned long n, unsigned a) {
	union header *new = malloc(sizeof *new + n);

	assert(a < NELEMS(arena));
	if (new == NULL) {
		error("insufficient memory\n");
		exit(1);
	}
	new->b.next = (void *)arena[a];
	arena[a] = new;
	return new + 1;
}


void deallocate(unsigned a) {
	union header *p, *q;

	assert(a < NELEMS(arena));
	for (p = arena[a]; p; p = q) {
		q = (void *)p->b.next;
		free(p);
	}
	arena[a] = NULL;
}


void *newarray(unsigned long m, unsigned long n, unsigned a) {
    return allocate(m * n, a);
}
// `PURIFY`未定义，不能用`malloc`和`free`来分配内存
#else
static struct block
	 first[] = {  { NULL },  { NULL },  { NULL } },
	*arena[] = { &first[0], &first[1], &first[2] };

static struct block *freeblocks;


void *allocate(unsigned long n, unsigned a) {
	struct block *ap;

	assert(a < NELEMS(arena));
	assert(n > 0);
	ap = arena[a];
	n = roundup(n, sizeof (union align));
    // 不断循环，直到`ap`所指向的块至少有`n`个字节的可分配空间
	while (n > ap->limit - ap->avail) {
		if ((ap->next = freeblocks) != NULL) {
			freeblocks = freeblocks->next;
			ap = ap->next;
		} 
        else {
            // 一个内存块包含了：块的头`header`，`n`个字节所需空间和`10KB`空闲空间
            unsigned m = sizeof(union header) + n + roundup(10 * 1024, sizeof(union align));
            ap->next = malloc(m);
            ap = ap->next;
            // 若不能满足条件
            if (ap == NULL) {
                error("insufficient memory\n");
                exit(1);
            }
            ap->limit = (char *)ap + m;
        }

		ap->avail = (char *)((union header *)ap + 1);
		ap->next = NULL;
		arena[a] = ap;

	}
	ap->avail += n;
	return ap->avail - n;
}


void *newarray(unsigned long m, unsigned long n, unsigned a) {
	return allocate(m*n, a);
}


// `deallocate`并不会释放内存，而是将收回的块放入空闲块列表中（和内存池一样）
// 下面代码只是简单将整个链表加入`freeblock`
void deallocate(unsigned a) {
	assert(a < NELEMS(arena));
	arena[a]->next = freeblocks;
	freeblocks = first[a].next;
	first[a].next = NULL;
	arena[a] = &first[a];
}
#endif
