#include <setjmp.h>
#include "global.h"
#include "lang.h"
#include "interpret.h"
#include "las.h"
#include "instrs.h"
#include "array.h"
#include "exec.h"
#include "object.h"
#include "simul_efun.h"
#include "incralloc.h"
#include "stralloc.h"
#include "main.h"
#include "operators.h"
#include "dynamic_buffer.h"
#include "lex.h"
#include "mapping.h"
#include "simulate.h"
#include "save_objectII.h"

#define LASDEBUG
int lasdebug=0;

static node *eval(node *);
static int eval_low(node *n);
static node *optimize(node *n,int needlval,node *parent);
static node *call_optimizer(node *n);

static struct vector *current_switch_mapping=0;
int *break_stack=0;
static int current_break,break_stack_size;
int *continue_stack=0;
static int current_continue,continue_stack_size;

struct program fake_prog;
struct object fake_object;

struct mem_block mem_block[NUMAREAS];

extern int exact_types;
extern int num_parse_error;
extern char **local_names;
extern unsigned short *type_of_locals;
extern int current_number_of_locals;
extern int current_line;
extern char *get_type_name(int);
void nice_print_tree(node *);

#define PC mem_block[A_PROGRAM].current_size

INLINE void add_to_mem_block(int n,char *data,int size)
{
  struct mem_block *mbp = &mem_block[n];
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  while (mbp->current_size + size > mbp->max_size) {
    mbp->max_size <<= 1;
    mbp->block = realloc((char *)mbp->block, mbp->max_size);
  }
  MEMCPY(mbp->block + mbp->current_size, data, size);
  mbp->current_size += size;
}

void ins_byte(unsigned char b,int area)
{
  add_to_mem_block(area, (char *)&b, 1);
}

/*
 * Store a 2 byte number. It is stored in such a way as to be sure
 * that correct byte order is used, regardless of machine architecture.
 * Also beware that some machines can't write a word to odd addresses.
 */
void ins_short(short l,int area)
{
  add_to_mem_block(area, (char *)&l, sizeof(short));
}

#if YYDEBUG
extern int yydebug;
#endif
void upd_short(int offset, short l)
{
#if YYDEBUG
  if(yydebug>1)
    fprintf(stderr,"Upd short %d %d\n",offset,l);
#endif
#ifdef HANDLES_UNALIGNED_MEMORY_ACCESS
  *((short *)(mem_block[A_PROGRAM].block+offset))=l;
#else
  mem_block[A_PROGRAM].block[offset + 0] = ((char *)&l)[0];
  mem_block[A_PROGRAM].block[offset + 1] = ((char *)&l)[1];
#endif
}

void upd_branch(int offset, int tmp)
{
#if YYDEBUG
  if(yydebug>1)
    fprintf(stderr,"Upd branch %d %d\n",offset,tmp);
#endif
  tmp-=offset;
#ifdef HANDLES_UNALIGNED_MEMORY_ACCESS
  *((int *)(mem_block[A_PROGRAM].block+offset))=tmp;
#else
  mem_block[A_PROGRAM].block[offset + 0] = ((char *)&tmp)[0];
  mem_block[A_PROGRAM].block[offset + 1] = ((char *)&tmp)[1];
  mem_block[A_PROGRAM].block[offset + 2] = ((char *)&tmp)[2];
  mem_block[A_PROGRAM].block[offset + 3] = ((char *)&tmp)[3];
#endif
}

short read_short(int offset)
{
#ifdef HANDLES_UNALIGNED_MEMORY_ACCESS
  return *((short *)(mem_block[A_PROGRAM].block+offset));
#else
  short l;
  ((char *)&l)[0] = mem_block[A_PROGRAM].block[offset + 0];
  ((char *)&l)[1] = mem_block[A_PROGRAM].block[offset + 1];
  return l;
#endif
}

/*
 * Store a 4 byte number. It is stored in such a way as to be sure
 * that correct byte order is used, regardless of machine architecture.
 */
void ins_long(long l,int area)
{
  add_to_mem_block(area, (char *)&l+0, sizeof(long));
}
#if YYDEBUG
extern int yydebug;
#endif

static int store_linenumbers=1;
static int last_line,last_pc;

void start_line_numbering() { last_pc=last_line=0; }

static void insert_small_number(int a,int area)
{
  if(a>-127 && a<=127)
  {
    ins_byte(a,area);
  }else if(a>=-32768 && a<32768){
    ins_byte(-127,area);
    ins_short(a,area);
  }else{
    ins_byte(-128,area);
    ins_long(a,area);
  }	
}

static void low_ins_f_byte(unsigned int b)
{
  if(store_linenumbers && last_line!=current_line)
  {
    insert_small_number(PC-last_pc,A_LINENUMBERS);
    insert_small_number(current_line-last_line,A_LINENUMBERS);
    last_line=current_line;
    last_pc=PC;
  }
#if YYDEBUG
  if(yydebug)
    fprintf(stderr,"Inserting f_byte %s (%d) at %d\n",get_instruction_name(b),b,
	    PC);
#endif
#if defined(LASDEBUG)
  if(lasdebug>1)
    if(store_linenumbers)
      fprintf(stderr,"Inserting f_byte %s (%d) at %d\n",get_instruction_name(b),b,
	      PC);
#endif
  b-=F_OFFSET;
#ifdef OPCPROF
  if(store_linenumbers) add_compiled(b);
#endif
  if(b>255)
  {
    switch(b/256)
    {
    case 1: low_ins_f_byte(F_ADD_256); break;
    case 2: low_ins_f_byte(F_ADD_512); break;
    case 3: low_ins_f_byte(F_ADD_768); break;
    case 4: low_ins_f_byte(F_ADD_1024); break;
    default:
      low_ins_f_byte(F_ADD_256X);
      ins_byte(b/256,A_PROGRAM);
    }
    b&=255;
  }
  ins_byte((unsigned char)b,A_PROGRAM);
}

static void ins_f_byte(unsigned int b)
{
  extern int T_flag;
  if(T_flag>2) low_ins_f_byte(F_WRITE_OPCODE);
  low_ins_f_byte(b);
}

static void ins_int(int i)
{
  if ( i == 0 ) {
    ins_f_byte(F_CONST0);
  }else if ( i == 1 ) {
    ins_f_byte(F_CONST1);
  } else if ( i == -1) {
    ins_f_byte(F_CONST_1);
  } else if ( i>1 && i<258) {
    ins_f_byte(F_BYTE); ins_byte(i-2,A_PROGRAM);
  } else if ( i<-1 && i>-258) {
    ins_f_byte(F_NEG_BYTE); ins_byte((1024-i-2)&0xff,A_PROGRAM);
  } else if( i>=-37768 && i<32768){
    ins_f_byte(F_SHORT); ins_short(i,A_PROGRAM);
  }else{
    ins_f_byte(F_NUMBER); ins_long(i,A_PROGRAM);
  }
}

void ins_float(float f)
{
  ins_f_byte(F_FLOAT);
  add_to_mem_block(A_PROGRAM,(char *)&f,sizeof(float));
}


/*
 * A mechanism to remember addresses on a stack. The size of the stack is
 * defined in config.h.
 */
int comp_stackp;
int comp_stack[COMPILER_STACK_SIZE];

void push_address()
{
 if (comp_stackp >= COMPILER_STACK_SIZE)
 {
   yyerror("Compiler stack overflow");
   comp_stackp++;
   return;
 }
 comp_stack[comp_stackp++] = PC;
}

void push_explicit(int address)
{
  if (comp_stackp >= COMPILER_STACK_SIZE)
  {
    yyerror("Compiler stack overflow");
    comp_stackp++;
    return;
  }
  comp_stack[comp_stackp++] = address;
}

int pop_address()
{
  if (comp_stackp == 0)
     fatal("Compiler stack underflow.\n");
  if (comp_stackp > COMPILER_STACK_SIZE)
  {
    --comp_stackp;
    return 0;
  }
  return comp_stack[--comp_stackp];
}

short store_prog_string(char *str)
{
  short i;
  char **p;

  p = (char **) mem_block[A_STRINGS].block;

  for (i=mem_block[A_STRINGS].current_size / sizeof str -1; i>=0; --i)
    if (p[i] == str)
      return i;

  str=copy_shared_string(str);
  add_to_mem_block(A_STRINGS, (char *)&str, sizeof str);
  return mem_block[A_STRINGS].current_size / sizeof str - 1;
}

int store_constant(struct svalue *foo)
{
  struct svalue *s,tmp;
  int size,e;
  s=(struct svalue *)mem_block[A_CONSTANTS].block;
  size=mem_block[A_CONSTANTS].current_size / sizeof(struct svalue);
  for(e=0;e<size;e++)
    if(is_equal(s+e,foo))
      return e;

  assign_svalue_no_free(&tmp,foo);
  add_to_mem_block(A_CONSTANTS,(char *)&tmp,sizeof(struct svalue));
  return size;
}

#ifdef LINMALLOC_DEBUG
extern void assert_malloced_memory(void *);

void assert_node(node *n)
{
  if(!n) return;
  assert_malloced_memory((char *)n);
  switch(n->token)
  {
  case F_CONSTANT:
    /* assert_svalue_malloced(&(n->u.sval)); */
    break;

  case F_STRING:
    /* assert_malloced_memory(n->u.s.a.str); */
    break;

  default:
    if(n->opt_info & OPT_A_IS_NODE)
      assert_node(n->u.s.a.node);
    if(n->opt_info & OPT_B_IS_NODE)
      assert_node(n->u.s.b.node);
  }
}
#endif



#define MAX_GLOBAL 256
#define MAX(X,Y) ((X)<(Y)?(Y):(X))
static int max_local_vital=-1;
static int max_global_vital=-1;
struct nodepair
{
  int used;
  node *assignment;
  node *index;
  struct nodepair *active;
};

static struct nodepair *active;
static struct nodepair local_vital[MAX_LOCAL];
static struct nodepair global_vital[MAX_GLOBAL];

static void reset_remember(struct nodepair *n)
{
  while(n)
  {
    struct nodepair *p;
    n->assignment=NULL;
    p=n->active;
    n->active=NULL;
    n=p;
  }
}

static void clear_remember_local()
{
  while(max_local_vital>=0)
    reset_remember(local_vital+max_local_vital--);
}

static void clear_remember_global()
{
  while(max_global_vital>=0)
    reset_remember(global_vital+max_global_vital--);
}

static void clear_remember()
{
  clear_remember_local();
  clear_remember_global();
}

struct nodepair *find_remember(node *a)
{

  switch(a->token)
  {
  case F_ASSIGN_AND_POP:
  case F_ASSIGN: return find_remember(a->u.s.b.node);

  case F_INC:
  case F_POST_INC:
  case F_INC_AND_POP:
  case F_DEC:
  case F_POST_DEC:
  case F_DEC_AND_POP:
  case F_INDEX: return find_remember(a->u.s.a.node);
  case F_LOCAL: return local_vital+a->u.s.a.number;
  case F_GLOBAL: return global_vital+a->u.s.a.number;
  default: return NULL;
  }
}

static void remember_remember(node *b,int used)
{
  node *a;
  
  a=b;
  while(1)
  {
    switch(a->token)
    {
    case F_ASSIGN_AND_POP:
    case F_ASSIGN:
      a=a->u.s.b.node;
      break;

    case F_INC:
    case F_POST_INC:
    case F_INC_AND_POP:
    case F_DEC:
    case F_POST_DEC:
    case F_DEC_AND_POP:
    case F_INDEX:
      a=a->u.s.a.node;
      break;

    case F_LOCAL:
      local_vital[a->u.s.a.number].assignment=b;
      local_vital[a->u.s.a.number].index=a;
      local_vital[a->u.s.a.number].used=used;
      local_vital[a->u.s.a.number].active=active;
      max_local_vital=MAX(max_local_vital,a->u.s.a.number);
      return;
    
    case F_GLOBAL:
      global_vital[a->u.s.a.number].assignment=b;
      global_vital[a->u.s.a.number].index=a;
      global_vital[a->u.s.a.number].used=used;
      global_vital[a->u.s.a.number].active=active;
      max_global_vital=MAX(max_global_vital,a->u.s.a.number);
      return;

    default:
      return;
    }
  }
}

void free_node(node *n)
{
  if(!n) return;
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  switch(n->token)
  {
  case F_LOCAL:
    if(local_vital[n->u.s.a.number].index==n)
      reset_remember(local_vital+n->u.s.a.number);
    break;

  case F_GLOBAL:
    if(global_vital[n->u.s.a.number].index==n)
      reset_remember(global_vital+n->u.s.a.number);
    break;

  case F_CONSTANT:
    free_svalue(&(n->u.sval));
    break;

  case F_STRING:
    free_string(n->u.s.a.str);
    break;

  default:
    if(n->opt_info & OPT_A_IS_NODE)
      free_node(n->u.s.a.node);
    if(n->opt_info & OPT_B_IS_NODE)
      free_node(n->u.s.b.node);
  }
  free((char *)n);
}

node *mknode(short token,node *a,node *b)
{
  node *res;
  res=(node *)xalloc(sizeof(node));
  res->u.s.a.node=a;
  res->u.s.b.node=b;
  res->token=token;
  res->type=0;
  res->opt_info=OPT_A_IS_NODE | OPT_B_IS_NODE;
  res->lnum=current_line;
  return res;
}

node *mkstrnode(char *str)
{
  node *res;
  res=(node *)xalloc(sizeof(node));
  res->token=F_STRING;
  res->type=TYPE_STRING;
  res->opt_info=OPT_CONST | OPT_OPT_COMPUTED | OPT_OPTIMIZED;
#ifdef DEBUG
  if(str!=debug_findstring(str))
    fatal("Mkstrnode on nonshared string.\n");
#endif
  res->u.s.a.str=str;
  res->lnum=current_line;
  return res;
}

node *mkintnode(int nr)
{
  node *res;
  res=(node *)xalloc(sizeof(node));
  res->token=F_NUMBER;
  if(nr)
    res->type=TYPE_NUMBER;
  else
    res->type=TYPE_ANY;
  res->opt_info=OPT_CONST | OPT_OPT_COMPUTED | OPT_OPTIMIZED;
  res->u.s.a.number=nr;
  res->lnum=current_line;
  return res;
}

node *mkfloatnode(float foo)
{
  node *res;
  res=(node *)xalloc(sizeof(node));
  res->token=F_FLOAT;
  res->type=TYPE_FLOAT;
  res->opt_info=OPT_CONST | OPT_OPT_COMPUTED | OPT_OPTIMIZED;
  res->u.s.a.fnum=foo;
  res->lnum=current_line;
  return res;
}

node *mkconstfunnode(int fun)
{
  node *res;
  res=(node *)xalloc(sizeof(node));
  res->token=F_CONSTANT_FUNCTION;
  res->type=TYPE_FUNCTION;
  res->opt_info=OPT_CONST;
  res->u.s.a.number=fun;
  res->lnum=current_line;
  return res;
}

node *mkglobalnode(int var)
{
  node *res;
  res=(node *)xalloc(sizeof(node));
  res->token=F_GLOBAL;
  res->type=VARIABLE(var)->type & TYPE_MOD_MASK;
  res->opt_info=OPT_COULD_BE_CONST;
  res->u.s.a.number=var;
  res->lnum=current_line;
  return res;
}

node *mklocalnode(int var)
{
  node *res;
  res=(node *)xalloc(sizeof(node));
  res->token=F_LOCAL;
  res->type=type_of_locals[var] & TYPE_MOD_MASK;
  res->opt_info=OPT_COULD_BE_CONST;
  res->u.s.a.number=var;
  res->lnum=current_line;
  return res;
}

node *mksimulefunnode(int fun)
{
  node *res;
  res=(node *)xalloc(sizeof(node));
  res->token=F_PUSH_SIMUL_EFUN;
  res->type=TYPE_FUNCTION;
  res->opt_info=OPT_CONST;
  res->u.s.a.number=fun;
  res->lnum=current_line;
  return res;
}

node *mkcastnode(int type,node *n)
{
  node *res;
  res=(node *)xalloc(sizeof(node));
  res->token=F_CAST;
  res->type=type;
  res->opt_info=OPT_A_IS_NODE;
  res->u.s.a.node=n;
  res->u.s.b.node=NULL;
  res->lnum=current_line;
  return res;
}

node *mkefunnode(int efun,node *args)
{
  node *res;
  res=(node *)xalloc(sizeof(node));
  res->token=F_EFUN_CALL;
  res->type=instrs[efun-F_OFFSET].ret_type;
  res->opt_info=OPT_B_IS_NODE;
  if(instrs[efun-F_OFFSET].ret_type & TYPE_MOD_CONSTANT)
  {
    res->opt_info|=OPT_CONST;
  }else{
    res->opt_info|=OPT_SIDE_EFFECT;
  }
  res->u.s.a.number=efun;
  res->u.s.b.node=args;
  res->lnum=current_line;
  return res;
}

static int can_use_constants(struct svalue *s,int svalues)
{
  int e;
  for(e=0;e<svalues;e++)
  {
    switch(s[e].type)
    {
    case T_FUNCTION:
    case T_OBJECT:
      return 0;

    case T_LIST:
    case T_MAPPING:
    case T_POINTER:
    case T_ALIST_PART:
      if(!can_use_constants(s[e].u.vec->item,s[e].u.vec->size))
	return 0;
    }
  }
  return 1;
}

int node_is_eq(node *a,node *b)
{
  if(a==b) return 1;
  if(!a || !b) return 0;
  if(a->token!=b->token) return 0;
  switch(a->token)
  {
  case F_STRING:
    return a->u.s.a.str==b->u.s.a.str;

  case F_CONSTANT_FUNCTION:
  case F_PUSH_SIMUL_EFUN:
  case F_GLOBAL:
  case F_LOCAL:
  case F_NUMBER:
    return a->u.s.a.number==b->u.s.a.number;

  case F_FLOAT:
    return a->u.s.a.fnum==b->u.s.a.fnum;

  case F_CAST:
    return a->type==b->type &&
      a->opt_info==b->opt_info &&
	node_is_eq(a->u.s.a.node,b->u.s.a.node);

  case F_EFUN_CALL:
    return a->u.s.a.number==b->u.s.a.number &&
      a->type==b->type &&
      a->opt_info==b->opt_info &&
	node_is_eq(a->u.s.b.node,b->u.s.b.node);

  case F_CONSTANT:
    return is_equal(&(a->u.sval),&(b->u.sval));

  default:
    return a->type==b->type &&
      a->opt_info==b->opt_info &&
	((a->opt_info & OPT_A_IS_NODE) && node_is_eq(a->u.s.a.node,b->u.s.a.node)) &&
	  ((a->opt_info & OPT_B_IS_NODE) && node_is_eq(a->u.s.b.node,b->u.s.b.node));
  }

}

static node *can_use_allocate(struct vector *v)
{
  int e;
  node *n,*n2;
  if(!v->size) return mkintnode(0);

  if(IS_ZERO(v->item))
  {
    for(e=1;e<v->size;e++)
      if(!IS_ZERO(v->item+e))
	return NULL;
    return mkintnode(v->size);
  }else if(v->item->type==T_POINTER){
    n=can_use_allocate(v->item->u.vec);
    if(!n) return NULL;
    for(e=1;e<v->size;e++)
    {
      n2=NULL;
      if(v->item[e].type!=T_POINTER ||
	 !node_is_eq(n,n2=can_use_allocate(v->item[e].u.vec)))
      {
	free_node(n);
	free_node(n2);
	return NULL;
      }
      free_node(n2);
    }
    return mknode(F_ARG_LIST,mkintnode(v->size),n);
  }
  return NULL;
}

