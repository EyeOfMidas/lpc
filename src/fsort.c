/* fsort- a smarter quicksort / Profezzorn */
/* Optimized for minimum amount of compares */

#include "global.h"
#include "interpret.h"
#include "object.h"
#include "regexp.h"
#include "exec.h"
#include "simulate.h"

static int (*cmpfun)(const void *, const void *);
static void (*swapfun)(register void *, register void *);
static long size;
static char *tmp_area;

#ifdef DEBUG
static long elements;
static void *first;

static void ftest(void *a)
{
  int e;
  e=(char *)a-(char *)first;
  if(a<first || (e % size) || (e/size)>=elements)
    fatal("Error in fsort.\n");
}
#else
#define ftest(X) 
#endif

static void byteswap(register void *a,register void *b)
{
  char foo;
  ftest(a);
  ftest(b);
  foo=*(char *)a;
  *(char *)a=*(char *)b;
  *(char *)b=foo;
}

static void shortswap(register void *a,register void *b)
{
  short foo;
  ftest(a);
  ftest(b);
  foo=*(short *)a;
  *(short *)a=*(short *)b;
  *(short *)b=foo;
}


static void longswap(register void *a,register void *b)
{
  long foo;
  ftest(a);
  ftest(b);
  foo=*(long *)a;
  *(long *)a=*(long *)b;
  *(long *)b=foo;
}

static void long2swap(register void *a,register void *b)
{
  long foo;
  ftest(a);
  ftest(b);
  foo=((long *)a)[0];
  ((long *)a)[0]=((long *)b)[0];
  ((long *)b)[0]=foo;
  foo=((long *)a)[1];
  ((long *)a)[1]=((long *)b)[1];
  ((long *)b)[1]=foo;
}

static void fswap(register void *a,register void *b)
{
  ftest(a);
  ftest(b);
  MEMCPY(tmp_area,a,size);
  MEMCPY(a,b,size);
  MEMCPY(b,tmp_area,size);
}

static void fsort2(register char *bas,register char *last)
{
  register char *a,*b;
  register dum;

  ftest(bas);
  if(bas>=last) return;
  ftest(last);
  a=bas+size;
  if(a==last)
  {
    if((*cmpfun)(bas,last) > 0) swapfun(bas,last);
    return;
  }
  dum=(last-bas)>>1;
  dum-=dum % size;
  swapfun(bas,bas+dum);
  b=last;
  while(a<b)
  {
    while(a<=b && (*cmpfun)(a,bas) < 0) a+=size;
    while(a< b && (*cmpfun)(bas,b) < 0) b-=size;
    if(a<b)
    {
      swapfun(a,b);
      a+=size;
      if(b-a>size) b-=size;
    }
  }
  swapfun(bas,a-=size);
  fsort2(bas,a-=size);
  fsort2(b,last);
}

void fsort(base,elms,elmSize, cmpfunc)
void *base;
long elms;
long elmSize;
int (*cmpfunc)(const void *, const void *);
{

    if(elms<=0) return;
    cmpfun=cmpfunc;
    size=elmSize;
#ifdef DEBUG
    elements=elms;
    first=base;
#endif
    switch(elmSize)
    {
    case sizeof(char): swapfun=byteswap; break;
    case sizeof(short): swapfun=shortswap; break;
    case sizeof(long): swapfun=longswap; break;
    case sizeof(long)*2: swapfun=long2swap; break;
    default:
      swapfun=fswap;
      tmp_area=(char *)alloca(elmSize);
    }
    fsort2((char *)base,((char *)base) + size * (elms - 1));
}


