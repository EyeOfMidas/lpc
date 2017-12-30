#include "global.h"
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include "array.h"
#include "interpret.h"
#include "object.h"
#include "regexp.h"
#include "exec.h"
#include "main.h"
#include "list.h"
#include "simulate.h"

struct vector *allocate_mapping(struct vector *v,struct vector *w)
{
  struct vector *m;

  m = allocate_array_no_init(3,0);
  m->item[0].u.vec = v;
  m->item[0].type = T_ALIST_PART;
  m->item[1].u.vec = w;
  m->item[1].type = T_ALIST_PART;
  SET_TO_UNDEFINED(m->item[2]);
  return m;
}

INLINE void map_set_default(struct vector *v,struct svalue *s)
{
  assign_svalue(&(v->item[2]),s);
}

INLINE struct svalue *map_get_default(struct vector *v)
{
  return &(v->item[2]);
}

INLINE void free_mapping(struct vector *m)
{
  free_vector(m);
}

struct vector *remove_mapping2(struct vector *m, struct svalue *s)
{
  int i;

  i = assoc(s, m->item[0].u.vec);
  if(i>=0) donk_alist_item(m,i);
  return m;
}

/* Is it a bird?
 * A Boing 747?
 * A flying whale?
 * no, it's Profezzorn's latest hack....
 */

struct vector *intersect_mapping(struct vector *a,struct vector *b)
{
  int q,w,d,num;
  struct vector *ret;

  num=q=w=0;

  while(q<a->item[0].u.vec->size && w<b->item[0].u.vec->size)
  {
    d=alist_cmp(&(a->item[0].u.vec->item[q]),&(b->item[0].u.vec->item[w]));
    if(d<0) q++;
    else if(d>0) w++;
    else num++,q++,w++;
  }
  ret=allocate_mapping(allocate_array_no_init(num,0),allocate_array_no_init(num,0));

  num=q=w=0;

  while(q<a->item[0].u.vec->size && w<b->item[0].u.vec->size)
  {
    d=alist_cmp(&(a->item[0].u.vec->item[q]),&(b->item[0].u.vec->item[w]));
    if(d<0) q++;
    else if(d>0) w++;
    else { mutilate_mapping(ret,num++,a,q++); b++; }
  }

  return ret;
}

struct vector *subtract_mapping(struct vector *a,struct vector *b)
{
  int q,w,d,num;
  struct vector *ret;

  num=q=w=0;

  while(q<a->item[0].u.vec->size && w<b->item[0].u.vec->size)
  {
    d=alist_cmp(&(a->item[0].u.vec->item[q]),&(b->item[0].u.vec->item[w]));
    if(!d) q++;
    else if(d>0) w++;
    else num++,q++;
  }
  num+=a->item[0].u.vec->size-q;
  ret=allocate_mapping(allocate_array_no_init(num,0),allocate_array_no_init(num,0));

  num=q=w=0;

  while(q<a->item[0].u.vec->size && w<b->item[0].u.vec->size)
  {
    d=alist_cmp(&(a->item[0].u.vec->item[q]),&(b->item[0].u.vec->item[w]));
    if(!d) q++;
    else if(d>0) w++;
    else mutilate_mapping(ret,num++,a,q++);
  }

  while(q<a->item[0].u.vec->size) mutilate_mapping(ret,num++,a,q++);
  return ret;  
}

/* This one just takes two mappings, sorry */

#define SUM_MAP_ERR(X) \
        { \
          free_vector(indices); \
          free_vector(values); \
          error((X)); \
        }

struct vector *sum_mappings(
	struct vector *mapa,
	struct vector *mapb,
	struct svalue *funa,
	struct svalue *funab,
	struct svalue *funb)
{
  int size,ap,bp,asize,bsize,tmp;
  struct svalue *aa,*bb,*ip,*vp,*ret;
  struct vector *indices;
  struct vector *values;

  asize=mapa->item[0].u.vec->size;
  bsize=mapb->item[0].u.vec->size;
  aa=& mapa->item[0].u.vec->item[0];
  bb=& mapb->item[0].u.vec->item[0];
  ap=bp=size=0;

  while(ap<bsize && bp<bsize)
  {
    size++;
    tmp=alist_cmp(aa+ap,bb+bp);

    if(tmp<0)		{ if(funa->type==T_FUNCTION) ap++; }
    else if(tmp>0)	{ if(funb->type==T_FUNCTION) bp++; }
    else		{ if(funab->type==T_FUNCTION) ap++,bp++; }
  }
  size+=asize-ap+bsize-bp;
  indices=allocate_array_no_init(size,0);
  values=allocate_array_no_init(size,0);
  ip=& indices->item[0];
  vp=& values->item[0];
  ap=bp=size=0;

  while(ap<bsize && bp<bsize)
  {
    tmp=alist_cmp(& aa[ap],& bb[bp]);
    if(!tmp)
    {
      if(funab->type==T_FUNCTION)
      {
        push_svalue(&mapa->item[1].u.vec->item[ap++]);
        push_svalue(&mapb->item[1].u.vec->item[bp++]);
        if(funab->u.ob->flags & O_DESTRUCTED)
          SUM_MAP_ERR("Object used by sum_mappings destructed.\n");

        ret=apply_lambda(funab,2,0);
        if(!ret) SUM_MAP_ERR("Function used by sum_mappings() not found.\n");

        assign_svalue_no_free(&ip[size],&aa[ap]);
        assign_svalue_no_free(&vp[size++],ret);
      }
    }else if(tmp<0){
      if(funa->type==T_FUNCTION)
      {
        push_svalue(&mapa->item[1].u.vec->item[ap++]);
        if(funa->u.ob->flags & O_DESTRUCTED)
          SUM_MAP_ERR("Object used by sum_mappings destructed.\n");

        ret=apply_lambda(funa,1,0);
        if(!ret) SUM_MAP_ERR("Function used by sum_mappings() not found.\n");

        assign_svalue_no_free(&ip[size],&aa[ap]);
        assign_svalue_no_free(&vp[size++],ret);
      }
    }else{
      if(funb->type==T_FUNCTION)
      {
        push_svalue(&mapb->item[1].u.vec->item[bp++]);
          SUM_MAP_ERR("Object used by sum_mappings destructed.\n");

        ret=apply_lambda(funb,1,0);
        if(!ret) SUM_MAP_ERR("Function used by sum_mappings() not found.\n");

        assign_svalue_no_free(&ip[size],&bb[bp]);
        assign_svalue_no_free(&vp[size++],ret);
      }
    }
  }
  return allocate_mapping(indices,values);
}
