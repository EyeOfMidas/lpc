#include "efuns.h"
#include "array.h"
#include "mapping.h"
#include "simulate.h"

void f_m_aggregate(int num_arg,struct svalue *argp)
{
  int i;
  struct vector *v, *w;

  if(num_arg%2)
    error("Odd number of arguments to m_aggregate.\n");
  num_arg/=2;
  v = allocate_array_no_init(num_arg,0);
  w = allocate_array_no_init(num_arg,0);
  for (i=0; i < num_arg; i ++)
  {
    v->item[i]=*argp;
#ifdef WARN
    argp->type=2000;
#endif
    argp++;
    w->item[i]=*argp;
#ifdef WARN
    argp->type=2000;
#endif
    argp++;
  }
  sp-=num_arg*2;
  v = allocate_mapping(v, w);
  order_alist(v);
  push_mapping(v);
  v->ref--;
}

void f_m_cleanup(int num_arg,struct svalue *argp)
{
  struct vector *v;
  extern struct vector *cleanup_map(struct vector *);
  v=cleanup_map(argp->u.vec);
  pop_stack();
  push_mapping(v);
  v->ref--;
}

void f_sum_mappings(int num_arg,struct svalue *argp)
{
  struct vector *v;
  extern struct vector *sum_mappings(struct vector *,struct vector *,struct svalue *,struct svalue *,struct svalue *);

  if(!IS_TYPE(argp[2],BT_FUNCTION | BT_NUMBER)) bad_arg(2,F_SUM_MAPPINGS);
  if(!IS_TYPE(argp[3],BT_FUNCTION | BT_NUMBER)) bad_arg(3,F_SUM_MAPPINGS);
  if(!IS_TYPE(argp[4],BT_FUNCTION | BT_NUMBER)) bad_arg(4,F_SUM_MAPPINGS);

  v=sum_mappings(argp[0].u.vec,argp[1].u.vec,argp+2,argp+3,argp+4);
  pop_n_elems(5);
  push_mapping(v);
  v->ref--;
}

void f_m_set_default(int num_arg,struct svalue *argp)
{
  map_set_default(argp[0].u.vec,argp+1);
  pop_stack();
}

void f_m_get_default(int num_arg,struct svalue *argp)
{
  struct vector *v;
  v=sp->u.vec;
  v->ref++;
  assign_svalue(sp,v->item+2);
  free_vector(v);
}

void f_filter_mapping(int num_arg,struct svalue *argp)
{
  struct vector *v,*res,*m;
  int e,s;

  m=argp[0].u.vec;
  check_alist_for_destruct(m);
  res = map_array(m->item[1].u.vec,argp+1,argp+2,num_arg-2);

  for(s=e=0;e<res->size;e++)
    if(res->item[e].type!=T_NUMBER || res->item[e].u.number!=0) s++;

  v = allocate_mapping(allocate_array_no_init(s,0),allocate_array_no_init(s,0));

  for(s=e=0;e<res->size;e++)
  {
    if(res->item[e].type!=T_NUMBER || res->item[e].u.number!=0)
    {
      assign_svalue_no_free(v->item[0].u.vec->item+s,m->item[0].u.vec->item+e);
      assign_svalue_no_free(v->item[1].u.vec->item+s,m->item[1].u.vec->item+e);
      s++;
    }
  }
  free_vector(res);
  pop_n_elems(num_arg);
  push_mapping(v);		/* This will make ref count == 2 */
  v->ref--;
}

void f_mkmapping(int num_arg,struct svalue *argp)
{
  struct vector *v;
  int i;

  i = (sp-1)->u.vec->size;
  if (i > sp->u.vec->size)
  {
    i = sp->u.vec->size;
  }
  v = allocate_mapping(slice_array((sp-1)->u.vec, 0, i - 1),
		       slice_array(sp->u.vec, 0, i - 1));
  pop_n_elems(2);
  order_alist(v);
  push_mapping(v);
  v->ref--;
}

void f_indices(int num_arg,struct svalue *argp)
{
  struct vector *v;
  switch(sp->type)
  {
  default:
    bad_arg(1,F_INDICES);
    break;
   
  case T_LIST:
  case T_MAPPING:
    v=sp->u.vec;
    check_alist_for_destruct(v);
    v=v->item[0].u.vec;
    v=slice_array(v,0,v->size-1);
    pop_stack();
    push_vector(v);
    v->ref--;
  }
}

void f_m_indices(int num_arg,struct svalue *argp)
{
  struct vector *v;

  v = sp->u.vec;
  check_alist_for_destruct(v);
  v = v->item[0].u.vec;
  v = slice_array(v, 0, v->size - 1);
  pop_stack();
  push_vector(v);
  v->ref--;			/* Will make ref count == 1 */
}

void f_m_values(int num_arg,struct svalue *argp)
{
  struct vector *v;

  v = sp->u.vec;
  check_alist_for_destruct(v);
  v = v->item[1].u.vec;
#if 0
  v = slice_array(v, 0, v->size - 1);
  pop_stack();
  push_vector(v);
  v->ref--;			/* Will make ref count == 1 */
#else
  v->ref++;
  pop_stack();
  push_vector(v);
  v->ref--;
#endif
}

void f_m_delete(int num_arg,struct svalue *argp)
{
  struct vector *v;
  check_alist_for_destruct((sp-1)->u.vec);
  v=remove_mapping2((sp-1)->u.vec,sp);
  pop_stack();
}

void f_map_mapping(int num_arg,struct svalue *argp)
{
  struct vector *res;

  check_eval_cost();

  check_alist_for_destruct(argp[0].u.vec);
  res = map_array(argp[0].u.vec->item[1].u.vec,argp+1,argp+2,num_arg-2);
  res = allocate_mapping(argp[0].u.vec->item[0].u.vec,res);
  argp[0].u.vec->item[0].u.vec->ref++;
  pop_n_elems(num_arg);
  push_mapping(res);
  res->ref--;
}


void f_solidify_mapping(int num_arg,struct svalue *argp)
{
  int e,num,num2;
  struct vector *v,*ind,*val,*ret_ind,*ret_val,*data;
  v=argp[0].u.vec;
  check_alist_for_destruct(v);
  ind=v->item[0].u.vec;
  val=v->item[1].u.vec;
  if(ind->size)
  {
    num=1;
    for(e=0;e<ind->size-1;e++)
      if(alist_cmp(ind->item+e,ind->item+e+1))
	num++;
  }else{
    num=0;
  }

  ret_ind=allocate_array_no_init(num,0);
  ret_val=allocate_array_no_init(num,0);
  num=0;
  for(e=0;e<ind->size;)
  {
    assign_svalue_raw(ret_ind->item+num,ind->item+e);
    for(num2=0;num2+e<ind->size-1 &&
	!alist_cmp(ind->item+e+num2,ind->item+e+num2+1);num2++);
    data=allocate_array_no_init(num2+1,0);
    copy_svalues_raw(data->item,val->item+e,num2+1);
    e+=num2+1;
    ret_val->item[num].u.vec=data;
    ret_val->item[num].type=T_POINTER;
    num++;
  }
  pop_stack();
  v=allocate_mapping(ret_ind,ret_val);
  push_mapping(v);
  v->ref--;
}
