#include "efuns.h"
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif
#include <arpa/telnet.h>
#include "simulate.h"
#include "array.h"
#include "main.h"
#include "stralloc.h"
#include "call_out.h"
#include "save_objectII.h"
#include "backend.h"
#include "otable.h"
#include "dbase_efuns.h"
#include "hash.h"

void f_get_function(int num_arg,struct svalue *argp)
{
  int function;
  struct object *ob;

  ob=argp[0].u.ob;

  function=find_shared_string_function(strptr(argp+1),ob->prog);

  pop_stack();
  pop_stack();
  if(function>=0)
  {
    sp++;
    sp->type=T_FUNCTION;
    sp->u.ob=ob;
    add_ref(ob, "F_GET_FUNCTION");
    sp->subtype=function;
  }else{
    push_number(0);
  }
}

void f_get_lfun(int num_arg,struct svalue *argp)
{
  int function;
  struct object *ob;

  ob=argp[0].u.ob;
  function=argp[1].u.number;
  
  if(function<0 || function>=num_lfuns)
    error("Too large lfun number.\n");
  
  if(ob->prog->funindex && function<ob->prog->num_lfuns)
  {
    function=ob->prog->lfuns[function];
  }else{
    function=find_shared_string_function(lfuns[function],ob->prog);
  }

  pop_stack();
  pop_stack();
  if(function>=0)
  {
    sp++;
    sp->type=T_FUNCTION;
    sp->u.ob=ob;
    add_ref(ob, "F_GET_FUNCTION");
    sp->subtype=function;
  }else{
    push_number(0);
  }
}

/* This is needed to write recursive lambda-functions */
void f_this_function(int num_arg,struct svalue *argp)
{
  if(current_object->flags & O_DESTRUCTED)
  {
    push_zero();
  }else{
    sp++;
    sp->type=T_FUNCTION;
    sp->u.ob=current_object;
    add_ref(current_object, "F_THIS_FUNCTION");
    sp->subtype=csp->func;
  }
}

void f_function_name(int num_arg,struct svalue *argp)
{
  char *name;
  if(sp->u.ob->flags & O_DESTRUCTED)
  {
    name=NULL;
  }else{
    name=copy_shared_string(FUNC(sp->u.ob->prog,sp->subtype)->name);
  }
  pop_stack();
  if(name!=NULL)
  {
    push_shared_string(name);
  }else{
    push_zero();
  }
}

void f_function_object(int num_arg,struct svalue *argp)
{
  if(sp->u.ob->flags & O_DESTRUCTED)
  {
    free_svalue(sp); 
    SET_TO_ZERO(*sp);
  }else{
    sp->type=T_OBJECT;
  }
}

void f_call_function(int num_arg,struct svalue *argp)
{
  if(argp[0].type==T_NUMBER)
  {
    argp[0].subtype=NUMBER_UNDEFINED;
    pop_n_elems(num_arg-1);
    return;
  }
  if(argp[0].type==T_POINTER)
  {
    struct svalue s;
    struct vector *v;
    s.type=T_NUMBER;
    s.subtype=NUMBER_NUMBER;
    s.u.number=-1;
    v=map_array(argp[0].u.vec,&s,argp+1,num_arg-1);
    pop_n_elems(num_arg);
    push_vector(v);
    v->ref--;
    return;
  }
  check_eval_cost();
  apply_lambda_low(argp->u.ob,argp->subtype,num_arg-1,0);
  if(sp==argp)
  {
    free_svalue(sp);
    SET_TO_ZERO(*sp);
  }else{
    free_svalue(sp-1);
    sp[-1]=sp[0];
    SET_TO_ZERO(*sp);
    sp--;
  }
}

void f_expunge(int num_arg,struct svalue *argp)
{
  current_object->flags |= O_EXPUNGE;
}

void f_find_object(int num_arg,struct svalue *argp)
{
  struct object *o;
  char *name;
  name=strptr(sp);
  if(*name=='/') name++;
  o=lookup_my_object_hash(name);
  pop_stack();
  if(o) push_object(o);
  else push_zero();
}


void f_l_sizeof(int num_arg,struct svalue *argp)
{
  int a;
  switch(sp->type)
  {
  case T_STRING:  a=my_strlen(sp); break;
  case T_POINTER:
    a=sp->u.vec->size; break;
  case T_LIST:
  case T_MAPPING: a=sp->u.vec->item[0].u.vec->size; break;
  case T_OBJECT:  a=sp->u.ob->prog->total_size; break;
  default: bad_arg(1,F_L_SIZEOF); return;
  }
  pop_stack();
  push_number(a);
}

void f_m_sizeof(int num_arg,struct svalue *argp) { f_l_sizeof(num_arg,argp); }
void f_size(int num_arg,struct svalue *argp) { f_l_sizeof(num_arg,argp); }

void f_make_object(int num_arg,struct svalue *argp)
{
  struct program *o;
  o=make_object(strptr(sp));
  pop_stack();
  if(!o) push_zero();
  else push_shared_string(copy_shared_string(o->name));
}

void f_strstr(int num_arg,struct svalue *argp)
{
  int start,alen;
  char *a,*c;

  check_argument(2,T_NUMBER,F_STRSTR);
  start=argp[2].u.number;
  pop_stack();

  c=a=strptr(argp);
  alen=my_strlen(argp);

  a+=start;
  alen-=start;
  if(alen>0 && (a=MEMMEM(strptr(argp+1),my_strlen(argp+1),a,alen)))
  {
    alen=a-c;
  }else{
    alen=-1;
  }
  
  pop_stack();
  pop_stack();
  push_number(alen);
}

void f_load(int num_arg,struct svalue *argp)
{
  if (!find_program(strptr(argp)))
  {
    pop_stack();
    push_zero();
  }
}

void f_object_cpu(int num_arg,struct svalue *argp)
{
  register int e;
  e=sp->u.ob->cpu;
  pop_stack();
  push_number(e);
}

void f_clone_number(int num_arg,struct svalue *argp)
{
  register int e;
  e=sp->u.ob->clone_number;
  pop_stack();
  push_number(e);
}

void f_next_clone(int num_arg,struct svalue *argp)
{
  struct object *pp;
  char *f;

  if(sp->type==T_STRING)
  {
    pp=obj_list;
    f=strptr(sp);
  }else{
    pp=sp->u.ob->next_all;
    f=sp->u.ob->prog->name;
  }

  if(f)
    while(pp && (!pp->clone_number || pp->prog->name!=f)) pp=pp->next_all;
  else 
    pp=0;
  pop_stack(); 
  if(pp)
    push_object(pp);
  else
    push_zero();
}

