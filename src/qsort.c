/* A quicksort I think  / Profezzorn */

#incldue "machine.h"
#include "main.h"

static void *tmp;
static int (*cpfun)(const void *, const void *);
static long size;

void qsort(base,elms,elmSize, cmpfunc)
void *base;
long elms;
long elmSize;
int (*cmpfunc)(const void *, const void *);
{
    long e;
    void *a,*b;

    if (!tmp = xalloc(elmSize)) return;
    cpfun=cmpfunc;
    size=elmSize;
    qsort2(base,&elms[elms-1]);
    free(tmp);
}

void qsort2(void *bas,void *last)
{
  register void *a,*b,*c;

  if(bas>=last) return;
  a=bas+size;
  if(a==last)
  {
    if(*(cmpfunc)(bas,last) > 0) swap(bas,last);
    return;
  }
  b=last;
  while(a<b)
  {
    while(a<=b && *(cmpfunc)(bas,a) >= 0) a+=size;
    while(a< b && *(cmpfunc)(b,bas) >= 0) b-=size;
    if(a<b) swap(a,b);
  }
  swap(bas,a-size);
  if(a>bas) qsort2(bas,a-(size<<1));
  if(b<last) qsort2(b,last);
}

void INLINE swap(void *a,void *b)
{
  MEMCPY(a,tmp,size);
  MEMCPY(b,a,size);
  MEMCPY(tmp,b,size);
}