node *mksvaluenode(struct svalue *s)
{
  node *res;
  int e;
  switch(s->type)
  {
  default:
    fatal("Unknown type?\n");
    break;

  case T_STRING:
    return mkstrnode(copy_shared_string(strptr(s)));

  case T_INT:
    if(s->subtype)
    {
      res=(node *)xalloc(sizeof(node));
      res->token=F_CONSTANT;
      res->type=TYPE_NUMBER;
      res->opt_info=OPT_CONST;
      res->lnum=current_line;
      assign_svalue_no_free(&(res->u.sval),s);
      return res;
    }
    return mkintnode(s->u.number);

  case T_FLOAT: return mkfloatnode(s->u.fnum);

  case T_REGEXP:
    res=(node *)xalloc(sizeof(node));
    res->token=F_CONSTANT;
    res->opt_info=OPT_CONST;
    res->lnum=current_line;
    assign_svalue_no_free(&(res->u.sval),s);
    res->type=TYPE_REGULAR_EXPRESSION;
    return res;

  case T_POINTER:
    if((res=can_use_allocate(s->u.vec)))
    {
      res=mkefunnode(F_ALLOCATE,res);
      res->type=TYPE_ANY | TYPE_MOD_POINTER;
      return res;
    }
    /* FALL THROUGH */

  case T_ALIST_PART:
  case T_LIST:
  case T_MAPPING:
    if(can_use_constants(s,1))
    {
      res=(node *)xalloc(sizeof(node));
      res->token=F_CONSTANT;
      res->lnum=current_line;
      assign_svalue_no_free(&(res->u.sval),s);
      if(res->u.sval.type==T_ALIST_PART)
	res->u.sval.type=T_POINTER;
      res->opt_info=OPT_CONST;
      if(s->type==T_POINTER || s->type==T_ALIST_PART)
	res->type=TYPE_ANY | TYPE_MOD_POINTER;
      else if(s->type==T_LIST)
	res->type=TYPE_LIST;
      else
	res->type=TYPE_MAPPING;
      return res;
    }else{
      switch(s->type)
      {
      case T_POINTER:
      case T_ALIST_PART:
	if(!s->u.vec->size)
	{
	  res=mkefunnode(F_AGGREGATE,NULL);
	}else{
	  res=mksvaluenode(s->u.vec->item);
	  for(e=1;e<s->u.vec->size;e++)
	    res=mknode(F_ARG_LIST,res,mksvaluenode(s->u.vec->item+e));
	  res=mkefunnode(F_AGGREGATE,res);
	  res->type=TYPE_ANY | TYPE_MOD_POINTER;
	  return res;
	}

      case T_MAPPING:
	if(!s->u.vec->item[0].u.vec->size)
	{
	  res=mkefunnode(F_M_AGGREGATE,NULL);
	}else{
	  res=mkefunnode(F_MKMAPPING,
			 mknode(F_ARG_LIST,mksvaluenode(s->u.vec->item),
				mksvaluenode(s->u.vec->item+1)));
	  res->type=TYPE_MAPPING;
	}
	return res;

      case T_LIST:
	if(!s->u.vec->item[0].u.vec->size)
	{
	  res=mkefunnode(F_L_AGGREGATE,NULL);
	}else{
	  res=mksvaluenode(s->u.vec->item[0].u.vec->item);
	  for(e=1;e<s->u.vec->size;e++)
	    res=mknode(F_ARG_LIST,res,
		       mksvaluenode(s->u.vec->item[0].u.vec->item+e));
	  res=mkefunnode(F_L_AGGREGATE,res);
	  res->type=TYPE_LIST;
	}
	return res;
      }
    }
    return NULL;

  case T_OBJECT:
    if(s->u.ob==&fake_object)
    {
      return mkefunnode(F_THIS_OBJECT,0);
    }else{
      res=(node *)xalloc(sizeof(node));      
      res->lnum=current_line;
      res->token=F_CONSTANT;
      assign_svalue_no_free(&(res->u.sval),s);
      res->opt_info=OPT_CONST;
      res->type=TYPE_OBJECT;
      return res;
    }

  case T_FUNCTION:
    if(s->u.ob==&fake_object)
    {
      return mkconstfunnode(s->subtype);
    }else{
      int e;
      for(e=0;e<num_simul_efuns;e++)
	if(!MEMCMP((char *)s,(char *)&(simul_efuns[e].fun),sizeof(struct svalue)))
	  break;
      if(e<num_simul_efuns){
	return mksimulefunnode(e);
      }else{
	res=(node *)xalloc(sizeof(node));      
	res->lnum=current_line;
	res->token=F_CONSTANT;
	assign_svalue_no_free(&(res->u.sval),s);
	res->opt_info=OPT_CONST;
	res->type=TYPE_FUNCTION;
	return res;
      }
    }
  }
  return 0;
}


/* here comes the typechecking */
/* note that the typechecking does certain casts that won't be made */
/* if typechecking is turned off. */

static int cant_check_types;
static int typep;
static int types[256];

static void get_types(node *n)
{
  if(typep>255 || !n)
    return;

#ifdef MALLOC_DEBUG
      check_sfltable();
#endif
  switch(n->token)
  {
    case F_ARG_LIST:
      get_types(n->u.s.a.node);
      get_types(n->u.s.b.node);
      break;
      
    case F_PUSH_ARRAY:
      cant_check_types=1;
      break;
      
    default:
      if(n->type!=TYPE_VOID)
	types[typep++]=n->type;
  }
#ifdef MALLOC_DEBUG
      check_sfltable();
#endif
}

static int match_arg(int *argp,int type)
{
  for(;*argp;argp++)
    if(TYPE(type,*argp)) return 1;
  return 0;
}

static int check_efun_types(int **argp,int offset,int left,node **n,char *name,int *changed)
{
  if(!*n) return offset;
  if(offset==left) return offset;
  if((*n)->token==F_ARG_LIST)
  {
    offset=check_efun_types(argp,offset,left,&((*n)->u.s.a.node),name,changed);
    offset=check_efun_types(argp,offset,left,&((*n)->u.s.b.node),name,changed);
    return offset;
  }
  if(!match_arg(*argp,(*n)->type))
  {
    if((*n)->type==TYPE_STRING && match_arg(*argp,TYPE_OBJECT))
    {
      *n=mkcastnode(TYPE_OBJECT,*n);
      *changed=1;
    }else if((*n)->type==TYPE_STRING && match_arg(*argp,TYPE_FUNCTION))
    {
      *changed=1;
      *n=mkcastnode(TYPE_FUNCTION,*n);
      *n=optimize(*n,0,0);
    }else if((*n)->type==TYPE_STRING &&
	     match_arg(*argp,TYPE_REGULAR_EXPRESSION))
    {
      *n=mkcastnode(TYPE_REGULAR_EXPRESSION,*n);
      *n=optimize(*n,0,0);
      *changed=1;
    }else{
      char buf[200];
      sprintf(buf, "Bad argument %d type to efun %s()",offset+1,name);
      yyerror(buf);
    }
  }
  while(**argp) (*argp)++;
  (*argp)++;
  return offset+1;
}

static void check_args_for_summation(node *n)
{
  int e,type;
  char buf[200];
  type=types[0];

  if(typep<1)
  {
    sprintf(buf,"Too few arguments to sumation.");
    yyerror(buf);
  }else{
    for(e=0;e<typep;e++)
    {
      types[e]&=TYPE_MOD_MASK;
      if(types[e]==TYPE_FUNCTION || types[0]==TYPE_OBJECT)
      {
	sprintf(buf,"Bad argument %d to + (%s).",e+1,get_type_name(types[e]));
	yyerror(buf);
      }
      if(type==TYPE_ANY) continue;
      if(types[e]==TYPE_ANY) { type=TYPE_ANY; continue; }
      if(types[e]==type) continue;
      if(types[e] & TYPE_MOD_POINTER) type=TYPE_MOD_POINTER;
      if(types[e]&type&TYPE_MOD_POINTER) continue;
      switch(type)
      {
      case TYPE_STRING:
	if(types[e]==TYPE_NUMBER || types[e]==TYPE_FLOAT) continue;
	break;

      case TYPE_NUMBER:
      case TYPE_FLOAT:
	if(types[e]==TYPE_STRING)
	{
	  type=TYPE_STRING;
	  continue;
	}
	break;

      }
    }
    if(type!=TYPE_ANY)
    {
      for(e=0;e<typep;e++)
      {
	switch(type)
	{
	case TYPE_STRING:
	  if(types[e]!=TYPE_STRING &&
	     types[e]!=TYPE_NUMBER &&
	     types[e]!=TYPE_FLOAT)
	  {
	    sprintf(buf,"Bad argument %d to + (%s).",e+1,
		    get_type_name(types[e]));
	    yyerror(buf);
	  }
	  break;

	case TYPE_MOD_POINTER:
	  if(!(types[e]&TYPE_MOD_POINTER))
	  {
	    sprintf(buf,"Bad argument %d to + (%s).",e+1,
		    get_type_name(types[e]));
	    yyerror(buf);
	  }
	  break;

	case TYPE_LIST:
	case TYPE_MAPPING:
	case TYPE_NUMBER:
	case TYPE_FLOAT:
	  if(types[e]!=type)
	  {
	    sprintf(buf,"Bad argument %d to + (%s).",e+1,
		    get_type_name(types[e]));
	    yyerror(buf);
	  }
	  break;
	}
      }
    }
  }
  n->type=type;
}

static int check_types(node *n)
{
  char buf[200];
  if(!n) return 0;
  typep=0;
  cant_check_types=0;

#ifdef LINMALLOC_DEBUG
  assert_node(n);
#endif
#ifdef MALLOC_DEBUG
      check_sfltable();
#endif

  switch(n->token)
  {
  case F_INDEX:
    get_types(n->u.s.a.node);
    get_types(n->u.s.b.node);
    if(typep!=2)
    {
      sprintf(buf,"Wrong number of arguments to %s.",get_f_name(n->token));
      yyerror(buf);
    }
    if(types[0]==TYPE_STRING || (types[0]&TYPE_MOD_POINTER))
    {
      if(!TYPE(types[1],TYPE_NUMBER))
      {
	sprintf(buf,"Index not integer.");
	yyerror(buf);
      }
      if(types[0]==TYPE_STRING)
      {
	n->type=TYPE_NUMBER;
      }else{
	n->type=types[0] & ~ TYPE_MOD_POINTER;
      }
    }else if(types[0]==TYPE_OBJECT || types[0]==TYPE_FUNCTION ||
	   types[0]==TYPE_NUMBER || types[0]==TYPE_FLOAT)
    {
      sprintf(buf,"Indexing on illegal type.");
      yyerror(buf);
    }else{
      n->type=TYPE_ANY;
    }
    break;

  case F_ADD:
    get_types(n->u.s.a.node);
    get_types(n->u.s.b.node);
    check_args_for_summation(n);
    break;

  case F_SUBTRACT:
  case F_AND:
  case F_XOR:
  case F_OR:
    get_types(n->u.s.a.node);
    get_types(n->u.s.b.node);
    if(typep!=2)
    {
      sprintf(buf,"Wrong number of arguments to %s.",get_f_name(n->token));
      yyerror(buf);
    }
    if(types[0]==TYPE_FUNCTION || types[0]==TYPE_FUNCTION ||
       (n->token!=F_SUBTRACT && types[0]==TYPE_FLOAT))
    {
      sprintf(buf,"Bad argument 1 to %s.",get_f_name(n->token));
      yyerror(buf);
    }
    if(!TYPE(types[0],types[1]))
    {
      sprintf(buf,"Bad arg 2 to %s.",get_f_name(n->token));
      yyerror(buf);
    }
    n->type=types[0];
    break;

  case F_MULTIPLY:
    get_types(n->u.s.a.node);
    get_types(n->u.s.b.node);
    if(typep!=2)
    {
      sprintf(buf,"Wrong number of arguments to %s.",get_f_name(n->token));
      yyerror(buf);
    }
    if(types[0]==TYPE_FUNCTION || types[0]==TYPE_OBJECT ||
       types[0]==TYPE_LIST || types[0]==TYPE_MAPPING)
    {
      sprintf(buf,"Bad arg 1 to *.");
      yyerror(buf);
    }
    if(types[1]==TYPE_FUNCTION || types[1]==TYPE_OBJECT ||
       types[1]==TYPE_LIST || types[1]==TYPE_MAPPING)
    {
      sprintf(buf,"Bad arg 2 to *.");
      yyerror(buf);
    }
    if((types[0]&TYPE_MOD_POINTER) && types[1]==TYPE_STRING)
    {
      n->type=TYPE_STRING;
    }else if(types[0]==TYPE_NUMBER && types[1]==TYPE_NUMBER){
      n->type=TYPE_NUMBER;
    }else if(types[0]==TYPE_FLOAT && types[1]==TYPE_FLOAT){
      n->type=TYPE_FLOAT;
    }else{
      n->type=TYPE_ANY;
    }
    break;

  case F_DIVIDE:
    get_types(n->u.s.a.node);
    get_types(n->u.s.b.node);
    if(typep!=2)
    {
      sprintf(buf,"Wrong number of arguments to %s.",get_f_name(n->token));
      yyerror(buf);
    }
    if(types[0]==TYPE_FUNCTION || types[0]==TYPE_OBJECT ||
       types[0]==TYPE_LIST || types[0]==TYPE_MAPPING)
    {
      sprintf(buf,"Bad arg 1 to /.");
      yyerror(buf);
    }
    if(types[1]==TYPE_FUNCTION || types[1]==TYPE_OBJECT ||
       types[1]==TYPE_LIST || types[1]==TYPE_MAPPING)
    {
      sprintf(buf,"Bad arg 2 to /.");
      yyerror(buf);
    }
    if(types[0]==TYPE_STRING && types[1]==TYPE_STRING)
    {
      n->type=TYPE_STRING | TYPE_MOD_POINTER;
    }else if(types[0]==TYPE_NUMBER && types[1]==TYPE_NUMBER){
      n->type=TYPE_NUMBER;
    }else if(types[0]==TYPE_FLOAT && types[1]==TYPE_FLOAT){
      n->type=TYPE_FLOAT;
    }else{
      n->type=TYPE_ANY;
    }
    break;

  case F_NEGATE:
    get_types(n->u.s.a.node);
    if(typep!=1)
    {
      sprintf(buf,"Wrong number of arguments to %s.",get_f_name(n->token));
      yyerror(buf);
    }
    if(!BASIC_TYPE(types[0],TYPE_NUMBER) && !BASIC_TYPE(types[0],TYPE_FLOAT))
    {
      sprintf(buf,"Bad 1 arg to subtract.");
      yyerror(buf);
    }
    n->type=types[0];
    break;

  case F_COMPL:
    get_types(n->u.s.a.node);
    if(typep!=1)
    {
      sprintf(buf,"Wrong number of arguments to %s.",get_f_name(n->token));
      yyerror(buf);
    }
    if(!BASIC_TYPE(types[0],TYPE_NUMBER))
    {
      sprintf(buf,"Bad 1 arg to ~.");
      yyerror(buf);
    }
    n->type=types[0];
    break;

  case F_EQ:
  case F_NE:
    get_types(n->u.s.a.node);
    get_types(n->u.s.b.node);
    if(typep!=2)
    {
      sprintf(buf,"Wrong number of arguments to %s.",get_f_name(n->token));
      yyerror(buf);
    }
    n->type=TYPE_NUMBER;
    break;
      
  case F_GT:
  case F_GE:
  case F_LT:
  case F_LE:
    get_types(n->u.s.a.node);
    get_types(n->u.s.b.node);
    if(typep!=2)
    {
      sprintf(buf,"Wrong number of arguments to %s.",get_f_name(n->token));
      yyerror(buf);
    }
    if(!BASIC_TYPE(types[0],TYPE_NUMBER) && !BASIC_TYPE(types[0],TYPE_FLOAT) &&
       !BASIC_TYPE(types[1],TYPE_STRING))
    {
      sprintf(buf,"Bad arg 1 to %s.",get_f_name(n->token));
      yyerror(buf);
	
    }
    if(!BASIC_TYPE(types[0],types[1]))
    {
      sprintf(buf,"Bad arg 2 to %s.",get_f_name(n->token));
      yyerror(buf);
    }
    n->type=TYPE_NUMBER;
    break;

  case F_NOT:
    n->type=TYPE_NUMBER;
    break;

  case F_MOD:
    get_types(n->u.s.a.node);
    get_types(n->u.s.b.node);
    if(typep!=2)
    {
      sprintf(buf,"Wrong number of arguments to %s.",get_f_name(n->token));
      yyerror(buf);
    }

    if(!BASIC_TYPE(types[0],TYPE_NUMBER) && !
       BASIC_TYPE(types[0],TYPE_FLOAT))
    {
      sprintf(buf,"Bad arg 1 to %s.",get_f_name(n->token));
      yyerror(buf);
	
    }

    if(!TYPE(types[0],types[1]))
    {
      sprintf(buf,"Bad argument 2 to %s.",get_f_name(n->token));
      yyerror(buf);
    }

    n->type=types[0];
    break;

  case F_RSH:
  case F_LSH:
    get_types(n->u.s.a.node);
    get_types(n->u.s.b.node);
    if(typep!=2)
    {
      sprintf(buf,"Wrong number of arguments to %s.",get_f_name(n->token));
      yyerror(buf);
    }
    if(!BASIC_TYPE(types[0],TYPE_NUMBER))
    {
      sprintf(buf,"Bad arg 1 to %s.",get_f_name(n->token));
      yyerror(buf);
	
    }
    if(!BASIC_TYPE(types[0],TYPE_NUMBER))
    {
      sprintf(buf,"Bad arg 2 to %s.",get_f_name(n->token));
      yyerror(buf);
    }
    n->type=TYPE_NUMBER;
    break;

  case F_SSCANF:
    get_types(n->u.s.a.node);
    if(typep!=2)
    {
      sprintf(buf,"Wrong number of arguments to sscanf.");
      yyerror(buf);
    }
    if(!BASIC_TYPE(types[0],TYPE_STRING))
    {
      sprintf(buf,"Bad arg 1 to sscanf.");
      yyerror(buf);
    }
    if(!BASIC_TYPE(types[1],TYPE_STRING))
    {
      sprintf(buf,"Bad arg 2 to sscanf.");
      yyerror(buf);
    }
    n->type=TYPE_NUMBER;
    break;

  case F_SWAP:
    n->type=TYPE_VOID;
    break;
      
  case F_LAND:
  case F_LOR:
    get_types(n->u.s.a.node);
    get_types(n->u.s.b.node);
    if(typep!=2)
    {
      sprintf(buf,"Wrong number of arguments to %s.",get_f_name(n->token));
      yyerror(buf);
    }
    if(types[0]==types[1])
      n->type=types[0];
    else
      n->type=TYPE_ANY;
    break;

  case F_ASSIGN:
  case F_ASSIGN_AND_POP:
    get_types(n->u.s.b.node);
    get_types(n->u.s.a.node);
    if(!TYPE(types[0],types[1]))
      yyerror("Different types to assign.");
    if(n->token==F_ASSIGN)
    {
      n->type=types[1];
    }else{
      n->type=TYPE_VOID;
    }
    break;

  case F_INC:
  case F_DEC:
  case F_POST_INC:
  case F_POST_DEC:
    get_types(n->u.s.a.node);
    if(typep!=1)
    {
      sprintf(buf,"Wrong number of arguments to %s.",get_f_name(n->token));
      yyerror(buf);
    }
    n->type=TYPE_NUMBER;
    break;

  case F_EFUN_CALL:
  {
    int min,max,def,ret,*argp;
    extern int efun_arg_types[];

    get_types(n->u.s.b.node);
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif

    if(n->u.s.a.number==F_SUM)
    {
      if(cant_check_types) break;
      check_args_for_summation(n);
      break;
    }

    min=instrs[n->u.s.a.number-F_OFFSET].min_arg;
    max=instrs[n->u.s.a.number-F_OFFSET].max_arg;
    def=instrs[n->u.s.a.number-F_OFFSET].Default;
    ret=instrs[n->u.s.a.number-F_OFFSET].ret_type;

    if(cant_check_types)
    {
      if(min==max)
	yyerror("Cannot use '@' on efun with fixed number of arguments.");
      break;
    }
      
    argp = &efun_arg_types[instrs[n->u.s.a.number-F_OFFSET].arg_index];
    ret&=TYPE_MOD_MASK;
    n->type=ret;
    if(def && typep==min-1)
    {
      node *tmp;
      if(instrs[def-F_OFFSET].efunc)
      {
	tmp=mkefunnode(def,0);
      }else{
	switch(def)
	{
	case F_CONST0:
	  tmp=mkintnode(0);
	  break;

	case F_CONST1:
	  tmp=mkintnode(1);
	  break;

	case F_CONST_1:
	  tmp=mkintnode(-1);
	  break;
	  
	default:
	  fatal("Illegal 'default' (%d) for opcode %d\n",def,n->u.s.a.number);
	  tmp=mkintnode(0); /* oh well.. */
	}
      }
      n->u.s.b.node=mknode(F_ARG_LIST,n->u.s.b.node,tmp);
      n->opt_info&=~(OPT_OPT_COMPUTED | OPT_OPTIMIZED);
      n->u.s.b.node=optimize(n->u.s.b.node,0,0);
      typep=0;
      get_types(n->u.s.b.node);
    }
    if(typep<min)
    {
      sprintf(buf, "Too few arguments to %s", get_f_name(n->u.s.a.number));
      yyerror(buf);
    }
    if(typep>max && max!=-1)
    {
      sprintf(buf, "Too many arguments to %s", get_f_name(n->u.s.a.number));
      yyerror(buf);
    }
#ifdef MALLOC_DEBUG
    check_sfltable();
#endif
    if(exact_types)
    {
      int foo=0;
      check_efun_types(&argp,0,max==-1?min:typep,&(n->u.s.b.node),get_f_name(n->u.s.a.number),&foo);
      return foo;
    }
    break;
  }

  case '?':
    get_types(n->u.s.a.node);
    get_types(n->u.s.b.node->u.s.a.node);
    get_types(n->u.s.b.node->u.s.b.node);
    if(types[0]==TYPE_VOID)
      yyerror("Unexpected void argument to if or ?>\n");
    if(typep!=3)
      n->type=TYPE_VOID;
    else if(types[1]==types[2])
      n->type=types[1];
    else
      n->type=TYPE_ANY;
    break;

  default:
    if(!n->type) n->type=TYPE_ANY;
  }
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  return 0;
}