void f_hash_name(int num_arg,struct svalue *argp)
{
  char *s;
  if((s=sp->u.ob->obj_index))
  {
    pop_stack();
    push_shared_string(copy_shared_string(s));
  }else{
    pop_stack();
    push_zero();
  }
}

void f_previous_objects(int num_arg,struct svalue *argp)
{
  struct vector *v;
  int e;
  struct control_stack *p;
  struct svalue sv;

  for (e=0,p = &control_stack[1]; p <= csp; p++) e++;
  v=allocate_array_no_init(e,0);
  for (e=0,p = &control_stack[1]; p <= csp; p++,e++)
  {
    if(!(p->ob->flags & O_DESTRUCTED))
    {
      sv.type=T_OBJECT;
      sv.u.ob=p->ob;
      assign_svalue_no_free(&(v->item[e]),&sv);
    }
  }
  push_vector(v);
  v->ref--;
}

void f_initiating_object(int num_arg,struct svalue *argp)
{
  if(control_stack[1].ob->flags & O_DESTRUCTED)
    push_zero();
  else
    push_object(control_stack[1].ob);
}

void f_created(int num_arg,struct svalue *argp)
{
  register int e;
  e=sp->u.ob->created;
  pop_stack();
  push_number(e);
}

void f_query_num_arg(int num_arg,struct svalue *argp)
{
  push_number(csp->num_of_arguments);
}

void f_get_varargs(int num_arg,struct svalue *argp)
{
  if(csp->va_args)
  {
    push_vector(csp->va_args);
  }else{
    push_zero();
  }
}

void f_update(int num_arg,struct svalue *argp)
{
  struct program *p;
  p=find_program2(strptr(sp));
  pop_stack();
  if(p)
  {
    remove_program_hash(p);
    free_prog(p,1);
    push_one();
  }else{
    push_zero();
  }
}

void f_array_of_index(int num_arg,struct svalue *argp)
{
  struct svalue *i;
  struct vector *v,*v1;
  int s,nr,e;

  v=argp[0].u.vec;
  s=v->size;
  nr=argp[1].u.number;
  v1=allocate_array_no_init(s,0);
  for(e=0;e<s;e++)
  {
    i=&(v->item[e]);
    if(i->type==T_POINTER && i->u.vec->size>nr)
      assign_svalue_no_free(&(v1->item[e]),&(i->u.vec->item[nr]));
    else
      SET_TO_ZERO(v1->item[e]);
  }
  pop_n_elems(2);
  push_vector(v1);
  v1->ref--;
}

int cp_cpu(struct object **o1,struct object **o2) 
{ return (*o1)->cpu - (*o2)->cpu; }
typedef int (*cmpfuntyp) (const void *,const void *);

void f_top_cpu(int num_arg,struct svalue *argp)
{
#define NUM_TOP 20
  struct vector *vec;
  struct object *obs[NUM_TOP],*p;
  int e,d;
  for(p=obj_list,e=0;e<NUM_TOP && p;p=p->next_all)
    if(p->clone_number) obs[e++]=p;
  fsort((void *)obs,e,sizeof(struct object *),(cmpfuntyp)cp_cpu);
  for(;p;p=p->next_all)
  {
    if(!p->clone_number || p->cpu <= (*obs)->cpu ) continue;
    for(d=1;d<NUM_TOP && p->cpu > obs[d]->cpu;d++) obs[d-1]=obs[d];
    obs[d-1]=p;
  }
  vec=allocate_array_no_init(e,0);
  for(d=0;e--;d++)
  {
    vec->item[d].type=T_OBJECT;
    (vec->item[d].u.ob=obs[e])->ref++;
  }
  push_vector(vec);
  vec->ref--;
}

void f_set_this_player(int num_arg,struct svalue *argp)
{
  if((sp->u.ob->flags & O_ENABLE_COMMANDS) && !(sp->u.ob->flags & O_DESTRUCTED))
    command_giver=sp->u.ob;
  pop_stack();
  if(command_giver)
  {
    push_object(command_giver);
  }else{
    push_zero();
  }
}

#ifdef F_FILE_LENGTH

/*
 * file_length() efun, returns the number of lines in a file.
 */
int file_length(char *file)
{
  struct stat st;
  FILE *f;
  unsigned int i;
  int ret;

  if (!file) return -1;
  if (stat(file, &st) == -1)
    return -1;
  if (st.st_mode == S_IFDIR)
    return -2;
  if (!(f = fopen(file, "r")))
    return -1;
  ret = 0;
  for (i=0; i<st.st_size; i++) if (fgetc(f) == '\n') ret++;
  fclose(f);
  return ret;
} /* end of file_length() */

void f_file_length(int num_arg,struct svalue *argp)
{
  int l;

  l = file_length(simple_check_valid_path(sp,"file_length",0));
  pop_stack();
  push_number(l);
}
#endif				/* F_FILE_LENGTH */

#ifdef F_REMOVE_ACTION

/*
 * removes an action from objects in the inventory and enviroment of
 * current_object.  the "cmd" must be defined by the object.
 */
int remove_action(cmd, from)
  char *cmd;
  struct object *from;
{
  struct sentence **s, *t;
  int n;

  n = 0;
  for (s=&(from->sent); *s; ) {
    if ((*s)->ob == current_object) {
      if (strcasecmp((*s)->verb, cmd)) {
        s = &((*s)->next);
        continue;
      }
      t = *s;
      s = &((*s)->next);
      free_sentence(t);
      n++;
    }
  }
  return n;
} /* end of remove_action() */

void f_remove_action(int num_arg,struct svalue *argp)
{
  int n;

  n = remove_action(strptr(sp-1), sp->u.ob);
  pop_n_elems(2);
  push_number(n);

}
#endif				/* F_REMOVE_ACTION */

#ifdef F_QUERY_ACTIONS

void f_query_actions(int num_arg,struct svalue *argp)
{
  struct vector *v;
  if(num_arg==1)
  {
    v=query_actions(argp[0].u.ob,NULL);
  }else{
    v=query_actions(argp[0].u.ob,argp[1].u.ob);
    pop_stack();
  }
  pop_stack();
  if(v)
  {
    push_vector(v);
    v->ref--;			/* fix ref count */
  }else{
    push_zero();
  }
}
#endif				/* F_QUERY_ACTIONS */

