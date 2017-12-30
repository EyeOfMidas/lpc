#include "efuns.h"

#ifdef HAVE_SYS_RUSAGE
#include <sys/rusage.h>
#endif

#ifdef GETRUSAGE_THROUGH_PROCFS
#include <sys/types.h>
#include <sys/procfs.h>
#endif

#include "stralloc.h"
#include "array.h"
#include "simulate.h"
#include "save_objectII.h"
#include "operators.h"
#include "main.h"

void f_aggregate(int num_arg,struct svalue *argp)
{
  int i;
  struct vector *v;
  v = allocate_array_no_init(num_arg,0);
#if 0
  for (i=0; i < num_arg; i++) v->item[i]=argp[i];
  sp-=num_arg-1;
#else
  for (i=0; i < num_arg; i++)
  {
    v->item[i]=argp[i];
#ifdef WARN
    argp[i].type=2000;
#endif
  }
  sp-=num_arg-1;
#endif
  sp->type = T_POINTER;
  sp->u.vec = v;		/* Ref count already initialized */
}

void f_sum_arrays(int num_arg,struct svalue *argp)
{
  struct svalue *s;
  struct vector *v;
  extern struct vector *sum_arrays(int,struct svalue *,struct svalue *);
  int values,e;
  s=argp+1;
  values=num_arg-1;
  for(e=0;e<values;e++) if(s[e].type!=T_POINTER) bad_arg(e+1,F_SUM_ARRAYS);
  v=sum_arrays(values,s,argp);
  pop_n_elems(num_arg);
  push_vector(v);
  v->ref--;
}

void string_replace(struct svalue *str,struct svalue *del,struct svalue *to)
{
  char *str_s,*del_s,*to_s;
  int str_l,del_l,to_l;
  int e,num;
  char *buff,*q;

  /* find number of delimeters */

  str_s=strptr(str);
  del_s=strptr(del);
  to_s=strptr(to);

  str_l=my_strlen(str);
  del_l=my_strlen(del);
  to_l=my_strlen(to);

  
  for (num=e=0; e<str_l;)
  {
    if (e+del_l<=str_l && !MEMCMP(str_s+e, del_s, del_l))
    {
      num++;
      e+=del_l?del_l:1;
    }else{
      e+=1;
    }
  }

  if(!num) return;

  q = buff = begin_shared_string(str_l+num*(to_l-del_l));

  for (e=0;e<str_l;)
  {
    if(e+del_l<=str_l && !MEMCMP(str_s+e, del_s, del_l))
    {
      MEMCPY(q,to_s,to_l);
      e += del_l?del_l:1;
      q += to_l;
    } else {
      *(q++)=str_s[e++];
    }
  }
  buff=end_shared_string(buff);
  free_svalue(str);
  SET_STR(str,buff);
}

struct tupel
{
  struct svalue ind,val;
};

typedef int (*cmpfuntyp) (const void *,const void *);

void replace_many(struct svalue *str,struct vector *from,struct vector *to)
{
  extern int batch_mode;
  extern int max_eval_cost;
  char *str_s;
  int str_l;
  struct tupel *v;
  int e,a,b,c;
  char set[256];

  str_s=strptr(str);
  str_l=my_strlen(str);

  if(from->size!=to->size)
    error("replace must have equal-sized from and to arrays.\n");
  eval_cost+=from->size;
  v=(struct tupel *)xalloc(sizeof(struct tupel)*from->size);
  for(e=0;e<256;e++) set[e]=0;
  for(e=0;e<from->size;e++)
  {
    if(from->item[e].type!=T_STRING)
      error("replace: from array not string *.\n");
    if(to->item[e].type!=T_STRING)
      error("replace: to array not string *.\n");
    set[EXTRACT_UCHAR(strptr(from->item+e))]=1;
    v[e].ind=from->item[e];
    v[e].val=to->item[e];
  }
  msort(v,from->size,sizeof(struct tupel),(cmpfuntyp)my_strcmp);

  init_buf();
  for(e=0;e<str_l;)
  {
    if(set[EXTRACT_UCHAR(str_s+e)])
    {
      eval_cost++;
      if(!batch_mode && eval_cost>max_eval_cost)
      {
	free((char *)v);
	error("Eval cost to big in replace.\n");
      }
      a=0;
      b=from->size;
      while(a<b)
      {
	c=(a+b)/2;
	if(strcmp(strptr(&v[c].ind),str_s+e)<=0)
	{
	  if(a==c) break;
	  a=c;
	}else{
	  b=c;
	}
      }
      if(a<from->size && 
	 !strncmp(strptr(&v[a].ind),str_s+e,my_strlen(&(v[a].ind))))
      {
	c=my_strlen(&(v[a].ind));
	if(!c) c++;
	e+=c;
	my_binary_strcat(strptr(&v[a].val),my_strlen(&(v[a].val)));
	continue;
      }
    }
    my_putchar(str_s[e++]);
  }

  free_svalue(str);
  SET_STR(str,free_buf());
  free((char *)v);
}