/* these routines operates on parsetrees and are mostly used by the
 * optimizer
 */

node *copy_node(node *n)
{
  node *b;
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  if(!n) return n;
  switch(n->token)
  {
    case F_NUMBER:
    case F_FLOAT:
    case F_LOCAL:
    case F_GLOBAL:
    case F_CONSTANT_FUNCTION:
    case F_PUSH_SIMUL_EFUN:
      b=mkintnode(0);
      *b=*n;
      return b;

    case F_STRING:
      b=mkstrnode(copy_shared_string(n->u.s.a.str));
      break;
    case F_EFUN_CALL:
      b=mkefunnode(n->u.s.a.number,copy_node(n->u.s.b.node));
      break;
    case F_CAST:
      b=mkcastnode(n->type,copy_node(n->u.s.a.node));
      break;
    case F_CONSTANT:
      b=mksvaluenode(&(n->u.sval));
      break;

    default:
      if((n->opt_info & OPT_A_IS_NODE) && (n->opt_info & OPT_B_IS_NODE))
	b=mknode(n->token,copy_node(n->u.s.a.node),copy_node(n->u.s.b.node));
      else if(n->opt_info & OPT_A_IS_NODE)
	b=mknode(n->token,copy_node(n->u.s.a.node),n->u.s.b.node);
      else if(n->opt_info & OPT_B_IS_NODE)
	b=mknode(n->token,n->u.s.a.node,copy_node(n->u.s.b.node));
      else
	b=mknode(n->token,n->u.s.a.node,n->u.s.b.node);
  }
  b->type=n->type;
  b->lnum=n->lnum;
  b->opt_info=n->opt_info;
  return b;
}


int is_const(node *n)
{
  return (n->opt_info & OPT_CONST) &&
	 !(n->opt_info & (OPT_SIDE_EFFECT |
			OPT_CASE |
			OPT_CONT |
			OPT_BREAK |
			OPT_LOCAL_ASSIGNMENT |
			OPT_GLOBAL_ASSIGNMENT |
			OPT_RESTORE_OBJECT |
			OPT_RETURN
	   ));
         
}

/* this one supposes that the value is optimized */
int node_is_true(node *n)
{
  if(!n) return 0;
  switch(n->token)
  {
    case F_NUMBER: return n->u.s.a.number!=0;
    case F_FLOAT: /* return n->u.s.a.fnum!=0.0; /wrong */
    case F_STRING: return 1;
    case F_CONSTANT:
    {
      switch(n->u.sval.type)
      {
      case F_NUMBER: return n->u.sval.u.number!=0;
      case F_FLOAT: return n->u.sval.u.fnum!=0.0;
      default: return 1;
      }
    }
    default: return 0;
  }
}

/* this one supposes that the value is optimized */
int node_is_false(node *n)
{
  if(!n) return 0;
  switch(n->token)
  {
    case F_NUMBER: return !n->u.s.a.number;
    case F_FLOAT: return n->u.s.a.fnum==0.0;
    case F_CONSTANT:
    {
      switch(n->u.sval.type)
      {
      case F_NUMBER: return n->u.sval.u.number==0;
      case F_FLOAT: return n->u.sval.u.fnum==0.0;
      default: return 1;
      }
    }
    default: return 0;
  }
}

static node **last_cmd(node **a)
{
  node **n;
  if(!a || !*a) return (node **)NULL;
  if((*a)->token==F_CAST) return last_cmd(&(*a)->u.s.a.node);
  if((*a)->token!='{' && (*a)->token!=F_ARG_LIST) return a;
  if((*a)->u.s.b.node)
  {
    if((*a)->u.s.b.node->token!='{' && (*a)->u.s.b.node->token!=F_CAST &&
       (*a)->u.s.a.node->token!=F_ARG_LIST)
      return &((*a)->u.s.b.node);
    if((n=last_cmd(&(*a)->u.s.b.node)))
      return n;
  }
  if((*a)->u.s.a.node)
  {
    if((*a)->u.s.a.node->token!='{' && (*a)->u.s.a.node->token!=F_CAST &&
       (*a)->u.s.a.node->token!=F_ARG_LIST)
      return &((*a)->u.s.a.node);
    if((n=last_cmd(&(*a)->u.s.a.node)))
      return n;
  }
  return 0;
}

static node **low_get_arg(node **a,int *nr)
{
  node **n;
  if(a[0]->token!=F_ARG_LIST)
  {
    if(!(*nr)--)
      return a;
    else
      return NULL;
  }
  if(a[0]->u.s.a.node)
    if((n=low_get_arg(&(a[0]->u.s.a.node),nr)))
      return n;

  if(a[0]->u.s.b.node)
    if((n=low_get_arg(&(a[0]->u.s.b.node),nr)))
      return n;

  return 0;
}

static node **my_get_arg(node **a,int n) { return low_get_arg(a,&n); }
static node **first_arg(node **a) { return my_get_arg(a,0); }

void print_tree(node *foo,int needlval)
{
  if(!foo) return;
#ifdef LINMALLOC_DEBUG
  assert_node(foo);
#endif
  switch(foo->token)
  {
  case F_CONSTANT_FUNCTION:
    printf("%s",FUNC(&fake_prog,foo->u.s.a.number)->name);
    break;

  case F_LOCAL:
    if(needlval) putchar('&');
    printf("$%d",foo->u.s.a.number);
    break;

  case '?':
    printf("(");
    print_tree(foo->u.s.a.node,0);
    printf(")?(");
    print_tree(foo->u.s.b.node->u.s.a.node,0);
    printf("):(");
    print_tree(foo->u.s.b.node->u.s.b.node,0);
    printf(")");
    break;

  case F_GLOBAL:
    if(needlval) putchar('&');
    printf("%s",VARIABLE(foo->u.s.a.number)->name);
    break;

  case F_NUMBER:
    printf("%d",foo->u.s.a.number);
    break;

  case F_FLOAT:
    printf("%f",foo->u.s.a.fnum);
    break;

  case F_STRING:
    printf("\"%s\"",foo->u.s.a.str);
    break;

  case F_ASSIGN:
  case F_ASSIGN_AND_POP:
    print_tree(foo->u.s.b.node,1);
    printf("=");
    print_tree(foo->u.s.a.node,0);
    break;

  case F_CAST:
    printf("(%s){",get_type_name(foo->type));
    print_tree(foo->u.s.a.node,0);
    printf("}");
    break;

  case F_ARG_LIST:
    print_tree(foo->u.s.a.node,0);
    if(foo->u.s.a.node && foo->u.s.b.node) putchar(',');
    print_tree(foo->u.s.b.node,needlval);
    return;

  case F_LVALUE_LIST:
    print_tree(foo->u.s.a.node,1);
    if(foo->u.s.a.node && foo->u.s.b.node) putchar(',');
    print_tree(foo->u.s.b.node,1);
    return;

  case '{':
    print_tree(foo->u.s.a.node,0);
    if(foo->u.s.a.node) printf(";\n ");
    print_tree(foo->u.s.b.node,needlval);
    return;

  case F_EFUN_CALL:
    printf("efun::%s(",get_f_name(foo->u.s.a.number));
    print_tree(foo->u.s.b.node,0);
    printf(")");
    return;

  default:
    if(!(foo->opt_info & OPT_A_IS_NODE) && !(foo->opt_info & OPT_B_IS_NODE))
    {
      printf("%s",get_f_name(foo->token));
      return;
    }
    if(foo->token<256)
    {
      printf("%c(",foo->token);
    }else{
      printf("%s(",get_f_name(foo->token));
    }
    if(foo->opt_info & OPT_A_IS_NODE) print_tree(foo->u.s.a.node,0);
    if((foo->opt_info & OPT_A_IS_NODE) && (foo->opt_info & OPT_B_IS_NODE)
       && foo->u.s.a.node && foo->u.s.b.node)
      putchar(',');
    if(foo->opt_info & OPT_B_IS_NODE) print_tree(foo->u.s.b.node,0);
    printf(")");
    return;
  }
}

void nice_print_tree(node *n)
{
  print_tree(n,0);
  printf("\n");
  fflush(stdout);
}

static int depend_p3(node *a,node *b)
{
  if(!a || is_const(a)) return 0;
  if(!(a->opt_info & OPT_COULD_BE_CONST)) return 2;
  if(a->token==F_GLOBAL)
    if(b->opt_info & OPT_RESTORE_OBJECT) return 2;

  if((a->opt_info & OPT_A_IS_NODE) && depend_p3(a->u.s.a.node,b))
    return 1;
  if((a->opt_info & OPT_B_IS_NODE) && depend_p3(a->u.s.b.node,b))
    return 1;
  return 0;
}


#if 1
struct used_vars
{
  char locals[MAX_LOCAL];
  char globals[MAX_GLOBAL];
};

#define VAR_BLOCKED 0
#define VAR_UNUSED 1
#define VAR_USED 3
#define VAR_SET 4
static void do_and_vars(struct used_vars *a,struct used_vars *b)
{
  int e;
  for(e=0;e<MAX_LOCAL;e++) a->locals[e]|=b->locals[e];
  for(e=0;e<MAX_GLOBAL;e++) a->globals[e]|=b->globals[e];
  free((char *)b);
}

static struct used_vars *copy_vars(struct used_vars *a)
{
  struct used_vars *ret;
  ret=(struct used_vars *)xalloc(sizeof(struct used_vars));
  MEMCPY((char *)ret,(char *)a,sizeof(struct used_vars));
  return ret;
}

static int find_used_variables(node *n,struct used_vars *p,int noblock)
{
  struct used_vars *a;

  if(!n) return 0;
  switch(n->token)
  {
  case F_LOCAL:
    if(n->opt_info & OPT_LVALUE_OVERWRITE)
    {
      if(p->locals[n->u.s.a.number]==VAR_UNUSED && !noblock)
	p->locals[n->u.s.a.number]=VAR_BLOCKED;
    }else{
      if(p->locals[n->u.s.a.number]==VAR_UNUSED)
	p->locals[n->u.s.a.number]=VAR_USED;
    }
    break;

  case F_GLOBAL:
    if(n->opt_info & OPT_LVALUE_OVERWRITE)
    {
      if(p->globals[n->u.s.a.number]==VAR_UNUSED && !noblock)
	p->globals[n->u.s.a.number]=VAR_BLOCKED;
    }else{
      if(p->globals[n->u.s.a.number]==VAR_UNUSED)
	p->globals[n->u.s.a.number]=VAR_USED;
    }
    break;

  case '?':
    find_used_variables(n->u.s.a.node,p,noblock);
    a=copy_vars(p);
    find_used_variables(n->u.s.b.node->u.s.a.node,a,noblock);
    find_used_variables(n->u.s.b.node->u.s.b.node,p,noblock);
    do_and_vars(p,a);
    break;

  case F_INC_NEQ_LOOP:
  case F_DEC_NEQ_LOOP:
  case F_INC_LOOP:
  case F_DEC_LOOP:
  case F_FOREACH:
  case F_FOR:
    find_used_variables(n->u.s.a.node,p,noblock);
    a=copy_vars(p);
    find_used_variables(n->u.s.b.node,a,noblock);
    do_and_vars(p,a);
    break;

  case F_SWITCH:
    find_used_variables(n->u.s.a.node,p,noblock);
    a=copy_vars(p);
    find_used_variables(n->u.s.b.node,a,1);
    do_and_vars(p,a);
    break;

  case F_DO:
    a=copy_vars(p);
    find_used_variables(n->u.s.a.node,a,noblock);
    do_and_vars(p,a);
    find_used_variables(n->u.s.b.node,p,noblock);
    break;

  case F_BRANCH:
    find_used_variables(n->u.s.a.node,p,noblock);
    find_used_variables(n->u.s.b.node,p,noblock);
    break;

  default:
    if(n->opt_info & OPT_A_IS_NODE)
      find_used_variables(n->u.s.a.node,p,noblock);
    if(n->opt_info & OPT_B_IS_NODE)
      find_used_variables(n->u.s.b.node,p,noblock);
  }
  return 0;
}

/* no subtility needed */
static void find_written_vars(node *n,struct used_vars *p)
{
  if(!n) return;
  switch(n->token)
  {
  case F_LOCAL:
    if(n->opt_info & (OPT_LVALUE_OVERWRITE | OPT_LVALUE_CHANGE))
      p->locals[n->u.s.a.number]=VAR_USED;
    break;

  case F_GLOBAL:
    if(n->opt_info & (OPT_LVALUE_OVERWRITE | OPT_LVALUE_CHANGE))
      p->globals[n->u.s.a.number]=VAR_USED;
    break;

  default:
    if(n->opt_info & OPT_A_IS_NODE) find_written_vars(n->u.s.a.node,p);
    if(n->opt_info & OPT_B_IS_NODE) find_written_vars(n->u.s.b.node,p);
  }
}

/* return 1 if A depends on B */
static int depend_p2(node *a,node *b)
{
  struct used_vars aa,bb;
  int e;

  if(!a || !b || is_const(a)) return 0;
  for(e=0;e<MAX_LOCAL;e++) aa.locals[e]=bb.locals[e]=VAR_UNUSED;
  for(e=0;e<MAX_GLOBAL;e++) aa.globals[e]=bb.globals[e]=VAR_UNUSED;

  find_used_variables(a,&aa,0);
  find_written_vars(b,&bb);

  for(e=0;e<MAX_LOCAL;e++)
    if(aa.locals[e]==VAR_USED && bb.locals[e]!=VAR_UNUSED)
      return 1;

  for(e=0;e<MAX_GLOBAL;e++)
    if(aa.globals[e]==VAR_USED && bb.globals[e]!=VAR_UNUSED)
      return 1;
  return 0;
}
#else
/* return 1 if a depends on b */
static int depend_p2(node *a,node *b)
{
  int foo;
  if(!a || !b || is_const(a)) return 0;

  switch(b->token)
  {
  case F_INDEX:
    if(b->opt_info & ( OPT_LVALUE_CHANGE | OPT_LVALUE_OVERWRITE ))
    {
      if(a->token==F_INDEX) return 1;
      switch(a->token)
      {
      default:
	if((a->opt_info & OPT_A_IS_NODE) && (foo=depend_p2(a->u.s.a.node,b)))
	  return foo;
	if((a->opt_info & OPT_B_IS_NODE) && (foo=depend_p2(a->u.s.b.node,b)))
	  return foo;
      }
    }
    break;
    
  case F_GLOBAL:
  case F_LOCAL:
    if(b->opt_info & ( OPT_LVALUE_CHANGE | OPT_LVALUE_OVERWRITE ))
    {
      if(a->token==b->token && a->u.s.a.number==b->u.s.a.number)
      {
/*	if(a->opt_info & OPT_LVALUE_OVERWRITE)
	  return -1;
	else
*/	  return 1;
      }
      switch(a->token)
      {
      default:
	if((a->opt_info & OPT_A_IS_NODE) && (foo=depend_p2(a->u.s.a.node,b)))
	  return foo;
	if((a->opt_info & OPT_B_IS_NODE) && (foo=depend_p2(a->u.s.b.node,b)))
	  return foo;
      }
    }
    break;
  }
  if((b->opt_info & OPT_A_IS_NODE) && (0<depend_p2(a,b->u.s.a.node)))
    return 1;
  if((b->opt_info & OPT_B_IS_NODE) && (0<depend_p2(a,b->u.s.b.node)))
    return 1;
  return 0;
}

#endif

static int depend_p(node *a,node *b)
{
  if(!b) return 0;
  if(!(b->opt_info & OPT_SIDE_EFFECT))
  {
    return depend_p2(a,b);
  }else{
    return depend_p3(a,b) || depend_p2(a,b);
  }
}

static int find_and_clear_optimize_flag(node *n,node *a)
{
  if(!a || !n) return 0;
  if(a==n) return 1;
  if(((n->opt_info & OPT_A_IS_NODE) &&
      find_and_clear_optimize_flag(n->u.s.a.node,a)) ||
     ((n->opt_info & OPT_B_IS_NODE) &&
      find_and_clear_optimize_flag(n->u.s.b.node,a)))
  {
    n->opt_info&=~(OPT_OPT_COMPUTED|OPT_OPTIMIZED);
    return 1;
  }
  return 0;
}

static int cntargs(node *n)
{
  if(!n) return 0;
  switch(n->token)
  {
    case F_ASSIGN_AND_POP:
    case F_INC_AND_POP:
    case F_DEC_AND_POP:
    case '{':         return 0;

    case F_CAST:
    case F_EFUN_CALL: return n->type!=TYPE_VOID;

    case F_FOREACH:   return 3;
    case F_INC_NEQ_LOOP:
    case F_DEC_NEQ_LOOP:
    case F_INC_LOOP:
    case F_DEC_LOOP:  return 2;

    case ' ':
    case F_LVALUE_LIST:
    case F_ARG_LIST:
      return cntargs(n->u.s.a.node)+cntargs(n->u.s.b.node);

    /* this might not be true, but it doesn't matter very much */
    default: return 1;
  }
}