void f_clone_object(int num_arg,struct svalue *argp)
{
  struct object *ob;
  ob = clone_object(strptr(sp),0);
  pop_stack();
  if (ob) {
    sp++;
    sp->type = T_OBJECT;
    sp->u.ob = ob;
    add_ref(ob, "F_CLONE_OBJECT");
  } else {
    push_zero();
  }
}

void f_save_object(int num_arg,struct svalue *argp)
{
  save_style=SAVE_DEFAULT;
  push_shared_string(save_object());
}

void f_code_value(int num_arg,struct svalue *argp)
{
  char *s;
  save_style=argp[1].u.number;
  pop_stack();	

  init_buf();
  save_svalue(argp);
  s=free_buf();
  pop_stack();

  push_shared_string(s);
}

#ifdef F_WRITE_FILE

void f_write_file(int num_arg,struct svalue *argp)
{
  FILE *f;
  char *file;
  int i;

  file=simple_check_valid_path(argp,"write_file",1);
  i=0;
  if(file)
  {
    f = fopen(file, "a");
    if (f == 0)
      error("Wrong permissions for opening file %s for append.\n", file);
    i=fwrite(strptr(argp+1),1, my_strlen(argp+1), f);
    fclose(f);
  }
  pop_n_elems(2);
  push_number(i);
}
#endif

#ifdef F_READ_FILE

void f_read_file(int num_arg,struct svalue *argp)
{
  char *str;
  int start = 0, len = 0;

  if (num_arg>2)
  {
    check_argument(2,T_NUMBER,F_READ_FILE);
    len = argp[2].u.number;
    sp--;
  }

  if (num_arg > 1)
  {
    start = argp[1].u.number;
    sp--;
  }

  str=simple_check_valid_path(argp,"read_file",0);
  if(str)  str = read_file(str, start, len);
  if(str)
  {
    pop_stack();
    push_new_shared_string(str);
    free(str);
  }else{
    free_svalue(sp);
    SET_TO_ZERO(*sp);
  }
}
#endif

#ifdef F_READ_BYTES

void f_read_bytes(int num_arg,struct svalue *argp)
{
  char *str;
  int start = 0, len = 0x7fffffff;

  if (num_arg>2)
  {
    check_argument(2,T_NUMBER,F_READ_BYTES);
    len = argp[2].u.number;
    pop_stack();
  }

  if (num_arg > 1)
  {
    start = argp[1].u.number;
    pop_stack();
  }

  str=simple_check_valid_path(argp,"read_bytes",0);
  if(str) str = read_bytes(str, start, len);
  pop_stack();
  if (!str)
    push_zero();
  else
    push_shared_string(str);
}
#endif

#ifdef F_WRITE_BYTES

void f_write_bytes(int num_arg,struct svalue *argp)
{
  char *str;
  int i;
  check_argument(2,T_STRING,F_WRITE_BYTES);
  
  str=simple_check_valid_path(argp,"write_bytes",1);
  i=0;
  if(str) i=write_bytes(str,argp[1].u.number, argp+2);
  pop_n_elems(3);
  push_number(i);
}
#endif

void f_file_size(int num_arg,struct svalue *argp)
{
  struct stat st;
  int i;
  char *file;
  file=simple_check_valid_path(argp,"file_size",0);
  i=-1;
  if(file)
  {
    if(stat(file,&st)!=-1)
    {
      if(S_IFDIR & st.st_mode)
      {
	i=-2;
      }else{
	i=st.st_size;
      }
    }
  }
  pop_stack();
  push_number(i);
}

void f_restore_object(int num_arg,struct svalue *argp)
{
  int i;
  i = restore_object(strptr(sp));
  pop_stack();
  push_number(i);
}

void f_decode_value(int num_arg,struct svalue *argp)
{
  sp[1]=sp[0];
  SET_TO_ZERO(*sp);
  sp++;
  restore_svalue(strptr(sp),sp-1);
  pop_stack();
}

void f_this_player(int num_arg,struct svalue *argp)
{
  if (command_giver && !(command_giver->flags & O_DESTRUCTED))
    push_object(command_giver);
  else
    push_zero();
}

#ifdef F_FIRST_INVENTORY
void f_first_inventory(int num_arg,struct svalue *argp)
{
  struct object *ob;
  ob = first_inventory(sp);
  pop_stack();
  if (ob)
    push_object(ob);
  else
    push_zero();
}
#endif				/* F_FIRST_INVENTORY */

#ifdef F_GETUID
void f_getuid(int num_arg,struct svalue *argp)
{
  struct object *ob;
  char *tmp;
  ob = sp->u.ob;
#ifdef DEBUG
  if (ob->user == 0) fatal("User is null pointer\n");
#endif
  tmp = copy_shared_string(ob->user);
  pop_stack();
  push_shared_string(tmp);
}
#endif				/* F_GETUID */

#ifdef F_GETEUID
void f_geteuid(int num_arg,struct svalue *argp)
{
  struct object *ob;
  ob = sp->u.ob;

  if (ob->eff_user)
  {
    char *tmp;
    tmp=copy_shared_string(ob->eff_user);
    pop_stack();
    push_shared_string(tmp);
  } else {
    pop_stack();
    push_zero();
  }
}
#endif				/* F_GETEUID */

#ifdef F_EXPORT_UID
void f_export_uid(int num_arg,struct svalue *argp)
{
  struct object *ob;
  if (current_object->eff_user == 0)
    error("Illegal to export uid 0\n");
  ob = sp->u.ob;
  if (ob->eff_user)		/* Only allowed to export when null */
    return;
  ob->user = current_object->eff_user;
}
#endif				/* F_EXPORT_UID */

#ifdef F_SETEUID
void f_seteuid(int num_arg,struct svalue *argp)
{
  struct svalue *ret;

  if (sp->type == T_NUMBER)
  {
    if (sp->u.number != 0) bad_arg(1,F_SETEUID);
    current_object->eff_user = 0;
    pop_stack();
    push_one();
    return;
  }
  argp = sp;
  if (argp->type != T_STRING) bad_arg(1,F_SETEUID);
  push_object(current_object);
  push_svalue(argp);
  APPLY_MASTER_OB(ret=,"valid_seteuid", 2);
  if (ret == 0 || ret->type != T_NUMBER || ret->u.number != 1)
  {
    pop_stack();
    push_zero();
    return;
  }
  if(current_object->eff_user)
    free_string(current_object->eff_user);
  
  current_object->eff_user = copy_shared_string(strptr(argp));
  pop_stack();
  push_one();
}
#endif				/* F_SETEUID */

