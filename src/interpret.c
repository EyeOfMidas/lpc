#include <arpa/telnet.h>
#include <math.h>
#include <sys/time.h>
#include <sys/types.h>		/* sys/types.h and netinet/in.h are here to enable include of comm.h below */
/* #include <netinet/in.h> Included in comm.h below */

#include "efuns.h"
#include "array.h"
#include "simulate.h"
#include "simul_efun.h"
#include "instrs.h"
#include "comm.h"
#include "list.h"
#include "operators.h"
#include "debug.h"
#include "main.h"
#include "stralloc.h"
#include "save_objectII.h"
#include "opcodes.h"
#include "lex.h"
#include "regexp.h"
#ifdef LACIP
#include "lacip/op.h"
#endif

extern struct object *master_ob;

extern void print_svalue PROT((struct svalue *));
int strpref PROT((char *, char *));
extern int do_rename PROT((char *, char *));     
extern void event PROT((struct svalue *, char *, int, struct svalue *));
extern int file_length PROT((char *));
extern struct vector *actions_defined PROT((struct object *, struct object *,
int));
extern int remove_action PROT((char *, struct object *));

extern int T_flag;
extern char *last_verb;
struct program *current_prog;
extern int current_time;
extern struct object *current_heart_beat;

/*
 * Inheritance:
 * An object X can inherit from another object Y. This is done with
 * the statement 'inherit "file";'
 * The inherit statement will clone a copy of that file, call reset
 * in it, and set a pointer to Y from X.
 * Y has to be removed from the linked list of all objects.
 * All variables declared by Y will be copied to X, so that X has access
 * to them.
 *
 * If Y isn't loaded when it is needed, X will be discarded, and Y will be
 * loaded separetly. X will then be reloaded again.
 */
extern int d_flag;
extern int eval_cost,max_eval_cost;

/*
 * These are the registers used at runtime.
 * The control stack saves registers to be restored when a function
 * will return. That means that control_stack[0] will have almost no
 * interesting values, as it will terminate execution.
 */
static char *pc;	/* Program pointer. */
struct svalue *fp;	/* Pointer to first argument. */
struct svalue *sp;		/* Points to value of last push. */
static int function_index_offset; /* Needed for inheritance */
static int variable_index_offset; /* Needed for inheritance */

struct svalue start_of_stack[EVALUATOR_STACK_SIZE];
struct svalue catch_value={ T_NUMBER };	/* Used to throw an error to a catch */
typedef struct svalue *svaluep;
static svaluep mark_stack[EVALUATOR_STACK_SIZE]; /* Yet Another Stack */
static svaluep *mark_sp;

struct control_stack control_stack[MAX_TRACE];
struct control_stack *csp;	/* Points to last element pushed */

/*
 * Information about assignments of values:
 *
 * There are three types of l-values: Local variables, global variables
 * and vector elements.
 *
 * The local variables are allocated on the stack together with the arguments.
 * the register 'frame_pointer' points to the first argument.
 *
 * The global variables must keep their values between executions, and
 * have space allocated at the creation of the object.
 *
 * Elements in vectors are similar to global variables. There is a reference
 * count to the whole vector, that states when to deallocate the vector.
 * The elements consists of 'struct svalue's, and will thus have to be freed
 * immediately when over written.
 */
 
/*
 * Push an object pointer on the stack. Note that the reference count is
 * incremented.
 * A destructed object must never be pushed onto the stack.
 */
INLINE void push_zero() { sp++; SET_TO_ZERO(*sp); }
INLINE void push_one() { sp++; SET_TO_ONE(*sp); }
INLINE void push_conditional(int f)
{ if(f) push_one(); else push_zero(); }

INLINE void push_object(struct object *ob)
{
  if(ob->flags & O_DESTRUCTED)
  {
    push_zero(); /* Try pushing something destructed now! Profezzorn */
  }else{  
    sp++;
    if (sp == &start_of_stack[EVALUATOR_STACK_SIZE])
	fatal("stack overflow\n");
    sp->type = T_OBJECT;
    sp->u.ob = ob;
    add_ref(ob, "push_object");
  }
}

/*
 * Push a number on the value stack.
 */

INLINE void push_number(int n)
{
  sp++;
  if (sp == &start_of_stack[EVALUATOR_STACK_SIZE])
    fatal("stack overflow\n");
  sp->type = T_NUMBER;
  sp->subtype=NUMBER_NUMBER;
  sp->u.number = n;
}

INLINE void push_float(float f)
{
    sp++;
    if (sp == &start_of_stack[EVALUATOR_STACK_SIZE])
	fatal("stack overflow\n");
    sp->type = T_FLOAT;
    sp->u.fnum = f;
}

/*
 * Push a string on the value stack.
 */
INLINE void push_string(char *p)
{
  sp++;
  if (sp == &start_of_stack[EVALUATOR_STACK_SIZE])
    fatal("stack overflow\n");
  SET_STR(sp,make_shared_string(p));
}

/*
 * Assign to a svalue.
 * This is done either when element in vector, or when to an identifier
 * (as all identifiers are kept in a vector pointed to by the object).
 */

INLINE void assign_svalue_no_free(struct svalue *to,struct svalue *from)
{
#ifdef DEBUG
  if (from == 0)
    fatal("Null pointer to assign_svalue().\n");
#endif
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  *to = *from;
  switch(from->type)
  {
  case T_FUNCTION:
    if(to->u.ob->flags & O_DESTRUCTED)
    {
      SET_TO_ZERO(*to);
      to->subtype=NUMBER_DESTRUCTED_FUNCTION;
    }else{
      add_ref(to->u.ob, "ass to var (function)");
    }
    break;

  case T_OBJECT:
    if(to->u.ob->flags & O_DESTRUCTED)
    {
      SET_TO_ZERO(*to);
      to->subtype=NUMBER_DESTRUCTED_OBJECT;
    }else{
      add_ref(to->u.ob, "ass to var");
    }
    break;

  case T_STRING:
  case T_REGEXP:
  case T_LIST:
  case T_POINTER:
  case T_MAPPING:
  case T_ALIST_PART:
    to->u.ref[0]++;
    break;
  }
}

INLINE void assign_short_svalue_no_free(union storage_union *to,
					struct svalue *from,
					unsigned short type)
{
#ifdef DEBUG
  if (from == 0)
    fatal("Null pointer to assign_svalue().\n");
#endif
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  if(type==T_ANY)
  {
    assign_svalue_no_free((struct svalue *)to,from);
  }else if(from->type!=type)
  {
    if(IS_ZERO(from))
    {
      free_short_svalue(to,type);
      to->number=0;
    }else{
      error("Type error.\n");
    }
  }else{
    switch(type)
    {
    case T_OBJECT:
      *to = from->u;
      if(to->ob)
      {
	if(to->ob->flags & O_DESTRUCTED)
	  to->ob=0;
	else
	  add_ref(to->ob, "ass to var");
      }
      break;

    case T_STRING:
    case T_REGEXP:
    case T_LIST:
    case T_POINTER:
    case T_MAPPING:
    case T_ALIST_PART:
      *to = from->u;
      if(to->ref) to->ref[0]++;
      break;

    case T_FUNCTION:
      fatal("Cannot fit functionpointer into short svalue.\n");

    case T_NUMBER:
    case T_FLOAT:
      *to = from->u;
    }
  }
}

INLINE void free_short_svalue(union storage_union *u,unsigned short type)
{
  if(type==T_ANY)
  {
    free_svalue((struct svalue *)u);
  }else if(u->number){
    if(!u->number) return;
    switch(type)
    {
    case T_STRING:
      free_string(strptr2(*u));
      u->string=0;
      break;

    case T_FUNCTION:
    case T_OBJECT:
    {
      struct object *o;
      o=u->ob;
      u->ob=0;
      free_object(o, "free_short_svalue");
      return;
    }

    case T_ALIST_PART:
    case T_POINTER:
    case T_LIST:
    case T_MAPPING:
    {
      struct vector *vv;
      vv=u->vec;
      u->vec=0;
      free_vector(vv);
      break;
    }

    case T_REGEXP:
      if(!--(u->regexp->ref))
      {
	free(u->regexp->str);
	free((char *)u->regexp);
      }
      u->regexp=0;
      break;

#ifdef DEBUG
    case T_NOTHING:
    case T_NUMBER:
    case T_FLOAT:
      break;

    default:
      fatal("Unknown type in free svalue.\n");
#endif
    }
  }
}


INLINE void assign_short_svalue(union storage_union *to,
				struct svalue *from,
				unsigned short type)
{
  free_short_svalue(to,type);
  assign_short_svalue_no_free(to,from,type);
}

INLINE void assign_svalue_from_short_no_free(struct svalue *to,
				     union storage_union *from,
				     unsigned short type)
{
  struct svalue tmp;
  tmp.subtype=0;
  tmp.u=*from;
  if(tmp.u.ob)
    tmp.type=type;
  else
    tmp.type=T_NUMBER;
  assign_svalue_no_free(to,&tmp);
}

extern struct lvalue lvalues[LVALUES];

void init_stacks()
{
  int e;
  for(e=0;e<NELEM(start_of_stack);e++) SET_TO_ZERO(start_of_stack[e]);
  for(e=0;e<NELEM(lvalues);e++) SET_TO_ZERO(lvalues[e].ind);
}

