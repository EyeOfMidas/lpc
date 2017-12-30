/* Satoria's malloc intended to be optimized for lpmud.
** this memory manager distinguishes between two sizes
** of blocks: small and large.  It manages them separately
** in the hopes of avoiding fragmentation between them.
** It expects small blocks to mostly be temporaries.
** It expects an equal number of future requests as small
** block deallocations.
*/
#include "global.h"
#include "dynamic_buffer.h"
#include "simulate.h"

#ifdef MSDOS
#define char void
#endif

#if 0
static void fake(char *s)
{
  fprintf(stderr,"%s\n",s);
}
#else
#define fake(s)
#endif

#define smalloc malloc
#define sfree free
#define srealloc realloc

#define SMALL_BLOCK_MAX_BYTES	40
#define SMALL_CHUNK_SIZE	0x4000
#define CHUNK_SIZE		0x40000

#define SINT sizeof(int)
#define SMALL_BLOCK_MAX (SMALL_BLOCK_MAX_BYTES/SINT)

#define PREV_BLOCK	0x80000000
#define THIS_BLOCK	0x40000000
#define MASK		0x0FFFFFFF

#define MAGIC		0x17952932

/* SMALL BLOCK info */

typedef size_t u;

static u *sfltable[SMALL_BLOCK_MAX];	/* freed list */
static u *next_unused=0;
static u unused_size;			/* until we need a new chunk */

/* LARGE BLOCK info */

static u *free_list=0;
static u *start_next_block=0;

/* STATISTICS */

static int small_count[SMALL_BLOCK_MAX];
static int small_total[SMALL_BLOCK_MAX];
static int small_max[SMALL_BLOCK_MAX];
static int small_free[SMALL_BLOCK_MAX];

typedef struct { unsigned counter, size; } stat;
#define count(a,b) { a.size+=(b); if ((b)<0) --a.counter; else ++a.counter; }
#define count_up(a,b) { a.size+=(b); ++a.counter; }
#define count_down(a,b) { a.size+=(b); --a.counter; }


int debugmalloc;	/* Only used when debuging malloc() */

/********************************************************/
/*  SMALL BLOCK HANDLER					*/
/********************************************************/

static char *large_malloc();
static void large_free();

#define s_size_ptr(p)	(p)
#define s_next_ptr(p)	((u **) (p+1))

static stat small_alloc_stat;
static stat small_free_stat;
static stat small_chunk_stat;

#ifdef MALLOC_DEBUG

static char my_watch;
static char *my_watchpoint=0;
void do_my_watch(char *p)
{
  my_watchpoint=p;
  my_watch=*p;
}

static apa()
{
  char *p;
  p="ABORTING\n";
  write(1,p,strlen(p));
  abort();
}

void verify_sfltable();
void check_sfltable()
{
  u *foo;
  u *last=0;
  int e;
  if(my_watchpoint)
  {
    if(my_watch!=*my_watchpoint)
    {
      fprintf(stderr,"Watchpoint alert! %d!=%d\n",my_watch , *my_watchpoint);
    }
  }
  
  if(!debugmalloc) return;

  for(e=0;e<SMALL_BLOCK_MAX;e++)
  {
    for(foo=sfltable[e];foo;foo=*s_next_ptr(foo))
    {
      if(last==foo)
      {
	fprintf(stderr,"sfltable[%d] out of order, or?\n",e);
	break;
      }
      if(*s_size_ptr(foo)!=e+2)
      {
	fprintf(stderr,"sfltable[%d] out of order\n",e);
	apa();
      }
      last=foo;
    }
  }
  verify_sfltable();
}

POINTER smalloc2(u size);
static u smsizeof(POINTER p);
POINTER smalloc(u size)
{
  char *foo;
  int msize;
  check_sfltable();
  foo=smalloc2(size);
  msize=smsizeof(foo);
  if((msize>SMALL_BLOCK_MAX_BYTES) ^ (size>SMALL_BLOCK_MAX_BYTES))
  {
    fprintf(stderr,"Urgh... malloc out of order.\n");
    apa();
  }
  check_sfltable();
  return foo;
}


