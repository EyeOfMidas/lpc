#include <math.h>
#include "efuns.h"
#include "list.h"
#include "array.h"
#include "main.h"
#include "stralloc.h"
#include "opcodes.h"
#include "simulate.h"
#include "operators.h"

INLINE int is_eq(const struct svalue *a,const struct svalue *b)
{
  if (a->type != b->type) return 0;
  switch(a->type)
  {
  case T_REGEXP:
  case T_NUMBER:
  case T_LIST:
  case T_POINTER:
  case T_OBJECT:
  case T_MAPPING:
  case T_ALIST_PART:
    return a->u.number == b->u.number;

  case T_STRING:
    return a->u.string == b->u.string;

  case T_FUNCTION:
    return (a->subtype==b->subtype && a->u.ob==b->u.ob);
      
  case T_FLOAT:
    return a->u.fnum == b->u.fnum;

  default:
    fatal("Unknown type %x\n",a->type);
    return 0; /* make gcc happy */
  }
}

struct processing
{
  struct processing *next;
  struct vector *v;
};

int low_is_equal(const struct svalue *a,const struct svalue *b,struct processing *p)
{
  struct svalue *ap,*bp;
  int e;
  struct processing curr;

  if(a->type!=b->type) return 0;
  if(is_eq(a,b)) return 1;
  switch(a->type)
  {
    case T_REGEXP:
    case T_NUMBER:
    case T_STRING:
    case T_OBJECT:
    case T_FLOAT:
    case T_FUNCTION:
      return 0;

    case T_POINTER:
    case T_ALIST_PART:
    case T_MAPPING:
    case T_LIST:
      if((e=a->u.vec->size)!=b->u.vec->size) return 0;
      curr.next=p;
      curr.v=a->u.vec;
      for(;p;p=p->next) if(p->v==a->u.vec) return 1;
      ap=a->u.vec->item;
      bp=b->u.vec->item;
      for(;e>0;e--) if(!is_equal(ap++,bp++)) return 0;
      break;

    default:
      fatal("Unknown type in is_equal.\n");
  }
  return 1; /* survived */
}

INLINE int is_equal(const struct svalue *a,const struct svalue *b)
{
  return low_is_equal(a,b,0);
}

INLINE int is_gt(const struct svalue *a,const struct svalue *b)
{
  if (a->type != b->type)
  {
    error("Cannot compare different types.\n");
  }
  switch(a->type)
  {
  default:
    error("Bad argument 1 tp '<'.\n");

  case T_NUMBER:
    return a->u.number > b->u.number;

  case T_STRING:
    return my_strcmp(a,b)>0;

  case T_FLOAT:
    return a->u.fnum > b->u.fnum;
  }
}

INLINE int is_lt(const struct svalue *a,const struct svalue *b)
{
  if (a->type != b->type)
  {
    error("Cannot compare different types.\n");
  }
  switch(a->type)
  {
  default:
    error("Bad arg 1 to '<='.\n");

  case T_NUMBER:
    return a->u.number < b->u.number;

  case T_STRING:
    return my_strcmp(a, b)<0;

  case T_FLOAT:
    return a->u.fnum < b->u.fnum;

  }
}

INLINE void f_eq()
{
  int i;
  i=is_eq(sp-1,sp);
  pop_stack();
  pop_stack();
  push_number(i);
}

INLINE void f_ne()
{
  int i;
  i=!is_eq(sp-1,sp);
  pop_stack();
  pop_stack();
  push_number(i);
}

INLINE void f_lt()
{
  int i;
  i=is_lt(sp-1,sp);
  pop_stack();
  pop_stack();
  push_number(i);
}

INLINE void f_gt()
{
  int i;
  i=is_gt(sp-1,sp);
  pop_stack();
  pop_stack();
  push_number(i);
}

INLINE void f_ge()
{
  int i;
  i=!is_lt(sp-1,sp);
  pop_stack();
  pop_stack();
  push_number(i);
}