INLINE void free_lvalue(struct lvalue *l)
{
  extern struct lvalue *lsp;
#ifdef DEBUG
  if(lsp+1<lvalues || l<lvalues)
    fatal("Arglebargle glop-glyf?\n");
#endif
  lsp=l-1;
  free_svalue(&(l->ind));
  switch(l->type)
  {
  case LVALUE_INDEX:
    free_lvalue(l-1);
  }
}

/*
 * Free the data that an svalue is pointing to. Not the svalue
 * itself.
 */
INLINE void free_svalue(struct svalue *v)
{
#ifdef DEBUG
  extern struct lvalue *lsp;

  if(v>sp && v<&start_of_stack[EVALUATOR_STACK_SIZE])
    fatal("Free svalue above stackpointer.\n");
#endif

  switch(v->type)
  {
  case T_LVALUE:
#ifdef DEBUG
    if(v->u.lvalue>lsp)
      fatal("Arglebargle glop-glyf!!\n");
#endif
    free_lvalue(v->u.lvalue);
    v->type=T_INT;
    break;

  case T_STRING:
    free_string(strptr(v));
    v->type=T_INT;
    break;

  case T_FUNCTION:
  case T_OBJECT:
  {
    struct object *o;
    /* note that free_object() can call destruct, which
       will call remove_object from stack, which will
       call free_svalue */
    o=v->u.ob;
    v->type=T_INT;
    free_object(o, "free_svalue");
    return;
  }
  case T_ALIST_PART:
  case T_POINTER:
  case T_LIST:
  case T_MAPPING:
  {
    struct vector *vv;
    vv=v->u.vec;
    v->type=T_INT;
    free_vector(vv);
    break;
  }

  case T_REGEXP:
    if(!--(v->u.regexp->ref))
    {
      free(v->u.regexp->str);
      free((char *)v->u.regexp);
    }
    v->type=T_INT;
    break;

#ifdef DEBUG
  case T_NOTHING:
  case T_NUMBER:
  case T_FLOAT:
    break;

  default:
    fatal("Unknown type in free svalue.\n");
#endif
  }
}

void free_svalues(struct svalue *s,int num)
{
  num++;
  while(--num>0) free_svalue(s++);
}

void copy_svalues_no_free(struct svalue *to,struct svalue *from,int num)
{
  num++;
  while(--num>0) assign_svalue_no_free(to++,from++);
}

#ifndef DEBUG

void copy_svalues_raw(struct svalue *to,struct svalue *from, int num)
{
  unsigned int a,b;
  int *c;
  a=MAX_REF_TYPE<<16;
  num++;
  while(--num>0)
  {
    *(((int *)to)++)=b=*(((int *)from)++);
    *(((int **)to)++)=c=*(((int **)from)++);
    if(b<=a) c[0]++;
  }
}
#else

void assign_svalue_raw(struct svalue *to,struct svalue *from)
{
  assign_svalue_no_free(to,from);
}

void copy_svalues_raw(struct svalue *to,struct svalue *from, int num)
{
  copy_svalues_no_free(to,from,num);
}
#endif

INLINE void move_svalue(struct svalue *to,struct svalue *from)
{
  free_svalue(to);
  *to=*from;
  SET_TO_ZERO(*from);
}

INLINE void assign_svalue(struct svalue *dest,struct svalue *v)
{
  /* First deallocate the previous value. */
  free_svalue(dest);
  assign_svalue_no_free(dest, v);
}

/*
 * Pop the top-most value of the stack.
 * Don't do this if it is a value that will be used afterwards, as the
 * data may be sent to free(), and destroyed.
 */
INLINE void pop_stack()
{
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
#ifdef DEBUG
  if (sp < start_of_stack)
    fatal("Stack underflow.\n");
#endif
  free_svalue(sp);
  sp--;
}

INLINE void pop_push_conditional(int f)
{ 
  pop_stack();
  if(f) push_one(); else push_zero(); 
}

/*
 * Push a pointer to a vector on the stack. Note that the reference count
 * is incremented. Newly created vectors normally have a reference count
 * initialized to 1.
 */
INLINE void push_vector(struct vector *v)
{
  v->ref++;
  sp++;
  sp->u.vec = v;
  sp->type = T_POINTER;
}

INLINE void push_list(struct vector *v)
{
  v->ref++;
  sp++;
  sp->u.vec = v;
  sp->type = T_LIST;
}

INLINE void push_mapping(struct vector *v)
{
  v->ref++;
  sp++;
  sp->u.vec = v;
  sp->type = T_MAPPING;
}

INLINE void push_svalue(struct svalue *v)
{
  sp++;
  assign_svalue_no_free(sp, v);
}

INLINE void push_shared_string(char *p)
{
#ifdef DEBUG
  if(p!=debug_findstring(p))
    fatal("push_shared_string on nonshared string.\n");
#endif
  sp++;
  SET_STR(sp,p);
}

INLINE void push_new_shared_string(char *p)
{
  push_shared_string(make_shared_string(p));
}


/*
 * Deallocate 'n' values from the stack.
 */
INLINE void pop_n_elems(int n)
{
#ifdef DEBUG
  if (n < 0)
    fatal("pop_n_elems: %d elements.\n",n);
#endif
  for (; n>0; n--)  pop_stack();
}

void bad_arg(int arg,int instruction)
{
  error("Bad argument %d to %s()\n",arg, get_instruction_name(instruction));
}

INLINE void push_control_stack(struct function *funp)
{
  if (csp == &control_stack[MAX_TRACE-1])
    error("Too deep recursion.\n");
  csp++;
  csp->funp = funp;		/* Only used for tracebacks */
  csp->ob = current_object;
  csp->comgiver=command_giver;
  csp->fp = fp;
  csp->prog = current_prog;
  csp->pc = pc;
  csp->function_index_offset = function_index_offset;
  csp->variable_index_offset = variable_index_offset;
}

/*
 * Pop the control stack one element, and restore registers.
 */
INLINE void pop_control_stack()
{
#if 0
  struct object *pobj;
  pobj=current_object;
  current_object = csp->ob;
  if(current_object && current_object!=pobj && 
     csp!=control_stack && current_object->flags & O_DESTRUCTED)
    error("Returning to destructed object.\n");
#else
#ifdef DEBUG
  if (csp == control_stack - 1)
    fatal("Popped out of the control stack");
#endif
  current_object = csp->ob;
#endif
  current_prog = csp->prog;
  command_giver=csp->comgiver;
  pc = csp->pc;
  fp = csp->fp;
  function_index_offset = csp->function_index_offset;
  variable_index_offset = csp->variable_index_offset;
  if(csp->va_args) free_vector(csp->va_args);
  csp--;
}

/*
 * Argument is the function to execute. If it is defined by inheritance,
 * then search for the real definition, and return it.
 * There is a number of arguments on the stack. Normalize them and initialize
 * local variables, so that the called function is pleased.
 */
static struct function *setup_new_frame(struct function_p *funp,
			    struct program *prog)
{
  struct function *fun;
  function_index_offset = prog->inherit[funp->prog].function_index_offset;
  variable_index_offset = prog->inherit[funp->prog].variable_index_offset;
  current_prog=prog->inherit[funp->prog].prog;
  fun=current_prog->functions+funp->fun;

  csp->func=funp-prog->function_ptrs;

  /* Remove excessive arguments */
  csp->num_of_arguments=csp->num_local_variables;
  if(funp->type & TYPE_MOD_VA_ARGS)
  {
    int a;
    a=csp->num_local_variables - fun->num_arg;
    csp->num_local_variables-=a;
    if(a<0) a=0;
    csp->va_args=allocate_array_no_init(a,0);
    while(a) 
    {
      csp->va_args->item[--a]=*sp;
      sp--;
    }
  }else{
    while(csp->num_local_variables > fun->num_arg)
    {
      pop_stack();
      csp->num_local_variables--;
    }
    csp->va_args=NULL;
  }
  /* Correct number of arguments and local variables */
  while(csp->num_local_variables < fun->num_arg + fun->num_local)
  {
    push_zero();
    csp->num_local_variables++;
  }
  fp = sp - csp->num_local_variables + 1;
  return fun;
}

void f_break_point(int num_arg,struct svalue *argp)
{
#if 0
  if (sp - fp - csp->num_local_variables + 1 != 0)
    fatal("Bad stack pointer.\n");
#else
  if (sp < fp - csp->num_local_variables + 1)
    fatal("Bad stack pointer.\n");
#endif
}

/* marion
 * maintain a small and inefficient stack of error recovery context
 * data structures.
 * This routine is called in three different ways:
 * push=-1	Pop the stack.
 * push=1	push the stack.
 * push=0	No error occured, so the pushed value does not have to be
 *		restored. The pushed value can simply be popped into the void.
 *
 * The stack is implemented as a linked list of stack-objects, allocated
 * from the heap, and deallocated when popped.
 */
