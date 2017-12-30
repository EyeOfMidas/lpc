#include "global.h"
#include "exec.h"
#include "interpret.h"
#include "object.h"
#include "array.h"
#include "stralloc.h"

unsigned int hashmem(const char *a,int len,int mlen)
{
  unsigned int ret;
  ret=9248339*len;
  if(len<mlen) mlen=len;
  switch(mlen&7)
  {
    case 7: ret^=*(a++);
    case 6: ret^=(ret<<4)+*(a++);
    case 5: ret^=(ret<<7)+*(a++);
    case 4: ret^=(ret<<6)+*(a++);
    case 3: ret^=(ret<<3)+*(a++);
    case 2: ret^=(ret<<7)+*(a++);
    case 1: ret^=(ret<<5)+*(a++);
  }

#ifdef HANDLES_UNALIGNED_MEMORY_ACCESS
  for(mlen>>=3;--mlen>=0;)
  {
    ret^=(ret<<7)+*(((unsigned int *)a)++);
    ret^=(ret>>6)+*(((unsigned int *)a)++);
  }
#else
  for(mlen>>=3;--mlen>=0;)
  {
    ret^=(ret<<7)+((((((*(a++)<<3)+*(a++))<<4)+*(a++))<<5)+*(a++));
    ret^=(ret>>6)+((((((*(a++)<<3)+*(a++))<<4)+*(a++))<<5)+*(a++));
  }
#endif
  return ret;
}

unsigned int hashstr(const char *str,int maxn)
{
  return hashmem(str,strlen(str),maxn);
}

unsigned int hash_svalue(struct svalue *s,unsigned int size)
{
  switch (s->type)
  {
    case T_NUMBER: return (s->u.number) % size;
    case T_OBJECT: return (s->u.ob->clone_number) % size;
    case T_STRING: return hashstr(strptr(s),100) % size;
    case T_POINTER:
    case T_ALIST_PART: return s->u.vec->size % size;
    case T_LIST:
    case T_MAPPING:  return s->u.vec->item[0].u.vec->size % size;
    default: return 0;
  }
}