static node *do_optimize(node *n,int needlval,node *parent);
static node *optimize(node *n,int needlval,node *parent)
{
  int save_current_line=current_line;
  if(!n) return n;

#ifdef LINMALLOC_DEBUG
  assert_node(n);
#endif

#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  if(n->opt_info & OPT_OPTIMIZED) return n;
  current_line=n->lnum;
  n=do_optimize(n,needlval,parent);
  if(n) n->opt_info|=OPT_OPTIMIZED;
  current_line=save_current_line;
  return n;
}

static node *call_optimizer(node *n)
{
  clear_remember();
  return optimize(n,0,NULL);
}

static node *do_optimize(node *n,int needlval,node *parent)
{
  node *tmp1,*tmp2,*tmp3;
  int opt_info1,opt_info2,try_optimize;

  if(!n) return NULL;

#ifdef DEBUG
  if(n->token>=F_MAX_INSTR)
  {
    fatal("Internal compiler breakdown.\n");
  }
#endif

#ifdef MALLOC_DEBUG
    check_sfltable();
#endif

  try_optimize=OPT_TRY_OPTIMIZE | OPT_COULD_BE_CONST;
  opt_info1=opt_info2=OPT_CONST;
  switch(n->token)
  {
  case F_NUMBER:
  case F_STRING:
  case F_FLOAT:
  case F_CONSTANT:
    try_optimize=OPT_COULD_BE_CONST;
    break;

  case F_CAST:
    n->u.s.a.node=optimize(n->u.s.a.node,needlval,n);
    opt_info1=n->u.s.a.node?n->u.s.a.node->opt_info:OPT_CONST;
    if(n->type==TYPE_OBJECT)
    {
      clear_remember();
      if((n->u.s.a.node->type & TYPE_MOD_MASK) == TYPE_STRING ||
	 (n->u.s.a.node->type & TYPE_MOD_MASK) == TYPE_ANY)
      {
	opt_info2=OPT_SIDE_EFFECT;
      }else if((n->u.s.a.node->type & TYPE_MOD_MASK) == TYPE_NUMBER){
	opt_info2=0;
      }
    }else{
    }
    break;

  case F_CONSTANT_FUNCTION:
    try_optimize=OPT_COULD_BE_CONST;
    if(needlval!=-1)
    {
      opt_info1=OPT_CONST;
    }else{
      if((FUNCTIONP(n->u.s.a.number)->flags & NAME_PROTOTYPE) ||
	 !(FUNCTIONP(n->u.s.a.number)->type & TYPE_MOD_INLINE))
      {
	opt_info1=OPT_SIDE_EFFECT | OPT_RESTORE_OBJECT; /* assume the worst */
      }else{
	opt_info1=FUNCTIONP(n->u.s.a.number)->type;
	opt_info1=(opt_info1&TYPE_MOD_CONSTANT?OPT_CONST:0) |
	  (opt_info1&TYPE_MOD_SIDE_EFFECT?OPT_SIDE_EFFECT:0) ;
	if(!(opt_info1&OPT_CONST)) try_optimize=OPT_TRY_OPTIMIZE;
      }
    }
    break;

  case F_PUSH_SIMUL_EFUN:
    try_optimize=OPT_COULD_BE_CONST;
    if(needlval!=-1)
    {
      opt_info1=OPT_CONST;
    }else{
      struct svalue *s;
      s=&(simul_efuns[n->u.s.a.number].fun);
      if(s->type==T_FUNCTION)
      {
	opt_info1=s->u.ob->prog->function_ptrs[s->subtype].type;
	opt_info1=(opt_info1&TYPE_MOD_CONSTANT?OPT_CONST:0) |
	  (opt_info1&TYPE_MOD_SIDE_EFFECT?OPT_SIDE_EFFECT:0) ;
	if(!(opt_info1&OPT_CONST)) try_optimize=OPT_TRY_OPTIMIZE;
      }else{
	fatal("Simul efun destructed unexpectedly.\n");
      }
    }
    break;

  case F_EFUN_CALL:
    switch(n->u.s.a.number)
    {
    case F_RESTORE_OBJECT:
      opt_info1=instrs[n->u.s.a.number-F_OFFSET].ret_type;
      opt_info1=(opt_info1&TYPE_MOD_CONSTANT?OPT_CONST:0) |
	(opt_info1&TYPE_MOD_SIDE_EFFECT?OPT_SIDE_EFFECT:0)|
	  OPT_RESTORE_OBJECT;
      n->u.s.b.node=optimize(n->u.s.b.node,0,n);
      opt_info2=n->u.s.b.node?n->u.s.b.node->opt_info:OPT_CONST;
      clear_remember();
      try_optimize=0;
      break;
      
    case F_CALL_FUNCTION:
      n->u.s.b.node=optimize(n->u.s.b.node,-1,n);
      opt_info1=n->u.s.b.node?n->u.s.b.node->opt_info:OPT_CONST;
      clear_remember();
      if(!(opt_info1&OPT_CONST)) opt_info1|=OPT_SIDE_EFFECT;
      break;

    case F_ALLOCATE:
      opt_info1=instrs[n->u.s.a.number-F_OFFSET].ret_type;
      opt_info1=(opt_info1&TYPE_MOD_CONSTANT?OPT_CONST:0) |
	(opt_info1&TYPE_MOD_SIDE_EFFECT?OPT_SIDE_EFFECT:0) ;
      n->u.s.b.node=optimize(n->u.s.b.node,0,n);
      opt_info2=n->u.s.b.node?n->u.s.b.node->opt_info:OPT_CONST;
      if(opt_info1&OPT_SIDE_EFFECT)
	clear_remember();

    default:
      opt_info1=instrs[n->u.s.a.number-F_OFFSET].ret_type;
      opt_info1=(opt_info1&TYPE_MOD_CONSTANT?OPT_CONST:0) |
	(opt_info1&TYPE_MOD_SIDE_EFFECT?OPT_SIDE_EFFECT:0) ;
      n->u.s.b.node=optimize(n->u.s.b.node,0,n);
      opt_info2=n->u.s.b.node?n->u.s.b.node->opt_info:OPT_CONST;
      if(opt_info1&OPT_SIDE_EFFECT)
	clear_remember();
      if(!(opt_info1&OPT_CONST) &&
	 n->u.s.a.number!=F_ALLOCATE)
	try_optimize=OPT_TRY_OPTIMIZE;
    }
    break;

  case F_CONTINUE:
    opt_info1=OPT_CONT;
    break;

  case F_BREAK:
    opt_info1=OPT_BREAK;
    break;

  case F_BRANCH:
  case F_DO:
    clear_remember();
    n->u.s.a.node=optimize(n->u.s.a.node,0,n);
    opt_info1=n->u.s.a.node?n->u.s.a.node->opt_info:OPT_CONST;
    opt_info1&=~OPT_CONT & ~OPT_BREAK;
    clear_remember();
    n->u.s.b.node=optimize(n->u.s.b.node,0,n);
    opt_info2=n->u.s.b.node?n->u.s.b.node->opt_info:OPT_CONST;
    clear_remember();
    break;

  case F_INC:
  case F_INC_AND_POP:
  case F_POST_INC:
  case F_DEC:
  case F_DEC_AND_POP:
  case F_POST_DEC:
  {
    struct nodepair *p;
    p=active;
    active=find_remember(n->u.s.a.node);
    n->u.s.a.node=optimize(n->u.s.a.node,OPT_LVALUE_CHANGE,n);
    active=p;
    opt_info1=n->u.s.a.node?n->u.s.a.node->opt_info:OPT_CONST;
    break;
  }

  case F_FOREACH:
  case F_INC_NEQ_LOOP:
  case F_DEC_NEQ_LOOP:
  case F_INC_LOOP:
  case F_DEC_LOOP:
  case F_FOR:
    clear_remember();
    n->u.s.a.node=optimize(n->u.s.a.node,0,n);
    opt_info2=n->u.s.a.node?n->u.s.a.node->opt_info:OPT_CONST;
    clear_remember();
    n->u.s.b.node=optimize(n->u.s.b.node,0,n);
    opt_info1=n->u.s.b.node?n->u.s.b.node->opt_info:OPT_CONST;
    opt_info1&= ~OPT_CONT & ~OPT_BREAK;
    clear_remember();
    break;

  case F_ASSIGN_AND_POP:
  case F_ASSIGN:
  {
    struct nodepair *p;
    p=active;
    active=find_remember(n->u.s.b.node);
    n->u.s.a.node=optimize(n->u.s.a.node,0,n);
    active=p;
    opt_info2=n->u.s.a.node?n->u.s.a.node->opt_info:OPT_CONST;
    n->u.s.b.node=optimize(n->u.s.b.node,OPT_LVALUE_OVERWRITE,n);
    opt_info1=n->u.s.b.node?n->u.s.b.node->opt_info:OPT_CONST;
    break;
  }

  case F_AND_EQ:
  case F_OR_EQ:
  case F_XOR_EQ:
  case F_LSH_EQ:
  case F_RSH_EQ:
  case F_ADD_EQ:
  case F_SUB_EQ:
  case F_MULT_EQ:
  case F_MOD_EQ:
  case F_DIV_EQ:
    n->u.s.b.node=optimize(n->u.s.b.node,0,n);
    opt_info2=n->u.s.b.node?n->u.s.b.node->opt_info:OPT_CONST;
    n->u.s.a.node=optimize(n->u.s.a.node,OPT_LVALUE_CHANGE,n);
    opt_info1=n->u.s.a.node?n->u.s.a.node->opt_info:OPT_CONST;
    break;

  case ' ':
    clear_remember();
    n->u.s.a.node=optimize(n->u.s.a.node,0,n);
    opt_info1=n->u.s.a.node?n->u.s.a.node->opt_info:OPT_CONST;
    clear_remember();
    n->u.s.b.node=optimize(n->u.s.b.node,OPT_LVALUE_CHANGE,n);
    opt_info2=n->u.s.b.node?n->u.s.b.node->opt_info:OPT_CONST;
    clear_remember();
    break;

  case F_LOCAL:
    if(needlval)
    {
      opt_info1=OPT_LOCAL_ASSIGNMENT;
    }else{
      opt_info1=0;
    }
    break;

  case F_GLOBAL:
    if(needlval)
    {
      opt_info1=OPT_GLOBAL_ASSIGNMENT;
    }else{
      opt_info1=OPT_GLOBAL_DEPEND;
    }
    break;

  case F_INDEX:
    /* hm */
    n->u.s.a.node=optimize(n->u.s.a.node,needlval?OPT_LVALUE_CHANGE:0,n);
    opt_info1=n->u.s.a.node?n->u.s.a.node->opt_info:OPT_CONST;
    n->u.s.b.node=optimize(n->u.s.b.node,0,n);
    opt_info2=n->u.s.b.node?n->u.s.b.node->opt_info:OPT_CONST;
    needlval=0;
    break;

  case '{':
  case F_ARG_LIST:
    try_optimize=OPT_COULD_BE_CONST;
    if(needlval==-1)
    {
      n->u.s.a.node=optimize(n->u.s.a.node,needlval,n);
      opt_info1=n->u.s.a.node?n->u.s.a.node->opt_info:OPT_CONST;
      n->u.s.b.node=optimize(n->u.s.b.node,0,n);
      opt_info2=n->u.s.b.node?n->u.s.b.node->opt_info:OPT_CONST;
    }else{
      n->u.s.a.node=optimize(n->u.s.a.node,0,n);
      opt_info1=n->u.s.a.node?n->u.s.a.node->opt_info:OPT_CONST;
      n->u.s.b.node=optimize(n->u.s.b.node,needlval,n);
      opt_info2=n->u.s.b.node?n->u.s.b.node->opt_info:OPT_CONST;
    }
    break;

  case F_LVALUE_LIST:
    try_optimize=OPT_COULD_BE_CONST;
    n->u.s.a.node=optimize(n->u.s.a.node,OPT_LVALUE_CHANGE,n);
    opt_info1=n->u.s.a.node?n->u.s.a.node->opt_info:OPT_CONST;
    n->u.s.b.node=optimize(n->u.s.b.node,OPT_LVALUE_CHANGE,n);
    opt_info2=n->u.s.b.node?n->u.s.b.node->opt_info:OPT_CONST;
    break;

  case F_CASE:
  case F_DEFAULT:
    clear_remember();
    n->u.s.a.node=optimize(n->u.s.a.node,0,n);
    opt_info1=n->u.s.a.node?n->u.s.a.node->opt_info:OPT_CONST;
    opt_info1|=OPT_CASE;
    clear_remember();
    n->u.s.b.node=optimize(n->u.s.b.node,0,n);
    opt_info2=n->u.s.b.node?n->u.s.b.node->opt_info:OPT_CONST;
    opt_info2|=OPT_CASE;
    clear_remember();
    break;

  case F_RETURN:
    opt_info1=OPT_RETURN;
    n->u.s.a.node=optimize(n->u.s.a.node,0,n);
    opt_info2=n->u.s.a.node?n->u.s.a.node->opt_info:OPT_CONST;
    clear_remember();
    break;

  case F_SWITCH:
    n->u.s.a.node=optimize(n->u.s.a.node,0,n);
    opt_info1=n->u.s.a.node?n->u.s.a.node->opt_info:OPT_CONST;
    clear_remember();
    n->u.s.b.node=optimize(n->u.s.b.node,0,n);
    opt_info2=n->u.s.b.node?n->u.s.b.node->opt_info:OPT_CONST;
    opt_info2&= ~OPT_CASE & ~OPT_BREAK;
    clear_remember();
    break;

  case F_ADD: case F_ADD_INT: case F_SUBTRACT: case F_MULTIPLY:
  case F_DIVIDE: case F_MOD: case F_NOT: case F_AND: case F_OR:
    if(n->opt_info & OPT_A_IS_NODE)
    {
      n->u.s.a.node=optimize(n->u.s.a.node,needlval,n);
      opt_info1=n->u.s.a.node?n->u.s.a.node->opt_info:OPT_CONST;
    }else{
      opt_info1=OPT_CONST;
    }
    if(n->opt_info & OPT_B_IS_NODE)
    {
      n->u.s.b.node=optimize(n->u.s.b.node,needlval,n);
      opt_info2=n->u.s.b.node?n->u.s.b.node->opt_info:OPT_CONST;
    }else{
      opt_info2=OPT_CONST;
    }
    break;
    
  default:
    clear_remember();
    if(n->opt_info & OPT_A_IS_NODE)
    {
      n->u.s.a.node=optimize(n->u.s.a.node,needlval,n);
      opt_info1=n->u.s.a.node?n->u.s.a.node->opt_info:OPT_CONST;
    }else{
      opt_info1=OPT_CONST;
    }
    clear_remember();
    if(n->opt_info & OPT_B_IS_NODE)
    {
      n->u.s.b.node=optimize(n->u.s.b.node,needlval,n);
      opt_info2=n->u.s.b.node?n->u.s.b.node->opt_info:OPT_CONST;
    }else{
      opt_info2=OPT_CONST;
    }
    clear_remember();
  }

#ifdef MALLOC_DEBUG
    check_sfltable();
#endif

  n->opt_info=
    (
     (opt_info1 & opt_info2 & OPT_CONST) |
     ((opt_info1 | opt_info2) &
      (OPT_SIDE_EFFECT | OPT_GLOBAL_ASSIGNMENT | OPT_LOCAL_ASSIGNMENT | OPT_RESTORE_OBJECT |
       OPT_TRY_OPTIMIZE | OPT_CONT | OPT_BREAK | OPT_RETURN | OPT_GLOBAL_DEPEND | OPT_CASE)) |
     (n->opt_info & (OPT_A_IS_NODE | OPT_B_IS_NODE)) |
     OPT_OPT_COMPUTED |
     try_optimize |
     (needlval & (OPT_LVALUE_CHANGE | OPT_LVALUE_OVERWRITE))
     );

#ifdef LASDEBUG
  if(lasdebug>3)
  {
    fprintf(stderr,"Computed [%c%c%c%c%c%c%c%c%c%c%c%c%c%c] opt info for:\n",
	    (n->opt_info & OPT_A_IS_NODE)?'A':'-',
	    (n->opt_info & OPT_B_IS_NODE)?'B':'-',
	    (n->opt_info & OPT_CONST)?'C':'-',
	    (n->opt_info & OPT_SIDE_EFFECT)?'X':'-',
	    (n->opt_info & OPT_LOCAL_ASSIGNMENT)?'l':'-',
	    (n->opt_info & OPT_GLOBAL_ASSIGNMENT)?'g':'-',
	    (n->opt_info & OPT_RESTORE_OBJECT)?'r':'-',
	    (n->opt_info & OPT_OPT_COMPUTED)?'O':'-',
	    (n->opt_info & OPT_TRY_OPTIMIZE)?'T':'-',
	    (n->opt_info & OPT_CASE)?'c':'-',
	    (n->opt_info & OPT_CONT)?'c':'-',
	    (n->opt_info & OPT_CONT)?'b':'-',
	    (n->opt_info & OPT_RETURN)?'r':'-',
	    (n->opt_info & OPT_GLOBAL_DEPEND)?'G':'-'
	    );
  }
#endif

#ifdef MALLOC_DEBUG
      check_sfltable();
#endif

 if(check_types(n)) 
    return do_optimize(n,needlval,parent);

#ifdef MALLOC_DEBUG
      check_sfltable();
#endif

  if(lasdebug>2)
  {
    fprintf(stderr,"do_optimizing:");
    nice_print_tree(n);
  }

#ifdef MALLOC_DEBUG
      check_sfltable();
#endif
  switch(n->token)
  {
  case ' ':
  case ':': return n;		/* hands of optimizer ! */

  case '?':
    /* (!X) ? (Y) : (Z) ->  (X) ? (Z) : (Y)*/
    while(n->u.s.a.node->token==F_NOT)
    {
      tmp1=n->u.s.b.node->u.s.a.node;
      n->u.s.b.node->u.s.a.node=n->u.s.b.node->u.s.b.node;
      n->u.s.b.node->u.s.b.node=tmp1;
      tmp1=n->u.s.a.node;
      n->u.s.a.node=tmp1->u.s.a.node;
      tmp1->u.s.a.node=0;
      free_node(tmp1);
    }
    /* 1 ? X : Y => X 
     * if(1) X; else Y; => X; 
     */
    if(node_is_true(n->u.s.a.node))
    {
      tmp1=n->u.s.b.node->u.s.a.node;
      n->u.s.b.node->u.s.a.node=NULL;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }

    /* 0 ? X : Y => Y 
     * if(0) X; else Y; => Y;
     */
    if(node_is_false(n->u.s.a.node))
    {
      tmp1=n->u.s.b.node->u.s.b.node;
      n->u.s.b.node->u.s.b.node=NULL;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }

    /* if(X) Y; else Y; => { X; Y; } */
    if(node_is_eq(n->u.s.b.node->u.s.a.node,n->u.s.b.node->u.s.b.node))
    {
      n->token=F_ARG_LIST;
      n->u.s.a.node=mkcastnode(TYPE_VOID,n->u.s.a.node);
      tmp1=n->u.s.b.node;
      n->u.s.b.node=tmp1->u.s.a.node;
      tmp1->u.s.a.node=NULL;
      free_node(tmp1);
      n->opt_info&=~OPT_OPT_COMPUTED;
      return optimize(n,needlval,parent);
    }
    break;
       
  case F_LOCAL:
  case F_GLOBAL:
  case F_INDEX:
  {
    struct nodepair *p;
    if((p=find_remember(n)) && p->assignment)
    {
      if(n->opt_info & (OPT_LVALUE_CHANGE | OPT_LVALUE_OVERWRITE))
      {
	if(n->opt_info & OPT_LVALUE_OVERWRITE)
	{
	  if(p->assignment && !p->used)
	  {
	    tmp1=p->assignment;
	    free_node(tmp1->u.s.b.node);
	    free_node(tmp1->u.s.a.node);
	    tmp1->u.s.a.node=NULL;
	    tmp1->u.s.b.node=NULL;
	    tmp1->token='{';
	  }else{
	    reset_remember(p);
	  }
	}else{
	  reset_remember(p);
	}
      }else{
	if(node_is_eq(n,p->assignment->u.s.a.node))
	{
	  if((p->assignment->token==F_ASSIGN ||
	      p->assignment->token==F_ASSIGN_AND_POP) &&
	     is_const(p->assignment->u.s.a.node))
	  {
	    free_node(n);
	    return optimize(copy_node(p->assignment->u.s.b.node),needlval,parent);
	  }else{
	    if(!p->used)
	    {
	      int tmp=0;
	      switch(p->assignment->token)
	      {
	      case F_INC_AND_POP:
	      case F_POST_INC:
	      case F_INC:
		tmp=F_INC;

	      case F_DEC_AND_POP:
	      case F_POST_DEC:
	      case F_DEC:
		tmp=F_DEC;
		break;

	      case F_ASSIGN_AND_POP:
	      case F_ASSIGN:
		tmp=F_ASSIGN;
		break;
	      }
	      if(!tmp)
		fatal("Optimizer fatal error!!\n");
	      tmp1=mknode(tmp,p->assignment->u.s.a.node,p->assignment->u.s.b.node);
	      p->assignment->u.s.a.node=NULL;
	      p->assignment->u.s.b.node=NULL;
	      p->assignment->token='{';
	      p->assignment->opt_info&=~OPT_OPT_COMPUTED;
	      reset_remember(p);
	      free_node(n);
	      return optimize(tmp1,needlval,parent);
	    }else{
	      reset_remember(p);
	    }
	  }
	}else{
	  reset_remember(p);
	}
      }
    }
    break;
  }
       
  case F_ARG_LIST:
  case '{':
    /* free useless nodes */
    if(!n->u.s.a.node && !n->u.s.b.node)
    {
      free_node(n);
      return NULL;
    }

    /* free useless nodes */
    if(!n->u.s.a.node)
    {
      tmp1=n->u.s.b.node;
      n->u.s.b.node=0;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }

    /* free useless nodes */
    if(!n->u.s.b.node)
    {
      tmp1=n->u.s.a.node;
      n->u.s.a.node=0;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }

    /* { X; return; Y; } -> { X; return; }
     * { X; break; Y; } -> { X; return; }
     * { X; continue; Y; } -> { X; return; }
     */

    if((n->u.s.a.node->token==F_RETURN ||
	n->u.s.a.node->token==F_BREAK ||
	n->u.s.a.node->token==F_CONTINUE ||
	((n->u.s.a.node->token=='{' || n->u.s.a.node->token==F_ARG_LIST) &&
	 n->u.s.a.node->u.s.b.node &&
	 (n->u.s.a.node->u.s.b.node->token==F_RETURN ||
	  n->u.s.a.node->u.s.b.node->token==F_BREAK ||
	  n->u.s.a.node->u.s.b.node->token==F_CONTINUE))) &&
       !(n->u.s.b.node->opt_info & OPT_CASE))
    {
      tmp1=n->u.s.a.node;
      n->u.s.a.node=0;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }

    if(n->token=='{')
    {
      node **prev,**curr;
      /* { (void)a; } => { a; } */
      while(n->u.s.a.node->token==F_CAST && n->u.s.a.node->type==TYPE_VOID)
      {
	tmp1=n->u.s.a.node;
	n->u.s.a.node=tmp1->u.s.a.node;
	tmp1->u.s.a.node=0;
	free_node(tmp1);
      }
      /* { (void)b; } => { b; } */
      while(n->u.s.b.node->token==F_CAST && n->u.s.b.node->type==TYPE_VOID)
      {
	tmp1=n->u.s.b.node;
	n->u.s.b.node=tmp1->u.s.a.node;
	tmp1->u.s.a.node=0;
	free_node(tmp1);
      }

      curr=&(n->u.s.b.node);
      prev=last_cmd(&(n->u.s.a.node));
      if(prev && (*prev) && (*curr))
      {
	if(((*curr)->token==F_INC ||
	    (*curr)->token==F_POST_INC ||
	    (*curr)->token==F_INC_AND_POP) &&
	   ((*prev)->token==F_ASSIGN ||
	    (*prev)->token==F_ASSIGN_AND_POP) &&
	   node_is_eq((*prev)->u.s.b.node,(*curr)->u.s.a.node))
	{
	  free_node(*curr);
	  *curr=0;
	  (*prev)->u.s.a.node=mknode(F_ADD,(*prev)->u.s.a.node,mkintnode(1));
	  find_and_clear_optimize_flag(n,*prev);
	  return optimize(n,needlval,parent);
	}

	if(((*curr)->token==F_DEC ||
	    (*curr)->token==F_POST_DEC ||
	    (*curr)->token==F_DEC_AND_POP) &&
	   ((*prev)->token==F_ASSIGN ||
	    (*prev)->token==F_ASSIGN_AND_POP) &&
	   node_is_eq((*prev)->u.s.b.node,(*curr)->u.s.a.node))
	{
	  free_node(*curr);
	  *curr=0;
	  (*prev)->u.s.a.node=mknode(F_ADD,(*prev)->u.s.a.node,mkintnode(-1));
	  find_and_clear_optimize_flag(n,*prev);
	  return optimize(n,needlval,parent);
	}
      }
      if(!(n->opt_info & (OPT_CONT | OPT_BREAK | OPT_CASE)) &&
	 cntargs(n->u.s.a.node)+cntargs(n->u.s.b.node)<10)
      {
	n->token=F_ARG_LIST;
	return optimize(mkcastnode(TYPE_VOID,n),needlval,parent);
      }
    }

    break;

  case F_MULTIPLY:
    if(SAME_TYPE(n->u.s.a.node->type,n->u.s.b.node->type & TYPE_MOD_MASK))
    {
      if(n->u.s.a.node->token==F_NUMBER || n->u.s.b.node->token==F_NUMBER)
      {
	int tmp,e;

	/* x*(X) -> (X)*x */
	if(n->u.s.a.node->token==F_NUMBER && n->u.s.b.node->token!=F_NUMBER)
	{
	  tmp1=n->u.s.a.node;
	  n->u.s.a.node=n->u.s.b.node;
	  n->u.s.b.node=tmp1;
	}
	tmp=n->u.s.b.node->u.s.a.number;
	/* X*1 -> X */
	if(tmp==1)
	{
	  tmp1=n->u.s.a.node;
	  n->u.s.a.node=0;
	  free_node(n);
	  return tmp1;
	}
	if(tmp==0)
	{
	  if(!(n->u.s.a.node->opt_info & OPT_NOT_THROWABLE))
	  {
	    free_node(n);
	    return mkintnode(0);
	  }
	}
	/* X*(1<<x) -> X<<x */
	for(e=1;e;e*=2)
	{
	  if(tmp==e)
	  {
	    n->u.s.b.node->u.s.a.number=0;
	    while(e>1)
	    {
	      n->u.s.b.node->u.s.a.number++;
	      e/=2;
	    }
	    n->token=F_LSH;
	    n->opt_info &=~ (OPT_OPT_COMPUTED | OPT_OPTIMIZED);
	    return optimize(n,needlval,parent);
	  }
	}
      }

      /* 1.0*X => X */
      /* X*0.0 => 0.0 */
      if((n->u.s.a.node->token==F_FLOAT && n->u.s.a.node->u.s.a.fnum==1.0) ||
	 (n->u.s.b.node->token==F_FLOAT && n->u.s.b.node->u.s.a.fnum==0.0 &&
	  !(n->u.s.a.node->opt_info & OPT_NOT_THROWABLE)))
      {
	tmp1=n->u.s.b.node;
	n->u.s.b.node=0;
	free_node(n);
	return optimize(tmp1,needlval,parent);
      }

      /* 1*X => X */
      if((n->u.s.b.node->token==F_FLOAT && n->u.s.b.node->u.s.a.fnum==1.0) ||
	 (n->u.s.a.node->token==F_FLOAT && n->u.s.a.node->u.s.a.fnum==0.0 &&
	   !(n->u.s.b.node->opt_info & OPT_NOT_THROWABLE)))
      {
	tmp1=n->u.s.a.node;
	n->u.s.a.node=0;
	free_node(n);
	return optimize(tmp1,needlval,parent);
      }
    }
    break;

  case F_DIVIDE:
    if(SAME_TYPE(n->u.s.a.node->type,n->u.s.b.node->type))
    {
      if(n->u.s.b.node->token==F_NUMBER)
      {
	int tmp,e;

	tmp=n->u.s.b.node->u.s.a.number;
	/* X*1 -> X */
	if(tmp==1)
	{
	  tmp1=n->u.s.a.node;
	  n->u.s.a.node=0;
	  free_node(n);
	  return tmp1;
	}

	/* X/(1<<x) -> X>>x */
	for(e=1;e;e*=2)
	{
	  if(tmp==e)
	  {
	    n->u.s.b.node->u.s.a.number=0;
	    while(e>1)
	    {
	      n->u.s.b.node->u.s.a.number++;
	      e/=2;
	    }
	    n->token=F_RSH;
	    n->opt_info &=~ (OPT_OPT_COMPUTED | OPT_OPTIMIZED);
	    return optimize(n,needlval,parent);
	  }
	}
      }

      /* X/1 => X */
      /* 0/X => 0 */
      if((n->u.s.a.node->token==F_NUMBER && n->u.s.a.node->u.s.a.number==0 &&
	  !(n->u.s.b.node->opt_info & OPT_NOT_THROWABLE)) ||
	 (n->u.s.a.node->token==F_FLOAT && n->u.s.a.node->u.s.a.fnum==0.0 &&
	  !(n->u.s.b.node->opt_info & OPT_NOT_THROWABLE)) ||
	 (n->u.s.b.node->token==F_NUMBER && n->u.s.b.node->u.s.a.number==1) ||
	 (n->u.s.b.node->token==F_FLOAT && n->u.s.b.node->u.s.a.fnum==1.0))
      {
	tmp1=n->u.s.a.node;
	n->u.s.a.node=0;
	free_node(n);
	return optimize(tmp1,needlval,parent);
      }
    }
    break;

  case F_ADD:
    /* X+0 => X */
    if(((n->u.s.a.node->type & TYPE_MOD_MASK) == TYPE_NUMBER &&
	n->u.s.b.node->token==F_NUMBER && n->u.s.b.node->u.s.a.number==0) ||
       ((n->u.s.a.node->type & TYPE_MOD_MASK) == TYPE_FLOAT &&
	n->u.s.b.node->token==F_FLOAT && n->u.s.b.node->u.s.a.fnum==0.0) ||
       ((n->u.s.a.node->type & TYPE_MOD_MASK) == TYPE_STRING &&
	n->u.s.b.node->token==F_STRING && !strlen(n->u.s.b.node->u.s.a.str)))
    {
      tmp1=n->u.s.a.node;
      n->u.s.a.node=0;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }
    /* 0+X => X */
    if(((n->u.s.b.node->type & TYPE_MOD_MASK) == TYPE_NUMBER &&
	n->u.s.a.node->token==F_NUMBER && n->u.s.a.node->u.s.a.number==0) ||
       ((n->u.s.b.node->type & TYPE_MOD_MASK) == TYPE_FLOAT &&
	n->u.s.a.node->token==F_FLOAT && n->u.s.a.node->u.s.a.fnum==0.0) ||
       ((n->u.s.b.node->type & TYPE_MOD_MASK) == TYPE_STRING &&
	n->u.s.a.node->token==F_STRING && !strlen(n->u.s.a.node->u.s.a.str)))
    {
      tmp1=n->u.s.b.node;
      n->u.s.b.node=0;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }
    /* (a+b)+c => sum(a,b,c) */
    if(n->u.s.a.node->token==F_ADD &&
       SAME_TYPE(n->u.s.a.node->type,n->u.s.b.node->type))
    {
      n->u.s.a.node->token=F_ARG_LIST;
      n->u.s.a.node->opt_info&=~(OPT_OPT_COMPUTED|OPT_OPTIMIZED);
      n->token=F_ARG_LIST;
      n->opt_info&=~OPT_OPT_COMPUTED;
      return optimize(mkefunnode(F_SUM,n),needlval,parent);
    }

    /* a+(b+c) => sum(a,b,c) */
    if(n->u.s.b.node->token==F_ADD &&
       SAME_TYPE(n->u.s.a.node->type,n->u.s.b.node->type))
    {
      n->u.s.b.node->token=F_ARG_LIST;
      n->u.s.b.node->opt_info&=~(OPT_OPT_COMPUTED|OPT_OPTIMIZED);
      n->token=F_ARG_LIST;
      n->opt_info&=~OPT_OPT_COMPUTED;
      return optimize(mkefunnode(F_SUM,n),needlval,parent);
    }

    /* sum(X)+c => sum(X,c) */
    if(n->u.s.a.node->token==F_EFUN_CALL &&
       n->u.s.a.node->u.s.a.number==F_SUM &&
       SAME_TYPE(n->u.s.a.node->type,n->u.s.b.node->type))
    {
      n->opt_info&=~(OPT_OPT_COMPUTED|OPT_OPTIMIZED);
      n->u.s.a.node->opt_info&=~(OPT_OPT_COMPUTED|OPT_OPTIMIZED);
      tmp1=n->u.s.a.node;
      n->token=F_ARG_LIST;
      n->u.s.a.node=tmp1->u.s.b.node;
      tmp1->u.s.b.node=n;
      return optimize(tmp1,needlval,parent);
    }

    /* c+sum(X) => sum(c,X) */
    if(n->u.s.b.node->token==F_EFUN_CALL &&
       n->u.s.b.node->u.s.a.number==F_SUM &&
       SAME_TYPE(n->u.s.a.node->type,n->u.s.b.node->type))
    {
      n->opt_info&=~(OPT_OPT_COMPUTED|OPT_OPTIMIZED);
      n->u.s.b.node->opt_info&=~(OPT_OPT_COMPUTED|OPT_OPTIMIZED);
      tmp1=n->u.s.b.node;
      n->token=F_ARG_LIST;
      n->u.s.b.node=tmp1->u.s.b.node;
      tmp1->u.s.b.node=n;
      return optimize(tmp1,needlval,parent);
    }
    break;

  case F_ASSIGN:
  case F_ASSIGN_AND_POP:
    /* X=X => {} */
    if(node_is_eq(n->u.s.a.node,n->u.s.b.node))
    {
      free_node(n);
      return NULL;
    }

    if(!n->u.s.b.node) break;

    /* X=X+1 => ++X */
    if(n->u.s.a.node->token==F_ADD &&
       n->u.s.b.node->type==TYPE_NUMBER &&
       node_is_eq(n->u.s.b.node,n->u.s.a.node->u.s.a.node) &&
       n->u.s.a.node->u.s.b.node->token==F_NUMBER &&
       n->u.s.a.node->u.s.b.node->u.s.a.number==1)
    {
      tmp1=mknode(F_INC,n->u.s.b.node,0);
      n->u.s.b.node=NULL;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }

    /* X=X-1 => --X */
    if(n->u.s.a.node->token==F_SUBTRACT &&
       n->u.s.b.node->type==TYPE_NUMBER &&
       node_is_eq(n->u.s.b.node,n->u.s.a.node->u.s.a.node) &&
       n->u.s.a.node->u.s.b.node->token==F_NUMBER &&
       n->u.s.a.node->u.s.b.node->u.s.a.number==1)
    {
      tmp1=mknode(F_DEC,n->u.s.b.node,0);
      n->u.s.b.node=NULL;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }

    if(n->token!=F_ASSIGN || !parent || parent->token=='{' ||
       (parent->token==F_CAST && parent->type==TYPE_VOID))
    {
      /* return value probably not used */
      remember_remember(n,0);
    }else{
      /* return value probably used */
      remember_remember(n,1);
    }
    break;

  case F_EFUN_CALL:
    switch(n->u.s.a.number)
    {
    case F_SIZEOF:
      /* sizeof(m_indices(X)) => size(X)
       * sizeof(m_values(X)) => size(X)
       * sizeof(indices(X)) => size(X) */
      if(n->u.s.b.node &&
	 n->u.s.b.node->token==F_EFUN_CALL)
      {
	switch(n->u.s.b.node->u.s.a.number)
	{
	case F_M_INDICES:
	case F_M_VALUES:
	case F_INDICES:
	  n->u.s.a.number=F_SIZE;
	  tmp1=n->u.s.b.node;
	  n->u.s.b.node=tmp1->u.s.b.node;
	  tmp1->u.s.b.node=NULL;
	  free_node(tmp1);
	  n->opt_info&=~(OPT_OPT_COMPUTED|OPT_OPTIMIZED);
	  return optimize(n,needlval,parent);
	}
      }

      /* this_object()->X => constant function */
    case F_GET_FUNCTION:
      if(n->u.s.b.node &&
	 n->u.s.b.node->token==F_ARG_LIST &&
	 n->u.s.b.node->u.s.a.node &&
	 n->u.s.b.node->u.s.b.node &&
	 n->u.s.b.node->u.s.a.node->token==F_EFUN_CALL &&
	 n->u.s.b.node->u.s.a.node->u.s.a.number==F_THIS_OBJECT &&
	 n->u.s.b.node->u.s.b.node->token==F_STRING)
      {
	int i;
	if((i=defined_function(n->u.s.b.node->u.s.b.node->u.s.a.str))!=-1)
	{
	  free_node(n);
	  return optimize(mkconstfunnode(i),needlval,parent);
	}
      }else{
	node **a;
	int e;
	a=my_get_arg(&(n->u.s.b.node),1);
	if(a && *a && (*a)->token==F_STRING)
	{
	  for(e=0;e<num_lfuns;e++)
	  {
	    if(lfuns[e]==(*a)->u.s.a.str) /* shared strings */
	    {
	      free_node(*a);
	      *a=mkintnode(e);
	      n->u.s.a.number=F_GET_LFUN;
	      break;
	    }
	  }
	}
      }
      break;
      
    case F_MAP_ARRAY:
    case F_SEARCH_ARRAY:
    case F_FILTER_ARRAY:
    case F_MAP_MAPPING:
    case F_FILTER_MAPPING:
    {
      node **a;
      int e;
      a=my_get_arg(&(n->u.s.b.node),1);
      if(a && *a && (*a)->token==F_STRING)
      {
	for(e=0;e<num_lfuns;e++)
	{
	  if(lfuns[e]==(*a)->u.s.a.str) /* shared strings */
	  {
	    free_node(*a);
	    *a=mkintnode(e);
	    break;
	  }
	}
      }
      break;
    }
	
      /* sum(a+b,X) => sum(a,b,X) */
    case F_SUM:
    {
      node **t,**tt;
#ifdef MALLOC_DEBUG
      check_sfltable();
#endif
      t=first_arg(&n->u.s.b.node);
      tt=first_arg(&n->u.s.b.node);
      if(t && tt &&
	 (*t)->token==F_ADD &&
	 SAME_TYPE((*t)->type,(*tt)->type))
      {
	(*t)->token=F_ARG_LIST;
	(*t)->opt_info&=~(OPT_OPT_COMPUTED|OPT_OPTIMIZED);
	return optimize(n,needlval,parent);
      }
#ifdef MALLOC_DEBUG
      check_sfltable();
#endif
      break;
    }
    }
    break;

  case F_XOR:
    /* X^0 => X */
    if(((n->u.s.a.node->type & TYPE_MOD_MASK) == TYPE_NUMBER &&
	n->u.s.b.node->token==F_NUMBER && n->u.s.b.node->u.s.a.number==0))
    {
      tmp1=n->u.s.a.node;
      n->u.s.a.node=0;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }
    /* 0^X => X */
    if(((n->u.s.a.node->type & TYPE_MOD_MASK) == TYPE_NUMBER &&
	n->u.s.b.node->token==F_NUMBER && n->u.s.b.node->u.s.a.number==-1))
    {
      tmp1=n->u.s.a.node;
      n->u.s.a.node=0;
      free_node(n);
      return optimize(mknode(F_COMPL,tmp1,0),needlval,parent);
    }
    break;

  case F_AND:
    /* X&-1 => X; */
    if((n->u.s.a.node->type== TYPE_NUMBER &&
	n->u.s.b.node->token==F_NUMBER && n->u.s.b.node->u.s.a.number==-1) ||
       (n->u.s.b.node->type== TYPE_NUMBER &&
	n->u.s.a.node->token==F_NUMBER && n->u.s.a.node->u.s.a.number==0))
    {
      tmp1=n->u.s.a.node;
      n->u.s.a.node=0;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }
    /* -1&X => X; */
    if((n->u.s.b.node->type == TYPE_NUMBER &&
	n->u.s.a.node->token==F_NUMBER && n->u.s.a.node->u.s.a.number==-1) ||
       (n->u.s.a.node->type == TYPE_NUMBER &&
	n->u.s.b.node->token==F_NUMBER && n->u.s.b.node->u.s.a.number==0))
    {
      tmp1=n->u.s.b.node;
      n->u.s.b.node=0;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }
    break;

  case F_LAND:
    /* !a and !b -> !(a or b) */
    if(n->u.s.a.node->token==F_NOT && n->u.s.b.node->token==F_NOT)
    {
      tmp1=mknode(F_NOT,mknode(F_LOR,n->u.s.a.node->u.s.a.node,
			       n->u.s.b.node->u.s.a.node),0);
      n->u.s.a.node->u.s.a.node=NULL;
      n->u.s.b.node->u.s.a.node=NULL;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }
    /* (a && b) && c -> a && (b && c) */
    while(n->u.s.a.node->token==F_LAND)
    {
      tmp1=n->u.s.a.node;
      n->u.s.a.node=tmp1->u.s.a.node;
      tmp1->u.s.a.node=tmp1->u.s.b.node;
      tmp1->u.s.b.node=n->u.s.b.node;
      n->u.s.b.node=tmp1;
    }

    /* 1 && X => X */
    if(node_is_true(n->u.s.a.node))
    {
      tmp1=n->u.s.b.node;
      n->u.s.b.node=NULL;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }

    /* X && 1 -> X */
    if(node_is_true(n->u.s.b.node))
    {
      tmp1=n->u.s.a.node;
      n->u.s.a.node=NULL;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }

    /* 0 && X => 0 */
    if(node_is_false(n->u.s.a.node))
    {
      free_node(n);
      return mkintnode(0);
    }
    break;

  case F_LOR:
    /* !a || !b -> !(a || b) */
    if(n->u.s.a.node->token==F_NOT && n->u.s.b.node->token==F_NOT)
    {
      tmp1=mknode(F_NOT,mknode(F_LAND,n->u.s.a.node->u.s.a.node,
			       n->u.s.b.node->u.s.a.node),0);
      n->u.s.a.node->u.s.a.node=NULL;
      n->u.s.b.node->u.s.a.node=NULL;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }
    /* (a && b) && c -> a && (b && c) */
    while(n->u.s.a.node->token==F_LOR)
    {
      tmp1=n->u.s.a.node;
      n->u.s.a.node=tmp1->u.s.a.node;
      tmp1->u.s.a.node=tmp1->u.s.b.node;
      tmp1->u.s.b.node=n->u.s.b.node;
      n->u.s.b.node=tmp1;
    }

    /* 0 || X => X */
    if(node_is_false(n->u.s.a.node))
    {
      tmp1=n->u.s.b.node;
      n->u.s.b.node=NULL;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }

    /* X || 0 => X */
    if(node_is_false(n->u.s.b.node))
    {
      tmp1=n->u.s.a.node;
      n->u.s.a.node=NULL;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }

    /* true || X => true */
    if(node_is_true(n->u.s.a.node))
    {
      tmp1=n->u.s.a.node;
      n->u.s.a.node=NULL;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }
    break;

  case F_OR:
    /* 0 | X => X */
    if((n->u.s.a.node->type == TYPE_NUMBER &&
	n->u.s.b.node->token==F_NUMBER && n->u.s.b.node->u.s.a.number==-1) ||
       (n->u.s.b.node->type == TYPE_NUMBER &&
	n->u.s.a.node->token==F_NUMBER && n->u.s.a.node->u.s.a.number==0))
    {
      tmp1=n->u.s.b.node;
      n->u.s.b.node=0;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }
    /* X | 0 => X */
    if((n->u.s.b.node->type == TYPE_NUMBER &&
	n->u.s.a.node->token==F_NUMBER && n->u.s.a.node->u.s.a.number==-1) ||
       (n->u.s.a.node->type== TYPE_NUMBER &&
	n->u.s.b.node->token==F_NUMBER && n->u.s.b.node->u.s.a.number==0))
    {
      tmp1=n->u.s.a.node;
      n->u.s.a.node=0;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }
    break;

  case F_CAST:
    if(n->type==TYPE_VOID)
    {
      if(!(n->u.s.a.node) || !(n->opt_info & (OPT_SIDE_EFFECT |
					      OPT_LOCAL_ASSIGNMENT |
					      OPT_GLOBAL_ASSIGNMENT |
					      OPT_RESTORE_OBJECT |
					      OPT_CASE |
					      OPT_RETURN |
					      OPT_CONT |
					      OPT_BREAK
					      )))
      {
	free_node(n);
	return NULL;
      }
      tmp1=n->u.s.a.node;
      switch(tmp1->token)
      {
      case F_ASSIGN:
	tmp1->token=F_ASSIGN_AND_POP;
	tmp1->type=TYPE_VOID;
	n->u.s.a.node=0;
	free_node(n);
	return optimize(tmp1,needlval,parent);

      case F_INC:
      case F_POST_INC:
	tmp1->token=F_INC_AND_POP;
	tmp1->type=TYPE_VOID;
	n->u.s.a.node=0;
	free_node(n);
	return optimize(tmp1,needlval,parent);

      case F_DEC:
      case F_POST_DEC:
	tmp1->token=F_DEC_AND_POP;
	tmp1->type=TYPE_VOID;
	n->u.s.a.node=0;
	free_node(n);
	return optimize(tmp1,needlval,parent);
      }
      /*
	 if(opt_to_assignments)
	 {
	 remember_all_variables();
	 eval(n->u.s.b.node);
	 compare_all_variables_and_make_assignments();
	 
	 }
	 */
     
    }

    if(!n->u.s.a.node)
    {
      switch(n->type)
      {
      case TYPE_NUMBER:
	free_node(n);
	return mkintnode(0);

      case TYPE_STRING:
	free_node(n);
	return mkstrnode(make_shared_string(""));

      }
    }
    if(n->type==(n->u.s.a.node->type & TYPE_MOD_MASK))
    {
      tmp1=n->u.s.a.node;
      n->u.s.a.node=0;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }
    if(n->type==TYPE_FUNCTION && n->u.s.a.node->token==F_STRING)
    {
      int i;
      if(-1!=(i=defined_function(n->u.s.a.node->u.s.a.str)))
      {
	free_node(n);
	return mkconstfunnode(i);
      }
    }
    break;

  case F_FOR:
  {
    node **last;
    int inc,less;
    if(node_is_false(n->u.s.a.node))
    {
      free_node(n);
      return NULL;
    }

    if(n->u.s.a.node &&
       (n->u.s.a.node->token==F_INC ||
	n->u.s.a.node->token==F_DEC ||
	n->u.s.a.node->token==F_POST_INC ||
	n->u.s.a.node->token==F_POST_DEC) &&
       (!n->u.s.b.node->u.s.a.node &&
	!(n->u.s.b.node->u.s.b.node->opt_info & OPT_CONT)))
    {
      if(n->u.s.a.node->token==F_DEC || n->u.s.a.node->token==F_POST_DEC)
	n->token=F_DEC_LOOP;
      else
	n->token=F_INC_LOOP;
      n->u.s.a.node->token=' ';
      n->u.s.a.node->u.s.b.node=n->u.s.a.node->u.s.a.node;
      n->u.s.a.node->u.s.a.node=mkintnode(0);
      n->u.s.a.node->opt_info&=~(OPT_OPT_COMPUTED|OPT_OPTIMIZED);
      n->u.s.b.node->token='{';
      n->u.s.b.node->opt_info&=~(OPT_OPT_COMPUTED|OPT_OPTIMIZED);
      n->opt_info&=~(OPT_OPT_COMPUTED|OPT_OPTIMIZED);
      return optimize(n,needlval,parent);
    }

    last=&(n->u.s.b.node->u.s.b.node);
    tmp1=last?*last:(node *)NULL;
    if(tmp1 && (tmp1->token==F_INC ||
		tmp1->token==F_POST_INC ||
		tmp1->token==F_INC_AND_POP ||
		tmp1->token==F_DEC_AND_POP ||
		tmp1->token==F_DEC ||
		tmp1->token==F_POST_DEC))
    {
      if(tmp1->token==F_INC || tmp1->token==F_POST_INC ||
	 tmp1->token==F_INC_AND_POP)
	inc=1;
      else
	inc=0;

      less=0;
      if(n->u.s.a.node->token!=F_GT &&
	 n->u.s.a.node->token!=F_GE &&
	 n->u.s.a.node->token!=F_LE &&
	 n->u.s.a.node->token!=F_NE &&
	 n->u.s.a.node->token!=F_LT)
	break;

      if(!node_is_eq(n->u.s.a.node->u.s.a.node,tmp1->u.s.a.node) ||
	 depend_p(n->u.s.a.node->u.s.b.node,n->u.s.b.node->u.s.a.node) ||
	 depend_p(n->u.s.a.node->u.s.b.node,n->u.s.a.node->u.s.a.node))
      {
	if(node_is_eq(n->u.s.a.node->u.s.b.node,tmp1->u.s.a.node) &&
	   !depend_p(n->u.s.a.node->u.s.a.node,n->u.s.b.node->u.s.a.node) &&
	   !depend_p(n->u.s.a.node->u.s.a.node,n->u.s.a.node->u.s.b.node))
	{
	  switch(n->u.s.a.node->token)
	  {
	  case F_LT: n->u.s.a.node->token=F_GT; break;
	  case F_LE: n->u.s.a.node->token=F_GT; break;
	  case F_GT: n->u.s.a.node->token=F_LE; break;
	  case F_GE: n->u.s.a.node->token=F_LE; break;
	  }
	  tmp2=n->u.s.a.node->u.s.a.node;
	  n->u.s.a.node->u.s.a.node=n->u.s.a.node->u.s.b.node;
	  n->u.s.a.node->u.s.b.node=tmp2;
	}else{
	  break;
	}
      }
      if(inc && n->u.s.a.node->token==F_LE)
	tmp3=mknode(F_ADD,n->u.s.a.node->u.s.b.node,mkintnode(1));
      else if(inc && n->u.s.a.node->token==F_LT)
	tmp3=n->u.s.a.node->u.s.b.node;
      else if(!inc && n->u.s.a.node->token==F_GE)
	tmp3=mknode(F_SUBTRACT,n->u.s.a.node->u.s.b.node,mkintnode(1));
      else if(!inc && n->u.s.a.node->token==F_GT)
	tmp3=n->u.s.a.node->u.s.b.node;
      else
	break;

      *last=0;
      if(n->u.s.a.node->token==F_NE)
      {
	if(inc)
	{
	  tmp2=mknode(F_INC_NEQ_LOOP,mknode(' ',tmp3,n->u.s.a.node->u.s.a.node),n->u.s.b.node->u.s.a.node);
	}else{
	  tmp2=mknode(F_DEC_NEQ_LOOP,mknode(' ',tmp3,n->u.s.a.node->u.s.a.node),n->u.s.b.node->u.s.a.node);
	}
      }else{
	if(inc)
	{
	  tmp2=mknode(F_INC_LOOP,mknode(' ',tmp3,n->u.s.a.node->u.s.a.node),n->u.s.b.node->u.s.a.node);
	}else{
	  tmp2=mknode(F_DEC_LOOP,mknode(' ',tmp3,n->u.s.a.node->u.s.a.node),n->u.s.b.node->u.s.a.node);
	}
      }
      n->u.s.a.node->u.s.a.node=0;
      n->u.s.a.node->u.s.b.node=0;
      n->u.s.b.node->u.s.a.node=0;
      n->u.s.b.node->u.s.b.node=0;
      free_node(n);
      if(inc)
      {
	tmp1->token=F_DEC;
      }else{
	tmp1->token=F_INC;
      }
      tmp1=mknode('{',mkcastnode(TYPE_VOID,tmp1),tmp2);
      return optimize(tmp1,needlval,parent);
    }
  }
    break;

  case F_ADD_EQ:
    if(n->u.s.a.node->type==TYPE_NUMBER &&
       n->u.s.b.node->token==F_NUMBER && n->u.s.b.node->u.s.a.number==1)
    {
      tmp1=mknode(F_INC,n->u.s.a.node,0);
      n->u.s.a.node=0;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }

    if(n->u.s.a.node->type==TYPE_NUMBER &&
       n->u.s.b.node->token==F_NUMBER && n->u.s.b.node->u.s.a.number==-1)
    {
      tmp1=mknode(F_DEC,n->u.s.a.node,0);
      n->u.s.a.node=0;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }
    break;

  case F_SUB_EQ:
    if(n->u.s.a.node->type==TYPE_NUMBER &&
       n->u.s.b.node->token==F_NUMBER && n->u.s.b.node->u.s.a.number==1)
    {
      tmp1=mknode(F_DEC,n->u.s.a.node,0);
      n->u.s.a.node=0;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }

    if(n->u.s.a.node->type==TYPE_NUMBER &&
       n->u.s.b.node->token==F_NUMBER && n->u.s.b.node->u.s.a.number==-1)
    {
      tmp1=mknode(F_ADD,n->u.s.a.node,0);
      n->u.s.a.node=0;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }
    break;

  case F_INC_NEQ_LOOP:
  case F_DEC_NEQ_LOOP:
  case F_INC_LOOP:
  case F_DEC_LOOP:
    if(!n->u.s.b.node)
    {
      n->u.s.a.node->token=F_ASSIGN;
      n->token=F_CAST;
      n->type=TYPE_VOID;
      n->opt_info&=~(OPT_OPT_COMPUTED|OPT_OPTIMIZED);
      n->u.s.a.node->opt_info&=~(OPT_OPT_COMPUTED|OPT_OPTIMIZED);
      n->u.s.a.node->type=n->u.s.a.node->u.s.b.node->type;
      return optimize(n,needlval,parent);
    }
    if(!((n->u.s.a.node->opt_info | n->u.s.b.node->opt_info)&
	 (OPT_SIDE_EFFECT | OPT_RETURN | OPT_CONT | OPT_BREAK)) &&
       !depend_p(n->u.s.a.node,n->u.s.b.node) &&
       !depend_p(n->u.s.b.node,n->u.s.b.node) &&
       !depend_p(n->u.s.b.node,n->u.s.a.node))
    {
      n->u.s.a.node->token=F_ASSIGN;
      n->token='{';
      n->type=TYPE_VOID;
      n->opt_info&=~(OPT_OPT_COMPUTED|OPT_OPTIMIZED);
      n->u.s.a.node->opt_info&=~(OPT_OPT_COMPUTED|OPT_OPTIMIZED);
      n->u.s.a.node->type=n->u.s.a.node->u.s.b.node->type;
      return optimize(n,needlval,parent);
    }
    break;

  case F_SWITCH:
  {
    node **tmp;
    while((tmp=last_cmd(&(n->u.s.b.node))) && (*tmp)->token==F_BREAK)
    {
      free_node(*tmp);
      *tmp=0;
    }
    if(!n->u.s.b.node)
    {
      n->token=F_CAST;
      n->type=TYPE_VOID;
      n->opt_info&=~(OPT_OPT_COMPUTED|OPT_OPTIMIZED);
      return optimize(n,needlval,parent);
    }
  }
  case F_DO:
  {
    if(node_is_true(n->u.s.b.node))
    {
      n->token=F_BRANCH;
      free_node(n->u.s.b.node);
      n->u.s.b.node=NULL;
    }
    if(node_is_false(n->u.s.b.node))
    {
      tmp1=n->u.s.a.node;
      n->u.s.a.node=NULL;
      free_node(n);
      return optimize(tmp1,needlval,parent);
    }
  }
  }

#ifdef MALLOC_DEBUG
    check_sfltable();
#endif
  if(is_const(n) &&
     (n->opt_info & OPT_TRY_OPTIMIZE) &&
     parent!=n && /* this is used to avoid doing things a zillion times */
/*     (!parent || !is_const(parent)) && I would like to do this
 * but it is not yet known if parent is const
 */
     !num_parse_error)
  {
    tmp1=eval(n);
    free_node(n);
    return tmp1;
  }
#ifdef LINMALLOC_DEBUG
  assert_node(n);
#endif
  return n;
}