void push_pop_error_context (int push)
{
  extern jmp_buf error_recovery_context;
  extern int error_recovery_context_exists;
  static struct error_context_stack
  {
    jmp_buf old_error_context;
    int old_exists_flag;
    struct control_stack *save_csp;
    struct object *save_command_giver;
    struct svalue *save_sp;
    struct error_context_stack *next;
    struct svalue **mark_sp;
  } *ecsp = 0, *p;

  if (push == 1)
  {
    /*
     * Save some global variables that must be restored separately
     * after a longjmp. The stack will have to be manually popped all
     * the way.
     */
    p = (struct error_context_stack *)xalloc (sizeof *p);
    p->save_sp = sp;
    p->save_csp = csp;	
    MEMCPY ((char *)p->old_error_context,
	    (char *)error_recovery_context,
	    sizeof error_recovery_context);
    p->old_exists_flag = error_recovery_context_exists;
    p->next = ecsp;
    p->mark_sp=mark_sp;
    ecsp = p;
  } else {
    p = ecsp;
    if (p == 0)
      fatal("Catch: error context stack underflow");
    if (push == 0) {
#ifdef DEBUG
      if(p->mark_sp!=mark_sp)
	fatal("Catch: mark sp }t finsp}ng.\n");

      if (csp != p->save_csp-1)
	fatal("Catch: Lost track of csp");
#endif
    } else {
      /* push == -1 !
       * They did a throw() or error. That means that the control
       * stack must be restored manually here.
       */
      while(csp>p->save_csp)
	pop_control_stack();

      pop_n_elems (sp - p->save_sp);
    }
    MEMCPY ((char *)error_recovery_context,
	    (char *)p->old_error_context,
	    sizeof error_recovery_context);
    error_recovery_context_exists = p->old_exists_flag;
    ecsp = p->next;
    mark_sp=p->mark_sp;
    free ((char *)p);
  }
}

INLINE void check_arg(int e,struct svalue *ss,int i)
{
  struct instr *d;
  struct svalue *sv;
  sv=ss+e-1;
  d=&(instrs[i]);
  if (d->type[e-1] != 0 && !IS_TYPE(*sv,d->type[e-1]))
  {
     bad_arg(e,i+F_OFFSET);
  }
}

struct processing
{
  struct processing *next;
  struct vector *v;
  struct svalue *s;
};

void low_copy_svalue(struct svalue *dest,struct svalue *src,struct processing *p)
{
  *dest=*src;
  eval_cost++;
  switch(dest->type)
  {
    case T_NUMBER:
    case T_FLOAT:
      break;

    case T_POINTER:
    case T_MAPPING:
    case T_LIST:
    case T_ALIST_PART:
    {
      struct processing curr;
      int e;
      struct vector *vs,*vd;

      curr.next=p;
      curr.v=vs=src->u.vec;
      curr.s=dest;

      for(;p;p=p->next)
      {
	if(vs==p->v)
	{
	  assign_svalue(dest,p->s);
	  return;
	}
      }

      vd=dest->u.vec=allocate_array_no_init(vs->size,0);
      for(e=0;e<vs->size;e++)
        low_copy_svalue(vd->item+e,vs->item+e,&curr);
      break;
    }

    case T_STRING:
      dest->u.string=BASE(copy_shared_string(strptr(dest)));
      break;

    case T_REGEXP:
      dest->u.regexp->ref++;
      break;

    case T_OBJECT:
    case T_FUNCTION:
     dest->u.ob->ref++;
     break;
  }
}

void copy_svalue(struct svalue *dest,struct svalue *src)
{
  low_copy_svalue(dest,src,0);
}


INLINE void check_eval_cost()
{
  extern void check_signals();
  extern unsigned long signals;
  if (eval_cost > max_eval_cost)
  {
    if(batch_mode)
    {
      max_eval_cost=0x7fffffff;
      return; /* who cares? */
    }
    fprintf(stderr,"eval_cost too big %d\n", eval_cost);
    error("Too long evaluation. Execution aborted.\n");
  }
  if(signals) check_signals();
}


/*
 * Evaluate instructions at address 'p'. All program offsets are
 * to current_prog->program. 'current_prog' must be setup before
 * call of this function.
 *
 * There must not be destructed objects on the stack. The destruct_object()
 * function will automatically remove all occurences. The effect is that
 * all called efuns knows that they won't have destructed objects as
 * arguments.
 */
#ifdef DEBUG
#define BACKLOG 200
static int previous_instruction[BACKLOG];
static int previous_instruction_offset[BACKLOG];
static int stack_size[BACKLOG];
static char *previous_pc[BACKLOG];
static unsigned int last;
#endif

#define CASE(X) case X-F_OFFSET: eval_cost+=instrs[X-F_OFFSET].eval_cost;

#ifdef HANDLES_UNALIGNED_MEMORY_ACCESS
#define EXTRACT_UWORD(X) (*((unsigned short *)(X)))
#define EXTRACT_WORD(X) (*((short *)(X)))
#define EXTRACT_UINT(X) (*((unsigned int *)(X)))
#define EXTRACT_INT(X) (*((int *)(X)))
#else
#define EXTRACT_UWORD(X) ((EXTRACT_UCHAR(X)<<8) | (EXTRACT_UCHAR((X)+1)))
#define EXTRACT_WORD(X) ((short)EXTRACT_UWORD(X))
#define EXTRACT_UINT(X) ((EXTRACT_UWORD(X)<<16) | (EXTRACT_UWORD((X)+2)))
#define EXTRACT_INT(X) ((int)EXTRACT_UINT(X))
#endif

#define get_relative_offset() (tmp2=EXTRACT_INT(pc),pc+=sizeof(int),pc+tmp2-sizeof(int))