INLINE void f_le()
{
  int i;
  i=!is_gt(sp-1,sp);
  pop_stack();
  pop_stack();
  push_number(i);
}


#ifdef ADD_CACHE_SIZE
struct add_cache_entry add_cache[ADD_CACHE_SIZE];

void clean_add_cache(void)
{
  int e;
  for(e=0;e<ADD_CACHE_SIZE;e++)
  {
    if(add_cache[e].hits) continue;
    if(!add_cache[e].a) continue;
    free_string(add_cache[e].a);
    free_string(add_cache[e].b);
    free_string(add_cache[e].sum);
    add_cache[e].a=0;
    add_cache[e].b=0;
    add_cache[e].sum=0;
    add_cache[e].hits=0;
  }
}

void free_add_cache(void)
{
  int e;
  for(e=0;e<ADD_CACHE_SIZE;e++)
  {
    if(!add_cache[e].a) continue;
    free_string(add_cache[e].a);
    free_string(add_cache[e].b);
    free_string(add_cache[e].sum);
    add_cache[e].a=0;
    add_cache[e].b=0;
    add_cache[e].sum=0;
    add_cache[e].hits=0;
  }
}

#endif


void f_sum(int num_arg,struct svalue *argp)
{
  int e,size;
  unsigned short types;

  for(types=e=0;e<num_arg;e++) types|=1<<argp[e].type;
    
  switch(types)
  {
  default:
    if(num_arg)
      if(argp[0].type==T_OBJECT || argp[e].type==T_NUMBER)
	bad_arg(1,F_SUM);
    error("Incompatible types to sum() or +\n");
    return; /* compiler hint */

  case BT_STRING:
  {
    char *buf,*str;
#ifdef ADD_CACHE_SIZE
    struct add_cache_entry *h;
#endif

    if(num_arg==1) return;
    for(size=e=0;e<num_arg;e++) size+=my_strlen(argp+e);

#ifdef ADD_CACHE_SIZE
    if(num_arg==2 && size<=ADD_CACHE_MAX_LENGTH)
    {
      /* oh my, it looks like lisp!  */
      h=add_cache+( (((unsigned int)strptr(argp))+(((unsigned int)(strptr(argp+1)))>>2)) % ADD_CACHE_SIZE );
      if(h->a==strptr(argp) && h->b==strptr(argp+1))
      {
	h->hits++;
	buf=copy_shared_string(h->sum);
      }else{
	if(h->a)
	{
	  free_string(h->a);
	  free_string(h->b);
	  free_string(h->sum);
	}
	buf=begin_shared_string(size);
	MEMCPY(buf,strptr(argp),my_strlen(argp));
	MEMCPY(buf+my_strlen(argp),strptr(argp+1),my_strlen(argp+1));
	buf=end_shared_string(buf);
	h->a=copy_shared_string(strptr(argp));
	h->b=copy_shared_string(strptr(argp+1));
	h->sum=copy_shared_string(buf);
	h->hits=0;
      }
    }else{
#endif
      buf=str=begin_shared_string(size);
      for(e=0;e<num_arg;e++)
      {
	MEMCPY(buf,strptr(argp+e),my_strlen(argp+e));
	buf+=my_strlen(argp+e);
      }
      buf=end_shared_string(str);
#ifdef ADD_CACHE_SIZE
    }
#endif
    for(e=0;e<num_arg;e++)
    {
      free_string(strptr(argp+e));
#ifdef WARN
      argp[e].type=2000;
#endif
    }
    sp-=num_arg;
    push_shared_string(buf);
    break;
  }

  case BT_STRING | BT_NUMBER:
  case BT_STRING | BT_FLOAT:
  case BT_STRING | BT_FLOAT | BT_NUMBER:
  {
    char *buf,*str;
    for(size=e=0;e<num_arg;e++)
    {
      switch(argp[e].type)
      {
      case T_STRING:
	size+=my_strlen(argp+e);
	break;

      case T_INT:
	size+=14;
	break;

      case T_FLOAT:
	size+=22;
	break;
      }
    }
    str=buf=malloc(size+1);
    size=0;
    for(size=e=0;e<num_arg;e++)
    {
      switch(argp[e].type)
      {
      case T_STRING:
	MEMCPY(buf,strptr(argp+e),my_strlen(argp+e));
	buf+=my_strlen(argp+e);
	break;

      case T_INT:
	sprintf(buf,"%d",argp[e].u.number);
	buf+=strlen(buf);
	break;

      case T_FLOAT:
	sprintf(buf,"%f",argp[e].u.fnum);
	buf+=strlen(buf);
	break;
      }
    }
    buf=make_shared_binary_string(str,buf-str);
    free(str);
    pop_n_elems(num_arg);
    push_shared_string(buf);
    break;
  }

    case BT_NUMBER:
    for(size=e=0;e<num_arg;e++) size+=argp[e].u.number;
    sp-=num_arg-1;
    sp->u.number=size;
    break;

    case BT_FLOAT:
    {
      float sum;
      sum=0.0;
      for(e=0;e<num_arg;e++) sum+=argp[e].u.fnum;
      sp-=num_arg-1;
      sp->u.fnum=sum;
    }
    break;

    case BT_POINTER:
    {
      struct vector *v;
      for(size=e=0;e<num_arg;e++)
      {
	check_vector_for_destruct(argp[e].u.vec);
        size+=argp[e].u.vec->size;
      }
      if(argp[0].u.vec->ref==1)
      {
	e=argp[0].u.vec->size;
	v=resize_array(argp[0].u.vec,size);
	SET_TO_ZERO(argp[0]);
	size=e;
	e=1;
      }else{
	v=allocate_array_no_init(size,0);
	v->types=0;
	e=size=0;
      }
      for(;e<num_arg;e++)
      {
	v->types|=argp[e].u.vec->types;
	copy_svalues_raw(v->item+size,argp[e].u.vec->item,argp[e].u.vec->size);
	size+=argp[e].u.vec->size;
      }
      pop_n_elems(num_arg);
      push_vector(v);
      v->ref--;
    }
    break;

    case BT_LIST:
    {
      struct vector *v,*w;
      struct svalue *r;
      int d;
      for(size=e=0;e<num_arg;e++)
      {
	check_alist_for_destruct(argp[e].u.vec);
        size+=argp[e].u.vec->size;
      }
      if(num_arg==2)
      {
	w = do_array_surgery(argp[0].u.vec,argp[1].u.vec,OPER_ADD,T_LIST);
      }else{
        v=allocate_array_no_init(size,0);
        for(size=e=0;e<num_arg;e++)
        {
          r=argp[e].u.vec->item[0].u.vec->item;
          for(d=0;d<argp[e].u.vec->size;d++)
          {
            assign_svalue_raw(v->item+size,r+d);
            size++;
          }
        }
        w=allocate_array_no_init(1,0);
        w->item[0].u.vec=v;
        w->item[0].type=T_ALIST_PART;
        order_alist(w);
      }
      pop_n_elems(num_arg);
      push_list(w);
      w->ref--;
    }
    break;

    case BT_MAPPING:
    {
      struct vector *v,*values,*w;
      struct svalue *r,*rr;
      int d;
      for(size=e=0;e<num_arg;e++)
      {
	check_alist_for_destruct(argp[e].u.vec);
        size+=argp[e].u.vec->size;
      }
      if(num_arg==2)
      {
	w = do_array_surgery(argp[0].u.vec,argp[1].u.vec,OPER_ADD,T_MAPPING);
      }else{
        v=allocate_array_no_init(size,0);
        values=allocate_array_no_init(size,0);
        for(size=e=0;e<num_arg;e++)
        {
          r=argp[e].u.vec->item[0].u.vec->item;
          rr=argp[e].u.vec->item[1].u.vec->item;
          for(d=0;d<argp[e].u.vec->size;d++)
          {
            assign_svalue_raw(v->item+size,r+d);
            assign_svalue_raw(values->item+size,rr+d);
            size++;
          }
        }
        w=allocate_array_no_init(2,0);
        w->item[0].u.vec=v;
        w->item[0].type=T_ALIST_PART;
        w->item[1].u.vec=values;
        w->item[1].type=T_ALIST_PART;
        order_alist(w);
      }
      pop_n_elems(num_arg);
      push_mapping(w);
      w->ref--;
    }
    break;
  }
}