void f_replace(int num_arg,struct svalue *argp)
{
  extern struct vector *array_replace PROT((struct vector *,struct svalue *, struct svalue *));

  if(argp[0].type==T_STRING)
  {
    if(argp[1].type==T_STRING &&
     argp[2].type==T_STRING)
    {
      string_replace(argp,argp+1,argp+2);
      pop_n_elems(2);
      return;
    }else if(argp[1].type==T_POINTER &&
     argp[2].type==T_POINTER)
    {
      replace_many(argp,argp[1].u.vec,argp[2].u.vec);
      pop_n_elems(2);
      return;
    }
  }else if(argp[0].type==T_POINTER){
    struct vector *v;
    v=array_replace(argp[0].u.vec,argp+1,argp+2);
    pop_n_elems(3);
    push_vector(v);
    v->ref--;
    return ;
  }
  error("Wrong arguments to replace()\n");
}

void f_regexp(int num_arg,struct svalue *argp)
{
  struct vector *v;
  v = match_regexp((sp-1)->u.vec, sp->u.regexp);
  pop_n_elems(2);
  if (v == 0)
  {
    push_zero();
  }else{
    push_vector(v);
    v->ref--;			/* Will make ref count == 1 */
  }
}

void f_explode(int num_arg,struct svalue *argp)
{
  struct vector *v;
  v = explode(sp-1, sp);
  pop_n_elems(2);
  push_vector(v);		/* This will make ref count == 2 */
  v->ref--;
}

void f_filter_array(int num_arg,struct svalue *argp)
{
  struct vector *v;
  v=argp[0].u.vec;
  check_vector_for_destruct(v);
  v=filter(v,argp+1,argp+2,num_arg-2);
  pop_n_elems(num_arg);
  if (v)
  {
    push_vector(v);		/* This will make ref count == 2 */
    v->ref--;
  }else{
    push_zero();
  }
  check_eval_cost();
}

void f_implode(int num_arg,struct svalue *argp)
{
  struct svalue tmp;
  check_vector_for_destruct(sp[-1].u.vec);
  implode(&tmp,sp[-1].u.vec,sp);
  pop_n_elems(2);
  sp++;
  *sp=tmp;
}

#ifdef F_INHERIT_LIST
void f_inherit_list(int num_arg,struct svalue *argp)
{
  struct vector *vec;
  extern struct vector *inherit_list PROT((struct object *));

  vec = inherit_list(sp->u.ob);
  pop_stack();
  push_vector(vec);
}
#endif				/* F_INHERIT_LIST */

void f_member_array(int num_arg,struct svalue *argp)
{
  struct vector *v;
  int i;

  v = sp->u.vec;
  check_vector_for_destruct(v);
  if(!IS_TYPE(*argp,v->types))
  {
    i=-1;
  }else{
    for (i=0; i < v->size; i++)
    {
      if(is_eq(v->item+i,argp))
	break;
    }
    if (i == v->size) i = -1; /* Return -1 for failure */
  }
  pop_stack();
  pop_stack();
  push_number(i);
}

void f_allocate(int num_arg,struct svalue *argp)
{
  extern int batch_mode;
  extern int max_eval_cost;
  struct vector *v;
  v=allocate_n_array(argp,num_arg);
  pop_n_elems(num_arg);
  push_vector(v);
  v->ref--;
  if (eval_cost > max_eval_cost && !batch_mode)
    error("Too big allocation. Execution aborted.\n");
}

void f_sizeof(int num_arg,struct svalue *argp)
{
  int i;
  if(sp->type==T_OBJECT)
    i=data_size(sp->u.ob);
  else
    i = sp->u.vec->size;
  pop_stack();
  push_number(i);
}