void f_shutdown(int num_arg,struct svalue *argp)
{
  startshutdowngame();
}


void f_query_load_average(int num_arg,struct svalue *argp)
{
  push_new_shared_string(query_load_av());
}

void f_throw(int num_arg,struct svalue *argp)
{
  /* marion
   * the return from catch is now done by a 0 throw
   */
  assign_svalue(&catch_value, sp--);
  throw_error();	/* do the longjump, with extra checks... */
}

void f_query_host_name(int num_arg,struct svalue *argp)
{
  push_shared_string(copy_shared_string(hostname));
}

#ifdef F_NEXT_INVENTORY
void f_next_inventory(int num_arg,struct svalue *argp)
{
  struct object *ob;
  ob = sp->u.ob;
  pop_stack();
  if (ob->next_inv)
    push_object(ob->next_inv);
  else
    push_zero();
}
#endif				/* F_NEXT_INVENTORY */

void f_all_inventory(int num_arg,struct svalue *argp)
{
  struct vector *vec;
  vec = all_inventory(sp->u.ob);
  pop_stack();
  if (vec == 0) {
    push_zero();
  } else {
    push_vector(vec);		/* This will make ref count == 2 */
    vec->ref--;
  }
}

void f_deep_inventory(int num_arg,struct svalue *argp)
{
  struct vector *vec;

  vec = deep_inventory(sp->u.ob, 0);
  free_svalue(sp);
  sp->type = T_POINTER;
  sp->u.vec = vec;
}

void f_environment(int num_arg,struct svalue *argp)
{
  struct object *ob;
  ob = environment(sp);
  pop_stack();

  if (ob)
    push_object(ob);
  else
    push_zero();
}

void f_this_object(int num_arg,struct svalue *argp)
{
  if (current_object->flags & O_DESTRUCTED)
    push_zero();
  else
    push_object(current_object);
}

void f_previous_object(int num_arg,struct svalue *argp)
{
  struct control_stack *p;
  for (p = csp; p >= &(control_stack[1]); p--)
  {
    if(p->ob!=current_object)
    {
      if(p->ob->flags & O_DESTRUCTED)
      {
	push_zero();
	sp->subtype=NUMBER_DESTRUCTED_OBJECT;
	return;
      }else{
	push_object(p->ob);
	return;
      }
    }
  }
  push_zero();
}

void f_time(int num_arg,struct svalue *argp)
{
  if(batch_mode)
    current_time=get_current_time();
  push_number(current_time);
}

void f_intp(int num_arg,struct svalue *argp)
{
  pop_push_conditional(sp->type==T_NUMBER);
}

void f_regular_expressionp(int num_arg,struct svalue *argp)
{
  pop_push_conditional(sp->type==T_REGEXP);
}

void f_floatp(int num_arg,struct svalue *argp)
{
  pop_push_conditional(sp->type==T_FLOAT);
}

void f_stringp(int num_arg,struct svalue *argp)
{
  pop_push_conditional(sp->type==T_STRING);
}

void f_objectp(int num_arg,struct svalue *argp)
{
  pop_push_conditional(sp->type==T_OBJECT);
}

void f_pointerp(int num_arg,struct svalue *argp)
{
  pop_push_conditional(sp->type==T_POINTER);
}

void f_zero_type(int num_arg,struct svalue *argp)
{
  if(sp->type!=T_NUMBER)
  {
    pop_stack();
    push_number(-1);
  }else{
    sp->u.number=sp->subtype;
  }
}

void f_living(int num_arg,struct svalue *argp)
{
  pop_push_conditional(sp->u.ob->flags & O_ENABLE_COMMANDS);
}

void f_mappingp(int num_arg,struct svalue *argp)
{
  pop_push_conditional(sp->type==T_MAPPING);
}

void f_listp(int num_arg,struct svalue *argp)
{
  pop_push_conditional(sp->type==T_LIST);
}

void f_functionp(int num_arg,struct svalue *argp)
{
  pop_push_conditional(sp->type==T_FUNCTION);
}

void f_query_verb(int num_arg,struct svalue *argp)
{
  if (last_verb == 0)
  {
    push_zero();
    return;
  }
  push_new_shared_string(last_verb);
}

void f_file_name(int num_arg,struct svalue *argp)
{
  char *name;

  name = copy_shared_string(sp->u.ob->prog->name);
  pop_stack();
  push_shared_string(name);
}

void f_write(int num_arg,struct svalue *argp)
{
  do_write(sp);
  pop_stack();
}

void f_move_object(int num_arg,struct svalue *argp)
{
  struct object *o;
  o = sp->u.ob;
  move_object(current_object,o);
  pop_stack();
}

void f_function_exists(int num_arg,struct svalue *argp)
{
  char *str;

  str = function_exists(strptr(sp-1), sp->u.ob);
  pop_n_elems(2);
  if (str){
    push_shared_string(copy_shared_string(str));
  } else {
    push_zero();
  }
}

void f_add_action(int num_arg,struct svalue *argp)
{
  check_argument(2,T_NUMBER,F_ADD_ACTION);
  if(argp[1].u.ob!=current_object)
    error("Add action with function pointer to other object not allowed.\n");

  add_action(argp,argp+1,argp[2].u.number);
  pop_n_elems(num_arg);
}

void f_crypt(int num_arg,struct svalue *argp)
{
  char salt[2];
  char *res;
  char *choise =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";

  if (sp->type == T_STRING && my_strlen(sp) >= 2) {
    salt[0] = strptr(sp)[0];
    salt[1] = strptr(sp)[1];
  } else {
    salt[0] = choise[random_number(strlen(choise))];
    salt[1] = choise[random_number(strlen(choise))];
  }
#ifdef HAVE_CRYPT
  res = make_shared_string((char *)crypt(strptr(sp-1), salt));
  pop_n_elems(2);
  push_shared_string(res);
#else
#ifdef HAVE__CRYPT
  res = make_shared_string(_crypt(strptr(sp-1) salt));
  pop_n_elems(2);
  push_shared_string(res);
#else
  pop_stack();
#endif
#endif
}

