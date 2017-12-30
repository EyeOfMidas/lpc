#include "efuns.h"

#include "array.h"
#include "call_out.h"
#include "dynamic_buffer.h"
#include "main.h"
#include "simulate.h"

call_out **pending_calls=NULL; /* pointer to first busy pointer */
int num_pending_calls; /* no of busy pointers in buffer */
static call_out **call_buffer=NULL; /* pointer to buffer */
static int call_buffer_size; /* no of pointers in buffer */

extern int current_time;

static void verify_call_outs()
{
#ifdef DEBUG
  struct vector *v;
  int e;
  if(call_buffer) return;

  if(num_pending_calls<0 || num_pending_calls>call_buffer_size)
    fatal("Error in call out tables.\n");

  if(pending_calls+num_pending_calls!=call_buffer+call_buffer_size)
    fatal("Error in call out tables.\n");

  for(e=0;e<num_pending_calls;e++)
  {
    if(e)
    {
      if(pending_calls[e-1]>pending_calls[e])
	fatal("Error in call out order.\n");
    }
    
    if(!(v=pending_calls[e]->args))
      fatal("No arguments to call\n");

    if(v->ref!=1)
      fatal("Array should exactly have one reference.\n");

    if(v->malloced_size<v->size)
      fatal("Impossible array.\n");
  }
#endif
}



/* start a new call out, return 1 for success */
static int new_call_out(int num_arg,struct svalue *argp)
{
  int e,c;
  call_out *new,**p,**pos;
  struct vector *v;

#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  if(!call_buffer)
  {
    call_buffer_size=20;
    call_buffer=(call_out **)xalloc(sizeof(call_out *)*call_buffer_size);
    if(!call_buffer) return 0;
    pending_calls=call_buffer+call_buffer_size;
    num_pending_calls=0;
  }

#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  if(num_pending_calls==call_buffer_size)
  {
    /* here we need to allocate space for more pointers */
    call_out **new_buffer;

    new_buffer=(call_out **)xalloc(sizeof(call_out *)*call_buffer_size*2);
    if(!new_buffer)
      return 0;

    MEMCPY((char *)(new_buffer+call_buffer_size),
	   (char *)call_buffer,
	   sizeof(call_out *)*call_buffer_size);
    free((char *)call_buffer);
    call_buffer=new_buffer;
    pending_calls=call_buffer+call_buffer_size;
    call_buffer_size*=2;
  }

#ifdef MALLOC_DEBUG
  check_sfltable();
#endif

  /* time to allocate a new call_out struct */
  v=allocate_array(num_arg-1);
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  if(!v) return 0;
  new=(call_out *)xalloc(sizeof(call_out));
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  if(!new)
  {
    free_vector(v);
    return 0;
  }
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  new->time=current_time+argp[1].u.number;
  if(new->time<=current_time) new->time=current_time+1;
  new->command_giver=command_giver;
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  if(command_giver) add_ref(command_giver, "call_out (command_giver)");
  new->args=v;

#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  move_svalue(v->item,argp);
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  for(e=1;e<num_arg-1;e++)  move_svalue(v->item+e,argp+e+1);

#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  /* time to link it into the buffer using binsearch */
  pos=pending_calls;
  if(new->time>current_time+1) /* do we need to search where ?*/
  {
    e=num_pending_calls;
    while(e>0)
    {
      c=e/2;
      if(new->time>pos[c]->time)
      {
	pos+=c+1;
	e-=c+1;
      }else{
	e=c;
      }
    }
  }
  pos--;
  pending_calls--;
  for(p=pending_calls;p<pos;p++) p[0]=p[1];
  *pos=new;
  num_pending_calls++;
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  return 1;
}

void f_call_out(int args,struct svalue *argp)
{
  verify_call_outs();
  if (!(current_object->flags & O_DESTRUCTED))
  {
    if(argp[0].u.ob!=current_object)
    {
      error("Call out only permitted to this_object()\n");
    }
    new_call_out(args,argp);
  }
  verify_call_outs();
  pop_n_elems(args);
}

void do_call_outs()
{
  call_out *c;
  int e,args;
  verify_call_outs();
  while(num_pending_calls && pending_calls[0]->time<=current_time)
  {
    /* unlink call out */
    c=pending_calls[0];
    pending_calls++;
    num_pending_calls--;
    
    if((command_giver=c->command_giver))
      free_object(command_giver, "call_out (command_giver)");

    args=c->args->size;
    for(e=0;e<args;e++) push_svalue(c->args->item+e);
    free_vector(c->args);
    free((char *)c);
    f_call_function(args,sp-args+1);
    pop_stack();
    verify_call_outs();
  }
  command_giver=0;
}