void eval_instruction(char *p)
{
  extern struct lvalue lvalues[LVALUES];
  extern struct svalue *fp;
  extern struct lvalue *lsp;

  int num_arg,tmp2;
  unsigned short tmp;
  unsigned int opcode;
  struct svalue *argp;

#ifdef DEBUG
  struct svalue *expected_stack;
#endif

  for(pc=p;;)
  {
#ifdef MALLOC_DEBUG
    check_sfltable();
#endif
    opcode=EXTRACT_UCHAR(pc);
    pc++;

  da_switch:
#ifdef DEBUG
    previous_instruction[last] = opcode + F_OFFSET;
    previous_instruction_offset[last] = pc - current_prog->program;
    previous_pc[last] = pc-1;
    stack_size[last] = sp - fp - csp->num_local_variables;
    last = (last + 1) % (sizeof previous_instruction / sizeof (int));
#ifdef WARN    
    sp[1].type=2000;
    sp[2].type=2000;
    sp[3].type=2000;
    sp[4].type=2000;
    sp[5].type=2000;
    sp[6].type=2000;
    sp[7].type=2000;
#endif
#endif

#ifdef OPCPROF
    check_cost_for_instr(opcode);
#endif
    switch(opcode)
    {
      CASE(F_ADD_256);
      opcode=EXTRACT_UCHAR(pc)+256;
      pc++;
      goto da_switch;

      CASE(F_ADD_512);
      opcode=EXTRACT_UCHAR(pc)+512;
      pc++;
      goto da_switch;

      CASE(F_ADD_768);
      opcode=EXTRACT_UCHAR(pc)+768;
      pc++;
      goto da_switch;

      CASE(F_ADD_1024);
      opcode=EXTRACT_UCHAR(pc)+1024;
      pc++;
      goto da_switch;

      CASE(F_ADD_256X);
      opcode=EXTRACT_UWORD(pc);
      pc+=sizeof(short);
      goto da_switch;

      CASE(F_WRITE_OPCODE);
      opcode=EXTRACT_UCHAR(pc);
      pc++;
    da_switch2:
      switch(opcode)
      {
	CASE(F_ADD_256);
	opcode=EXTRACT_UCHAR(pc)+256;
	pc++;
	goto da_switch2;

	CASE(F_ADD_512);
	opcode=EXTRACT_UCHAR(pc)+512;
	pc++;
	goto da_switch2;

	CASE(F_ADD_768);
	opcode=EXTRACT_UCHAR(pc)+768;
	pc++;
	goto da_switch2;

	CASE(F_ADD_1024);
	opcode=EXTRACT_UCHAR(pc)+1024;
	pc++;
	goto da_switch2;

	CASE(F_ADD_256X);
        opcode=EXTRACT_UWORD(pc);
        pc+=sizeof(short);
	goto da_switch2;
      }
      if(T_flag>2)
      {
	putc('%',stderr);
	for(tmp2=-1;tmp2<csp-control_stack;tmp2++) putc(' ',stderr);
	fprintf(stderr,"%s\n",get_instruction_name(opcode+F_OFFSET));
      }
      goto da_switch;
      
    default:
      if(opcode<F_MAX_OPCODE-F_OFFSET)
      {
	eval_cost+=instrs[opcode].eval_cost;
	if(instrs[opcode].efunc)
	{
	  if(instrs[opcode].min_arg != instrs[opcode].max_arg)
	  {
	    argp=*(mark_sp--);
	    num_arg=sp-argp;
	    argp++;
	    if (num_arg > 0)
	    {
	      check_arg(1,argp,opcode);
	      if (num_arg > 1)
		check_arg(2,argp,opcode);
	    }
	  } else {
	    num_arg = instrs[opcode].min_arg;
	    argp= sp - num_arg + 1;
	    if (instrs[opcode].min_arg > 0)
	    {
	      check_arg(1,argp,opcode);
	      if (instrs[opcode].min_arg > 1)
		check_arg(2,argp,opcode);
	    }
	  }

	  if(num_arg<instrs[opcode].min_arg)
	  {
	    error("Too few arguments to %s.\n",
		  get_instruction_name(F_OFFSET+opcode));
	  }

#ifdef DEBUG
	  /* Support the actual void /profezzorn */
	  if((instrs[opcode].ret_type & TYPE_MOD_MASK)==TYPE_VOID)
	    expected_stack=argp-1;
	  else
	    expected_stack=argp;
#endif

	  if(T_flag>1)
	  {
	    int e;

	    init_buf();
	    putc('%',stderr);
	    for(e=-1;e<csp-control_stack;e++)
	      putc(' ',stderr);
	    my_strcat(get_instruction_name(opcode+F_OFFSET));
	    my_putchar('(');
	    e=num_arg>-1?num_arg:instrs[opcode].min_arg;
	    save_style=SAVE_AS_ONE_LINE;
	    for(e=-e+1;e<=0;e++)
	    {
	      save_svalue(sp+e);
	      if(e) my_putchar(',');
	    }
	    my_putchar(')');
	    fprintf(stderr,"%s\n",return_buf());
	  }

	  /*
	   * Execute the opcodes. The number of arguments are correct,
	   * and the type of the two first arguments are also correct.
	   */

	  (instrs[opcode].efunc)(num_arg,argp);

	  if(T_flag>1)
	  {
	    int e;

	    putc('%',stderr);
	    for(e=-1;e<csp-control_stack;e++)
	      putc(' ',stderr);
	    save_style=SAVE_AS_ONE_LINE;
	    if((instrs[opcode].ret_type & TYPE_MOD_MASK)!=TYPE_VOID)
	    {
	      init_buf();
	      save_svalue(sp);
	      fprintf(stderr,"return: %s\n",return_buf());
	    }
	  }
#ifdef DEBUG
	  if ((expected_stack && expected_stack != sp) ||
	      sp < fp + csp->num_local_variables - 1)
	  {
	    fatal("Bad stack after evaluation. Opcode %d, num arg %d\n",
		  opcode+F_OFFSET, num_arg);
	  }
#endif /* DEBUG */
	}else{
	  fatal("Undefined opcode %s (%d)\n",
		get_instruction_name(opcode+F_OFFSET),opcode);
	}
      }else if(opcode-(F_MAX_OPCODE-F_OFFSET)<
	       current_object->prog->num_function_ptrs){
	opcode+=F_OFFSET-F_MAX_OPCODE;
	check_eval_cost();
	argp=*(mark_sp--);
	num_arg=sp-argp;
	argp++;
	apply_lambda_low(current_object,opcode+function_index_offset,num_arg,0);
	if(sp<argp) push_zero();
      }else{
	fatal("Undefined opcode %s (%d)\n",
	      get_instruction_name(opcode+F_OFFSET),opcode);
      }
      break;
      
      CASE(F_ASSIGN); f_assign(); break;
      CASE(F_ASSIGN_AND_POP); f_assign_and_pop(); break;

      CASE(F_NOT);
      if(sp->type==T_NUMBER)
      {
	sp->u.number=!sp->u.number;
      }else{
	free_svalue(sp);
	SET_TO_ZERO(*sp);
      }
      break;

      CASE(F_COMPL);
      if(sp->type!=T_NUMBER) error("Bad argument to ~\n");
      sp->u.number=~sp->u.number;
      break;

      CASE(F_NEGATE);
      if(sp->type==T_NUMBER) sp->u.number=-sp->u.number;
      else if(sp->type==T_FLOAT) sp->u.fnum=-sp->u.fnum;
      else error("Bad argument to unary -\n");
      break;

      CASE(F_EQ); tmp= is_eq(sp-1,sp); pop_n_elems(2); push_number(tmp); break;
      CASE(F_NE); tmp=!is_eq(sp-1,sp); pop_n_elems(2); push_number(tmp); break;
      CASE(F_LT); tmp= is_lt(sp-1,sp); pop_n_elems(2); push_number(tmp); break;
      CASE(F_LE); tmp=!is_gt(sp-1,sp); pop_n_elems(2); push_number(tmp); break;
      CASE(F_GT); tmp= is_gt(sp-1,sp); pop_n_elems(2); push_number(tmp); break;
      CASE(F_GE); tmp=!is_lt(sp-1,sp); pop_n_elems(2); push_number(tmp); break;

      CASE(F_CAST_TO_OBJECT); f_cast_to_object(); break;
      CASE(F_CAST_TO_STRING); f_cast_to_string(); break;
      CASE(F_CAST_TO_INT); f_cast_to_int(); break;
      CASE(F_CAST_TO_FLOAT); f_cast_to_float(); break;
      CASE(F_CAST_TO_FUNCTION); f_cast_to_function(); break;
      CASE(F_CAST_TO_REGEXP); f_cast_to_regexp(); break;

      CASE(F_RSH);
      if (sp->type != T_NUMBER) bad_arg(2,F_RSH);
      sp--;
      if (sp->type != T_NUMBER) bad_arg(1,F_RSH);
      sp->u.number = sp[0].u.number >> sp[1].u.number;
      break;

      CASE(F_LSH);
      if (sp->type != T_NUMBER) bad_arg(2,F_LSH);
      sp--;
      if (sp->type != T_NUMBER) bad_arg(1,F_LSH);
      sp->u.number = sp[0].u.number << sp[1].u.number;
      break;

      CASE(F_MULTIPLY); f_multiply(); break;
      CASE(F_DIVIDE); f_divide(); break;
      CASE(F_MOD); f_mod(); break;
      CASE(F_SUBTRACT); f_subtract(); break;
      CASE(F_OR); f_or(); break;
      CASE(F_AND); f_and(); break;
      CASE(F_XOR); f_xor(); break;

      CASE(F_INC_AND_POP); f_inc_and_pop(); break;
      CASE(F_DEC_AND_POP); f_dec_and_pop(); break;
      CASE(F_POST_DEC); f_post_dec(); break;
      CASE(F_POST_INC); f_post_inc(); break;
      CASE(F_INC); f_inc(); break;
      CASE(F_DEC); f_dec(); break;

      CASE(F_PUSH_ARRAY); f_push_array(); break;
      CASE(F_SWAP_VARIABLES); f_swap_variables(); break;
      CASE(F_SWAP); f_swap(); break;
      CASE(F_INDIRECT); f_indirect(); break;

      CASE(F_PUSH_INDEXED_LVALUE)
      {
	struct lvalue *l;
	l=push_indexed_lvalue(sp-1,sp);
	pop_stack();
	sp->type=T_LVALUE;
	sp->u.lvalue=l;
	break;
      }

      CASE(F_INDEX)
      {
	struct svalue s;
	struct lvalue *l;
	l=push_indexed_lvalue(sp-1,sp);
	lvalue_to_svalue_no_free(&s,l);
	free_lvalue(l);
	pop_stack();
	pop_stack();
	sp++;
	*sp=s;
	break;
      }

      CASE(F_ADD_INT);
      if(sp[0].type==T_NUMBER || sp[-1].type==T_NUMBER)
      {
	sp--;
	sp[0].u.number+=sp[1].u.number;
      }else{
	f_sum(2,sp-1);
      }
      break;

      CASE(F_ADD);
      if(sp[0].type==sp[-1].type)
      {
	switch(sp[0].type)
	{
	case T_NUMBER: 
	  sp--;
	  sp[0].u.number+=sp[1].u.number;
	  break;
	case T_FLOAT:
	  sp--;
	  sp[0].u.fnum+=sp[1].u.fnum;
	  break;
	default:
	  f_sum(2,sp-1);
	}
      }else{
	f_sum(2,sp-1);
      }
      break;

      CASE(F_PUSH_LTOSVAL);
      if(sp->type!=T_LVALUE) error("RHS: not lvalue!\n");
      sp++;
      lvalue_to_svalue_no_free(sp,sp[-1].u.lvalue);
      break;

      CASE(F_PUSH_LTOSVAL2);
      if(sp[-1].type!=T_LVALUE)	error("RHS: not lvalue!\n");
      sp[1]=*sp;
      lvalue_to_svalue_no_free(sp,sp[-1].u.lvalue);
      sp++;
      /* this will make some things faster */
      if(sp[-2].u.lvalue->ptr.sval)
      {
	if(sp[-2].u.lvalue->rttype==T_ANY &&
	   sp[-2].u.lvalue->ptr.sval->type==T_POINTER)
	  free_svalue(sp[-2].u.lvalue->ptr.sval);
	else
	  if(sp[-2].u.lvalue->rttype==T_POINTER)
	    free_short_svalue(sp[-2].u.lvalue->ptr.uval,T_POINTER);
      }
      break;

      CASE(F_CATCH)
      {
	/*
	 * Catch/Throw errors in system or other peoples routines.
	 */
	extern jmp_buf error_recovery_context;
	extern int error_recovery_context_exists;
	extern struct svalue catch_value;
	char *new_pc;
	struct svalue *save_sp;

	new_pc=get_relative_offset();

	save_sp=sp;
	push_control_stack(0);
	csp->num_local_variables = 0; /* No extra variables */
	csp->pc = new_pc;
	csp->num_local_variables = (csp-1)->num_local_variables; /* marion */
	/*
	 * Save some global variables that must be restored separately
	 * after a longjmp. The stack will have to be manually popped all
	 * the way.
	 */
	push_pop_error_context (1);
	
	/* signal catch OK - print no err msg */
	error_recovery_context_exists = 2;
	if (setjmp(error_recovery_context))
	{
	  /*
	   * They did a throw() or error. That means that the control
	   * stack must be restored manually here.
	   * Restore the value of expected_stack also. It is always 0
	   * for catch().
	   */
	  push_pop_error_context (-1);
	  pop_control_stack();
	  assign_svalue_no_free(++sp, &catch_value);
	}else{
	  /* next error will return 1 by default */

	  free_svalue(&catch_value);
	  SET_TO_ONE(catch_value);
	  eval_instruction(pc);
	  pop_control_stack();
	  push_pop_error_context(0);
	  while(sp>save_sp) pop_stack();
	  push_zero();
	}
	break;
      }

      CASE(F_STRING);
      push_shared_string(copy_shared_string(current_prog->strings[EXTRACT_UWORD(pc)]));
      pc+=sizeof(short);
      break;

      CASE(F_SHORT_STRING);
      push_shared_string(copy_shared_string(current_prog->strings[EXTRACT_UCHAR(pc)]));
      pc++;
      break;

      CASE(F_CONST0); push_zero(); break;
      CASE(F_CONST1); push_one(); break;
      CASE(F_CONST_1); push_number(-1); break;
      CASE(F_BYTE);
      push_number(EXTRACT_UCHAR(pc)+2);
      pc++;
      break;

      CASE(F_NEG_BYTE);
      push_number(-EXTRACT_UCHAR(pc)-2);
      pc++;
      break;

      CASE(F_SHORT);
      push_number(EXTRACT_WORD(pc));
      pc+=sizeof(short);
      break;

      CASE(F_NUMBER)
 	push_number(EXTRACT_INT(pc));
	pc+=sizeof(int);
	break;
      
      CASE(F_POP_N_ELEMS); pop_n_elems(EXTRACT_UCHAR(pc)); pc++; break;
      CASE(F_PUSH_COST); push_number(eval_cost); break;
      CASE(F_POP_VALUE); pop_stack(); break;
      CASE(F_MARK); *++mark_sp=sp; break;

      CASE(F_SSCANF)
      {
	int i;
	num_arg = EXTRACT_UCHAR(pc);
	pc++;
      
	i = inter_sscanf(num_arg);
	pop_n_elems(num_arg);
	push_number(i);
	break;
      }

      CASE(F_SWITCH)
      { 
	/* Ta-da, and the wonderful wizard of oz waved his wand, */ 
	/* and all amylaars code was gone.        /Profezzorn    */
	struct vector *m,*val,*ind;

	tmp=EXTRACT_UWORD(pc);
	pc+=sizeof(short);
	m=current_prog->switch_mappings[tmp];
	ind=m->item[0].u.vec;
	val=m->item[1].u.vec;

	tmp=search_alist(sp,ind);
	if(tmp<ind->size &&
	   (!alist_cmp(ind->item+tmp,sp) || val->item[tmp].subtype==2))
	{
	  pc=current_prog->program+val->item[tmp].u.number;
	}else{
	  pc=current_prog->program+m->item[2].u.number;
	}
	pop_stack();
	break;
      }

      CASE(F_ASSIGN_LOCAL);
      assign_svalue(fp+EXTRACT_UCHAR(pc),sp);
      pc++;
      break;

      CASE(F_ASSIGN_LOCAL_AND_POP);
      assign_svalue(fp+EXTRACT_UCHAR(pc),sp);
      pc++;
      pop_stack();
      break;

      CASE(F_ASSIGN_GLOBAL);
      tmp=EXTRACT_UCHAR(pc)+variable_index_offset;
#ifdef DEBUG
      if(tmp>current_object->prog->num_variables)
	fatal("Illegal variable access %d.\n",tmp);
#endif
      pc++;
      assign_short_svalue(current_object->variables+tmp,
			  sp,
			  current_object->prog->variable_names[tmp].rttype);
      break;

      CASE(F_ASSIGN_GLOBAL_AND_POP);
      tmp=EXTRACT_UCHAR(pc)+variable_index_offset;
#ifdef DEBUG
      if(tmp>current_object->prog->num_variables)
	fatal("Illegal variable access %d.\n",tmp);
#endif
      pc++;
      assign_short_svalue(current_object->variables+tmp,
			  sp,
			  current_object->prog->variable_names[tmp].rttype);
      pop_stack();
      break;
      
      CASE(F_PUSH_LOCAL_LVALUE);
      lsp++;
      if(lsp>=lvalues+LVALUES)
	error("Lvalue stack overflow.\n");
      lsp->ptr.sval = fp + EXTRACT_UCHAR(pc);
      lsp->type=LVALUE_LOCAL;
      lsp->rttype=T_ANY;
      pc++;
      sp++;
      sp->type = T_LVALUE;
      sp->u.lvalue=lsp;
      break;

      CASE(F_LOCAL);
      assign_svalue_no_free(++sp, fp + EXTRACT_UCHAR(pc));
      pc++;
      break;

      CASE(F_PUSH_GLOBAL_LVALUE);
      tmp=EXTRACT_UCHAR(pc)+variable_index_offset;
#ifdef DEBUG
      if(tmp>current_object->prog->num_variables)
	fatal("Illegal variable access %d.\n",(int)tmp);
#endif
      pc++;
      lsp++;
      if(lsp>=lvalues+LVALUES)	error("Lvalue stack overflow.\n");
      lsp->ptr.sval=(struct svalue *)(current_object->variables+tmp);
      if(current_object->prog->variable_names[tmp].rttype==T_ANY)
      {
	lsp->type=LVALUE_GLOBAL;
	lsp->rttype=T_ANY;
      }else{
	lsp->type=LVALUE_SHORT_GLOBAL;
	lsp->rttype=current_object->prog->variable_names[tmp].rttype;
      }
      sp++;
      sp->type = T_LVALUE;
      sp->u.lvalue=lsp;
      break;

      CASE(F_GLOBAL);
      tmp=EXTRACT_UCHAR(pc)+variable_index_offset;
#ifdef DEBUG
      if(tmp>current_object->prog->num_variables)
	fatal("Illegal variable access %d.\n",(int)tmp);
#endif
      pc++;
      sp++;
      if(current_object->prog->variable_names[tmp].rttype==T_ANY)
      {
	assign_svalue_no_free(sp,(struct svalue *)
			      (current_object->variables+tmp));
      }else{
	assign_svalue_from_short_no_free(sp,
					 current_object->variables+tmp,
					 current_object->prog->variable_names[tmp].rttype);
      }
      break;

      CASE(F_SHORT_BRANCH);
      pc-=EXTRACT_UCHAR(pc);
      check_eval_cost();
      break;

      CASE(F_SHORT_BRANCH_WHEN_NON_ZERO);
      if (sp->type != T_NUMBER || sp->u.number != 0)
	pc-=EXTRACT_UCHAR(pc);
      else
	pc++;
      pop_stack();
      check_eval_cost();
      break;

      CASE(F_SHORT_BRANCH_WHEN_ZERO);
      if (sp->type == T_NUMBER && sp->u.number == 0)
	pc-=EXTRACT_UCHAR(pc);
      else
	pc++;
      pop_stack();
      check_eval_cost();
      break;

      CASE(F_BRANCH_WHEN_NON_ZERO);
      if (sp->type != T_NUMBER || sp->u.number != 0)
	pc=get_relative_offset();
      else
	pc+=4;
      pop_stack();
      check_eval_cost();
      break;

      CASE(F_BRANCH_WHEN_ZERO); 
      if (sp->type == T_NUMBER && sp->u.number == 0)
	pc=get_relative_offset();
      else
	pc+=4;
      pop_stack();
      check_eval_cost();
      break;

      CASE(F_BRANCH);
      pc=get_relative_offset();
      check_eval_cost();
      break;

      CASE(F_PUSH_SIMUL_EFUN);
      tmp=EXTRACT_UWORD(pc);
      pc+=sizeof(short);
      if(simul_efuns[tmp].fun.type==T_FUNCTION)
      {
	if(simul_efuns[tmp].fun.u.ob->flags & O_DESTRUCTED)
	{
	  free_svalue(&(simul_efuns[tmp].fun));
	  push_zero();
	}else{
	  *(++sp)=simul_efuns[tmp].fun;
	  add_ref(sp->u.ob,"Push simul efun");
	}
      }else{
	push_zero();
      }
      break;

      CASE(F_CONSTANT_FUNCTION);
      tmp=EXTRACT_UWORD(pc);
      pc+=sizeof(short);

    push_function:
      if(current_object->flags & O_DESTRUCTED)
      {
	push_zero();
      }else{
	sp++;
	sp->type=T_FUNCTION;
	sp->subtype=tmp+function_index_offset;
	sp->u.ob=current_object;
	add_ref(current_object,"constant_function");
      }
      break;

      CASE(F_SHORT_CONSTANT_FUNCTION);
      tmp=EXTRACT_UCHAR(pc);
      pc++;
      goto push_function;

      CASE(F_LOR); 
      if (sp->type != T_NUMBER || sp->u.number != 0)
      {
	pc=get_relative_offset();
      }else{
	pop_stack();
	pc+=4;
      }
      break;

      CASE(F_LAND);
      if (sp->type == T_NUMBER && sp->u.number == 0)
      {
	pc = get_relative_offset();
      }else{
	pop_stack();
	pc+=4;
      }
      break;

      CASE(F_DUP);
      sp++;
      assign_svalue_raw(sp, sp-1);
      break;

      CASE(F_FLOAT);
      sp++;
      sp->type=T_FLOAT;
      MEMCPY((char *)&(sp->u.fnum),pc,sizeof(float));
      pc+=sizeof(float);
      break;

      CASE(F_CONSTANT);
      tmp=EXTRACT_UWORD(pc);
      pc+=sizeof(short);
      assign_svalue_no_free(++sp,current_prog->constants+tmp);
      break;

      CASE(F_SHORT_CONSTANT);
      assign_svalue_no_free(++sp,current_prog->constants+EXTRACT_UCHAR(pc));
      pc++;
      break;

      CASE(F_FOREACH);
      if((sp-2)->type != T_POINTER) bad_arg(1,F_FOREACH);
      if((sp-1)->type != T_LVALUE) bad_arg(2,F_FOREACH);
      if(sp->type != T_NUMBER) bad_arg(3,F_FOREACH);
      
      if(sp->u.number<(sp-2)->u.vec->size)
      {
	assign((sp-1)->u.lvalue,&(sp-2)->u.vec->item[sp->u.number++]);
	pc=get_relative_offset();
      }else{
	pc+=4;
      }
      check_eval_cost();
      break;

      CASE(F_INC_LOOP)
      {
	int *i;
	if(sp[-1].type != T_NUMBER) bad_arg(1,F_INC_LOOP);
	if(sp->type != T_LVALUE) bad_arg(2,F_INC_LOOP);
	i=lvalue_to_intp(sp->u.lvalue,"++Loop over non-integer.\n");
	if(!i) error("Too complex value for ++loop.\n");
    
	if((++*i)<sp[-1].u.number)
	  pc=get_relative_offset();
	else
	  pc+=4;
	check_eval_cost();
	break;
      }

      CASE(F_DEC_LOOP)
      {
	int *i;
	if(sp[-1].type != T_NUMBER) bad_arg(1,F_DEC_LOOP);
	if(sp->type != T_LVALUE) bad_arg(2,F_DEC_LOOP);
	i=lvalue_to_intp(sp->u.lvalue,"--Loop over non-integer.\n");
	if(!i) error("Too complex value for --loop.\n");
    
	if((--*i)>sp[-1].u.number)
	  pc=get_relative_offset();
	else
	  pc+=4;
	check_eval_cost();
	break;
      }

      CASE(F_INC_NEQ_LOOP)
      {
	int *i;
	if(sp[-1].type != T_NUMBER) bad_arg(1,F_INC_NEQ_LOOP);
	if(sp->type != T_LVALUE) bad_arg(2,F_INC_NEQ_LOOP);
	i=lvalue_to_intp(sp->u.lvalue,"++Loop over non-integer.\n");
	if(!i) error("Too complex value for ++loop.\n");

	if((++*i)!=sp[-1].u.number)
	  pc=get_relative_offset();
	else
	  pc+=4;
	check_eval_cost();
	break;
      }

      CASE(F_DEC_NEQ_LOOP)
      {
	int *i;
	if(sp[-1].type != T_NUMBER) bad_arg(1,F_DEC_NEQ_LOOP);
	if(sp->type != T_LVALUE) bad_arg(2,F_DEC_NEQ_LOOP);
	i=lvalue_to_intp(sp->u.lvalue,"--Loop over non-integer.\n");
	if(!i) error("Too complex value for --loop.\n");

	if((--*i)!=sp[-1].u.number)
	  pc=get_relative_offset();
	else
	  pc+=4;
	check_eval_cost();
	break;
      }

      CASE(F_TAILRECURSE)
      {
	int fun,e,num_arg;
	struct object *ob;
	struct svalue *argp;
	extern struct svalue *fp;
	extern int T_flag;

	argp=*(mark_sp--);
	num_arg=sp-argp;
	argp++;
	fun=argp[0].subtype;

	if(!IS_TYPE(argp[0],BT_NUMBER | BT_FUNCTION | BT_POINTER))
	  bad_arg(1,F_CALL_FUNCTION);
	if(argp[0].type==T_NUMBER || (argp[0].type==T_FUNCTION && fun==-1))
	{
	  while(sp>=fp) pop_stack();
	  push_zero();
	  return;
	}
	
	check_eval_cost();
	ob=argp[0].u.ob;
  
	/* can't tailrecurse here */
	if(argp[0].type!=T_FUNCTION && ob!=current_object)
	{
	  /* in this case we still saved some stack,
	   * and one revolution in the mainloop
	   */
	  /* get rid of local variables, move arguments down the stack */
	  if(fp!=argp)
	    for(e=0;e<num_arg;e++)
	      assign_svalue(fp+e,argp+e);
	  while(fp+num_arg<=sp) pop_stack();

	  f_call_function(num_arg,fp);
	  return;
	}else{
	  struct function_p *pr;
	  struct function *func;

	  num_arg--;
	  argp++;
	  if(fp!=argp)
	    for(e=0;e<num_arg;e++)
	      assign_svalue(fp+e,argp+e);
	  while(fp+num_arg<=sp) pop_stack();

	  pr=ob->prog->function_ptrs+fun;
	  func=ob->prog->inherit[pr->prog].prog->functions+pr->fun;
	  if(T_flag)
	  {
	    int e;
	    init_buf();
	    putc('%',stderr);
	    for(e=-1;e<csp-control_stack;e++) putc(' ',stderr);
	    save_object_desc(ob);
	    my_strcat("->");
	    my_strcat(current_prog->inherit[pr->prog].prog->name);
	    my_strcat("::");
	    my_strcat(func->name);
	    my_putchar('(');
	    save_style=SAVE_AS_ONE_LINE;
	    for(e=-num_arg+1;e<=0;e++)
	    {
	      save_svalue(sp+e);
	      if(e) my_putchar(',');
	    }
	    my_putchar(')');
	    fprintf(stderr,"%s\n",simple_free_buf());
	  }

	  pop_control_stack();
	  push_control_stack(func);
	  csp->num_local_variables = num_arg;
	  setup_new_frame(pr,ob->prog);
	  current_object=ob;
	  pc=current_prog->program+func->offset;
	}
	break;
      }

      CASE(F_RETURN);
      if(sp>fp)
      {
	assign_svalue(fp,sp);
	pop_n_elems(sp-fp);
      }
      /* fall through */
      CASE(F_DUMB_RETURN);
      return;
    }
  }
}