void f_destruct(int num_arg,struct svalue *argp)
{
  if(sp->u.ob != current_object)
  {
    struct object *a;
    struct svalue *r; 
    a=sp->u.ob;
    APPLY_MASTER_OB(r=,"query_valid_destruct",1);
    if(!r || (r->type==T_NUMBER && r->u.number==0))
      error("Illegal call to destruct().\n");
    push_object(a);
  }
  destruct_object(sp->u.ob);
  pop_stack();
}

void f_random(int num_arg,struct svalue *argp)
{

  if (sp->u.number <= 0)
  {
    sp->u.number = 0;
    return;
  }
  sp->u.number = random_number(sp->u.number);
}

void f_strlen(int num_arg,struct svalue *argp)
{
  int i;
  i = my_strlen(sp);
  pop_stack();
  push_number(i);
}

void f_lower_case(int num_arg,struct svalue *argp)
{
  int i;
  char *str = begin_shared_string(my_strlen(sp));
  MEMCPY(str,strptr(sp),my_strlen(sp));
  for (i = my_strlen(sp)-1; i>=0; i--)
    if (str[i]>='A' && str[i]<='Z')
      str[i] -= 'A' - 'a';
  pop_stack();
  push_shared_string(end_shared_string(str));
}

void f_upper_case(int num_arg,struct svalue *argp)
{
  int i;
  char *str = begin_shared_string(my_strlen(sp));
  MEMCPY(str,strptr(sp),my_strlen(sp));
  for (i = my_strlen(sp)-1; i>=0; i--)
    if (str[i]>='a' && str[i]<='z')
      str[i] += 'A' - 'a';
  pop_stack();
  push_shared_string(end_shared_string(str));
}

void f_set_heart_beat(int num_arg,struct svalue *argp)
{
  int i;
  i = set_heart_beat(current_object, sp->u.number);
  sp->u.number = i;
}

void f_capitalize(int num_arg,struct svalue *argp)
{
  if (islower(strptr(sp)[0]))
  {
    char *str;

    str = begin_shared_string(my_strlen(sp));
    MEMCPY(str,strptr(sp),my_strlen(sp));
    str[0] += 'A' - 'a';
    pop_stack();
    push_shared_string(end_shared_string(str));
  }
}

void f_command(int num_arg,struct svalue *argp)
{
  int i;
  i = command_for_object(strptr(argp+0), argp[1].u.ob);
  pop_n_elems(num_arg);
  push_number(i);
}

void f_rm(int num_arg,struct svalue *argp)
{
  int i;
  char *p;
  i=0;
  p=simple_check_valid_path(argp,"remove_file",1);
  if(p)
  {
    do_close_database();
    if(unlink(p)!=-1) i=1;
  }
  pop_stack();
  push_number(i);
}

void f_mkdir(int num_arg,struct svalue *argp)
{
  char *path;
  int i;
  path = simple_check_valid_path(sp, "mkdir", 1);

  if(path && mkdir(path, 0770) != -1)
    i=1;
  else
    i=0;
  pop_stack();
  push_number(i);
}

void f_rmdir(int num_arg,struct svalue *argp)
{
  char *path;
  path = simple_check_valid_path(sp, "rmdir", 1);
  pop_push_conditional(path && rmdir(path) != -1);
}

void f_enable_commands(int num_arg,struct svalue *argp)
{
  enable_commands(1);
}

void f_disable_commands(int num_arg,struct svalue *argp)
{
  enable_commands(0);
}

void f_ctime(int num_arg,struct svalue *argp)
{
  char *cp,*str;
  str = time_string(sp->u.number);
  cp = STRCHR(str, '\n');
  if (cp) *cp = '\0';
  pop_stack();
  push_new_shared_string(str); /* Per, som vill ha ctime :) */
}

/*
 * do_rename is used by the efun rename. It is basically a combination
 * of the unix system call rename and the unix command mv. Please shoot
 * the people at ATT who made Sys V.
 */

#ifdef F_RENAME
int do_rename(char *from,char * to)
{
  if(!from || !to) return 1;

  if(!strlen(to) || !strcmp(to, "/"))
  {
    to = (char *)alloca(3);
    sprintf(to, "./");
  }
  strip_trailing_slashes (from);
  if (isdir (to))
  {
    /* Target is a directory; build full target filename. */
    char *cp;
    char *newto;
	    
    cp = STRRCHR (from, '/');
    if (cp)
      cp++;
    else
      cp = from;
	    
    newto = (char *) alloca (strlen (to) + 1 + strlen (cp) + 1);
    sprintf (newto, "%s/%s", to, cp);
    return do_move (from, newto);
  }
  else
    return do_move (from, to);
}

void f_rename(int num_arg,struct svalue *argp)
{
  int i;
  char *from,*to;
  i=1;
  from=simple_check_valid_path(argp,"do_rename",1);
  if(from)
  {
    to=simple_check_valid_path(argp+1,"do_rename",1);
    if(to) i = do_rename(from,to);
  }
  pop_n_elems(2);
  push_number(i);
}
#endif /* F_RENAME */