/* depend_optim: nullify a node at a time and see anything depends on it
 * beeing there
 */
static int depend_optim(node **a,node *b,int chronologie,int voided,node *tree2)
{
  node *n,*n2;
  if(!(n=*a)) return 0;
#ifdef LINMALLOC_DEBUG
  assert_node(*a);
#endif
#ifdef MALLOC_DEBUG
    check_sfltable();
#endif
  if(!(n->opt_info & (OPT_SIDE_EFFECT | OPT_CASE | OPT_CONT | OPT_BREAK |
	 OPT_GLOBAL_ASSIGNMENT | OPT_RETURN)))
  {
    *a=0;
    if(!depend_p(b,n) && !depend_p(tree2,n))
    {
      free_node(n);
      return 1;
    }
    *a=n;
  }
  switch(n->token)
  {
  case F_DO:
  {
    int i;
    tree2=mknode('{',n->u.s.b.node,tree2);
    i=depend_optim(&(n->u.s.a.node),b,0,0,tree2);
    n2=tree2->u.s.a.node;
    tree2->u.s.b.node=tree2->u.s.a.node=NULL;
    free_node(tree2);
    tree2=n2;
    if(i)
    {
      n->opt_info&=~OPT_OPTIMIZED;
      return 1;
    }
    return 0;
  }

  case F_INC_NEQ_LOOP:
  case F_DEC_NEQ_LOOP:
  case F_INC_LOOP:
  case F_DEC_LOOP:
  case F_FOREACH:
  case F_FOR:
  {
    int i;
    tree2=mknode('{',n->u.s.a.node,tree2);
    i=depend_optim(&(n->u.s.b.node),b,0,0,tree2);
    n2=tree2->u.s.a.node;
    tree2->u.s.b.node=tree2->u.s.a.node=NULL;
    free_node(tree2);
    tree2=n2;
    if(i)
    {
      n->opt_info&=~OPT_OPTIMIZED;
      return 1;
    }
    return 0;
  }

  case F_CAST:
    if(n->type==TYPE_VOID)
    {
      if(depend_optim(&(n->u.s.a.node),b,chronologie,1,tree2))
      {
	n->opt_info&=~OPT_OPTIMIZED;
	return 1;
      }else{
	return 0;
      }
    }
    return 0;

  case F_ARG_LIST:
  case '{':
    if(n->token == F_ARG_LIST && !voided) return 0;
    if(chronologie)
    {
      int i;
      /* if chronological dependency is wanted, nullify and ignore
       * things that happened before this node while testing
       */
      while(depend_optim(&(n->u.s.a.node),b,chronologie,1,tree2))
	n->opt_info&=~OPT_OPTIMIZED;

      n2=n->u.s.a.node;
      n->u.s.a.node=0;
      i=depend_optim(&(n->u.s.b.node),b,chronologie,1,tree2);
      if(i) n->opt_info&=~OPT_OPTIMIZED;
      n->u.s.a.node=n2;
      return i;
    }else{
      int i;

      tree2=mknode('{',n->u.s.b.node,tree2);
      n->u.s.b.node=NULL;
      i=depend_optim(&(n->u.s.a.node),b,chronologie,1,tree2);
      n->u.s.b.node=tree2->u.s.a.node;
      n2=tree2;
      tree2=tree2->u.s.b.node;
      n2->u.s.a.node=n2->u.s.b.node=NULL;
      free_node(n2);

      tree2=mknode('{',tree2,n->u.s.a.node);
      n->u.s.a.node=NULL;
      i|=depend_optim(&(n->u.s.b.node),b,chronologie,1,tree2);
      n->u.s.a.node=tree2->u.s.b.node;
      n2=tree2;
      tree2=tree2->u.s.a.node;
      n2->u.s.b.node=n2->u.s.a.node=NULL;
      free_node(n2);
      if(i) n->opt_info&=~OPT_OPTIMIZED;
      return i;
    }
  }
  return 0;
}