INLINE void f_add() { f_sum(2,sp-1); }

void f_subtract()
{
  if ((sp-1)->type != sp->type )
    error("Subtract on different types.\n");

  switch(sp->type)
  {
  case T_POINTER:
  {
    extern struct vector *subtract_array
      PROT((struct vector *,struct vector*));
    struct vector *v;

    /* subtract_array already takes care of destructed objects */
    check_vector_for_destruct(sp[-1].u.vec);
    check_vector_for_destruct(sp[0].u.vec);
    if(sp[0].u.vec->types & sp[-1].u.vec->types)
    {
      v = subtract_array((sp-1)->u.vec, sp->u.vec);
    }else{
      v=slice_array(sp[-1].u.vec,0,sp[-1].u.vec->size-1);
    }
    pop_stack();
    pop_stack();
    push_vector(v);
    v->ref--;
    return;
  }
  case T_LIST:
  case T_MAPPING:
  {
    struct vector *v;
    int type=sp->type;

    v=do_array_surgery((sp-1)->u.vec,sp->u.vec,OPER_SUB,type);
    pop_stack();
    pop_stack();
    push_vector(v);
    v->ref--;
    sp->type=type;
    return;
  }
  case T_FLOAT:
    sp--;
    sp->u.fnum=sp[0].u.fnum-sp[1].u.fnum;
    return;

  case T_NUMBER:
    sp--;
    sp->u.number = sp[0].u.number - sp[1].u.number;
    return;

  case T_STRING:
    push_svalue(&const_empty_string);
    f_replace(3,sp-2);
    return;

  default:
    bad_arg(1,F_SUBTRACT);
  }
}