static int ccsave;
#ifdef LACIP
int refused=0;
#endif

int apply_lambda_low(struct object *ob,int fun,int num_arg,int ignorestatic)
{
  struct function_p *pr;
  struct function *func;
  struct program *progp;
  struct control_stack *save_csp;
  int csave=eval_cost;
  int cccsave=ccsave;

#ifdef LACIP
  if(!refused) refusemouse();
  refused=1;
#endif

#ifdef MALLOC_DEBUG
  check_sfltable();
#endif

  if(fun==-1)
  {
    pop_n_elems(num_arg);
    return 0;
  }
  ob->time_of_ref = current_time;
  /*
   * This object will now be used, and is thus a target for
   * reset later on (when time due).
   */
  ob->flags &= ~O_RESET_STATE;

  progp = ob->prog;
#ifdef DEBUG
  if (ob->flags & O_DESTRUCTED)
    fatal("apply_lambda() on destructed object\n");
#endif

  pr=progp->function_ptrs+fun;
  /* Static functions may not be called from outside. */
  /* now they may be called by ROOT (supersture) */
  if(pr->flags & NAME_UNDEFINED)
    error("Calling undefined function.\n");

  if(!ignorestatic &&
     current_object && 
     (pr->type & (TYPE_MOD_STATIC|TYPE_MOD_PRIVATE)) &&
     current_object != ob)
  {
    assert_master_ob_loaded();
    if(!current_object->eff_user || current_object->eff_user!=master_ob->user)
    {
      pop_n_elems(num_arg);
      return 0;
    }
  }
  func=progp->inherit[pr->prog].prog->functions+pr->fun;
  if(T_flag)
  {
    int e;

    init_buf();
    putc('%',stderr);
    for(e=-1;e<csp-control_stack;e++) putc(' ',stderr);
    save_object_desc(ob);
    my_strcat("->");
    my_strcat(progp->inherit[pr->prog].prog->name);
    my_strcat("::");
    my_strcat(func->name);
    my_putchar('(');
    save_style=SAVE_AS_ONE_LINE;
    for(e=-num_arg+1;e<=0;e++)
    {
      save_svalue(sp+e);
      if(e) my_putchar(',');
    }
    my_putchar(')');
    fprintf(stderr,"%s\n",return_buf());
  }
  push_control_stack(func);
  csp->num_local_variables = num_arg;
  current_prog = progp;
  setup_new_frame(pr,progp);
  current_object = ob;
  save_csp = csp;
  eval_instruction(current_prog->program + func->offset);
  pop_control_stack();
#ifdef DEBUG
  if (save_csp-1 != csp)
    fatal("Bad csp after execution in apply_lambda_low\n");
#endif
  csave-=eval_cost+cccsave-ccsave;
  ob->cpu-=csave;
  ccsave-=csave;
  if(T_flag)
  {
    int e;
    save_style=SAVE_AS_ONE_LINE;
    putc('%',stderr);
    for(e=-1;e<csp-control_stack;e++) putc(' ',stderr);
    init_buf();
    save_svalue(sp);
    fprintf(stderr,"return: %s\n",return_buf());
  }
  return 1;
}