/* we need to dome computations to see if the arguments are pointer arguments
 * if so, let the function depend on those, so we won't remove code that
 * changes shared arrays.
 */
static void do_depend_optim(node **n,int args)
{
  node *tree2;
  int e;
  tree2=NULL;
  for(e=0;e<args;e++)
  {
    if(type_of_locals[e]==TYPE_MAPPING ||
       type_of_locals[e]&TYPE_MOD_POINTER)
    {
      tree2=mknode(F_ARG_LIST,mklocalnode(e),tree2);
    }
  }
  tree2=call_optimizer(mknode(F_RETURN,mkefunnode(F_AGGREGATE,tree2),0));
  while(depend_optim(n,*n,1,0,tree2));
  free_node(tree2);
}

static void do_pop(int nr)
{
  if(!nr) return;
  if(nr==1)
  {
    ins_f_byte(F_POP_VALUE);
  }else if(nr<256){
    ins_f_byte(F_POP_N_ELEMS);
    ins_byte(nr,A_PROGRAM);
  }else{
    ins_f_byte(F_POP_N_ELEMS);
    ins_byte(255,A_PROGRAM);
    do_pop(nr-255);
  }
}

#define JUMP_CONDITIONAL 1
#define JUMP_UNSET 2

struct jump
{
  unsigned char flags;
  unsigned char size;
  short token;
  int whereto;
  int wherefrom;
  int address;
};