void f_and()
{
  if(sp->type == (sp-1)->type && 
     IS_TYPE(*sp,BT_POINTER | BT_LIST | BT_MAPPING))
   {
    struct vector *v;
    int type=sp->type;

    check_vector_for_destruct(sp[-1].u.vec);
    check_vector_for_destruct(sp[0].u.vec);
    if(sp[-1].u.vec->types & sp[0].u.vec->types)
    {
      v=do_array_surgery((sp-1)->u.vec,sp->u.vec,OPER_AND,type);
    }else{
      v=allocate_array(0);
    }
    pop_stack();
    pop_stack();
    push_vector(v);
    v->ref--;
    sp->type=type;
    return;
  }

  if ((sp-1)->type != T_NUMBER) bad_arg(1,F_AND);
  if (sp->type != T_NUMBER) bad_arg(2,F_AND);
  sp--;
  sp->u.number = sp[0].u.number & sp[1].u.number;
}

void f_or()
{
  if(sp->type == (sp-1)->type &&
     IS_TYPE(*sp,BT_POINTER | BT_LIST | BT_MAPPING))
  {
    struct vector *v;
    int type=sp->type;

    check_vector_for_destruct(sp[-1].u.vec);
    check_vector_for_destruct(sp[0].u.vec);
    if(sp[-1].u.vec->types & sp[0].u.vec->types)
    {
      v=do_array_surgery((sp-1)->u.vec,sp->u.vec,OPER_OR,type);
    }else{
      f_add();
      return;
    }
    pop_stack();
    pop_stack();
    push_vector(v);
    v->ref--;
    sp->type=type;
    return;
  }

  if ((sp-1)->type != T_NUMBER) bad_arg(1,F_OR);
  if (sp->type != T_NUMBER) bad_arg(2,F_OR);
  sp--;
  sp->u.number = sp[0].u.number | sp[1].u.number;
}