struct svalue ret_value = { T_NUMBER };

struct svalue *apply_lfun(int fun,
			  struct object *ob,
			  int num_arg,
			  int ignorestatic)
{
#ifdef DEBUG
    struct svalue *expected_sp;
    expected_sp = sp - num_arg;
    if(ob->flags & O_DESTRUCTED)
      fatal("Destructed object in apply_lfun.\n");
#endif
    if(fun<0 || fun>=num_lfuns)
      error("Number to apply_lfun out of range.\n");

    if(fun>=ob->prog->num_lfuns)
      return apply_shared(lfuns[fun],ob,num_arg,ignorestatic);

#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
    fun=ob->prog->lfuns[fun];
    if(fun<0)
    {
      pop_n_elems(num_arg);
      return 0; /* flash */
    }

    if (apply_lambda_low(ob,fun, num_arg,ignorestatic) == 0)
    {
      fatal("Something went wrong when calling lfun!\n");
    }
    assign_svalue(&ret_value, sp);
    pop_stack();
#ifdef DEBUG
    if (expected_sp != sp)
	fatal("Corrupt stack pointer.\n");
#endif
    return &ret_value;
}

struct svalue *apply_numbered_fun(struct object *o,
				  int fun,
				  int num_arg,
				  int ignorestatic)
{
#ifdef DEBUG
    struct svalue *expected_sp;
    expected_sp = sp - num_arg;
#endif
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif

    if(o->flags & O_DESTRUCTED)
      error("Apply_lambda to function fun in destructed object.\n");

    if (apply_lambda_low(o,fun, num_arg,ignorestatic) == 0)
	return 0;
    assign_svalue(&ret_value, sp);
    pop_stack();
#ifdef DEBUG
    if (expected_sp != sp)
      fatal("Corrupt stack pointer.\n");
#endif
    return &ret_value;
}