static int jump_ptr, max_jumps;
static struct jump jumps[256];

static void set_branch(int address,int whereto)
{
  int j,e;

  if(address<0) return;
  j=jump_ptr;
  e=max_jumps;
  while(e>=0)
  {
    if(jumps[j].address==address && (jumps[j].flags & JUMP_UNSET))
      break;
    if(j) j--; else j=max_jumps;
    e--;
  }
  if(e<0) j=-1; /* not found */

  for(e=0;e<=max_jumps;e++)
  {
    if(jumps[e].flags & JUMP_UNSET) continue;
    if(jumps[e].flags & JUMP_CONDITIONAL) continue;
    if(jumps[e].wherefrom!=whereto) continue;
#if defined(LASDEBUG)
    if(lasdebug>1) printf("Optimized Jump to a jump\n");
#endif
    whereto=jumps[e].whereto;
    break;
  }
  
  if(e>=0)
  {
    if(!(jumps[j].flags & JUMP_CONDITIONAL))
    {
      for(e=0;e<=max_jumps;e++)
      {
	if(jumps[e].flags & JUMP_UNSET) continue;
	if(jumps[e].size!=4) continue;
	if(jumps[e].whereto==jumps[j].wherefrom)
	{
	  upd_branch(jumps[e].address,whereto);
	  jumps[e].whereto=whereto;
#if defined(LASDEBUG)
	  if(lasdebug>1) printf("Optimized Jump to a jump\n");
#endif
	}
      }
    }
    jumps[j].whereto=whereto;
    jumps[j].flags&=~JUMP_UNSET;
  }
  upd_branch(address,whereto);
}

static int do_jump(int token,int whereto)
{
  jump_ptr=(jump_ptr+1)%NELEM(jumps);
  if(jump_ptr>max_jumps) max_jumps=jump_ptr;

  jumps[jump_ptr].flags=JUMP_UNSET;
  jumps[jump_ptr].whereto=-1;
  jumps[jump_ptr].token=token;
  jumps[jump_ptr].size=sizeof(int);

  if(token!=F_BRANCH && token!=F_SHORT_BRANCH)
    jumps[jump_ptr].flags|=JUMP_CONDITIONAL;

  if(token==F_SHORT_BRANCH ||
     token==F_SHORT_BRANCH_WHEN_ZERO ||
     token==F_SHORT_BRANCH_WHEN_NON_ZERO) /* not used (yet) */
    jumps[jump_ptr].size=1;

  jumps[jump_ptr].wherefrom=PC;
  ins_f_byte(token);
  jumps[jump_ptr].address=PC;
  ins_long(0,A_PROGRAM);
  if(whereto!=-1) set_branch(jumps[jump_ptr].address,whereto);
  return jumps[jump_ptr].address;
}

static void clean_jumptable() { max_jumps=jump_ptr=-1; }

static void push_break_stack()
{
  push_explicit((int)break_stack);
  push_explicit(current_break);
  push_explicit(break_stack_size);
  break_stack_size=10;
  break_stack=(int *)xalloc(sizeof(int)*break_stack_size);
  current_break=0;
}

static void pop_break_stack(int jump)
{
  for(current_break--;current_break>=0;current_break--)
    set_branch(break_stack[current_break],jump);

  free((char *)break_stack);
  break_stack_size=pop_address();
  current_break=pop_address();
  break_stack=(int *)pop_address();
}

static void push_continue_stack()
{
  push_explicit((int)continue_stack);
  push_explicit(current_continue);
  push_explicit(continue_stack_size);
  continue_stack_size=10;
  continue_stack=(int *)xalloc(sizeof(int)*continue_stack_size);
  current_continue=0;
}

static void pop_continue_stack(int jump)
{
  for(current_continue--;current_continue>=0;current_continue--)
    set_branch(continue_stack[current_continue],jump);

  free((char *)continue_stack);
  continue_stack_size=pop_address();
  current_continue=pop_address();
  continue_stack=(int *)pop_address();
}

static int do_docode2(node *n,int needlval);

static int do_docode(node *n,int needlval)
{
  int i;
  int save_current_line=current_line;
  if(!n) return 0;
  current_line=n->lnum;
  i=do_docode2(n,needlval);
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  current_line=save_current_line;
  return i;
}

static int docode(node *n)
{
  clean_jumptable();
  return do_docode(n,0);
}

static int do_jump_when_non_zero(node *n,int j)
{
  if(!(n->opt_info & OPT_SIDE_EFFECT))
  {
    if(node_is_true(n))
      return do_jump(F_BRANCH,j);

    if(node_is_false(n))
      return -1;
  }
  if(do_docode(n,0)!=1)
    fatal("Infernal compiler skiterror.\n");
  return do_jump(F_BRANCH_WHEN_NON_ZERO,j);
}

static void ins_add(node *a,node *b)
{
  if(a->type==b->type)
  {
    if(a->type==TYPE_NUMBER)
      ins_f_byte(F_ADD_INT);
    else
      ins_f_byte(F_ADD);
  }else{
    ins_f_byte(F_ADD);
  }
}

static int do_jump_when_zero(node *n,int j)
{
  if(!(n->opt_info & OPT_SIDE_EFFECT))
  {
    if(node_is_true(n))
      return -1;

    if(node_is_false(n))
      return do_jump(F_BRANCH,j);
  }
  if(do_docode(n,0)!=1)
    fatal("Infernal compiler skiterror.\n");
  return do_jump(F_BRANCH_WHEN_ZERO,j);
}