void f_get_dir(int num_arg,struct svalue *argp)
{
  struct vector *v;
  v=get_dir(simple_check_valid_path(argp,"get_dir",0),argp[1].u.number);
  pop_n_elems(2);

  if (v)
  {
    push_vector(v);
    v->ref--;			/* Will now be 1. */
  } else{
    push_zero();
  }
}

#ifdef F_RUSAGE

#ifdef GETRUSAGE_THROUGH_PROCFS

static INLINE int get_time_int(timestruc_t * val) 
{
  return val->tv_sec * 1000 + val->tv_nsec/1000000;
}
 
void f_rusage(int num_arg,struct svalue *argp)
{
  struct vector *v;
  prusage_t  pru;
  prstatus_t prs;
  extern int proc_fd;

  if(ioctl(proc_fd, PIOCUSAGE, &pru) < 0)
  {
    push_number(0);
    return;
  }

  
  if(ioctl(proc_fd, PIOCSTATUS, &prs) < 0)
  {
    push_number(0);
    return;
  }
  
  v = allocate_array(29);

  v->item[0].u.number = get_time_int(&pru.pr_utime);  /* user time */
  v->item[1].u.number = get_time_int(&pru.pr_stime);  /* system time */
  v->item[2].u.number = 0;                           /* maxrss */
  v->item[3].u.number = 0;                           /* idrss */
  v->item[4].u.number = 0;                           /* isrss */
  v->item[5].u.number = 0;                           /* minflt */
  v->item[6].u.number = pru.pr_minf;           /* minor pagefaults */
  v->item[7].u.number = pru.pr_majf;           /* major pagefaults */
  v->item[8].u.number = pru.pr_nswap;          /* swaps */
  v->item[9].u.number = pru.pr_inblk;          /* block input op. */
  v->item[10].u.number = pru.pr_oublk;         /* block outout op. */
  v->item[11].u.number = pru.pr_msnd;          /* messages sent */
  v->item[12].u.number = pru.pr_mrcv;          /* messages received */
  v->item[13].u.number = pru.pr_sigs;          /* signals received */
  v->item[14].u.number = pru.pr_vctx;          /* voluntary context switches */
  v->item[15].u.number = pru.pr_ictx;         /* involuntary  "        " */
  v->item[16].u.number = pru.pr_sysc;
  v->item[17].u.number = pru.pr_ioch;
  v->item[18].u.number = get_time_int(&pru.pr_rtime);
  v->item[19].u.number = get_time_int(&pru.pr_ttime);
  v->item[20].u.number = get_time_int(&pru.pr_tftime);
  v->item[21].u.number = get_time_int(&pru.pr_dftime);
  v->item[22].u.number = get_time_int(&pru.pr_kftime);
  v->item[23].u.number = get_time_int(&pru.pr_ltime);
  v->item[24].u.number = get_time_int(&pru.pr_slptime);
  v->item[25].u.number = get_time_int(&pru.pr_wtime);
  v->item[26].u.number = get_time_int(&pru.pr_stoptime);
  v->item[27].u.number = prs.pr_brksize;
  v->item[28].u.number = prs.pr_stksize;
  push_vector(v);
  v->ref--;
}

#else /* GETRUSAGE_THROUGH_PROCFS */

void f_rusage(int num_arg,struct svalue *argp)
{
  struct vector *v;
  struct rusage rus;
  long utime, stime;
  int maxrss;

  if (getrusage(RUSAGE_SELF, &rus) < 0)
  {
    push_zero();
  }else{
    v=allocate_array(16);
    utime = rus.ru_utime.tv_sec * 1000 + rus.ru_utime.tv_usec / 1000;
    stime = rus.ru_stime.tv_sec * 1000 + rus.ru_stime.tv_usec / 1000;

    maxrss = rus.ru_maxrss;
#ifdef sun
    maxrss *= getpagesize() / 1024;
#endif
    v->item[0].u.number=utime;
    v->item[1].u.number=stime;
    v->item[2].u.number=maxrss;
    v->item[3].u.number=rus.ru_ixrss;
    v->item[4].u.number=rus.ru_idrss;
    v->item[5].u.number=rus.ru_isrss;
    v->item[6].u.number=rus.ru_minflt;
    v->item[7].u.number=rus.ru_majflt;
    v->item[8].u.number=rus.ru_nswap;
    v->item[9].u.number=rus.ru_inblock;
    v->item[10].u.number=rus.ru_oublock;
    v->item[11].u.number=rus.ru_msgsnd;
    v->item[12].u.number=rus.ru_msgrcv;
    v->item[13].u.number=rus.ru_nsignals;
    v->item[14].u.number=rus.ru_nvcsw;
    v->item[15].u.number=rus.ru_nivcsw;
    push_vector(v);
    v->ref--;
  }
}
#endif /* SOLARIS */
#endif /* F_RUSAGE */

