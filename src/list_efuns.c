#include "efuns.h"
#include "array.h"
#include "list.h"

void f_l_aggregate(int num_arg,struct svalue *argp)
{
  int i;
  struct vector *v, *w;

  v = allocate_array_no_init(num_arg,0);
  for (i=0; i < num_arg; i++)
  {
    v->item[i]=argp[i];
    SET_TO_ZERO(argp[i]);
  }
  sp-=num_arg;
  w=allocate_array_no_init(1,0);
  w->item[0].type=T_ALIST_PART;
  w->item[0].u.vec=v;
  order_alist(w);
  push_list(w);
  w->ref--;
}

void f_m_list(int num_arg,struct svalue *argp)
{
  struct vector *v;

  v = sp->u.vec;
  check_alist_for_destruct(v);
  v = v->item[0].u.vec;
  v = allocate_list(slice_array(v, 0, v->size - 1));
  pop_stack();
  push_list(v);
  v->ref--;			/* Will make ref count == 1 */
}

void f_l_delete(int num_arg,struct svalue *argp)
{
  int i;
  check_alist_for_destruct(sp[-1].u.vec);
  i=assoc(sp,sp[-1].u.vec->item[0].u.vec);
  if(i>=0)  donk_alist_item(sp[-1].u.vec,i);
  pop_stack();
}

void f_mklist(int num_arg,struct svalue *argp)
{
  struct vector *v;
  check_vector_for_destruct(sp->u.vec);
  v=mklist(sp->u.vec);
  pop_stack();
  push_list(v);
  v->ref--;
}