POINTER smalloc2(u size)
#else
POINTER smalloc(u size)
#endif
{
  int i;
  u *temp;

  if (size == 0)
      fatal("Malloc size 0.\n");
  if (size>SMALL_BLOCK_MAX_BYTES)
    return large_malloc(size,0);

  i = (size - 1) >> 2;
  size = i+2;				/* block size in ints */
  count_up(small_alloc_stat,size << 2);

  small_count[i] += 1;			/* update statistics */
  small_total[i] += 1;
  if (small_count[i] >= small_max[i])
    small_max[i] = small_count[i];

  if (sfltable[i]) 
  {				/* allocate from the free list */
    count_down(small_free_stat, -(int) (size << 2));
    temp = sfltable[i];
    sfltable[i] = * (u **) (temp+1);
    fake("From free list.");
    return (char *) (temp+1);
  } /* else allocate from the chunk */

  if (unused_size<size)			/* no room in chunk, get another */
  {
    fake("Allocating new small chunk.");
    next_unused = (u *) large_malloc(SMALL_CHUNK_SIZE,1);
    if (next_unused == 0)
      return 0;
    count_up(small_chunk_stat, SMALL_CHUNK_SIZE+SINT);
    unused_size = SMALL_CHUNK_SIZE / SINT;
  }else{
    fake("Allocated from chunk.");
  }


  temp = (u *) s_next_ptr(next_unused); 

  *s_size_ptr(next_unused) = size;
  next_unused += size;
  unused_size -= size;

  return (char *) temp;
}

static char *debug_free_ptr;

#ifdef FREE_RETURNS_VOID
void sfree(POINTER ptr)
#else
int sfree(POINTER ptr)
#endif
{
  u *block;
  u i;
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  debug_free_ptr = ptr;
  block = (u *) ptr;
  block -= 1;
  if ((*s_size_ptr(block) & MASK) > SMALL_BLOCK_MAX + 1)
  {
    large_free(ptr);
  }else{

    i = *block - 2;
    count_down(small_alloc_stat, - (int) ((i+2) << 2));
    count_up(small_free_stat, (i+2) << 2);
#ifdef DEBUG
  {
    u *e;
    for(e=sfltable[i];e;e=*s_next_ptr(e))
    {
      if(e==block)
      {
	fprintf(stderr,"Freeing small block again.\n");
	apa();
      }
    }
  }
#endif
    *s_next_ptr(block) = sfltable[i];
    sfltable[i] = block;
    small_free[i] += 1;
    fake("Freed");
  }
#ifndef FREE_RETURNS_VOID
  return 1;
#endif
}

/************************************************/
/*	LARGE BLOCK HANDLER			*/
/************************************************/

#define BEST_FIT	0
#define FIRST_FIT	1
#define HYBRID		2
int fit_style =BEST_FIT;

#define l_size_ptr(p)		(p)
#define l_next_ptr(p)		(*((u **) (p+1)))
#define l_prev_ptr(p)		(*((u **) (p+2)))
#define l_next_block(p)		(p + (*(p)))
#define l_prev_block(p) 	(p - (*(p-1)))
#define l_prev_free(p)		(!(*p & PREV_BLOCK))
#define l_next_free(p)		(!(*l_next_block(p) & THIS_BLOCK))

void show_block(u *ptr)
{
  fprintf(stderr,"[%c%d: %d]  ",(*ptr & THIS_BLOCK ? '+' : '-'),
		(int) ptr, *ptr & MASK);
}

void show_free_list()
{
  u *p;
  p = free_list;
  while (p) {
    show_block(p);
    p = l_next_ptr(p);
  }
  fprintf(stderr,"\n");
}

static stat large_free_stat;     
static void remove_from_free_list(u *ptr)
{
  count_down(large_free_stat, - (int) (*ptr & MASK) << 2);

  if (l_prev_ptr(ptr))
    l_next_ptr(l_prev_ptr(ptr)) = l_next_ptr(ptr);
  else
    free_list = l_next_ptr(ptr);

  if (l_next_ptr(ptr))
    l_prev_ptr(l_next_ptr(ptr)) = l_prev_ptr(ptr);
}

static void add_to_free_list(u *ptr)
{
  extern int puts();
  count_up(large_free_stat, (*ptr & MASK) << 2);

  if (free_list && l_prev_ptr(free_list)) 
    puts("Free list consistency error.");

  l_next_ptr(ptr) = free_list;
  if (free_list) 
    l_prev_ptr(free_list) = ptr;
  l_prev_ptr(ptr) = 0;
  free_list = ptr;
}

/* build a properly annotated unalloc block */
static void build_block(u *ptr,u size)
{
  *(ptr) = (*ptr & PREV_BLOCK) | size;		/* mark this block as free */
  *(ptr+size-1) = size;
  *(ptr+size) &= (MASK | THIS_BLOCK); /* unmark previous block */
}