void f_debug_info(int num_arg,struct svalue *argp)
{
  struct object *ob;
  char *s;
  char buff[100];
  int i;

  switch ( argp[0].u.number )
  {
  case 0:
  {
    int flags;
    struct object *obj2;

    if (num_arg != 2)
      error("bad number of arguments to debug_info");
    check_argument(1,T_OBJECT,F_DEBUG_INFO);

    ob = argp[1].u.ob;
    flags = ob->flags;

    init_buf();
    my_strcat("O_HEART_BEAT      : ");
    my_strcat( flags&O_HEART_BEAT ? "TRUE\n" : "FALSE\n" );

    my_strcat("O_ENABLE_COMMANDS  : ");
    my_strcat( flags&O_ENABLE_COMMANDS ? "TRUE\n" : "FALSE\n" );

    my_strcat("O_DESTRUCTED       : ");
    my_strcat( flags&O_DESTRUCTED ? "TRUE\n" : "FALSE\n" );
    
    my_strcat("O_APPROVED         : ");
    my_strcat( flags&O_APPROVED ? "TRUE\n" : "FALSE\n" );

    my_strcat("O_APPROVES         : ");
    my_strcat( flags&O_APPROVES ? "TRUE\n" : "FALSE\n" );

    my_strcat("O_RESET_STATE      : ");
    my_strcat( flags&O_RESET_STATE ? "TRUE\n" : "FALSE\n" );

    my_strcat("O_WILL_CLEAN_UP    : ");
    my_strcat( flags&O_WILL_CLEAN_UP ? "TRUE\n" : "FALSE\n" );
    
    my_strcat("O_REF_CYCLE        : ");
    my_strcat( flags&O_REF_CYCLE ? "TRUE\n" : "FALSE\n" );
    
    my_strcat("O_EXPUNGE          : ");
    my_strcat( flags&O_EXPUNGE ? "TRUE\n" : "FALSE\n" );
    
    my_strcat("O_EFUN_SOCKET      : ");
    my_strcat( flags&O_EFUN_SOCKET ? "TRUE\n" : "FALSE\n" );
    

    sprintf(buff,"next_reset  : %d\n", ob->next_reset);
    my_strcat(buff);

    sprintf(buff,"time_of_ref : %d\n", ob->time_of_ref);
    my_strcat(buff);
    sprintf(buff,"ref         : %d\n", ob->ref);
#ifdef DEBUG
    sprintf(buff,"extra_ref   : %d\n", ob->extra_ref);
    my_strcat(buff);
#endif
    sprintf(buff,"clone_number: %d\n",ob->clone_number);
    my_strcat(buff);
    sprintf(buff,"name        : '%s'\n", ob->prog->name);
    my_strcat(buff);
    sprintf(buff,"next_all    : OBJ(%s)\n",
		 ob->next_all?ob->next_all->prog->name:"NULL");
    my_strcat(buff);
    if (obj_list == ob)
      my_strcat("This object is the head of the object list.\n");

    for (obj2=obj_list,i=1; obj2; obj2=obj2->next_all,i++)
    {
      if (obj2->next_all == ob)
      {
	sprintf(buff,"Previous object in object list: OBJ(%s)\n",
		     obj2->prog->name);
	my_strcat(buff);
	sprintf(buff,"position in object list:%d\n",i);
	my_strcat(buff);
      }
    }
    s=free_buf();
    pop_n_elems(num_arg);
    push_shared_string(s);
    return;
  }

  case 1:
  {
    if (num_arg != 2)
      error("bad number of arguments to debug_info");
    check_argument(1,T_OBJECT,F_DEBUG_INFO);

    ob = argp[1].u.ob;
		
    init_buf();
    sprintf(buff,"program ref's %d\n", ob->prog->ref);
    my_strcat(buff);
    sprintf(buff,"Name %s\n", ob->prog->name);
    my_strcat(buff);
    sprintf(buff,"program size %d\n",
		 ob->prog->program_size);
    my_strcat(buff);
    sprintf(buff,"num func's %d (%d) \n", ob->prog->num_functions
		 ,(int)(ob->prog->num_functions * sizeof(struct function)));
    my_strcat(buff);
    sprintf(buff,"num function ptrs %d (%d) \n", ob->prog->num_function_ptrs,
	    (int)(ob->prog->num_function_ptrs * sizeof(struct function_p)));
    my_strcat(buff);
    sprintf(buff,"callable functions %d (%d) \n", ob->prog->num_funindex,
	    (int)(ob->prog->num_funindex * sizeof(short)));
    my_strcat(buff);
    sprintf(buff,"num strings %d (%d)\n", ob->prog->num_strings,
	    (int)(ob->prog->num_strings*sizeof(char *)));
    my_strcat(buff);
    sprintf(buff,"num vars %d (%d)\n", ob->prog->num_variables
		 ,(int)(ob->prog->num_variables * sizeof(struct variable)));
    my_strcat(buff);
    sprintf(buff,"num constants %d (%d)\n", ob->prog->num_constants,
	    (int)(ob->prog->num_constants * sizeof(struct svalue)));
    my_strcat(buff);
    sprintf(buff,"num switch_mappings %d (%d)\n", ob->prog->num_switch_mappings,
	    (int)(ob->prog->num_switch_mappings * sizeof(struct vector *)));
    my_strcat(buff);
    sprintf(buff,"num inherits %d (%d)\n", ob->prog->num_inherited
		 ,(int)(ob->prog->num_inherited * sizeof(struct inherit)));
    my_strcat(buff);
    sprintf(buff,"linenumber space (%d)\n",
	    (int)(ob->prog->num_line_numbers * sizeof(signed char)));
    my_strcat(buff);
    sprintf(buff,"total size %d\n", ob->prog->total_size);
    my_strcat(buff);
    
    s=free_buf();
    pop_n_elems(num_arg);
    push_shared_string(s);
    return;
  }
  default: bad_arg(1,F_DEBUG_INFO);
  }
}

void f_master(int num_arg, struct svalue *argp) { push_object(master_ob); }

extern struct object *obj_list;
extern struct program *obj_table[OTABLE_SIZE];
void gc_svalue(struct svalue *foo);

void gc_short_svalue(union storage_union *foo,unsigned short type)
{
  switch(type)
  {
  case T_ANY:
    gc_svalue((struct svalue *)foo);
    break;
      
  case T_OBJECT:
    if(foo->ob && (foo->ob->flags & O_DESTRUCTED))
    {
      free_object(foo->ob,"gc_short_svalue");
      foo->ob=0;
    }
    break;

  case T_POINTER:
    if(foo->vec)
      check_vector_for_destruct(foo->vec);
    break;
     
  case T_MAPPING:
  case T_LIST:
    if(foo->vec)
      check_alist_for_destruct(foo->vec);
  }
}


void gc_object(struct object *ob)
{
  int e,tmp;
  for(e=0;e<ob->prog->num_variables;e++)
  {
    tmp=ob->prog->variable_names[e].rttype;
    if(tmp==T_NOTHING) continue;
    gc_short_svalue(ob->variables+e,tmp);
  }
}

void gc_svalue(struct svalue *foo)
{
  switch(foo->type)
  {
    case T_OBJECT:
    case T_FUNCTION:
     if(foo->u.ob->flags & O_DESTRUCTED)
     {
       short tmp;
       tmp=foo->type=T_OBJECT;
       free_svalue(foo);
       SET_TO_ZERO(*foo);
       foo->subtype=tmp?NUMBER_DESTRUCTED_OBJECT:NUMBER_DESTRUCTED_FUNCTION;
     }
     break;

    case T_POINTER:
     check_vector_for_destruct(foo->u.vec);
     break;
     
    case T_MAPPING:
    case T_LIST:
     check_alist_for_destruct(foo->u.vec);
  }
}
void clean_add_cache(void);

