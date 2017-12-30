#include "global.h"
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include "array.h"
#include "interpret.h"
#include "object.h"
#include "regexp.h"
#include "exec.h"
#include "main.h"
#include "stralloc.h"
#include "simulate.h"
#include "operators.h"


#define VECTOR_SIZE(nelem) (sizeof(struct vector) +sizeof(struct svalue) * (nelem-1))
#define ALLOC_VECTOR(nelem)  (struct vector *)xalloc(VECTOR_SIZE(nelem))

/*
 * This file contains functions used to manipulate arrays.
 * Some of them are connected to efuns, and some are only used internally
 * by the game driver.
 */

extern int d_flag;

int num_arrays;
int total_array_size;

/*
 * Make an empty vector for everyone to use, never to be deallocated.
 * It is cheaper to reuse it, than to use malloc() and allocate.
 */
struct vector null_vector = {
    1,	/* Ref count, which will ensure that it will never be deallocated */
    0,	/* size */
    0,  /* malloced size */
    1   /* is clean */
};

struct vector *allocate_array_no_init(int n,int overhead)
{
  int i;
  struct vector *p;

  if (!batch_mode && (n < 0 || n > MAX_ARRAY_SIZE))
    error("Illegal array size.\n");

  if (n == 0)
  {
    p = &null_vector;
    p->ref++;
  }else{
    num_arrays++;
    total_array_size += VECTOR_SIZE(n);

    /* overhead stuff */
    i=n+overhead;
    if(i>MAX_ARRAY_SIZE && !batch_mode) i=MAX_ARRAY_SIZE;
    p = ALLOC_VECTOR(i);
    p->malloced_size=i;
    p->ref = 1;
    p->size = n;
    p->flags=ref_cycle;
    p->types=-1;

#ifdef DEBUG
    p->next=null_vector.next;
    null_vector.next=p;
#endif
  }
  return p;
}

/*
 * Allocate an array of size 'n'.
 * Observe that the overhead is not guarateed.
 */
struct vector *allocate_array_with_overhead(int n,int overhead)
{
  struct vector *p;
  int i;
  p=allocate_array_no_init(n,overhead);
  for (i=0; i<n; i++) SET_TO_ZERO(p->item[i]);
  return p;
}

struct vector *allocate_array(int n)
{
  return allocate_array_with_overhead(n,0);
}