static int find_call_out(struct svalue *fun)
{
  int e;
  for(e=0;e<num_pending_calls;e++)
  {
    if(pending_calls[e]->args->item[0].u.ob==fun->u.ob &&
       pending_calls[e]->args->item[0].type==fun->type &&
       pending_calls[e]->args->item[0].subtype==fun->subtype)
      return e;
  }
  return -1;
}

void f_find_call_out(int argc,struct svalue *argp)
{
  int e;
  verify_call_outs();
  e=find_call_out(argp);
  if(e==-1)
  {
    free_svalue(sp);
    SET_TO_UNDEFINED(*sp);
  }else{
    pop_stack();
    push_number(pending_calls[e]->time-current_time);
  }
    verify_call_outs();
}

void f_remove_call_out(int args,struct svalue *argp)
{
  int e;
  verify_call_outs();
  e=find_call_out(argp);
  if(e!=-1)
  {
    pop_stack();
    push_number(pending_calls[e]->time-current_time);
    free_vector(pending_calls[e]->args);
    if(pending_calls[e]->command_giver)
      free_object(pending_calls[e]->command_giver,
		  "remove_call_out (command_giver)");
    free((char*)(pending_calls[e]));
    for(;e>0;e--)
      pending_calls[e]=pending_calls[e-1];
    pending_calls++;
    num_pending_calls--;

  }else{
    free_svalue(sp);
    SET_TO_UNDEFINED(*sp);
  }
  verify_call_outs();
}

char *print_call_out_usage(int verbose)
{
  int i,svalues,len;
  char b[100];

  verify_call_outs();
  init_buf();
  for (len=svalues=i=0; i<num_pending_calls; i++)
  {
    len+=pending_calls[i]->time-current_time;
    svalues+=pending_calls[i]->args->size;
  }

  if(num_pending_calls)
    len/=num_pending_calls;

  if (verbose)
  {
    my_strcat("\nCall out information:\n");
    my_strcat("---------------------\n");
    sprintf(b,"Number of allocated call outs: %8d, %8d bytes\n",
	    num_pending_calls,
	    (int)(num_pending_calls *
	    (sizeof(call_out) + sizeof(struct vector) - sizeof(struct svalue))+
	    svalues*sizeof(struct svalue) ));
    my_strcat(b);
    sprintf(b,"Average length: %d\n", len);
    my_strcat(b);
  } else {
    sprintf(b,"call out:\t\t\t%8d %8d (current length %d)\n",
	    num_pending_calls,
	    (int)(num_pending_calls *
	    (sizeof(call_out) + sizeof(struct vector) - sizeof(struct svalue))+
	    svalues*sizeof(struct svalue)),
	    len );
    my_strcat(b);
  }
  return free_buf();
}

/* return a vector containing info about all call outs:
 * ({  ({ delay, command_giver, function, args, ... }), ... })
 */
struct vector *get_all_call_outs()
{
  int e,d;
  struct vector *ret;

  verify_call_outs();
  ret=allocate_array(num_pending_calls);
  for(e=0;e<num_pending_calls;e++)
  {
    struct vector *v;
    v=allocate_array(pending_calls[e]->args->size+2);
    v->item[0].u.number=pending_calls[e]->time-current_time;

    if(pending_calls[e]->command_giver)
    {
      add_ref(pending_calls[e]->command_giver,"get_all_call_outs");
      v->item[1].u.ob=pending_calls[e]->command_giver;
      v->item[1].type=T_OBJECT;
    }

    for(d=0;d<pending_calls[e]->args->size;d++)
      assign_svalue(v->item+d+2,pending_calls[e]->args->item+d);
    ret->item[e].u.vec=v;
    ret->item[e].type=T_POINTER;
  }
  return ret;
}

void f_call_out_info(int num_arg,struct svalue *argp)
{
  push_vector(get_all_call_outs());
  sp->u.vec->ref--;		/* Was set to 1 at allocation */
}

void free_all_call_outs()
{
  int e;
  verify_call_outs();
  for(e=0;e<num_pending_calls;e++)
  {
    free_vector(pending_calls[e]->args);
    if(pending_calls[e]->command_giver)
      free_object(pending_calls[e]->command_giver,
		  "free_all_call_outs (command_giver)");
    free((char*)(pending_calls[e]));
  }
  if(call_buffer) free((char*)call_buffer);
  num_pending_calls=0;
  call_buffer=NULL;
  pending_calls=NULL;
}