static void mark_block(u *ptr)		/* mark this block as allocated */
{
  *l_next_block(ptr) |= PREV_BLOCK;
  *ptr |= THIS_BLOCK;
}

/*
 * It is system dependent how sbrk() aligns data, so we simply use brk()
 * to insure that we have enough.
 */
static stat sbrk_stat;
static char *esbrk(u size)
{
#ifndef HAVE_UNISTD_H
  extern char *sbrk();
  extern int brk();
#endif
  static char *current_break;

  if (current_break == 0)
    current_break = sbrk(0);
  if (brk(current_break + size) == -1)
    return 0;
  count_up(sbrk_stat,size);
  current_break += size;
  return current_break - size;
}

stat large_alloc_stat;
static char *large_malloc(u size,int force_more)
{
  u best_size, real_size;
  u *first, *best, *ptr;

  size = (size + 7) >> 2;	/* plus overhead */
  count_up(large_alloc_stat, size << 2);
  first = best = 0;
  best_size = MASK;

  if (force_more)
    ptr = 0;
  else
    ptr = free_list;

  while (ptr) {
    u tempsize;
    /* Perfect fit? */
    tempsize = *ptr & MASK;
    if (tempsize == size) {
      best = first = ptr;
      break;			/* always accept perfect fit */
    }

    /* does it really even fit at all */
    if (tempsize >= size + SMALL_BLOCK_MAX + 1)
    {
      /* try first fit */
      if (!first) 
      {
	first = ptr;
	if (fit_style == FIRST_FIT)
	  break;		/* just use this one! */
      }
      /* try best fit */
      tempsize -= size;
      if (tempsize>0 && tempsize<=best_size) 
      {
	best = ptr;
	best_size = tempsize;
      }
    }
    ptr = l_next_ptr(ptr);
  } /* end while */

  if (fit_style==BEST_FIT) ptr = best;
  else ptr = first;		/* FIRST_FIT and HYBRID both leave it in first */

  if (!ptr)			/* no match, allocate more memory */
  {
    u chunk_size, block_size;
    block_size = size*SINT;
    if (force_more || (block_size>CHUNK_SIZE))
      chunk_size = block_size;
    else
      chunk_size = CHUNK_SIZE;

    if (!start_next_block) {
      start_next_block = (u *) esbrk(SINT);
      if (!start_next_block)
	return 0;
      *(start_next_block) = PREV_BLOCK;
      fake("Allocated little fake block");
    }

    ptr = (u *) esbrk(chunk_size);
    if (ptr == 0)
      return 0;
    ptr -= 1;			/* overlap old memory block */
    block_size = chunk_size / SINT;

    /* configure header info on chunk */
      
    build_block(ptr,block_size);
    if (force_more)
    {
      fake("Build little block");
    }else{
      fake("Built memory block description.");
    }
    *l_next_block(ptr)=THIS_BLOCK;
    add_to_free_list(ptr);
  } /* end of creating a new chunk */

  remove_from_free_list(ptr);
  real_size = *ptr & MASK;

  if (real_size != size) {
    /* split block pointed to by ptr into two blocks */
    build_block(ptr+size, real_size-size);
    fake("Built empty block");
    add_to_free_list(ptr+size);
    build_block(ptr, size);
  }

  mark_block(ptr);
  fake("built allocated block");
  return (char *) (ptr + 1);
}

static void large_free(char *ptr)
{
  u size, *p;
  p = (u *) ptr;
  p-=1;
  size = *p & MASK;
  count(large_alloc_stat, - (int) (size << 2));

#ifdef DEBUG
  {
    u *b;
    for(b=free_list;b;b=l_next_ptr(b))
    {
      if(b==p)
      {
	fprintf(stderr,"Freeing large block twice.\n");
	apa();
      }
    }
  }
#endif

  if (l_next_free(p)) {
    remove_from_free_list(l_next_block(p));
    size += (*l_next_block(p) & MASK);
    *p = (*p & PREV_BLOCK) | size;
  }

  if (l_prev_free(p)) {
    remove_from_free_list(l_prev_block(p));
    size += (*l_prev_block(p) & MASK);
    p = l_prev_block(p);
  }

  build_block(p, size);

  add_to_free_list(p);
}