void do_gc(int foo)
{
  struct object *o;
  int e,d;
  int om;
  struct program *p,*next;

  not_backend_stralloc_gc();
  if(foo<1) return;

  /* halfhearted, let stralloc_gc do the real job */
  for(o=obj_list;o;o=o->next_all) gc_object(o);

  for (e=0;e<num_pending_calls;e++)
    for(d=0;d<pending_calls[e]->args->size;d++)
      gc_svalue(pending_calls[e]->args->item+d);

  clean_add_cache();
  if(foo<2) return;


  do{
    om=0;
    for(e=0;e<OTABLE_SIZE;e++)
    {
      for(p=obj_table[e];p;p=next)
      {
	next=p->next_hash;
	if(p->ref==1)
	{
	  remove_program_hash(p);
	  free_prog(p,1);
	  p=obj_table[e];
	  om=1;
	}
      }
    }
  }while(om);
}

void f_gc(int num_arg, struct svalue *argp)
{
  int i;
  i=argp->u.number;
  if(i>1 && 
     (!current_object->eff_user ||
      current_object->eff_user!=master_ob->user))
    i=1;

  do_gc(i);
  pop_stack();
}

#ifdef F_DUMP_MALLOC_DATA
#ifndef MALLOC_smalloc
void f_dump_malloc_data(int num_arg, struct svalue *argp)
{
  push_new_shared_string("No information about malloc available.\n");
}
#else
void f_dump_malloc_data(int num_arg, struct svalue *argp)
{
  extern char *dump_malloc_data();
  push_shared_string(dump_malloc_data());
}
#endif
#endif

void f_dump_string_data(int num_arg, struct svalue *argp)
{
  int i;
  i=sp->u.number;
  pop_stack();
  push_shared_string(add_string_status(i));
}

void f_dump_heart_beat_data(int num_arg, struct svalue *argp)
{
  int i;
  i=sp->u.number;
  pop_stack();
  push_shared_string(heart_beat_status(i));
}

void f_dump_call_out_data(int num_arg, struct svalue *argp)
{
  int i;
  i=sp->u.number;
  pop_stack();
  push_shared_string(print_call_out_usage(i));
}

void f_dump_prog_table_data(int num_arg, struct svalue *argp)
{
  int i;
  i=sp->u.number;
  pop_stack();
  push_shared_string(show_otable_status(i));
}

void f_dump_index_table_data(int num_arg, struct svalue *argp)
{
  extern char *show_ohash_status(int);
  int i;
  i=sp->u.number;
  pop_stack();
  push_shared_string(show_ohash_status(i));
}

int svalue_size(struct svalue *v)
{
  int i, total;

  switch(v->type)
  {
  case T_FUNCTION:
  case T_FLOAT:
  case T_NUMBER:
  case T_OBJECT:
    return 0;

  case T_STRING:
    return my_strlen(v) + 4;	/* Includes some malloc overhead. */

  case T_ALIST_PART:
  case T_LIST:
  case T_MAPPING:
  case T_POINTER:
    for (i=0, total = 0; i < v->u.vec->size; i++) {
      total += svalue_size(&v->u.vec->item[i]) + sizeof (struct svalue);
    }
    return total;

  default:
    fatal("Illegal type: %d\n",v->type);
  }
  /*NOTREACHED*/
  return 0;
}

int short_svalue_size(union storage_union *u,unsigned short type)
{
  int i,total;
  switch(type)
  {
  case T_ANY:
    return svalue_size((struct svalue *)u);
    
  case T_NOTHING:
  case T_FUNCTION:
  case T_FLOAT:
  case T_NUMBER:
  case T_OBJECT:
    return 0;

  case T_STRING:
    if(!u->ref) return 0;
    return my_strlen2(*u) + 4;	/* Includes some malloc overhead. */

  case T_ALIST_PART:
  case T_LIST:
  case T_MAPPING:
  case T_POINTER:
    if(!u->ref) return 0;
    for (i=0, total = 0; i < u->vec->size; i++)
      total += svalue_size(&u->vec->item[i]) + sizeof (struct svalue);
    return total;

  default:
    fatal("Illegal type: %d\n",type);
  }
  /*NOTREACHED*/
  return 0;
}

int data_size(struct object *ob)
{
  int total = 0, i,tmp;
  if (ob->prog)
  {
    for (i = 0; i < ob->prog->num_variables; i++)
    {
      tmp=ob->prog->variable_names[i].rttype;
      total += short_svalue_size(&ob->variables[i],tmp) + sizeof (union storage_union);
    }
  }
  return total;
}

void f_dumpallobj(int num_arg, struct svalue *argp)
{
  char buffer[1000];
  struct object *ob;

  init_buf();
  for (ob = obj_list; ob; ob = ob->next_all)
  {
    sprintf(buffer, "%s(%d) %d ref %d %s %s %s (%d)\n", ob->prog->name,
	    ob->clone_number,
	    (int)(data_size(ob) + sizeof (struct object)), ob->ref,
	    ob->flags & O_HEART_BEAT ? "HB" : "-",
	    ob->obj_index ? "HASHED" : "-",
	    ob->super ? ob->super->prog->name : "NO_ENV",ob->cpu);
    my_strcat(buffer);
  }
  push_shared_string(free_buf());
}

void f_set_max_eval_cost(int num_arg, struct svalue *argp)
{
  int i;
  extern int max_eval_cost;
  i=argp[0].u.number;
  assert_master_ob_loaded();
  if(!current_object->eff_user || current_object->eff_user!=master_ob->user)
    return;
  max_eval_cost=i;
  pop_stack();
}

void f_reset_eval_cost(int num_arg, struct svalue *argp)
{
  extern int eval_cost;
  assert_master_ob_loaded();
  if(!current_object->eff_user || current_object->eff_user!=master_ob->user)
    return;
  eval_cost=0;
}

