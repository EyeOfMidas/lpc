#include "global.h"

/* Ta-da */
#if 0
#define TEST(X,Y) if((X)<(Y) || (X)>(Y)+(elms-1)*elmSize) fprintf(stderr,"gahhh, sort out of bounds!\n"),abort();
#else
#define TEST(X,Y)
#endif


void msort(base, elms, elmSize, cmpfunc)
void *base;
long elms;
long elmSize;
int (*cmpfunc)(const void *, const void *);
{
  char *dum,*dum2,*dum3,*dum4,*p1,*p2,*p3;
  int i,j,i1,i2;

  if(elms<=0) return;
  dum=(char *)base;
  dum4=dum2=(char *)malloc(elms*elmSize);

  for (i = 2; (i>>1)<elms; i *= 2)
  {
    for(j=0;j<elms;j+=i)
    {
      i2 = i1 = i >> 1;
      if(j+i1>elms) i1=elms-j;
      if(j+i1+i2>elms) i2=elms-i1-j;

      p1 = dum + j * elmSize;
      p2 = p1  + i1* elmSize;
      p3 = dum2+ j * elmSize;

      while (i2 > 0 && i1 > 0)
      {
        if ((*cmpfunc)(p1, p2) <= 0)
        {
          MEMCPY(p3,p1,elmSize);
	TEST(p1,dum)
	TEST(p3,dum2)
          --i1;
          p1 += elmSize;
        }else{
          MEMCPY(p3,p2,elmSize);
	TEST(p2,dum)
	TEST(p3,dum2)
          --i2;
          p2 += elmSize;
        }
        p3+=elmSize;
      }
      if(i1>0)
      {	
	TEST(p1,dum);
	TEST(p3,dum2);
	TEST(p1+(i1-1)*elmSize,dum);
	TEST(p3+(i1-1)*elmSize,dum2);
	MEMCPY(p3,p1,i1*elmSize);
      }
      if(i2>0)
      {
	TEST(p2,dum);
	TEST(p3,dum2);
	TEST(p2+(i2-1)*elmSize,dum);
	TEST(p3+(i2-1)*elmSize,dum2);
	MEMCPY(p3,p2,i2*elmSize);
      }
    }
    dum3=dum; dum=dum2; dum2=dum3;
  }
  if(dum!=base)
  {
    MEMCPY(base,dum,elmSize*elms);
    dum3=dum; dum=dum2; dum2=dum3;
  }
  if(dum2==base)
  {
    fprintf(stderr,"GAHHH, sort out of bounds!\n");
    abort();
  }
  free(dum4);
}