struct svalue *apply_lambda(struct svalue *lambda,
			    int num_arg,
			    int ignorestatic)
{
  if(lambda->type!=T_FUNCTION)
    error("Apply_lambda to non-function.\n");

  return apply_numbered_fun(lambda->u.ob,lambda->subtype, num_arg,ignorestatic);
}


/*
 * Apply a fun 'fun' to the program in object 'ob', with
 * 'num_arg' arguments (already pushed on the stack).
 * If the function is not found, search in the object pointed to by the
 * inherit pointer.
 * If the function name starts with '::', search in the object pointed out
 * through the inherit pointer by the current object. The 'current_object'
 * stores the base object, not the object that has the current function being
 * evaluated. Thus, the variable current_prog will normally be the same as
 * current_object->prog, but not when executing inherited code. Then,
 * it will point to the code of the inherited object. As more than one
 * object can be inherited, the call of function by index number has to
 * be adjusted. The function number 0 in a superclass object must not remain
 * number 0 when it is inherited from a subclass object. The same problem
 * exists for variables. The global variables function_index_offset and
 * variable_index_offset keep track of how much to adjust the index when
 * executing code in the superclass objects.
 *
 * There is a special case when called from the heart beat, as
 * current_prog will be 0. When it is 0, set current_prog
 * to the 'ob->prog' sent as argument.
 *
 * Arguments are always removed from the stack.
 * If the function is not found, return 0 and nothing on the stack.
 * Otherwise, return 1, and a pushed return value on the stack.
 *
 * Note that the object 'ob' can be destructed. This must be handled by
 * the caller of apply().
 *
 * If the function failed to be called, then arguments must be deallocated
 * manually !
 */

INLINE static int low_find_shared_string_function(char *name,
						  struct program *prog)
{
  int max,min,tst;
  struct function_p *funcs,*funp;
  struct function *fun;
  unsigned short *funindex;

#ifdef MALLOC_DEBUG
  check_sfltable();
#endif

  if((funindex=prog->funindex))
  {
    funcs=prog->function_ptrs;
    max=prog->num_funindex;
    min=0;
    while(max!=min)
    {
      tst=(max+min)>>1;
      funp=funcs+funindex[tst];
      fun=prog->inherit[funp->prog].prog->functions+funp->fun;
      if(fun->name==name) return funindex[tst];
      if(fun->name>name)
      {
	max=tst;
      }else{
	min=tst+1;
      }
    }
  }else{
    int i,t;
    for(i=0;i<prog->num_function_ptrs;i++)
    {
      funp=prog->function_ptrs+i;
      if(funp->flags & (NAME_HIDDEN|NAME_UNDEFINED)) continue;
      fun=prog->inherit[funp->prog].prog->functions+funp->fun;
      if(fun->name!=name) continue;
      if(funp->flags & NAME_INHERITED)
      {
        if(funp->type & TYPE_MOD_PRIVATE) continue;
	for(t=0;t>=0 && t<prog->num_function_ptrs;t++)
	{
	  struct function_p *funpb;
	  struct function *funb;

	  if(t==i) continue;
	  funpb=prog->function_ptrs+i;
	  funb=prog->inherit[funpb->prog].prog->functions+funpb->fun;

	  if(fun->name==funb->name) t=-10;
	}
	if(t<0) continue;
      }
      return i;
    }
  }
  return -1;
}

#ifdef FIND_FUNCTION_HASHSIZE
struct ff_hash
{
  char *name;
  int id;
  int fun;
};
#endif

INLINE int find_shared_string_function(char *name,struct program *prog)
{
#ifdef FIND_FUNCTION_HASHSIZE
  static struct ff_hash cache[FIND_FUNCTION_HASHSIZE];
  extern struct program fake_prog;
  if(prog!=&fake_prog)
  {
    unsigned int hashval;
    hashval=(unsigned int)name;
    hashval+=prog->id;
    hashval^=(unsigned int)prog;
    hashval-=*name;
    hashval%=FIND_FUNCTION_HASHSIZE;
    if(cache[hashval].name==name && cache[hashval].id==prog->id)
      return cache[hashval].fun;

    cache[hashval].name=name;
    cache[hashval].id=prog->id;
    return cache[hashval].fun=low_find_shared_string_function(name,prog);
  }
#endif /* FIND_FUNCTION_HASHSIZE */

  return low_find_shared_string_function(name,prog);
}

int find_function(char *name,struct program *prog)
{
  name=findstring(name);
  if(!name) return -1;
  return find_shared_string_function(name,prog);
}

/* needs fun to be a shared string */
int apply_lower(char *fun, struct object *ob, int num_arg,int ignorestatic)
{
  int function;

  if (fun[0] == ':')
    error("Illegal function call (colon in function name)\n");

#ifdef DEBUG
  if (ob->flags & O_DESTRUCTED)
    fatal("apply() on destructed object\n");

  if(fun!=debug_findstring(fun))
    fatal("apply_lower on nonshared string\n");
#endif

  function=find_shared_string_function(fun,ob->prog);
#ifdef DEBUG
  if(function>=0)
  {
    struct function_p *funp;
    struct function *func;
    funp=ob->prog->function_ptrs+function;
    func=ob->prog->inherit[funp->prog].prog->functions+funp->fun;
	
    if(strcmp(func->name,fun))
      fatal("I didn't call that!\n");
  }
#endif
  return apply_lambda_low(ob,function,num_arg,ignorestatic);
}

INLINE int apply_low(char *fun, struct object *ob, int num_arg,int ignorestatic)
{
  fun=findstring(fun);
  if(!fun)
  {
    pop_n_elems(num_arg);
    return 0;
  }
  return apply_lower(fun,ob,num_arg,ignorestatic);
}

/*
 * Arguments are supposed to be
 * pushed (using push_string() etc) before the call. A pointer to a
 * 'struct svalue' will be returned. It will be a null pointer if the called
 * function was not found. Otherwise, it will be a pointer to a static
 * area in apply(), which will be overwritten by the next call to apply.
 * Reference counts will be updated for this value, to ensure that no pointers
 * are deallocated.
 */

struct svalue *apply(char *fun,struct object *ob,int num_arg,int ignorestatic)
{
#ifdef DEBUG
  struct svalue *expected_sp;
  expected_sp = sp - num_arg;
#endif
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  if(ob->flags & O_DESTRUCTED)
    error("Apply to fun: %s  in destructed object.",fun);
  if (apply_low(fun, ob, num_arg,ignorestatic) == 0)
    return 0;
  assign_svalue(&ret_value, sp);
  pop_stack();
#ifdef DEBUG
  if (expected_sp != sp)
    fatal("Corrupt stack pointer.\n");
#endif
  return &ret_value;
}

struct svalue *apply_shared(char *fun,
			    struct object *ob,
			    int num_arg,
			    int ignorestatic)
{
#ifdef DEBUG
  struct svalue *expected_sp;
  expected_sp = sp - num_arg;
#endif
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  if(!fun) return 0;
  if(ob->flags & O_DESTRUCTED)
    error("Apply to fun: %s  in destructed object.",fun);
  if (apply_lower(fun, ob, num_arg,ignorestatic) == 0)
    return 0;
  assign_svalue(&ret_value, sp);
  pop_stack();
#ifdef DEBUG
  if (expected_sp != sp)
    fatal("Corrupt stack pointer.\n");
#endif
  return &ret_value;
}

/*
 * this is a "safe" version of apply
 * this allows you to have dangerous driver mudlib dependencies
 * and not have to worry about causing serious bugs when errors occur in the
 * applied function and the driver depends on being able to do something
 * after the apply. (such as the ed exit function, and the net_dead function)
 */

int safe_apply_lambda_low(struct object *ob,
			  int fun,
			  int num_arg)
{
  extern jmp_buf error_recovery_context;
  extern int error_recovery_context_exists;
  VOLATILE int ret;
  struct svalue *save_sp;

  debug(32768, ("safe_apply: before sp = %d\n", sp));
  ret = 0;

  save_sp=sp-num_arg;

  push_control_stack(0);
  sp-=num_arg; /* fool push_pop_error_context */
  push_pop_error_context(1);
  sp+=num_arg;
  error_recovery_context_exists=3;
  if (!setjmp(error_recovery_context))
  {
    if(!(ob->flags & O_DESTRUCTED))
      ret = apply_lambda_low(ob,fun,num_arg,1);
    pop_control_stack();
    push_pop_error_context(0);
  }else{
    ret = 0;
    push_pop_error_context(-1);
    pop_control_stack();

    fprintf(stderr,"Error within safe apply call.\n");
    if(catch_value.type==T_STRING)
      fprintf(stderr,strptr(&catch_value));

  }
  debug(32768, ("safe_apply: after sp = %d\n", sp));
  return ret;
}