POINTER srealloc(POINTER p,u size)
{
   unsigned *q, old_size;
   char *t;
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
   
   q = (unsigned *) p;
   --q;
   old_size = ((*q & MASK)-1)*sizeof(int);
   if (old_size >= size)
      return p;

   t = malloc(size);
   if (t == 0) return (char *) 0;

   MEMCPY(t, p, old_size);
   free(p);
   return t;
}

static u smsizeof(POINTER p)
{
  unsigned *q;
  q= (unsigned *)p;
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  --q;
  return (( *q & MASK) -1) *sizeof(int);
}

int resort_free_list() { return 0; }

#define dump_stat(str,stat) sprintf(buffer,str,stat.counter,stat.size); my_strcat(buffer);

char *dump_malloc_data()
{
  char buffer[300];
  init_buf();

  my_strcat("Type                   Count      Space (bytes)\n");
  dump_stat("sbrk requests:     %8d        %10d (a)\n",sbrk_stat);
  dump_stat("large blocks:      %8d        %10d (b)\n",large_alloc_stat);
  dump_stat("large free blocks: %8d        %10d (c)\n\n",large_free_stat);
  dump_stat("small chunks:      %8d        %10d (d)\n",small_chunk_stat);
  dump_stat("small blocks:      %8d        %10d (e)\n",small_alloc_stat);
  dump_stat("small free blocks: %8d        %10d (f)\n",small_free_stat);

  sprintf(buffer,"unused from current chunk          %10d (g)\n\n",unused_size<<2);
  my_strcat(buffer);
  sprintf(buffer,"    Small blocks are stored in small chunks, which are allocated as\n");
  my_strcat(buffer);
  sprintf(buffer,"large blocks.  Therefore, the total large blocks allocated (b) plus\n");
  my_strcat(buffer);
  sprintf(buffer,"the large free blocks (c) should equal total storage from sbrk (a).\n");
  my_strcat(buffer);
  sprintf(buffer,"Similarly, (e) + (f) + (g) equals (d).  The total amount of storage\n");
  my_strcat(buffer);
  sprintf(buffer,"wasted is (c) + (f) + (g); the amount allocated is (b) - (f) - (g).\n");
  my_strcat(buffer);
  return free_buf();
}

/*
 * calloc() is provided because some stdio packages uses it.
 */
POINTER calloc(u nelem,u sizel)
{
  char *p;
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif

  if (nelem == 0 || sizel == 0)
    return 0;
  p = malloc(nelem * sizel);
  if (p == 0)
    return 0;
  (void)MEMSET(p, '\0', nelem * sizel);
  return p;
}

/*
 * Functions below can be used to debug malloc.
 */

#ifdef MALLOC_DEBUG
/*
 * Verify that the free list is correct. The upper limit compared to
 * is very machine dependant.
 */
void verify_sfltable()
{
  u *p;
  int i, j;
  extern int end;

  if (!debugmalloc)
    return;
  if (unused_size > SMALL_CHUNK_SIZE / SINT)
    apa();
  for (i=0; i < SMALL_BLOCK_MAX; i++) {
    for (j=0, p = sfltable[i]; p; p = * (u **) (p + 1), j++) {
      if (p < (u *)&end || p > (u *) 0xfffff)
	apa();
      if (*p - 2 != i)
	apa();
    }
    if (p >= next_unused && p < next_unused + unused_size)
      apa();
  }
  p = free_list;
  while (p) {
    if (p >= next_unused && p < next_unused + unused_size)
      apa();
    p = l_next_ptr(p);
  }
}

void verify_free(u *ptr)
{
  u *p;
  int i, j;

  if (!debugmalloc)
    return;
  for (i=0; i < SMALL_BLOCK_MAX; i++) {
    for (j=0, p = sfltable[i]; p; p = * (u **) (p + 1), j++) {
      if (*p - 2 != i)
	apa();
      if (ptr >= p && ptr < p + *p)
	apa();
      if (p >= ptr && p < ptr + *ptr)
	apa();
      if (p >= next_unused && p < next_unused + unused_size)
	apa();
    }
  }

  p = free_list;
  while (p) {
    if (ptr >= p && ptr < p + (*p & MASK))
      apa();
    if (p >= ptr && p < ptr + (*ptr & MASK))
      apa();
    if (p >= next_unused && p < next_unused + unused_size)
      apa();
    p = l_next_ptr(p);
  }
  if (ptr >= next_unused && ptr < next_unused + unused_size)
    apa();
}


static char *ref;
void test_malloc(char * p)
{
  if (p == ref)
    printf("Found 0x%p\n", p);
}

#endif /* 0 (never) */