void f_xor()
{
  if(sp->type == (sp-1)->type &&
     IS_TYPE(*sp,BT_POINTER | BT_LIST | BT_MAPPING ))
  {
    struct vector *v;
    int type=sp->type;

    check_vector_for_destruct(sp[-1].u.vec);
    check_vector_for_destruct(sp[0].u.vec);
    if(sp[-1].u.vec->types & sp[0].u.vec->types)
    {
      v=do_array_surgery((sp-1)->u.vec,sp->u.vec,OPER_XOR,type);
    }else{
      f_add();
      return;
    }
    pop_stack();
    pop_stack();
    push_vector(v);
    v->ref--;
    sp->type=type;
    return;
  }

  if ((sp-1)->type != T_NUMBER) bad_arg(1,F_XOR);
  if (sp->type != T_NUMBER) bad_arg(2,F_XOR);
  sp--;
  sp->u.number = sp[0].u.number ^ sp[1].u.number;
}

void f_lsh()
{
  if ((sp-1)->type != T_NUMBER) bad_arg(1,F_LSH);
  if (sp->type != T_NUMBER) bad_arg(2,F_LSH);
  sp--;
  sp->u.number = sp[0].u.number << sp[1].u.number;
}

void f_rsh()
{
  if ((sp-1)->type != T_NUMBER) bad_arg(1,F_RSH);
  if (sp->type != T_NUMBER) bad_arg(2,F_RSH);
  sp--;
  sp->u.number = sp[0].u.number >> sp[1].u.number;
}

void f_multiply()
{
  switch(sp[-1].type)
  {
  case T_POINTER:
    if(sp->type!=T_STRING) bad_arg(2,F_MULTIPLY);
    f_implode(2,sp-1);
    return;

  case T_FLOAT:
    if(sp->type!=T_FLOAT) bad_arg(2,F_MULTIPLY);
    sp--;
    sp->u.fnum=sp[0].u.fnum*sp[1].u.fnum;
    return;

  case T_INT:
    if(sp->type!=T_INT) bad_arg(2,F_MULTIPLY);
    sp--;
    sp->u.number=sp[0].u.number*sp[1].u.number;
    return;

  default:
    bad_arg(1,F_MULTIPLY);
    
  }
}

void f_divide()
{
  if(sp[-1].type!=sp[0].type) bad_arg(2,F_DIVIDE);
  switch(sp[0].type)
  {
  case T_STRING:
    f_explode(2,sp-1);
    return;

  case T_FLOAT:
    if(sp->u.fnum==0.0)
      error("Division by zero.\n");
    sp--;
    sp->u.fnum = sp[0].u.fnum / sp[1].u.fnum;
    return;

  case T_NUMBER:
    if (sp->u.number == 0)
      error("Division by zero\n");
    sp--;
    sp->u.number = sp[0].u.number / sp[1].u.number;
    return;
    
  default:
    bad_arg(1,F_DIVIDE);
  }
}

void f_mod()
{
  if(sp[-1].type!=sp[0].type) bad_arg(2,F_MOD);

  if (sp->type == T_FLOAT)
  {
    float foo;
    if(sp->u.fnum==0.0)
      error("Modulo by zero.\n");
    foo=(sp-1)->u.fnum / sp->u.fnum;
    foo=(sp-1)->u.fnum-sp->u.fnum*floor(foo);
    sp--;
    sp->u.fnum=foo;
    return;
  }
  if (sp->type != T_NUMBER) bad_arg(1,F_MOD);
  if (sp->u.number == 0)
    error("Modulus by zero.\n");
  sp--;
  sp->u.number = sp[0].u.number % sp[1].u.number;
}

