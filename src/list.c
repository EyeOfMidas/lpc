#include "global.h"
#include "array.h"
#include "interpret.h"
#include "list.h"
#include "object.h"
#include "stralloc.h"
#include "mapping.h"

void free_list(struct vector *v) { free_vector(v); }

struct vector *allocate_list(struct vector *v)
{
    struct vector *m;

    m = allocate_array_no_init(1,0);
    m->item[0].u.vec = v;
    m->item[0].type = T_ALIST_PART;
    return m;
}

struct vector *mklist(struct vector *v)
{
  v=allocate_list(slice_array(v,0,v->size));
  order_alist(v);
  return v;
}

void mutilate_mapping(struct vector *a,int ap,struct vector *b,int bp)
{
  assign_svalue_raw(&(a->item[0].u.vec->item[ap]),&(b->item[0].u.vec->item[bp]));
  assign_svalue_raw(&(a->item[1].u.vec->item[ap]),&(b->item[1].u.vec->item[bp]));
}

void list_assign(struct vector *a,int ap,struct vector *b,int bp)
{
  assign_svalue_raw(&(a->item[0].u.vec->item[ap]),&(b->item[0].u.vec->item[bp]));
}

void array_assign(struct vector *a,int ap,struct vector *b,int bp)
{
  assign_svalue_raw(&(a->item[ap]),&(b->item[0].u.vec->item[bp]));
}


/*
 * This function handles or, xor, and, sub on lists, mapping and arrays
 * /Profezzorn
 */

struct vector *do_array_surgery(
		struct vector *a,
		struct vector *b,
		int op,
		int type )
{
  register int ap,bp,d,num;
  int asize,bsize;
  struct vector *ret;
  struct svalue *ap2,*bp2;
  struct svalue *ap3,*bp3;
  void (*assignfunc)(struct vector *,int,struct vector *,int);

  asize=bsize=0; ap2=bp2=0; assignfunc=0; /* make gcc happy */
  switch(type)
  {
    case T_POINTER:	/* All surgery on arrays return alists */
      {
        struct vector *tt;
        tt=allocate_array_no_init(1,0);
        tt->item[0].type=T_POINTER;
        tt->item[0].u.vec=slice_array(a,0,a->size-1);
        order_alist(a=tt);

        tt=allocate_array_no_init(1,0);
        tt->item[0].type=T_POINTER;
        tt->item[0].u.vec=slice_array(b,0,b->size-1);
        order_alist(b=tt);

	asize=a->item[0].u.vec->size;
	bsize=b->item[0].u.vec->size;
	ap2=& a->item[0].u.vec->item[0];
	bp2=& b->item[0].u.vec->item[0];
	assignfunc=&array_assign;
      }
      break;

    case T_LIST:
      asize=a->item[0].u.vec->size;
      bsize=b->item[0].u.vec->size;
      ap2=& a->item[0].u.vec->item[0];
      bp2=& b->item[0].u.vec->item[0];
      assignfunc=&list_assign;
      break;
    case T_MAPPING:
      asize=a->item[0].u.vec->size;
      bsize=b->item[0].u.vec->size;
      ap2=& a->item[0].u.vec->item[0];
      bp2=& b->item[0].u.vec->item[0];
      assignfunc=&mutilate_mapping;
      break;
  }
  num=ap=bp=0;
  ap3=ap2;
  bp3=bp2;

  while(ap<asize && bp<bsize)
  {
    d=alist_cmp(ap2,bp2);

    if(!d) d=op & 7;
    else if(d<0) d=op>>4 & 7;
    else d=op>>8 & 7;

    switch(d)
    {
      case GETA:
        num++;
      case NOTA:
        ap++;
        ap2++;
        break;

      case GETB:
        num++;
      case NOTB:
        bp++;
        bp2++;
        break;

      case BOTH:
	num++;
      case EITHER:
	num++;
      case NONE:
        ap++;
        ap2++;
        bp++;
        bp2++;
       
    }
  }
  if(OPER_A_TAKE==(op & 0x0f0) ) num += asize-ap;
  if(OPER_B_TAKE==(op & 0xf00) ) num += bsize-bp;

  if(type==T_MAPPING)
    ret=allocate_mapping(allocate_array_no_init(num,0),allocate_array_no_init(num,0));
  else if(type==T_LIST)
    ret=allocate_list(allocate_array_no_init(num,0));
  else
    ret=allocate_array_no_init(num,0);

  num=ap=bp=0;
  ap2=ap3;
  bp2=bp3;

  while(ap<asize && bp<bsize)
  {
    d=alist_cmp(ap2,bp2);

    if(!d) d=op & 7;
    else if(d<0) d=op>>4 & 7;
    else d=op>>8 & 7;

    switch(d)
    {
      case GETA:
        assignfunc(ret,num++,a,ap);
      case NOTA:
        ap++;
        ap2++;
        break;
      case GETB:
        assignfunc(ret,num++,b,bp);
      case NOTB:
        bp++;
        bp2++;
        break;

      case BOTH:
        assignfunc(ret,num++,a,ap);
      case EITHER:
        assignfunc(ret,num++,b,bp);
      case NONE:
        bp++;
        ap++;
        bp2++;
        ap2++;
    }
  }

  if(OPER_A_TAKE==(op & 0x0f0)) while(ap<asize) assignfunc(ret,num++,a,ap++);
  if(OPER_B_TAKE==(op & 0xf00)) while(bp<bsize) assignfunc(ret,num++,b,bp++);

  if(type==T_POINTER)
  {
    free_vector(a);
    free_vector(b);
  }
  return ret;  
}

/* Ha ha Now I can clean the shit out in a quick manner ! */
struct vector *cleanup_map(struct vector *map)
{
  int size,e,newsize;
  struct svalue *i,*v,*ii,*vv,*def;
  struct vector *indices,*values,*ret;

  check_alist_for_destruct(map);

  size=map->item[0].u.vec->size;
  def=& map->item[2];

  newsize=0;
  v=& map->item[1].u.vec->item[0];
  for(e=0;e<size;e++,v++)
    if(alist_cmp(def,v))
      newsize++;

  indices=allocate_array_no_init(newsize,0);
  values=allocate_array_no_init(newsize,0);

  i=& map->item[0].u.vec->item[0];
  v=& map->item[1].u.vec->item[0];
  ii=& indices->item[0];
  vv=& values ->item[0];
  for(e=0;e<size;e++)
  {
    if(alist_cmp(def,v))
    {
      assign_svalue_raw(ii++,i);
      assign_svalue_raw(vv++,v);
    }
    i++; v++;
  }
  ret=allocate_mapping(indices,values);
  assign_svalue(map->item+2,ret->item+2);
  return ret;
}