#ifdef DEBUG
void free_vector(struct vector *p)
{
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  p->ref--;
  if (p->ref > 0)
    return;
  if(p->size>p->malloced_size)
  {
    fatal("Impossible array.");
  }
  if(p->ref<0) fatal("Freeing array too many times.\n");
#else

void real_free_vector(struct vector *p)
{  
#endif

#ifdef DEBUG
  if (p == &null_vector)
    fatal("Tried to free the zero-size shared vector.\n");

{
  struct vector *v;
  for(v=&null_vector;v;v=v->next)
  {
    if(v->next==p)
    {
      v->next=p->next;
      break;
    }
  }
  if(!v) fatal("Array not found in linked list.\n");
  if((v=null_vector.next))
  {
    do{
      if(!v->ref)
	fatal("Extremely fatal error in arrays.\n");
    }while(v->size && v->item[0].type==T_POINTER && (v=v->item[0].u.vec));
  }
}
#endif

  free_svalues(p->item,p->size);
  num_arrays--;
  total_array_size -= sizeof (struct vector) + sizeof (struct svalue) *
    (p->size-1);
  free((char *)p);
}

/* OBS, destructive */
struct vector *resize_array(struct vector *p,int n)
{
  struct vector *res;
  int e;
  if(p->size==n) return p;
  if(n>p->size)
  {
    /* we should grow the array */
    if(n<=p->malloced_size)
    {
      /* it fits */
      p->types|=BT_NUMBER;
      for(e=p->size+1;e<n;e++) SET_TO_ZERO(p->item[e]);
      e=n-p->size;
      total_array_size +=sizeof(struct svalue) *e ;
      p->size=n;
      return p;
    }
    res=allocate_array_no_init(n,(n>>4)+1);
    
    for(e=0;e<p->size;e++) res->item[e]=p->item[e];
    for(;e<n;e++) SET_TO_ZERO(res->item[e]);
    res->types=p->types | BT_NUMBER;
    p->size=0; /* ugly trick */
    free_vector(p);
    return res;
  }else{
    /* we should shrink the array */
    if(n<=(p->malloced_size>>11))
    {
      /* overhead to big, reallocate the array */
      res = slice_array(p, 0, n-1);
      free_vector(p);
      return res;
    }
    e=n-p->size;
    total_array_size += sizeof (struct svalue) * e;
    while(p->size>n) free_svalue(p->item+--(p->size));
    p->size = n;
    return p;
  }
}

struct vector *shrink_array(struct vector *p,int n)
{
  struct vector *res;
  int e;
  if(p->size==n) return p;
  if(n<=(p->malloced_size>>11))
  {
    /* overhead to big, reallocate the array */
    res = slice_array(p, 0, n-1);
    free_vector(p);
    return res;
  }
  e=n-p->size;
  total_array_size += sizeof (struct svalue) * e;
  p->size = n;
  return p;
}

static struct vector *insert_array(struct vector *v,int n,struct svalue *s)
{
  struct vector *res;
  int e;
#ifdef DEBUG
  if(n<0 || n>v->size) fatal("Illegal insert.\n");
#endif

  if(v->size<v->malloced_size)
  {
    /* it fits */
    for(e=v->size;e>n;e--) v->item[e]=v->item[e-1];
    v->size++;
    SET_TO_ZERO(v->item[n]);
  }else{
    res=allocate_array_no_init(v->size+1,(v->size>>4)+1);
    for(e=0;e<n;e++) res->item[e]=v->item[e];
    SET_TO_ZERO(res->item[e]);
    for(;e<v->size;e++) res->item[e+1]=v->item[e];
    v->size=0; /* ugly trick */
    free_vector(v);
    v=res;
  }
  assign_svalue(v->item+n,s);
  v->types |= 1<< (s->type);
  return v;
}

/* Rewritten by Profezzorn */

/* a binary string capable explode */

struct vector *explode(struct svalue *str,struct svalue *del)
{
  int str_len,del_len,e,tmp,begin;
  struct vector *v;
  char *s,*d;

  s=strptr(str);
  d=strptr(del);

  str_len=my_strlen(str);
  del_len=my_strlen(del);

  if(!del_len)
  {
    v=allocate_array_no_init(str_len,0);
    for(e=0;e<str_len;e++)
    {
      SET_STR(v->item+e,make_shared_binary_string(s+e,1));
    }
  }else{
    for(tmp=e=0;e<=str_len-del_len;)
    {
      if(!MEMCMP(s+e,d,del_len))
      {
        tmp++;
        e+=del_len;
      }else{
        e++;
      }
    }

    v=allocate_array_no_init(tmp+1,0);
    if(!tmp)
    {
      assign_svalue_no_free(v->item,str);
      return v;
    }
    for(begin=tmp=e=0;e<=str_len-del_len;)
    {
      if(!MEMCMP(s+e,d,del_len))
      {
	SET_STR(v->item+tmp,make_shared_binary_string(s+begin,e-begin));
        tmp++;
        e+=del_len;
        begin=e;
      }else{
        e++;
      }
    }
    SET_STR(v->item+tmp,make_shared_binary_string(s+begin,str_len-begin));
  }
  v->types=BT_STRING; /* nothing but us strings here */
  return v;
}

/* a binary implode */

void implode(struct svalue *to,struct vector *v,struct svalue *del)
{
  int del_len,len,e;
  char *s,*s2,*del_str;
  
  del_len=my_strlen(del);
  del_str=strptr(del);

  len=del_len*(v->size-1);
  for(e=0;e<v->size;e++)
  {
    if(v->item[e].type!=T_STRING) continue;
    len+=my_strlen(v->item+e);
  }

  if(len<=0)
  {
    assign_svalue_no_free(to,&const_empty_string);
  }else{
    s2=s=begin_shared_string(len);
    
    for(e=0;e<v->size;e++)
    {
      if(e)
      {
        MEMCPY(s2,del_str,del_len);
        s2+=del_len;
      }
      if(v->item[e].type!=T_STRING) continue;

      len=my_strlen(v->item+e);
      MEMCPY(s2,strptr(v->item+e),len);
      s2+=len;
    }
    SET_STR(to,end_shared_string(s)); /* may or may not use same area */
  }
}


/* The following two functions are for the replace() efun....
 *   Profezzorn
 */

struct vector *array_replace(struct vector *from,struct svalue *what,struct svalue *to)
{
  int e;
  struct vector *ret;
  check_vector_for_destruct(from);
  ret=allocate_array_no_init(from->size,0);
  for(e=0;e<from->size;e++)
  {
    if(is_eq(what,&(from->item[e])))
      assign_svalue_no_free(&(ret->item[e]),to);
    else
      assign_svalue_raw(&(ret->item[e]),&(from->item[e]));
  }
  return ret;
}

/*
 * Slice of an array.
 */
struct vector *slice_array(struct vector *p,int from,int to)
{
  struct vector *d;
    
  if (from < 0) from = 0;
  if (from >= p->size)  return allocate_array(0);	/* Slice starts above array */
  if (to >= p->size)  to = p->size-1;
  if (to < from)  return allocate_array(0); 
    
  d = allocate_array_no_init(to-from+1,0);
  d->types=p->types;

  copy_svalues_no_free(d->item,p->item+from,to-from+1);
  return d;
}

/* EFUN: filter_array
   
   Runs all elements of an array through ob->func()
   and returns an array holding those elements that ob->func
   returned 1 for.
   Hacking a little: if ob==0 then we call in the object in the array
                     instead.......
   */
struct vector *filter(p, lambda, extra, num_extra_arg)
  struct vector *p;
  struct svalue *lambda;
  struct svalue *extra;
  int num_extra_arg;
{
  struct vector *v,*r;
  int e,num;

  if (p->size<1) return allocate_array(0);
  v=map_array(p,lambda,extra,num_extra_arg);
  for(num=e=0;e<v->size;e++)
    if(v->item[e].type!=T_NUMBER || (v->item[e].u.number!=0)) num++;

  r=allocate_array_no_init(num,0);
  num=0;
  for(e=0;e<v->size;e++)
    if(v->item[e].type!=T_NUMBER || (v->item[e].u.number!=0))
      assign_svalue_no_free(&r->item[num++], &p->item[e]);
  free_vector(v);
  return r;
}

/* Concatenation of two arrays into one
 */
struct vector *add_array(struct vector *p,struct vector *r)
{
  struct vector *d;
    
  d = allocate_array_no_init(p->size+r->size,0);
  copy_svalues_no_free(d->item,p->item,p->size);
  copy_svalues_no_free(d->item+p->size,p->item,p->size);
  return d;
}

int search_alist(struct svalue *key,struct vector * keylist)
{
  int a,b,c,d;
  a=0;
  b=keylist->size;
  while(a<b)
  {
    c=(a+b)>>1;
    d=alist_cmp(key,keylist->item+c);
    if(d<0) b=c;
    else if(d>0) a=c+1;
    else return c;
  }
  if(a<keylist->size && alist_cmp(key,keylist->item+a)>0) a++;
  return a;
}

int assoc(struct svalue *key,struct vector *keylist)
{
  int a,b,c,d;
  a=0;
  b=keylist->size;
  while(a!=b)
  {
    c=(a+b)>>1;
    d=alist_cmp(key,keylist->item+c);
    if(d<0) b=c;
    else if(d>0) a=c+1;
    else return c;
  }
  return -1;
}

struct vector *subtract_array(struct vector *minuend,
			      struct vector * subtrahend)
{
  struct vector *vtmp;
  struct vector *difference;
  struct svalue *source,*dest;
  int i;

  vtmp=allocate_array(1);
  vtmp->item[0].type=T_ALIST_PART;
  if(subtrahend->ref==1)
  {
    vtmp->item[0].u.vec = subtrahend;
    subtrahend->ref++;
  }else{
    vtmp->item[0].u.vec = slice_array(subtrahend,0,subtrahend->size);
  }
  order_alist(vtmp);
  subtrahend = vtmp->item[0].u.vec;
  difference = allocate_array(minuend->size);
  check_vector_for_destruct(minuend);
  for (source = minuend->item, dest = difference->item, i = minuend->size;
       i--; source++)
  {
    if ( assoc(source, subtrahend) < 0 )
      assign_svalue_raw(dest++, source);
  }
  free_vector(vtmp);
  return shrink_array(difference, dest-difference->item);
}


/* Returns an array of all objects contained in 'ob'
 */
struct vector *all_inventory(struct object *ob)
{
  struct vector *d;
  struct object *cur;
  int cnt,res;
    
  cnt=0;
  for (cur=ob->contains; cur; cur = cur->next_inv)
    cnt++;
    
  if (!cnt)
    return allocate_array(0);

  d = allocate_array_no_init(cnt,0);
  cur=ob->contains;
    
  for (res=0;res<cnt;res++)
  {
    d->item[res].type=T_OBJECT;
    d->item[res].u.ob = cur;
    add_ref(cur,"all_inventory");
    cur=cur->next_inv;
  }
  return d;
}


/* Runs all elements of an array through ob::func
   and replaces each value in arr by the value returned by ob::func
   Hacking a little: if ob==0 then we call in the object in the array
                     instead.......
   */
struct vector *map_array(
	struct vector *p,
	struct svalue *lambda,
	struct svalue *extra,
	int num_extra_arg)
{
  struct vector *r;
  struct svalue *v;
  char *func=0; /* make gcc happy */
  int cnt,no;
    
  r=0;
  switch(lambda->type)
  {
  case T_STRING:
    func=strptr(lambda);
    break;

  case T_FUNCTION:
    if(lambda->u.ob->flags & O_DESTRUCTED)
      error("Invalid lambda to map_array.\n");
    break;

  case T_NUMBER:
    break;

  default:
    error("Bad arg to map_array.\n");
  }

  if (p->size<1) return allocate_array(0);
 
  r = allocate_array(p->size);
 
  for (cnt=0;cnt<p->size;cnt++)
  {
    check_eval_cost();
    switch(lambda->type)
    {
    case T_NUMBER:
      if(lambda->subtype!=NUMBER_NUMBER)
      {
	v=0;
	continue;
      }
      if(lambda->u.number<0)
      {
        if(p->item[cnt].type!=T_FUNCTION ||
	   (p->item[cnt].u.ob->flags & O_DESTRUCTED)) continue;
        for(no=0;no<num_extra_arg;no++) push_svalue(extra+no);
        v=apply_lambda(p->item+cnt,no,0);
      }else{
	if(p->item[cnt].type!=T_OBJECT ||
	   (p->item[cnt].u.ob->flags & O_DESTRUCTED)) continue;
	for(no=0;no<num_extra_arg;no++) push_svalue(extra+no);
	v=apply_lfun(lambda->u.number,p->item[cnt].u.ob,no,0);
      }
      break;

    case T_STRING:
      if(p->item[cnt].type!=T_OBJECT ||
	 (p->item[cnt].u.ob->flags & O_DESTRUCTED)) continue;
      for(no=0;no<num_extra_arg;no++) push_svalue(extra+no);
      v=apply_shared(func,p->item[cnt].u.ob,no,0);
      break;

    case T_FUNCTION:
      if(lambda->u.ob->flags & O_DESTRUCTED)
	error("object used by map_array destructed\n");
      push_svalue(&p->item[cnt]);
      for(no=0;no<num_extra_arg;no++) push_svalue(extra+no);
      v=apply_lambda(lambda,no+1,0);
      break;

    default:
      fatal("Fatal error in map_array.\n");
      return 0;
    }
    if(v) assign_svalue_no_free(&r->item[cnt], v);
  }
  return r;
}

int search_array(
	struct vector *p,
	struct svalue *lambda,
	struct svalue *extra,
	int num_extra_arg)
{
  struct svalue *v;
  char *func=0;  /* make gcc happy */
  int cnt,no;
    
  if(lambda->type==T_STRING)
  {
    func=strptr(lambda);
  }else if(lambda->type==T_FUNCTION){
    if(lambda->u.ob->flags & O_DESTRUCTED)
    {
      error("Invalid lambda to map_array.\n");
    }
  }else if(lambda->type==T_NUMBER){
    /* no action here */
  }else{
    error("Bad arg to map_array.\n");
  }

  if (p->size<1) return -1;
 
  for (cnt=0;cnt<p->size;cnt++)
  {
    check_eval_cost();
    switch(lambda->type)
    {
      case T_NUMBER:
        if(p->item[cnt].type!=T_FUNCTION ||
          (p->item[cnt].u.ob->flags & O_DESTRUCTED)) continue;
        for(no=0;no<num_extra_arg;no++) push_svalue(extra+no);
        v=apply_lambda(p->item+cnt,no,0);
        break;

      case T_STRING:
        if(p->item[cnt].type!=T_OBJECT ||
          (p->item[cnt].u.ob->flags & O_DESTRUCTED)) continue;
        for(no=0;no<num_extra_arg;no++) push_svalue(extra+no);
        v=apply_shared(func,p->item[cnt].u.ob,no,0);
        break;

      case T_FUNCTION:
        if(lambda->u.ob->flags & O_DESTRUCTED)
          error("object used by map_array destructed\n");
        push_svalue(&p->item[cnt]);
        for(no=0;no<num_extra_arg;no++) push_svalue(extra+no);
        v=apply_lambda(lambda,no+1,0);
        break;

      default:
	fatal("Fatal error in search_array.\n");
	return 0;
    }
    if(!IS_ZERO(v)) return cnt;
  }
  return -1;
}

static struct svalue *func;
static struct svalue *extra_arg;
static int num_extra_arg;

static int sort_array_cmp(struct svalue *p1,struct svalue *p2)
{
  struct svalue *d;
  int e;
  if(!func || func->type!=T_FUNCTION)
  {
    if(p1->type>p2->type) return 1;
    if(p1->type<p2->type) return -1;
    return is_gt(p1,p2);
  }

  if (func->u.ob->flags & O_DESTRUCTED)
    error("object used by sort_array destructed");

  push_svalue(p1);
  push_svalue(p2);
  for(e=0;e<num_extra_arg;e++)
  {
    push_svalue(extra_arg+e);
  }
  d = apply_lambda(func,num_extra_arg+2,0);
  if (!d) return 0;
  if(d->type!=T_NUMBER) return 1;
  return d->u.number;
}
typedef int (*cmpfuntyp) (const void *,const void *);

struct vector *sort_array(
	struct vector *v,
	struct svalue *fun,
	struct svalue *extra,
	int num)
{
  struct vector *outlist;
  int n;

  func=fun;
  extra_arg=extra;
  num_extra_arg=num;

  n=v->size;
  outlist=slice_array(v,0,n-1);
  msort(outlist->item,n,sizeof(struct svalue),(cmpfuntyp)sort_array_cmp);
  return outlist;
}

/*
 * deep_inventory()
 *
 * This function returns the recursive inventory of an object. The returned 
 * array of objects is flat, ie there is no structure reflecting the 
 * internal containment relations.
 *
 */
int number_deep_inv(struct object *ob)
{
  int num;
  num=1;
  for(ob=ob->contains;ob;ob=ob->next_inv)
    num+=number_deep_inv(ob);
  return num;
}

/* This one is faster, but the array is even more scrambled compared to
 * the normal deep_inventory() /Profezzorn
 */
struct vector *deep_inventory(struct object *ob,int take_top)
{
  int num;
  struct vector *res;
  struct svalue *curr,*next;
  num=number_deep_inv(ob);
  if(take_top)
  {
    res=allocate_array_no_init(num,0);
    add_ref(ob,"deep_inventory");
    res->item[0].type=T_OBJECT;
    res->item[0].u.ob=ob;
    next=curr=res->item+1;
  }else{
    res=allocate_array(num-1);
    curr=next=res->item;
  }
  for(;curr<=next;curr++)
  {
    for(ob=ob->contains;ob;ob=ob->next_inv)
    {
      add_ref(ob,"deep_inventory");
      next->type=T_OBJECT;
      next->u.ob=ob;
      next++;
    }
    ob=curr->u.ob;
  }
  return res;
}

INLINE int alist_cmp(struct svalue *p1,struct svalue *p2)
{
    register int d;

    if ((d = p1->type - p2->type)) return d;
    switch(p1->type)
    {
      case T_FLOAT:
        if(p1->u.fnum>p2->u.fnum) return 1;
        if(p1->u.fnum<p2->u.fnum) return -1;
        return 0;

      case T_FUNCTION:
        if ((d = p1->u.number - p2->u.number)) return d;
        if ((d = p1->subtype - p2->subtype)) return d;
        return 0;

      default:
        if(p1->u.number>p2->u.number) return 1;
        if(p1->u.number<p2->u.number) return -1;
        return 0;

    }
}

struct vector *globber;

int new_alist_cmp(int *a,int *b)
{
  return alist_cmp(globber->item+*a,globber->item+*b);
}

void order_alist(struct vector *inlist)
{
  int *tmp;
  struct vector *tmplist,*tmptmplist;
  int e,d;
  globber=inlist->item[0].u.vec;
  if(!globber->size) return;
  tmp=(int *)malloc(globber->size*sizeof(int));

  for(e=0;e<globber->size;e++) tmp[e]=e;
  fsort(tmp,globber->size,sizeof(int),(cmpfuntyp)new_alist_cmp);

  tmplist=allocate_array_no_init(globber->size,0);
  for(e=0;e<inlist->size;e++)
  {
    if(inlist->item[e].type==T_ALIST_PART)
    {
      tmptmplist=inlist->item[e].u.vec;
      for(d=0;d<tmplist->size;d++) tmplist->item[d]=tmptmplist->item[tmp[d]];
      inlist->item[e].u.vec=tmplist;
      tmplist=tmptmplist;
    }
  }
  /* for(e=0;e<tmplist->size;e++) tmplist->item[e].type=T_INVALID; */
  tmplist->size=0; /* instead of setting all the items in it to const0 */
  free_vector(tmplist);
  free((char *)tmp);
}


/* obs, destructive /Profezzorn */

struct vector *insert_alist(
	struct svalue *key,
	struct svalue *key_data,
	struct vector *list)
{
  int i,ix;
  int keynum;
  int insert;
  struct vector *tmp;
  struct svalue *sv;

#if 0
  for(i=0;i<list->size;i++)
  {
    if(list->item[i].type!=T_ALIST_PART) continue;
    sv= i ? key_data+i-1 : key;
    if(IS_TYPE(*sv,BT_VECTOR))
      if(check_for_circularity(sv->u.vec,list))
	error("Trying to make circular alist.\n");
  }
#endif

  keynum = list->item[0].u.vec->size;
  ix = search_alist(key,list->item[0].u.vec);
  insert=ix==keynum || alist_cmp(key, &list->item[0].u.vec->item[ix]);

  for (i=0; i < list->size; i++)
  {
    if(list->item[i].type==T_ALIST_PART)
    {
      tmp = list->item[i].u.vec;
#ifdef DEBUG
      if(tmp->size!=keynum)
	fatal("If this is an alist, then I'll be buggered.\n");
#endif
      if(insert)
      {
	sv= i ? key_data+i-1 : key;
	list->item[i].u.vec=insert_array(list->item[i].u.vec,ix,sv);
      }else{
        if(i)
	{
	  sv= key_data+i-1;
          assign_svalue(tmp->item+ix, sv);
	  tmp->types|=sv->type;
	}
      }
    }
  }
#ifdef DEBUG
  for(i=0;i<list->size;i++)
  {
    if(list->item[i].type==T_ALIST_PART && list->item[0].u.vec->size!=
       list->item[i].u.vec->size)
      fatal("Hmm, this is _not_ what I had in mind....\n");
  }
#endif
  return list;
}

struct vector *match_regexp(struct vector *v,regexp *reg)
{
  char *res;
  int i, num_match;
  struct vector *ret;
  extern int eval_cost;

  if (v->size == 0)
    return allocate_array(0);
  
  res = (char *)alloca(v->size);
  for (num_match=i=0; i < v->size; i++)
  {
    res[i] = 0;
    if (v->item[i].type != T_STRING)
      continue;
    eval_cost++;
    if (regexec(reg, strptr(v->item+i)) == 0)
      continue;
    res[i] = 1;
    num_match++;
  }
  ret = allocate_array_no_init(num_match,0);
  for (num_match=i=0; i < v->size; i++)
  {
    if (!res[i]) continue;
    assign_svalue_raw(&ret->item[num_match], &v->item[i]);
    num_match++;
  }
  return ret;
}

/*
 * Returns a list of all inherited files.
 *
 * Must be fixed so that any number of files can be returned, now max 256
 * (Sounds like a contradiction to me /Lars).
 * Now it can return any number of files. /Profezzorn
 */
struct vector *inherit_list(struct object *ob)
{
  struct vector *ret;
  struct program *pr;
  int e;

  pr=ob->prog;
  ret=allocate_array_no_init(pr->num_inherited,0);
  for(e=0;e<pr->num_inherited;e++)
  {
    SET_STR(ret->item+e,copy_shared_string(pr->inherit[e].prog->name));
  }
  return ret;
}

/*
 * When a vector is given as argument to an efun, all items has to be
 * checked if there would be an destructed object.
 * A bad problem currently is that a vector can contain another vector, so this
 * should be tested too. But, there is currently no prevention against
 * recursive vectors, which means that this can not be tested. Thus, the game
 * may crash if a vector contains a vector that contains a destructed object
 * and this top-most vector is used as an argument to an efun.
 */
/* The game won't crash when doing simple operations like assign_svalue
 * on a destructed object. You have to watch out, of course, that you don't
 * apply a function to it.
 * to save space it is preferable that destructed objects are freed soon.
 *   amylaar
 */

void donk_alist_item(struct vector *m,int i)
{
  int e,d,s;
  for(s=e=0;e<m->size;e++)
  {
    if(m->item[e].type!=T_ALIST_PART) continue;
    free_svalue(m->item[e].u.vec->item+i);
    for(d=i+1;d<m->item[e].u.vec->size;d++)
      m->item[e].u.vec->item[d-1]=m->item[e].u.vec->item[d];
    m->item[e].u.vec->size--;
  }
  total_array_size -= s*sizeof (struct svalue);
}

/* I'm a big beleiver in lazy evaluation algorithms */

void check_vector_for_destruct(struct vector *v)
{
  int types,e;
  /* if there are no objects or function, why check? */
  if(!(v->types & (BT_OBJECT | BT_FUNCTION)))
    return;

  types=0;
  for(e=v->size-1;e>=0;e--)
  {
    if(IS_TYPE(v->item[e],BT_OBJECT | BT_FUNCTION))
    {
      if(v->item[e].u.ob->flags & O_DESTRUCTED)
      {
	short tmp;
	tmp=v->item[e].type=T_OBJECT;
        free_svalue(v->item+e);
	v->item[e].subtype=tmp?NUMBER_DESTRUCTED_OBJECT:NUMBER_DESTRUCTED_FUNCTION;
      }
    }

    types|=1<<v->item[e].type;
  }
  v->types=types;
}

/* { 0,1,1,0,0,1 }
   { 1,0,0,1,1,0 }
*/
void check_alist_for_destruct(struct vector *v)
{
  int e,types;
  struct vector *w;
  check_vector_for_destruct(v);
  w=v->item[0].u.vec;

  if(w->types & (BT_OBJECT | BT_FUNCTION))
  {
    types=0;
    for(e=0;e<w->size;e++)
    {
      if(IS_TYPE(w->item[e],BT_OBJECT | BT_FUNCTION) &&
        (w->item[e].u.ob->flags & O_DESTRUCTED))
      {
        donk_alist_item(v,e);
        e--;
      }else{
        types|=1<<w->item[e].type;
      }
    }
    w->types=types;
  }

  for(e=1;e<v->size;e++)
    if(v->item[e].type==T_ALIST_PART)
      check_vector_for_destruct(v->item[e].u.vec);
}

/* check if v2 is in v */
int check_for_circularity(struct vector *v,struct vector *v2)
{
  int types,e;

  if(v==v2) return 1;
  /* if there are no vectors, why search? */
  if(!(v->types & BT_VECTOR))
    return 0;

  types=0;
  for(e=v->size-1;e>=0;e--)
  {
    if(IS_TYPE(v->item[e],BT_VECTOR))
      if(check_for_circularity(v->item[e].u.vec,v2))
        return 1;

    types|=1<<v->item[e].type;
  }

  /* if we reach this, types is updated.
   * as recursive arrays gives errors, we should come here
   * almost every time.
   */
  v->types=types;
  return 0;
}


struct vector *allocate_n_array(struct svalue *sp,int t)
{
  struct vector *v;
  int e;
  extern int eval_cost;
  if(sp->type!=T_NUMBER)
    error("Non-numeric array dimension.\n");
  v=allocate_array(sp->u.number);
  eval_cost+=sp->u.number*AVERAGE_COST;
  t--;
  v->types=BT_NUMBER;
  if(!t) return v;
  sp++;
  v->types=BT_POINTER;
  for(e=0;e<v->size && eval_cost<MAX_COST;e++)
  {
    v->item[e].type=T_POINTER;
    v->item[e].u.vec=allocate_n_array(sp,t);
  }
  return v;
}

struct vector *sum_arrays(
	int num_of_arrays,
	struct svalue *arrays,
	struct svalue *fun)
{
  int d,e,size;
  struct vector *res;
  struct svalue *ret;

  size=arrays->u.vec->size;

  res=allocate_array_no_init(size,0);
  for(e=0;e<size;e++)
  {
    for(d=0;d<num_of_arrays;d++)
    {
      if(arrays[d].u.vec->size>e)
        push_svalue(& arrays[d].u.vec->item[e]);
      else
        push_number(0);
    }
    if(fun->u.ob->flags & O_DESTRUCTED)
    {
      free_vector(res);
      error("Object used by sum_arrays destructed.\n");
    }
    ret=apply_lambda(fun,num_of_arrays,0);
    if(!ret)
    {
      free_vector(res);
      error("Function used by sum_arrays() not found.\n");
    }
    assign_svalue_no_free(& res->item[e],ret);
  }
  return res;
}


struct vector *file_stat(char *file,int raw)
{
  struct vector *tmp;
  struct stat    stbuf;  
  char          *p;

  p = file;
  if(!batch_mode)
    while(*p && *p == '/')
      p++;

  if(raw)
  {
    if(lstat(p, &stbuf) == -1)
      return 0;
  }else{
    if(stat(p, &stbuf) == -1)
      return 0;
  }
    
  tmp = allocate_array(5);
  
  tmp->item[0].u.number = stbuf.st_mode;
  tmp->item[1].u.number =
    (S_IFDIR == (S_IFMT & stbuf.st_mode)) ? -2 :
      (S_IFLNK == (S_IFMT & stbuf.st_mode)) ? -3 : stbuf.st_size;
  tmp->item[2].u.number = stbuf.st_atime;
  tmp->item[3].u.number = stbuf.st_mtime;
  tmp->item[4].u.number = stbuf.st_ctime;
  
  tmp->types=BT_NUMBER;
  return tmp;
}