struct svalue *safe_apply_lambda(struct svalue *lambda,int num_arg)
{
  int ret;

  if (lambda->type!=T_FUNCTION)
  {
    fprintf(stderr,"safe_apply_lambda() on non-function\n");
    pop_n_elems(num_arg);
    return NULL;
  }
  if (lambda->u.ob->flags & O_DESTRUCTED)
  {
    fprintf(stderr,"safe_apply_lambda() on destructed object\n");
    pop_n_elems(num_arg);
    return NULL;
  }

  ret=safe_apply_lambda_low(lambda->u.ob,lambda->subtype,num_arg);
  if (!ret) return 0;
  assign_svalue(&ret_value, sp);
  pop_stack();
  return &ret_value;
}

struct svalue *safe_apply(char *fun,struct object *ob,int num_arg)
{
  int function,ret;

  if (fun[0] == ':')
  {
    fprintf(stderr,"Illegal safe_apply call (colon in function name)\n");
    pop_n_elems(num_arg);
    return NULL;
  }

  if (ob->flags & O_DESTRUCTED)
  {
    fprintf(stderr,"safe_apply() on destructed object\n");
    pop_n_elems(num_arg);
    return NULL;
  }

  function=find_function(fun,ob->prog);
#ifdef DEBUG
  if(function>=0 && strcmp(ob->prog->functions[function].name,fun))
  {
    fatal("I didn't call that!\n");
  }
#endif
  ret=safe_apply_lambda_low(ob,function,num_arg);
  if (!ret) return 0;
  assign_svalue(&ret_value, sp);
  pop_stack();
  return &ret_value;
}

char *function_exists_in_prog(char *fun,struct program *prog)
{
  int function;
  struct function_p *funp;
  struct function *func;

  function=find_function(fun,prog);
  if(function==-1)
  {
    return 0;
  }else{
    funp=prog->function_ptrs+function;
    func=prog->inherit[funp->prog].prog->functions+funp->fun;
    return func->name;
  }
}

char *function_exists(char *fun, struct object *ob)
{
#ifdef DEBUG
  if (ob->flags & O_DESTRUCTED)
    fatal("function_exists() on destructed object\n");
#endif
  return function_exists_in_prog(fun,ob->prog);
}


static int get_small_number(signed char **q)
{
  int ret;
  switch(ret=(*q)++[0])
  {
  case -127:
    ret=EXTRACT_WORD(*q);
    *q+=2;
    return ret;

  case -128:
    ret=EXTRACT_INT(*q);
    *q+=4;
    return ret;

  default:
    return ret;
  }
}


static int get_line_number(char *pc,struct program *prog)
{
  int off,line,offset;
  signed char *cnt;

  if (prog == 0) return 0;
  offset = pc - prog->program;

#ifdef DEBUG
  if (offset > prog->program_size || offset<0)
    fatal("Illegal offset %d in object %s\n", offset, prog->name);
#endif

  cnt=prog->line_numbers;
  off=line=0;

  while(cnt < prog->line_numbers + prog->num_line_numbers)
  {
    off+=get_small_number(&cnt);
    if(off > offset) return line;
    line+=get_small_number(&cnt);
  }
  return line;
}
   
/*
 * Write out a trace. If there is an heart_beat(), then return the
 * object that had that heart beat.
 */
struct object *dump_trace(int how)
{
  struct control_stack *p;
  struct object *ret = 0;
#ifdef DEBUG
  int last_instructions PROT((void));
#endif

  if (current_prog == 0)
    return 0;
  if (csp < &control_stack[0])
  {
    (void)printf("No trace.\n");
    debug_message("No trace.\n");
    return 0;
  }
#ifdef DEBUG
  if (how)
    (void)last_instructions();
#endif
  for (p = &control_stack[0]; p < csp; p++) {
    (void)fprintf(stderr,"'%15s' in '%20s' ('%20s')line %d\n",
		  p[0].funp ? p[0].funp->name : "CATCH",
		  p[1].prog->name, p[1].ob->prog->name,
		  get_line_number(p[1].pc, p[1].prog));
    debug_message("'%15s' in '%20s' ('%20s')line %d\n",
		  p[0].funp ? p[0].funp->name : "CATCH",
		  p[1].prog->name, p[1].ob->prog->name,
		  get_line_number(p[1].pc, p[1].prog));
    if (p->funp && strcmp(p->funp->name, "heart_beat") == 0)
      ret = p->ob;
  }
  (void)fprintf(stderr,"'%15s' in '%20s' ('%20s')line %d\n",
		p[0].funp ? p[0].funp->name : "CATCH",
		current_prog->name, current_object->prog->name,
		get_line_number(pc, current_prog));
  debug_message("'%15s' in '%20s' ('%20s')line %d\n",
		p[0].funp ? p[0].funp->name : "CATCH",
		current_prog->name, current_object->prog->name,
		get_line_number(pc, current_prog));
  return ret;
}

int get_line_number_if_any()
{
  if (current_prog)
    return get_line_number(pc, current_prog);
  return 0;
}

/*
 * Reset the virtual stack machine.
 */
void reset_machine(int first)
{
  extern struct lvalue *lsp;
  extern struct lvalue lvalues[LVALUES];
  mark_sp=mark_stack-1;
  max_eval_cost=MAX_COST;
  csp = control_stack - 1;
  if (first)
    sp = start_of_stack - 1;
  else
    pop_n_elems(sp - start_of_stack + 1);
  lsp=lvalues-1;
}

#ifdef DEBUG

char *get_arg(int a,int b)
{
  int iarg;
  short sarg;
  static char buff[30];
  char *from;

  from = previous_pc[a];
  switch(previous_pc[b]-from)
  {
  case 2:
    iarg=(int)EXTRACT_UCHAR(from+1);
    break;

  case 3:
    ((char *)&sarg)[0] = from[1];
    ((char *)&sarg)[1] = from[2];
    iarg=(int)sarg;
    break;

  case 5:
    ((char *)&iarg)[0] = from[1];
    ((char *)&iarg)[1] = from[2];
    ((char *)&iarg)[2] = from[3];
    ((char *)&iarg)[3] = from[4];
    break;

  default: return "        ";
  }

  sprintf(buff, "%d", iarg);
  return buff;
}

int last_instructions()
{
  char *s,*s2;
  
  int i;
  i = last;
  do
  {
    if (previous_instruction[i] != 0)
    {
      s=get_arg(i, (i+1) % NELEM(previous_instruction));
      s2=get_instruction_name(previous_instruction[i]);

      fprintf(stderr,"%6x (%4d): %3d %8s %-25s (%d)\n",
	      (int)(previous_pc[i]),
	      previous_instruction[i],
	      previous_instruction_offset[i],s,s2,
	      stack_size[i] + 1);
    }
    i = (i + 1) % (sizeof previous_instruction / sizeof (int));
  } while (i != last);
  return last;
}

#endif /* DEBUG */

struct svalue *apply_master_ob(char *fun,int num_arg)
{
  extern struct object *master_ob;

  assert_master_ob_loaded();
  return apply(fun, master_ob, num_arg,1);
}

void assert_master_ob_loaded()
{
  char *tmp;
  extern struct object *master_ob;
  static int inside = 0;

  if (master_ob == 0 || master_ob->flags & O_DESTRUCTED)
  {
    /*
     * The master object has been destructed. Free our reference,
     * and load a new one.
     *
     * This test is needed because the master object is called from
     * yyparse() at an error to find the wizard name. However, and error
     * when loading the master object will cause a recursive call to this
     * point.
     *
     * The best solution would be if the yyparse() did not have to call
     * the master object to find the name of the wizard.
     */
    if (inside) {
      fprintf(stderr, "Failed to load master object.\n");
      exit(1);
    }
    if (master_ob)
    {
      fprintf(stderr, "assert_master_ob_loaded: Reloading master.c\n");
      free_object(master_ob, "assert_master_ob_loaded");
    }
    /*
     * Clear the pointer, in case the load failed.
     */
    master_ob = 0;
    inside = 1;

    tmp=getenv("LPC_MASTER");
    if(!tmp)
    {
      if(batch_mode)
      {
	tmp=(char *)alloca(strlen(BINDIR)+30);
	sprintf(tmp,"%s/master.c",BINDIR);
      }else{
	tmp="secure/master";
      }
    }

    /* master_ob is automatically set in give_uid_to_object */
    clone_object(tmp,0);
 
    if (master_ob == 0)
    {
	fprintf(stderr, "The file %s must be loadable.\n",tmp);
	exit(1);
    }

    inside = 0;
  }
}

/*
 * When an object is destructed, all references to it must be removed
 * from the stack.
 */
void remove_object_from_stack(struct object *ob)
{
  struct svalue *svp;

  for (svp = start_of_stack; svp <= sp; svp++)
  {
    if (IS_TYPE(*svp,BT_OBJECT | BT_FUNCTION))
    {
      int tmp=svp->type==T_OBJECT;
      if (svp->u.ob != ob)
	continue;
      free_object(svp->u.ob, "remove_object_from_stack");
      svp->type = T_NUMBER;
      svp->subtype = tmp?NUMBER_DESTRUCTED_OBJECT:NUMBER_DESTRUCTED_FUNCTION;
      svp->u.number = 0;
    }
  }
}

INLINE int strpref(char *p, char *s)
{
  while (*p)
    if (*p++ != *s++)
      return 0;
  return 1;
}