static int do_docode2(node *n,int needlval)
{
  int tmp1,tmp2,tmp3;

#ifdef MALLOC_DEBUG
    check_sfltable();
#endif

  if(!n) return 0;

  if(needlval & 1)
  {
    switch(n->token)
    {
    case F_ASSIGN:
      if(needlval & 2) break;

    default:
      yyerror("Illegal lvalue.");
      ins_int(0);
      return 1;

    case F_LVALUE_LIST:
    case F_LOCAL:
    case F_GLOBAL:
    case F_INDEX:
    case F_ARG_LIST:
      break;
    }
  }
  switch(n->token)
  {
  case F_PUSH_ARRAY:
    tmp1=do_docode(n->u.s.a.node,2);
    if(tmp1!=1)
    {
      fatal("Internal compiler error, Yikes!\n");
    }
    ins_f_byte(F_PUSH_ARRAY);
    return -0x7ffffff;

  case F_STRING:
    tmp1=store_prog_string(n->u.s.a.str);
    if(tmp1<256)
    {
      ins_f_byte(F_SHORT_STRING);
      ins_byte(tmp1,A_PROGRAM);
    }else{
      ins_f_byte(F_STRING);
      ins_short(tmp1,A_PROGRAM);
    }
    return 1;

  case F_NUMBER:
    ins_int(n->u.s.a.number);
    return 1;

  case F_FLOAT:
    ins_float(n->u.s.a.fnum);
    return 1;

  case '?':
  {
    int tmp5=0;			/* make gcc happy */
    tmp1=do_jump_when_zero(n->u.s.a.node,-1);
    tmp3=do_docode(n->u.s.b.node->u.s.a.node,0);
    if(tmp3)
    {
      ins_f_byte(F_POP_N_ELEMS);
      tmp5=PC;
      ins_byte(0,A_PROGRAM);
    }
    if(n->u.s.b.node->u.s.b.node)
    {
      int tmp4;
      tmp2=do_jump(F_BRANCH,-1);
      set_branch(tmp1,PC);
      tmp4=do_docode(n->u.s.b.node->u.s.b.node,0);
      if(tmp4>tmp3)
      {
	do_pop(tmp4-tmp3);
      }else if(tmp4<tmp3){
	mem_block[A_PROGRAM].block[tmp5]=tmp3-tmp4;
	tmp3=tmp4;
      }
      tmp1=tmp2;
    }else{
      if(tmp3)
      {
	mem_block[A_PROGRAM].block[tmp5]=tmp3;
	tmp3=0;
      }
    }
    set_branch(tmp1,PC);
    return tmp3;
  }
      
  case F_AND_EQ:
  case F_OR_EQ:
  case F_XOR_EQ:
  case F_LSH_EQ:
  case F_RSH_EQ:
  case F_ADD_EQ:
  case F_SUB_EQ:
  case F_MULT_EQ:
  case F_MOD_EQ:
  case F_DIV_EQ:
    tmp1=do_docode(n->u.s.a.node,1);

    if(n->u.s.a.node->type & TYPE_MOD_POINTER)
    {
      if(do_docode(n->u.s.b.node,2)!=1)
	fatal("Internal compiler error, shit happens\n");
      ins_f_byte(F_PUSH_LTOSVAL2);
    }else{
      ins_f_byte(F_PUSH_LTOSVAL);
      if(do_docode(n->u.s.b.node,2)!=1)
	fatal("Internal compiler error, shit happens (again)\n");
    }


    switch(n->token)
    {
    case F_ADD_EQ:
      ins_add(n->u.s.a.node,n->u.s.b.node);
      break;
    case F_AND_EQ: ins_f_byte(F_AND); break;
    case F_OR_EQ:  ins_f_byte(F_OR);  break;
    case F_XOR_EQ: ins_f_byte(F_XOR); break;
    case F_LSH_EQ: ins_f_byte(F_LSH); break;
    case F_RSH_EQ: ins_f_byte(F_RSH); break;
    case F_SUB_EQ: ins_f_byte(F_SUBTRACT); break;
    case F_MULT_EQ:ins_f_byte(F_MULTIPLY);break;
    case F_MOD_EQ: ins_f_byte(F_MOD); break;
    case F_DIV_EQ: ins_f_byte(F_DIVIDE); break;
    }
    ins_f_byte(F_ASSIGN);
    return tmp1;

  case F_ASSIGN:
  case F_ASSIGN_AND_POP:
    switch(n->u.s.a.node->token)
    {
    case F_AND:
    case F_OR:
    case F_XOR:
    case F_LSH:
    case F_RSH:
    case F_ADD:
    case F_MOD:
    case F_SUBTRACT:
    case F_DIVIDE:
    case F_MULTIPLY:
      if(node_is_eq(n->u.s.b.node,n->u.s.a.node->u.s.a.node))
      {
	tmp1=do_docode(n->u.s.b.node,1);
	if((n->u.s.b.node->type & TYPE_MOD_POINTER) ||
	   (n->u.s.b.node->type & TYPE_MOD_MASK)==TYPE_ANY)
	{
	  if(do_docode(n->u.s.a.node->u.s.b.node,2)!=1)
	    fatal("Infernal compiler error (dumpar core |ver hela mattan).\n");
	  ins_f_byte(F_PUSH_LTOSVAL2);
	}else{
	  ins_f_byte(F_PUSH_LTOSVAL);
	  if(do_docode(n->u.s.a.node->u.s.b.node,2)!=1)
	    fatal("Infernal compiler error (dumpar core).\n");
	}

	if(n->u.s.a.node->token==F_ADD)
	  ins_add(n->u.s.a.node->u.s.a.node,n->u.s.a.node->u.s.b.node);
	else
	  ins_f_byte(n->u.s.a.node->token);

	ins_f_byte(n->token);
	return n->token==F_ASSIGN;
      }

    default:
      if(n->u.s.b.node->token!=F_LOCAL && n->u.s.b.node->token!=F_GLOBAL)
	tmp1=do_docode(n->u.s.b.node,1);

      if(do_docode(n->u.s.a.node,0)!=1)
	fatal("Infernal compiler error *sigh*.\n");
      switch(n->u.s.b.node->token)
      {
      case F_LOCAL:
	ins_f_byte(n->token==F_ASSIGN?F_ASSIGN_LOCAL:F_ASSIGN_LOCAL_AND_POP);
	ins_byte(n->u.s.b.node->u.s.a.number,A_PROGRAM);
	break;

      case F_GLOBAL:
	ins_f_byte(n->token==F_ASSIGN?F_ASSIGN_GLOBAL:F_ASSIGN_GLOBAL_AND_POP);
	ins_byte(n->u.s.b.node->u.s.a.number,A_PROGRAM);
	break;

      default:
	ins_f_byte(n->token);
	break;
      }
      return n->token==F_ASSIGN;
    }

  case F_LAND:
  case F_LOR:
    if(do_docode(n->u.s.a.node,0)!=1)
      fatal("Compiler internal error.\n");
    tmp1=do_jump(n->token,-1);
    if(do_docode(n->u.s.b.node,0)!=1)
      fatal("Compiler internal error.\n");
    set_branch(tmp1,PC);
    return 1;

  case F_ADD:
    tmp1=do_docode(n->u.s.a.node,2);
    if(do_docode(n->u.s.b.node,2)!=1)
      fatal("Compiler internal error.\n");
    ins_add(n->u.s.a.node,n->u.s.b.node);
    return tmp1;

  case F_EQ:
  case F_NE:
    tmp1=do_docode(n->u.s.a.node,0);
    if(do_docode(n->u.s.b.node,0)!=1)
      fatal("Compiler internal error (gnng!).\n");
    ins_f_byte(n->token);
    return tmp1;

  case F_LT:
  case F_LE:
  case F_GT:
  case F_GE:
  case F_SUBTRACT:
  case F_MULTIPLY:
  case F_DIVIDE:
  case F_MOD:
  case F_LSH:
  case F_RSH:
  case F_XOR:
  case F_OR:
  case F_AND:
    tmp1=do_docode(n->u.s.a.node,2);
    if(do_docode(n->u.s.b.node,2)!=1)
      fatal("Compiler internal error.\n");
    ins_f_byte(n->token);
    return tmp1;

  case F_RANGE:
    tmp1=do_docode(n->u.s.a.node,2);
    if(do_docode(n->u.s.b.node,2)!=2)
      fatal("Compiler internal error.\n");
    ins_f_byte(n->token);
    return tmp1;

  case F_INC:
  case F_DEC:
  case F_POST_INC:
  case F_POST_DEC:
    tmp1=do_docode(n->u.s.a.node,1);
    ins_f_byte(n->token);
    return tmp1;

  case F_INC_AND_POP:
  case F_DEC_AND_POP:
    tmp1=do_docode(n->u.s.a.node,1);
    ins_f_byte(n->token);
    return tmp1-1;

  case F_NOT:
  case F_COMPL:
  case F_NEGATE:
    tmp1=do_docode(n->u.s.a.node,2);
    ins_f_byte(n->token);
    return tmp1;

  case F_FOR:
  {
    struct vector *tmp_store;
    tmp_store=current_switch_mapping;
    current_switch_mapping=(struct vector *)NULL;
    push_break_stack();
    push_continue_stack();
    tmp1=do_jump(F_BRANCH,-1);
    tmp2=PC;
    if(n->u.s.b.node)
      do_pop(do_docode(n->u.s.b.node->u.s.a.node,0));
    pop_continue_stack(PC);
    if(n->u.s.b.node)
      do_pop(do_docode(n->u.s.b.node->u.s.b.node,0));
    set_branch(tmp1,PC);
    do_jump_when_non_zero(n->u.s.a.node,tmp2);
    pop_break_stack(PC);
    current_switch_mapping=tmp_store;
    return 0;
  }

  case ' ':
    return do_docode(n->u.s.a.node,0)+do_docode(n->u.s.b.node,1);

  case F_FOREACH:
  {
    struct vector *tmp_store;
    tmp_store=current_switch_mapping;
    current_switch_mapping=(struct vector *)NULL;
    tmp2=do_docode(n->u.s.a.node,2);
    ins_f_byte(F_CONST0);
    tmp3=do_jump(F_BRANCH,-1);
    tmp1=PC;
    push_break_stack();
    push_continue_stack();
    do_pop(do_docode(n->u.s.b.node,0));
    pop_continue_stack(PC);
    set_branch(tmp3,PC);
    do_jump(n->token,tmp1);
    pop_break_stack(PC);
    current_switch_mapping=tmp_store;
    return tmp2+1;
  }

  case F_INC_NEQ_LOOP:
  case F_DEC_NEQ_LOOP:
  case F_INC_LOOP:
  case F_DEC_LOOP:
  {
    struct vector *tmp_store;
    tmp_store=current_switch_mapping;
    current_switch_mapping=(struct vector *)NULL;
    tmp2=do_docode(n->u.s.a.node,0);
    tmp3=do_jump(F_BRANCH,-1);
    tmp1=PC;
    push_break_stack();
    push_continue_stack();
    do_pop(do_docode(n->u.s.b.node,0));
    pop_continue_stack(PC);
    set_branch(tmp3,PC);
    do_jump(n->token,tmp1);
    pop_break_stack(PC);
    current_switch_mapping=tmp_store;
    return tmp2;
  }

  case F_DO:
  {
    struct vector *tmp_store;
    tmp_store=current_switch_mapping;
    current_switch_mapping=(struct vector *)NULL;
    tmp2=PC;
    push_break_stack();
    push_continue_stack();
    do_pop(do_docode(n->u.s.a.node,0));
    pop_continue_stack(PC);
    do_jump_when_non_zero(n->u.s.b.node,tmp2);
    pop_break_stack(PC);
    current_switch_mapping=tmp_store;
    return 0;
  }

  case F_BRANCH:
  {
    struct vector *tmp_store;
    tmp_store=current_switch_mapping;
    current_switch_mapping=(struct vector *)NULL;
    tmp2=PC;
    push_break_stack();
    push_continue_stack();
    do_pop(do_docode(n->u.s.a.node,0));
    pop_continue_stack(PC);
    do_jump(F_BRANCH,tmp2);
    pop_break_stack(PC);
    current_switch_mapping=tmp_store;
    return 0;
  }

  case F_CAST:
    tmp1=do_docode(n->u.s.a.node,0);
    if(n->type==TYPE_VOID)
    {
      do_pop(tmp1);
      return 0;
    }
    if(!tmp1) { ins_f_byte(F_CONST0); tmp1=1; }
    switch(n->type)
    {
    case TYPE_STRING:
      ins_f_byte(F_CAST_TO_STRING);
      break;

    case TYPE_NUMBER:
      ins_f_byte(F_CAST_TO_INT);
      break;

    case TYPE_FLOAT:
      ins_f_byte(F_CAST_TO_FLOAT);
      break;

    case TYPE_OBJECT:
      ins_f_byte(F_CAST_TO_OBJECT);
      break;

    case TYPE_FUNCTION:
      ins_f_byte(F_CAST_TO_FUNCTION);
      break;

    case TYPE_REGULAR_EXPRESSION:
      ins_f_byte(F_CAST_TO_REGEXP);
      break;
    }
    return tmp1;

  case F_EFUN_CALL:
    if(n->u.s.a.number==F_CALL_FUNCTION)
    {
      node **a,*b;
      a=first_arg(&(n->u.s.b.node));
      if(a && a[0]->token==F_CONSTANT_FUNCTION)
      {
	ins_f_byte(F_MARK);
	b=a[0];
	a[0]=NULL;
	do_docode(n->u.s.b.node,0);
	a[0]=b;
	ins_f_byte(a[0]->u.s.a.number+F_MAX_OPCODE);
	return 1;
      }
    }

    if(instrs[n->u.s.a.number-F_OFFSET].min_arg!=instrs[n->u.s.a.number-F_OFFSET].max_arg)
    {
      ins_f_byte(F_MARK);
      do_docode(n->u.s.b.node,0);
      ins_f_byte(n->u.s.a.number);
    }else{
      if(n->u.s.b.node && BASIC_TYPE(n->u.s.b.node->type,TYPE_MAPPING))
      {
	tmp1=do_docode(n->u.s.b.node,0);
      }else{
	tmp1=do_docode(n->u.s.b.node,2);
      }

      if(tmp1>256)
        yyerror("To many arguments to function call.");

      if(tmp1<0)
        yyerror("Can't use '@' with fixed argument efun.\n");

      if(tmp1!=instrs[n->u.s.a.number-F_OFFSET].min_arg)
        fatal("GAAH, INTERNAL COMPILER ERROR.\n");
      ins_f_byte(n->u.s.a.number);
    }
    if((n->type & TYPE_MOD_MASK)==TYPE_VOID)
      return 0;
    else
      return 1;

  case '{':
    do_pop(do_docode(n->u.s.a.node,2));
    do_pop(do_docode(n->u.s.b.node,2));
    return 0;

  case F_ARG_LIST:
    return do_docode(n->u.s.a.node,needlval & ~1)+
      do_docode(n->u.s.b.node,needlval);

  case F_SWITCH:
  {
    int e,range;
    struct vector *v;
    struct vector *tmp_store=current_switch_mapping;
    if(do_docode(n->u.s.a.node,0)!=1)
      fatal("Internal compiler error, time to panic\n");        
    current_switch_mapping=allocate_mapping(allocate_array(0),
					    allocate_array(0));
    ins_f_byte(F_SWITCH);
    tmp1=PC;
    ins_short(0,A_PROGRAM);
    push_break_stack();
    do_pop(do_docode(n->u.s.b.node,2));

    v=current_switch_mapping->item[1].u.vec;
    for(range=e=0;e<v->size;e++)
    {
      char *fel,*tmp;
      switch(v->item[e].subtype)
      {
      case 0: if(range) break; continue;
      case 1: if(range++) break; continue;
      case 2: range--; continue;
      default:
	fatal("Internal error in switch mapping.\n");
      }
      switch(v->item[e].subtype==0)
      {
      default: fel="Case inside range"; break;
      case 1: fel="Ranges overlap"; break;
      }
      init_buf();
      my_strcat(fel);
      my_strcat(" at ");
      save_svalue(current_switch_mapping->item[0].u.vec->item+e);
      tmp=simple_free_buf();
      yyerror(tmp);
      free(tmp);
    }
        
    if(!current_switch_mapping->item[2].u.number)
    {
      current_switch_mapping->item[2].u.number=PC;
    }
    pop_break_stack(PC);
    upd_short(tmp1,mem_block[A_SWITCH_MAPPINGS].current_size /
	      sizeof(struct vector *));
    add_to_mem_block(A_SWITCH_MAPPINGS, (char *)&current_switch_mapping,
		     sizeof(struct vector *));
    current_switch_mapping=tmp_store;
    return 0;
  }

  case F_CASE:
  {
    extern struct svalue *sp;
    struct svalue bar;
    if(!current_switch_mapping)
    {
      yyerror("Case outside switch.");
    }else{
      if(!is_const(n->u.s.a.node))
	yyerror("Case label isn't constant.");

      tmp1=eval_low(n->u.s.a.node);
      if(tmp1<1)
      {
	yyerror("Error in case label.");
	return 0;
      }

      if(!n->u.s.b.node)
      {
	if(assoc(sp,current_switch_mapping->item[0].u.vec)>=0)
	{
	  yyerror("Duplicate case");
	}else{
	  bar.type=T_NUMBER;
	  bar.subtype=NUMBER_NUMBER;
	  bar.u.number=PC;
	  insert_alist(sp,&bar,current_switch_mapping);
	}
      }else{
	if(assoc(sp,current_switch_mapping->item[0].u.vec)>=0)
	{
	  yyerror("Duplicate case");
	}else{
	  bar.type=T_NUMBER;
	  bar.subtype=1;
	  bar.u.number=PC;
	  insert_alist(sp,&bar,current_switch_mapping);
	}
	if(!is_const(n->u.s.b.node))
	  yyerror("Case label isn't constant.");

	tmp1+=tmp2=eval_low(n->u.s.b.node);
	if(tmp2<1)
	{
	  pop_n_elems(tmp1);
	  yyerror("Error in case label.");
	  return 0;
	}

	if(sp->type!=sp[-1].type)
	  yyerror("Mixed types in case range");

	if(assoc(sp,current_switch_mapping->item[0].u.vec)>=0)
	{
	  yyerror("Duplicate case");
	}else{
	  bar.subtype=2;
	  insert_alist(sp,&bar,current_switch_mapping);
	}
      }
      pop_n_elems(tmp1);
    }
  }
    return 0;

  case F_DEFAULT:
    if(!current_switch_mapping)
    {
      yyerror("Default outside switch.");
    }else{
      if(current_switch_mapping->item[2].u.number)
      {
	yyerror("Dual Default in switch.");
      }
      current_switch_mapping->item[2].u.number=PC;
    }
    return 0;

  case F_BREAK:
    if(!break_stack)
    {
      yyerror("Break outside loop or switch.");
    }else{
      if(current_break>=break_stack_size)
      {
	break_stack_size*=2;
	break_stack=(int *)realloc((char *)break_stack,
				   sizeof(int)*break_stack_size);
      }
      break_stack[current_break++]=do_jump(F_BRANCH,-1);
    }
    return 0;

  case F_CONTINUE:
    if(!continue_stack)
    {
      yyerror("continue outside loop or switch.");
    }else{
      if(current_continue>=continue_stack_size)
      {
	continue_stack_size*=2;
	continue_stack=(int *)realloc((char *)continue_stack,
				      sizeof(int)*continue_stack_size);
      }
      continue_stack[current_continue++]=do_jump(F_BRANCH,-1);
    }
    return 0;

  case F_RETURN:
    if(n->u.s.a.node->token==F_EFUN_CALL &&
       n->u.s.a.node->u.s.a.number==F_CALL_FUNCTION)
    {
      ins_f_byte(F_MARK);
      do_docode(n->u.s.a.node->u.s.b.node,0);
      ins_f_byte(F_TAILRECURSE);
    }else{
      if(do_docode(n->u.s.a.node,0)!=1)
	ins_f_byte(F_CONST0);
      ins_f_byte(F_RETURN);
    }
    return 0;

  case F_SSCANF:
    tmp1=do_docode(n->u.s.a.node,2);
    tmp2=do_docode(n->u.s.b.node,2);
    ins_f_byte(F_SSCANF);
    ins_byte(tmp2+2,A_PROGRAM);
    return tmp1-1;

  case F_SWAP_VARIABLES:
    tmp1=do_docode(n->u.s.a.node,0);
    if(tmp1!=2)
      fatal("Infernal compiler error (skit oxo)\n");        
    ins_f_byte(F_SWAP_VARIABLES);
    return 0;

  case F_CATCH:
  {
    struct vector *tmp_store;
    tmp_store=current_switch_mapping;
    current_switch_mapping=(struct vector *)NULL;
    tmp1=do_jump(F_CATCH,-1);
    push_break_stack();
    push_continue_stack();
    do_pop(do_docode(n->u.s.a.node,0));
    pop_continue_stack(PC);
    pop_break_stack(PC);
    ins_f_byte(F_DUMB_RETURN);
    set_branch(tmp1,PC);
    current_switch_mapping=tmp_store;
    return 1;
  }

  case F_GAUGE:
  {
    struct vector *tmp_store;
    tmp_store=current_switch_mapping;
    current_switch_mapping=(struct vector *)NULL;
    ins_f_byte(F_PUSH_COST);
    do_pop(do_docode(n->u.s.a.node,2));
    ins_f_byte(F_PUSH_COST);
    ins_f_byte(F_SUBTRACT);
    ins_f_byte(F_NEGATE);
    current_switch_mapping=tmp_store;
    return 1;
  }

  case F_LVALUE_LIST:
    return do_docode(n->u.s.a.node,1)+do_docode(n->u.s.b.node,1);

  case F_INDEX:
    tmp1=do_docode(n->u.s.a.node,needlval | 2);
    if(do_docode(n->u.s.b.node,2)!=1)
      fatal("Internal compiler error, please report this.");

    if(needlval & 1)
    {
      ins_f_byte(F_PUSH_INDEXED_LVALUE);
    }else{
      ins_f_byte(F_INDEX);
      if(!(needlval & 2))
      {
	while(n && n->token==F_INDEX) n=n->u.s.a.node;
	if(n->token==F_CONSTANT)
	  ins_f_byte(F_COPY_VALUE);
      }
    }
    return tmp1;

  case F_CONSTANT:
    tmp1=store_constant(&(n->u.sval));
    if(tmp1>255)
    {
      ins_f_byte(F_CONSTANT);
      ins_short(tmp1,A_PROGRAM);
    }else{
      ins_f_byte(F_SHORT_CONSTANT);
      ins_byte(tmp1,A_PROGRAM);
    }
    if(!(needlval & 2)) /* copy later */
      if(IS_TYPE(n->u.sval,BT_VECTOR))
	ins_f_byte(F_COPY_VALUE);
    return 1;

  case F_LOCAL:
    if(needlval & 1)
    {
      ins_f_byte(F_PUSH_LOCAL_LVALUE);
      ins_byte(n->u.s.a.number,A_PROGRAM);
    }else{
      ins_f_byte(F_LOCAL);
      ins_byte(n->u.s.a.number,A_PROGRAM);
    }
    return 1;

  case F_GLOBAL:
    if(needlval & 1)
    {
      ins_f_byte(F_PUSH_GLOBAL_LVALUE);
      ins_byte(n->u.s.a.number,A_PROGRAM);
    }else{
      ins_f_byte(F_GLOBAL);
      ins_byte(n->u.s.a.number,A_PROGRAM);
    }
    return 1;

  case F_CONSTANT_FUNCTION:
    if(needlval & 1)
    {
      yyerror("Function name is not a lvalue.");
    }
    if(n->u.s.a.number<256)
    {
      ins_f_byte(F_SHORT_CONSTANT_FUNCTION);
      ins_byte(n->u.s.a.number,A_PROGRAM);
    }else{
      ins_f_byte(F_CONSTANT_FUNCTION);
      ins_short(n->u.s.a.number,A_PROGRAM);
    }
    return 1;

  case F_PUSH_SIMUL_EFUN:
    ins_f_byte(F_PUSH_SIMUL_EFUN);
    ins_short(n->u.s.a.number,A_PROGRAM);
    return 1;
    
  default:
    fatal("Infernal compiler error (unknown parse-tree-token).\n");
    return 0;			/* make gcc happy */
  }
}

void setup_fake_program()
{
  fake_prog.next_hash=0;
  fake_prog.program=mem_block[A_PROGRAM].block;
  fake_prog.program_size=mem_block[A_PROGRAM].current_size;
  fake_prog.line_numbers=(char *)mem_block[A_LINENUMBERS].block;
  fake_prog.num_line_numbers=mem_block[A_LINENUMBERS].current_size;
  fake_prog.strings=(char **)mem_block[A_STRINGS].block;
  fake_prog.num_strings=
    mem_block[A_STRINGS].current_size/sizeof(char **);
  fake_prog.constants=(struct svalue *)mem_block[A_CONSTANTS].block;
  fake_prog.num_constants=
    mem_block[A_CONSTANTS].current_size/sizeof(struct svalue);
  fake_prog.functions=(struct function *)mem_block[A_FUNCTIONS].block;
  fake_prog.num_functions=
    mem_block[A_FUNCTIONS].current_size/sizeof(struct function);
  fake_prog.function_ptrs=
    (struct function_p *)mem_block[A_FUNCTION_PTRS].block;
  fake_prog.num_function_ptrs=
    mem_block[A_FUNCTION_PTRS].current_size/sizeof(struct function_p);
  fake_prog.switch_mappings=(struct vector **)
    mem_block[A_SWITCH_MAPPINGS].block;
  fake_prog.num_switch_mappings=
    mem_block[A_SWITCH_MAPPINGS].current_size/sizeof(struct vector **);
  fake_prog.inherit=(struct inherit *)mem_block[A_INHERITS].block;
  fake_prog.num_inherited = mem_block[A_INHERITS].current_size /
    sizeof (struct inherit);
  fake_prog.inherit[0].prog=&fake_prog;
  fake_prog.lfuns=NULL;
  fake_prog.num_lfuns=0;

  fake_object.prog=&fake_prog;
  fake_object.ref=1;
}

static int eval_low(node *n)
{
  extern struct svalue *sp;
  extern int error_recovery_context_exists;

  extern jmp_buf error_recovery_context;

  extern struct object *command_giver,*current_object;
  struct object *save_command_giver = command_giver;
  struct object *save_current_object = current_object;
  extern struct svalue catch_value;
  extern struct control_stack *csp;
  extern struct program *current_prog;
  extern struct svalue *fp;

  int returnval;
  VOLATILE int foo,fo2;
  int e;
  char **bar;
  struct svalue *save_sp,*gazonk;
  int jump=PC;
  save_sp=sp;

  if(num_parse_error)
    return -1;

#ifdef MALLOC_DEBUG
    check_sfltable();
#endif

  store_linenumbers=0;
  do_docode(n,0);
  ins_f_byte(F_DUMB_RETURN);
  store_linenumbers=1;

  if(num_parse_error)
    return -1;

  foo=mem_block[A_STRINGS].current_size/sizeof(char *);
  fo2=mem_block[A_CONSTANTS].current_size/sizeof(struct svalue);
  
  push_control_stack(0);
  push_pop_error_context (1);

  csp->num_local_variables = 0; /* No extra variables */
  fp = sp - csp->num_local_variables + 1;

  setup_fake_program();
  current_prog=&fake_prog;
  current_object=&fake_object;
  /* signal catch OK - print no err msg */
  error_recovery_context_exists = 2;

  if (setjmp(error_recovery_context))
  {
    push_pop_error_context(-1);
    pop_control_stack();
    if(catch_value.type==T_STRING)
    {
      if(strcmp(strptr(&catch_value),"**"))
      {
	yyerror(strptr(&catch_value));
      }
    }else{
      yyerror("Eval error");
    }
    while(sp>save_sp) pop_stack();
    returnval=-1;
  }else{
    eval_instruction(mem_block[A_PROGRAM].block+jump);
    pop_control_stack();
    push_pop_error_context(0);
    returnval=sp-save_sp;
  }
  PC=jump;

  /* free any strings we no longer need */
  bar=(char **)mem_block[A_STRINGS].block;
  e=mem_block[A_STRINGS].current_size/sizeof(char *)-1;
  for(;e>=foo;e--) free_string(bar[e]);
  mem_block[A_STRINGS].current_size=foo*sizeof(char *);

  /* remove constants not needed */
  gazonk=(struct svalue *)mem_block[A_CONSTANTS].block;
  e=mem_block[A_CONSTANTS].current_size/sizeof(struct svalue)-1;
  for(;e>=fo2;e--) free_svalue(gazonk+e);
  mem_block[A_CONSTANTS].current_size=fo2*sizeof(struct svalue);

  command_giver=save_command_giver;
  current_object=save_current_object;
  return returnval;
}

static node *eval(node *n)
{
  node *new;
  int args;
  extern struct svalue *sp;
  if(!is_const(n)) return copy_node(n);
  args=eval_low(n);

#ifdef MALLOC_DEBUG
    check_sfltable();
#endif

  switch(args)
  {
  case -1:
    n=copy_node(n);
    break;

  case 0:
    n=NULL;
    break;

  case 1:
    new=mksvaluenode(sp);
    pop_stack();
    n=optimize(new,0,new);
    break;

  default:
    new=NULL;
    while(args--)
    {
      new=mknode(F_ARG_LIST,mksvaluenode(sp),new);
      pop_stack();
    }
    n=optimize(new,0,new);
  }
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  return n;
}

int last_function_opt_info;

void dooptcode(node *n,int args)
{
  last_function_opt_info=OPT_SIDE_EFFECT;
#ifdef LASDEBUG
  if(lasdebug)
  {
    fprintf(stderr,"Optimizing:\n");
    nice_print_tree(n);
  }
#endif
  if(!num_parse_error)
  {
    n=call_optimizer(n);
#ifdef LASDEBUG
    if(lasdebug>1)
    {
      fprintf(stderr,"Result after optimize:\n");
      nice_print_tree(n);
    }
#endif
    do_depend_optim(&n,args);
    clear_remember();
    n=call_optimizer(n);
#ifdef LASDEBUG
    if(lasdebug)
    {
      fprintf(stderr,"Result after depend_optim:\n");
      nice_print_tree(n);
    }
#endif
  }
  if(!num_parse_error)
  {
    do_pop(docode(n));
    last_function_opt_info=n->opt_info;
  }
  free_node(n);
}

int get_opt_info()
{
  if (last_function_opt_info&(OPT_SIDE_EFFECT | OPT_GLOBAL_ASSIGNMENT))
    return TYPE_MOD_SIDE_EFFECT;
  if(last_function_opt_info & OPT_GLOBAL_DEPEND)
    return 0;
  else
    return TYPE_MOD_CONSTANT;
}