void f_range(int num_arg, struct svalue *argp)
{
  int len, from, to;
  check_argument(1,T_NUMBER,F_RANGE);

  if (argp[0].type == T_POINTER)
  {
    struct vector *v;

    len = sp[-2].u.vec->size;
    from = argp[1].u.number;
    to = argp[2].u.number;

    if(from<0) from+=len;
    v = slice_array(sp[-2].u.vec, from, to);
    pop_n_elems(3);
    if (v)
    {
      push_vector(v);
      v->ref--;			/* Will make ref count == 1 */
    }else{
      push_zero();
    }
  } else if (argp[0].type == T_STRING) {
    char *res;
	    
    len = my_strlen(argp);
    from = argp[1].u.number;
    to = argp[2].u.number;

    if(from<0) from+=len;
    if (from >= len || to<from)
    {
      pop_n_elems(3);
      push_svalue(&const_empty_string);
      return;
    }
    if (to >= len) to = len-1;
    if (to == len-1 && from==0)
    {
      pop_stack();
      pop_stack();
      return;
    }
    res = make_shared_binary_string(strptr(sp-2) + from, to-from+1);
    pop_n_elems(3);
    push_shared_string(res);
  } else {
    error("Bad argument to [ .. ] range operand.\n");
  }
}

void f_combine_path(int num_arg, struct svalue *argp)
{
  char *path;
  if(num_arg==2)
  {
    path=combine_path(strptr(argp+1),strptr(argp));
  }else{
    path=combine_path(0,strptr(argp));
  }
  pop_n_elems(2);
  push_shared_string(make_shared_string(path));
  free(path);
}

void f_copy_value(int num_arg,struct svalue *argp)
{
  struct svalue s;
  s=*argp;
  copy_svalue(argp,argp);
  free_svalue(&s);
}

void f_hash(int num_arg,struct svalue *argp)
{
  int a;
  a=hashmem(strptr(argp),my_strlen(argp),20) & 0x7fffffff;
  if(num_arg==2)
  {
    if(!argp[1].u.number) error("Modulo by zero.\n");
    a%=(unsigned int)argp[1].u.number;
    pop_stack();
  }
  pop_stack();
  push_number(a);
}


/* function *get_function_list(object); */
void f_get_function_list(int num_arg,struct svalue *argp)
{
  int e;
  int funs;
  struct program *p;
  struct vector *v;
  struct object *o;
  o=argp[0].u.ob;

  p=o->prog;
  for(funs=e=0;e<p->num_funindex;e++)
    if(!(p->function_ptrs[e].type & TYPE_MOD_STATIC))
      funs++;

  v=allocate_array_no_init(funs,0);
  funs=0;

  for(e=0;e<p->num_funindex;e++)
  {
    if(!(p->function_ptrs[e].type & TYPE_MOD_STATIC))
    {
      v->item[funs].type=T_FUNCTION;
      v->item[funs].u.ob=o;
      v->item[funs].subtype=e;
      add_ref(o,"get_function_list");
      funs++;
    }
  }
  v->types=BT_FUNCTION;
  free_svalue(argp);
  argp->type=T_POINTER;
  argp->u.vec=v;
}

/* int function_args(function); */
void f_function_args(int num_arg,struct svalue *argp)
{
  int i;
  i=FUNC(argp[0].u.ob->prog,argp[0].subtype)->num_arg;
  free_svalue(argp);
  argp->type=T_INT;
  argp->subtype=0;
  argp->u.number=i;
}

/*
 * Fuzzymatch code:
 * original code by Stig Sæther Bakken (Auronthas) 940715
 * Less original code: Profezzorn
 */

/* The fuzziness between two strings, STRING1 and STRING2, is calculated by
 * finding the position in STRING2 of a prefix of STRING1.  The first character
 * of STRING1 is found in STRING2.  If we find it, we continue matching
 * successive characters from STRING1 at successive STRING2 positions.  When we
 * have found the longest prefix of STRING1 in STRING2, we decide whether it is
 * a match.  It is considered a match if the length of the prefix is greater or
 * equal to the offset of the beginning of the prefix of STRING1 in STRING2.
 * This means that "food" will match "barfoo" because "foo" (the prefix)
 * matches "foo" in "barfoo" with an offset and length of 3.  However, "food"
 * will not be considered to match "barfu", since the length is 1 while the
 * offset is 3.  The fuzz value of the match is the length of the prefix.  If
 * we find a match, we take the prefix off STRING1 and the string upto the end
 * of the match in STRING2.  If we do not find a match, we take off the first
 * character in STRING1.  Then we try and find the next prefix.
 *
 * So, to walk through an example:
 *
 * (FM-matchiness "pigface" "pigsfly"):
 *
 * STRING1		STRING2		MATCH LENGTH	OFFSET		FUZZ
 * pigface		pigsfly		3		0		3
 * face			sfly		1		1		1
 * ace			ly		0		0		0
 * ce			ly		0		0		0
 * c			ly		0		0		0
 *
 *      => 4
 * 
 * (FM-matchiness "begining-of-l" "beginning-of-l"):
 * 
 * STRING1		STRING2		MATCH LENGTH	OFFSET		FUZZ
 * begining-of-l	beginning-of-l	5		0		5
 * ing-of-l		ning-of-l	8		1		8
 *
 *      => 13
 */

static int fuzzymatch(char *str1,char *str2)
{
  int fuzz;
  fuzz=0;
  while(*str1 && *str2)
  {
    char *tmp1,*tmp2;
    int offset,length;

    /* Now we will look for the first character of tmp1 in tmp2 */
    if((tmp2=STRCHR(str2,str1[0])))
    {
      /* Ok, so we have found one character, let's check how many more */
      tmp1 = str1;
      offset = tmp2 - str2;
      length = 1;
      while (*(++tmp1)==*(++tmp2) && *tmp1) length++;

      /* We consider this a hit only if the length of the match is
	 bigger than or equal to the offset */
      if(length>=offset)
      {
	fuzz += length;
	str1 += length;
	str2 += length + offset;
	continue;
      }
    }
    str1++;
  }
  return fuzz;
}

void f_fuzzymatch(int num_arg,struct svalue *argp)
{
  int fuzz;

  if(my_string_is_equal(argp,argp+1))
  {
    fuzz=100;
  }else{
    fuzz =fuzzymatch(strptr(argp+1),strptr(argp));
    fuzz+=fuzzymatch(strptr(argp),strptr(argp+1));
    fuzz=fuzz*100/(my_strlen(argp)+my_strlen(argp+1));
  }

  pop_stack();
  pop_stack();
  push_number(fuzz);
}