void f_not()
{
  if(sp->type==T_FLOAT)
  {
    pop_push_conditional(sp->u.fnum==0.0);
    return;
  } 
  pop_push_conditional(sp->type == T_NUMBER && sp->u.number == 0);
}

void f_compl()
{
  if (sp->type != T_NUMBER)
    error("Bad argument to ~\n");
  sp->u.number = ~ sp->u.number;
}

void f_negate()
{
  if(sp->type==T_FLOAT)
  {
    sp->u.fnum=-sp->u.fnum;
    return;
  }
  if (sp->type != T_NUMBER)
    error("Bad argument to unary minus\n");
  sp->u.number = - sp->u.number;
}

void f_inc()
{
  int *i;
  if (sp->type != T_LVALUE)  error("Bad argument to ++\n");
  i=lvalue_to_intp(sp->u.lvalue,"++ of non-integer variable.\n");
  
  if(i)
  {
    i[0]++;
    pop_stack();
    push_number(*i);
  }else{
    sp++;
    lvalue_to_svalue_no_free(sp,sp[-1].u.lvalue);
    push_number(1);
    f_add();
    f_assign();
  }
}
void f_inc_and_pop() { f_inc(); pop_stack(); }

void f_dec()
{
  int *i;
  if (sp->type != T_LVALUE)  error("Bad argument to --\n");
  i=lvalue_to_intp(sp->u.lvalue,"-- of non-integer variable.\n");
  
  if(i)
  {
    i[0]--;
    pop_stack();
    push_number(*i);
  }else{
    sp++;
    lvalue_to_svalue_no_free(sp,sp[-1].u.lvalue);
    push_number(-1);
    f_add();
    f_assign();
  }
}

void f_dec_and_pop() { f_dec(); pop_stack(); }
 

void f_post_inc()
{
  int *i;
  if (sp->type != T_LVALUE)  error("Bad argument to ++\n");
  i=lvalue_to_intp(sp->u.lvalue,"++ of non-integer variable.\n");
  
  if(i)
  {
    int t;
    t=i[0]++;
    pop_stack();
    push_number(t);
  }else{
    sp++;
    lvalue_to_svalue_no_free(sp,sp[-1].u.lvalue);
    f_swap();
    sp++;
    assign_svalue_no_free(sp,sp-2);
    push_number(1);
    f_add();
    f_assign_and_pop();
  }
}

void f_post_dec()
{
  int *i;
  if (sp->type != T_LVALUE)  error("Bad argument to --\n");
  i=lvalue_to_intp(sp->u.lvalue,"-- of non-integer variable.\n");
  
  if(i)
  {
    int t;
    t=i[0]--;
    pop_stack();
    push_number(t);
  }else{
    sp++;
    lvalue_to_svalue_no_free(sp,sp[-1].u.lvalue);
    f_swap();
    sp++;
    assign_svalue_no_free(sp,sp-2);
    push_number(-1);
    f_add();
    f_assign_and_pop();
  }
}

void f_assign()
{
#ifdef DEBUG
  if (sp[-1].type != T_LVALUE)
    error("Bad argument to F_ASSIGN\n");
#endif
  assign(sp[-1].u.lvalue,sp);
  free_svalue(sp-1);
  sp[-1]=*sp;
  SET_TO_ZERO(*sp);
  sp--;
}

void f_assign_and_pop()
{
#ifdef DEBUG
  if (sp[-1].type != T_LVALUE)
    error("Bad argument to F_ASSIGN\n");
#endif
  assign(sp[-1].u.lvalue,sp);
  pop_stack();
  pop_stack();
}

void f_is_equal(int num_arg,struct svalue *argp)
{
  int i;
  i=is_equal(argp,argp+1);
  pop_stack();
  pop_stack();
  push_number(i);
}