void f_map_array(int num_arg,struct svalue *argp)
{
  struct vector *v;
  check_eval_cost();
  v=argp[0].u.vec;
  check_vector_for_destruct(v);
  v=map_array(v,argp+1,argp+2,num_arg-2);
  pop_n_elems(num_arg);
  if (v)
  {
    push_vector(v);		/* This will make ref count == 2 */
    v->ref--;
  }else{
    push_number(0);
  }
}

void f_search_array(int num_arg,struct svalue *argp)
{
  int i;
  check_eval_cost();
  check_vector_for_destruct(argp[0].u.vec);
  i=search_array(argp[0].u.vec,argp+1,argp+2,num_arg-2);
  pop_n_elems(num_arg);
  push_number(i);
}

void f_sort_array(int num_arg,struct svalue *argp)
{
  extern struct vector *sort_array
    PROT((struct vector *,struct svalue *,struct svalue *,int));
  struct vector *res;

  check_eval_cost();

  check_vector_for_destruct(argp[0].u.vec);
  if(num_arg<2)
  {
    res = sort_array (argp->u.vec,0,0,0);
  }else{
    res = sort_array (argp->u.vec,argp+1,argp+2,num_arg-2);
  }
  pop_n_elems (num_arg);
  push_vector(res);
  res->ref--;
}

void f_file_stat(int num_arg,struct svalue *argp)
{
  extern struct vector *file_stat(char *f,int raw);
  struct vector *v;
	  
  v = file_stat(strptr(argp),argp[1].u.number);
  pop_n_elems(2);
  if(!v) 
    push_number(0);
  else {
    push_vector(v);
    sp->u.vec->ref--;		/* Was set to 1 at allocation */
  }
}


struct find_tmp
{
  int seenfrom;
  int next;
};

void f_find_shortest_path(int num_arg,struct svalue *argp)
{
  struct find_tmp *tmp;
  struct vector *v,*w;
  int e,d,from,to,next;

  check_argument(2,T_NUMBER,F_FIND_SHORTEST_PATH);

  from=argp[1].u.number;
  to=argp[2].u.number;

  if(to==from)
  {
    pop_n_elems(3);
    push_vector(v=allocate_array(0));
    v->ref--;
    return;
  }

  v=argp[0].u.vec;

  if(from<0 || from>=v->size)
    error("'from' argument out of range.\n");

  if(to<0 || to>=v->size)
    error("'to' argument out of range.\n");

  tmp=(struct find_tmp *)xalloc(sizeof(struct find_tmp)*v->size);

  for(e=0;e<v->size;e++)
  {
    tmp[e].seenfrom=-1;
    tmp[e].next=-1;
  }

  tmp[from].seenfrom=-2;

  for(;from!=-1;from=next)
  {
    for(next=-1;from!=-1;from=tmp[from].next)
    {
      if(v->item[from].type!=T_POINTER)
      {
        free((char *)tmp);
        error("graph should be array of array.\n");
        
      }
      w=v->item[from].u.vec;
      for(d=0;d<w->size;d++)
      {
        if(w->item[d].type!=T_NUMBER)
        {
          free((char *)tmp);
          error("graph should be array of array of int.\n");
        }
        e=w->item[d].u.number;
        if(e<0 || e>=v->size)
        {
          free((char *)tmp);
          error("graph contains indexes out of range.\n");
        }
        if(tmp[e].seenfrom!=-1) continue;

        tmp[e].seenfrom=from;
        tmp[e].next=next;
        next=e;

        if(e==to)
        {
          for(e=0,d=to;d!=-2;d=tmp[d].seenfrom) e++;
          w=allocate_array(e);
          for(d=to;d!=-2;d=tmp[d].seenfrom)
          {
            e--;
            w->item[e].type=T_NUMBER;
            w->item[e].u.number=d;
          }
          free((char *)tmp);
          pop_n_elems(3);
          push_vector(w);
          w->ref--;
	  return;
        }
      }
    }
  }
  free((char *)tmp);  /* no path found */
  pop_n_elems(3);
  push_zero();
}
