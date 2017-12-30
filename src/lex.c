#include "efuns.h"
#include <ctype.h>
#include <math.h>

#include "array.h"
#include "lex.h"
#include "instrs.h"
#include "efun_protos.h"
#include "opc_cost.h"
#include "main.h"
#include "stralloc.h"
#include "simulate.h"
#include "dynamic_buffer.h"
#include "simul_efun.h"

#define LEXDEBUG 0

int current_line;
int total_lines;
static int old_line;
char *current_file;

int pragma_strict_types;	/* Force usage of strict types. */
int pragma_save_types;		/* Save argument types after compilation */
int pragma_all_inline;          /* inline all possible inlines */

char **inc_list;
int inc_list_size;

struct lpc_predef_s *lpc_predefs=NULL;

#define EXPANDMAX 50000
static int nexpands;

#define MIN(X,Y) ((X)<(Y)?(X):(Y))
#define isidchar(X) (isalnum(X) || (X)=='_')
static int calc();
static void calc1();

void free_memory_in_lex_at_exit()
{
  int i;
  extern char **local_names;
  extern unsigned short *type_of_locals;
  if(local_names) free((char *)local_names);
  if(type_of_locals) free((char *)type_of_locals);
  if(inc_list)
  {
    for(i=0;i<inc_list_size;i++) free_string(inc_list[i]);
    free((char *)inc_list);
  }
}

/*
 * The number of arguments stated below, are used by the compiler.
 * If min == max, then no information has to be coded about the
 * actual number of arguments. Otherwise, the actual number of arguments
 * will be stored in the byte after the instruction.
 * A maximum value of -1 means unlimited maximum value.
 *
 * If an argument has type 0 (T_INVALID) specified, then no checks will
 * be done at run time.
 *
 * The argument types are currently not checked by the compiler,
 * only by the runtime.
 */
struct keyword predefs[] =
#include "efun_defs.c"

struct keyword reswords[] = {
{ "break",		F_BREAK, },
{ "case",		F_CASE, },
{ "catch",		F_CATCH, },
{ "continue",		F_CONTINUE, },
{ "default",		F_DEFAULT, },
{ "do",			F_DO, },
{ "efun",		F_EFUN, },
{ "else",		F_ELSE, },
{ "float",		F_FLOAT_TOKEN, },
{ "for",		F_FOR, },
{ "foreach",		F_FOREACH, },
{ "function",		F_FUNCTION, },
{ "gauge",		F_GAUGE, },
{ "if",			F_IF, },
{ "inherit",		F_INHERIT, },
{ "inline",		F_INLINE, },
{ "int",		F_INT, },
{ "lambda",		F_LAMBDA, },
{ "list",		F_LIST, },
{ "mapping",		F_MAPPING, },
{ "mixed",		F_MIXED, },
{ "nomask",		F_NO_MASK, },
{ "object",		F_OBJECT, },
{ "private",		F_PRIVATE, },
{ "protected",		F_PROTECTED, },
{ "public",		F_PUBLIC, },
{ "regular_expression",	F_REGULAR_EXPRESSION, },
{ "return",		F_RETURN, },
{ "sscanf",		F_SSCANF, },
{ "swap",		F_SWAP_VARIABLES, },
{ "static",		F_STATIC, },
{ "status",		F_STATUS, },
{ "string",		F_STRING_DECL, },
{ "switch",		F_SWITCH, },
{ "varargs",		F_VARARGS, },
{ "void",		F_VOID, },
{ "while",		F_WHILE, },
{ "call_other",		F_CALL_OTHER, },
};

int number_predefs=NELEM(predefs);
int number_reswords=NELEM(reswords);
int real_number_predefs=NELEM(predefs);

struct instr instrs[F_MAX_INSTR-F_OFFSET];

static void add_instr_name(char *name,int n)
{
  if(n>=F_MAX_INSTR)
  {
    fatal("invalid add_instr_name (%s,%d)\n",name,n);
  }
  instrs[n - F_OFFSET].name = name;
  instrs[n - F_OFFSET].efunc = NULL;
}

static int keword_sort_fun(const void *a,const void *b)
{
  return ((struct keyword *)a)->word - ((struct keyword *)b)->word;
}

void init_num_args()
{
  int i, n;
#ifdef YYOPT
  for(i=0; i<NELEM(reswords); i++)
  {
    reswords[i].word=make_shared_string(reswords[i].word);
  }
#endif

  for(i=0; i<NELEM(instrs);i++)
    instrs[i].eval_cost=AVERAGE_COST;

  for(i=0; i<NELEM(predefs); i++)
  {
#ifdef YYOPT
    predefs[i].word=make_shared_string(predefs[i].word);
#endif
    n = predefs[i].token - F_OFFSET;
    if (n < 0 || n > NELEM(instrs))
      fatal("Token %s has illegal value %d.\n", predefs[i].word,n);
    instrs[n].min_arg = predefs[i].min_args;
    instrs[n].max_arg = predefs[i].max_args;
    instrs[n].name = predefs[i].word;
    instrs[n].type[0] = predefs[i].arg_type1;
    instrs[n].type[1] = predefs[i].arg_type2;
    instrs[n].Default = predefs[i].Default;
    instrs[n].ret_type = predefs[i].ret_type;
    instrs[n].arg_index = predefs[i].arg_index;
    instrs[n].efunc=predefs[i].efunc;
  }
  for(i=0;i<NELEM(func_cost_alist);i+=2)
  {
    n = func_cost_alist[i] - F_OFFSET;
    instrs[n].eval_cost=func_cost_alist[i+1];
  }

  i=0;
  number_predefs--;
  while(i<number_predefs)
  {
    struct keyword tmp;
    while(predefs[i].efunc) i++;
    while(!predefs[number_predefs].efunc)
      number_predefs--;
    if(i<number_predefs)
    {
      tmp=predefs[i];
      predefs[i]=predefs[number_predefs];
      predefs[number_predefs]=tmp;
    }
  }
  number_predefs++;
#ifdef YYOPT
  fsort((char *)reswords,NELEM(reswords),sizeof(reswords[0]),keword_sort_fun);
  fsort((char *)predefs,number_predefs,sizeof(predefs[0]),keword_sort_fun);
#endif
  add_instr_name("add 256 to opcode",F_ADD_256);
  add_instr_name("add 512 to opcode",F_ADD_512);
  add_instr_name("add 768 to opcode",F_ADD_768);
  add_instr_name("add X*256 to opcode",F_ADD_256X);
  add_instr_name("sscanf",F_SSCANF);
  add_instr_name("swap",F_SWAP_VARIABLES);
  add_instr_name("swap on stack",F_SWAP);
  add_instr_name("(function)",F_CAST_TO_FUNCTION);
  add_instr_name("(string)",F_CAST_TO_STRING);
  add_instr_name("<", F_LT);
  add_instr_name(">", F_GT);
  add_instr_name("<=", F_LE);
  add_instr_name(">=", F_GE);
  add_instr_name("==", F_EQ);
  add_instr_name("!", F_NOT);
  add_instr_name("local", F_LOCAL);
  add_instr_name("-", F_SUBTRACT);
  add_instr_name("pop", F_POP_VALUE);
  add_instr_name("const-1", F_CONST_1);
  add_instr_name("+", F_ADD);
  add_instr_name("-", F_SUBTRACT);
  add_instr_name("*", F_MULTIPLY);
  add_instr_name("/", F_DIVIDE);
  add_instr_name("%", F_MOD);
  add_instr_name("!=", F_NE);
  add_instr_name("++x", F_INC);
  add_instr_name("x++", F_POST_INC);
  add_instr_name("x++ and pop", F_INC_AND_POP);
  add_instr_name("--x", F_DEC);
  add_instr_name("x--", F_POST_DEC);
  add_instr_name("x-- and pop", F_DEC_AND_POP);
  add_instr_name("branch when zero",F_BRANCH_WHEN_ZERO);
  add_instr_name("branch non zero",F_BRANCH_WHEN_NON_ZERO);
  add_instr_name("constant function",F_CONSTANT_FUNCTION);
  add_instr_name("push simulated efun",F_PUSH_SIMUL_EFUN);
  add_instr_name("(int)",F_CAST_TO_INT);
  add_instr_name("(float)",F_CAST_TO_FLOAT);
  add_instr_name("(object)",F_CAST_TO_OBJECT);
  add_instr_name("&=",F_AND_EQ);
  add_instr_name("|=",F_OR_EQ);
  add_instr_name("^=",F_XOR_EQ);
  add_instr_name("<<=",F_LSH_EQ);
  add_instr_name(">>=",F_RSH_EQ);
  add_instr_name("<<",F_LSH);
  add_instr_name(">>",F_RSH);
  add_instr_name("+=",F_ADD_EQ);
  add_instr_name("-=",F_SUB_EQ);
  add_instr_name("*=",F_MULT_EQ);
  add_instr_name("%=",F_MOD_EQ);
  add_instr_name("/=",F_DIV_EQ);
  add_instr_name("&&",F_LAND);
  add_instr_name("||",F_LOR);
  add_instr_name("case",F_CASE);
  add_instr_name("default",F_DEFAULT);
  add_instr_name("while",F_WHILE);
  add_instr_name("lvalue_list",F_LVALUE_LIST);
  add_instr_name("push_ltosval",F_PUSH_LTOSVAL);
  add_instr_name("do-while",F_DO);
  add_instr_name("++Loop",F_INC_LOOP);
  add_instr_name("--Loop",F_DEC_LOOP);
  add_instr_name("dumb return",F_DUMB_RETURN);
  add_instr_name("break",F_BREAK);
  add_instr_name("continue",F_CONTINUE);
  add_instr_name("for",F_FOR);
}

void free_reswords()
{
#ifdef YYOPT
  int i;
  for(i=0; i<NELEM(reswords); i++)
    free_string(reswords[i].word);
  for(i=0; i<NELEM(predefs); i++)
    free_string(predefs[i].word);
#endif
}

char *get_f_name(int n)
{
  if (n<F_MAX_INSTR &&instrs[n-F_OFFSET].name)
  {
    return instrs[n-F_OFFSET].name;
  }else {
    static char buf[30];
    sprintf(buf, "<OTHER %d>", n);
    return buf;
  }
}

char *get_instruction_name(int n)
{
  static char buf[40];
  if(n<F_MAX_OPCODE)
  {
    if(n>0)
    {
      if (instrs[n-F_OFFSET].name)
      {
	return instrs[n-F_OFFSET].name;
      }else {
	sprintf(buf, "<OTHER %d>", n);
      }
    }else{
      sprintf(buf, "<ILLIGAL %d>", n);
    }
  }else{
    sprintf(buf, "<FUNCTION %d>", n);
  }
  return buf;
}

/* foo must be a shared string */
static int lookupword(char *foo, struct keyword *words,int h)
{
  int i,l;
  if(!foo) return -1;
  for(l=0;;)
  {
    i=(l+h)/2;
    if(foo==words[i].word) return words[i].token;
    else if(l==i) return -1;
    else if(foo>words[i].word) l=i;
    else h=i;
  }
}

int lookup_predef(char *s)
{
  s=findstring(s);
  if(!s) return -1;
  return lookupword(s, predefs, number_predefs);
}

/*** input routines ***/
struct inputstate
{
  struct inputstate *next;
  FILE *f;
  char *data;
  int buflen;
  int pos;

  int (*my_getc)();
  int (*gobble)(int);
  int (*look)();
  void (*my_ungetc)(int);
  void (*ungetstr)(char *,int);
};

static struct inputstate *istate=NULL;

static void link_inputstate(struct inputstate *i)
{
  i->next=istate;
  istate=i;
}

static void free_inputstate(struct inputstate *i)
{
  if(!i) return;
  if(i->f) fclose(i->f);
  if(i->data) free(i->data);
  free_inputstate(i->next);
  free((char *)i);
}

static struct inputstate *new_inputstate();
static struct inputstate *memory_inputstate(int size);

static int default_gobble(int c)
{
  if(istate->look()==c)
  {
    istate->my_getc();
    return 1;
  }else{
    return 0;
  }
}

static void default_ungetstr(char *s,int len)
{
  link_inputstate(memory_inputstate(len+1000));
  istate->ungetstr(s,len);
}

static void default_ungetc(int s)
{
  char c=s;
  istate->ungetstr(&c,1);
}

static struct inputstate *new_inputstate()
{
  struct inputstate *i;
  i=(struct inputstate *)xalloc(sizeof(struct inputstate));
  i->f=NULL;
  i->data=NULL;
  i->next=NULL;
  i->gobble=default_gobble;
  i->ungetstr=default_ungetstr;
  i->my_ungetc=default_ungetc;
  return i;
}

/*** EOF input ***/
static int end_getc() { return EOF; }
static int end_gobble(int c) { return c==EOF; }
static void end_ungetc(int c)
{
  if(c==EOF) return;
  default_ungetc(c);
}

static struct inputstate *end_inputstate()
{
  struct inputstate *i;
  i=new_inputstate();
  i->gobble=end_gobble;
  i->look=end_getc;
  i->my_getc=end_getc;
  i->my_ungetc=end_ungetc;
  return i;
}

/*** MEMORY input ***/
static void memory_ungetstr(char *s,int len)
{
  int tmp;
  tmp=MIN(len,istate->pos);
  if(tmp)
  {
    MEMCPY(istate->data + istate->pos - tmp, s+len-tmp , tmp);
    istate->pos-=tmp;
    len-=tmp;
  }
  if(len) default_ungetstr(s,len);
}

static int memory_getc()
{
  if(istate->pos<istate->buflen)
  {
#if LEXDEBUG>9
    fprintf(stderr,"lex: reading from memory '%c' (%d).\n",istate->data[istate->pos],istate->data[istate->pos]);
#endif
    return istate->data[(istate->pos)++];
  }else{
    struct inputstate *i;
    i=istate;
    istate=i->next;
    free((char *)i->data);
    free((char *)i);
    return istate->my_getc();
  }
}

static int memory_look()
{
  if(istate->pos<istate->buflen)
  {
    return istate->data[istate->pos];
  }else{
    struct inputstate *i;
    i=istate;
    istate=i->next;
    free((char *)i->data);
    free((char *)i);
    return istate->look();
  }
}

/* allocate an empty memory state */
static struct inputstate *memory_inputstate(int size)
{
  struct inputstate *i;
  if(!size) size=10000;
  i=new_inputstate();
  i->data=xalloc(size);
  i->buflen=size;
  i->pos=size;
  i->ungetstr=memory_ungetstr;
  i->my_getc=memory_getc;
  i->look=memory_look;
  return i;
}

/*** FILE input ***/

static int file_getc()
{
  int c;
  c=getc(istate->f);
#if LEXDEBUG>9
  fprintf(stderr,"lex: reading '%c' (%d)\n",c,c);
#endif
  if(c!=EOF)
  {
    return c;
  }else{
    struct inputstate *i;
    i=istate->next;
    fclose(istate->f);
    free((char *)istate);
    istate=i;
    return istate->my_getc();
  }
}


static int file_look()
{
  int c;
  c=getc(istate->f);
  if(c!=EOF)
  {
    ungetc(c,istate->f);
    return c;
  }else{
    struct inputstate *i;
    i=istate->next;
    fclose(istate->f);
    free((char *)istate);
    istate=i;
    return istate->look();
  }
}

static void file_ungetc(int c)
{
  ungetc(c,istate->f);
}

static struct inputstate *file_inputstate(FILE *f)
{
  struct inputstate *i;
  i=new_inputstate();
  i->f=f;
  i->look=file_look;
  i->my_getc=file_getc;
  i->my_ungetc=file_ungetc;
  return i;
}

static int GETC()
{
  int c;
  c=istate->my_getc();
  if(c=='\n') current_line++;
  return c;
}

static void UNGETC(int c)
{
  if(c=='\n') current_line--;
  istate->my_ungetc(c);
}

static void UNGETSTR(char *s,int len)
{
  int e;
  for(e=0;e<len;e++) if(s[e]=='\n') current_line--;
  istate->ungetstr(s,len);
}

static int GOBBLE(char c)
{
  if(istate->gobble(c))
  {
    if(c=='\n') current_line++;
    return 1;
  }else{
    return 0;
  }
}

#define LOOK() (istate->look())
#define SKIPWHITE() { int c; while(isspace(c=GETC())); UNGETC(c); }
#define SKIPTO(X) { int c; while((c=GETC())!=(X) && (c!=EOF)); }
#define SKIPUPTO(X) { int c; while((c=GETC())!=(X) && (c!=EOF)); UNGETC(c); }
#define READBUF(X) { \
  register int p,C; \
  for(p=0;(C=GETC())!=EOF && p<sizeof(buf) && (X);p++) \
  buf[p]=C; \
  if(p==sizeof(buf)) { \
    yyerror("Internal buffer overflow.\n"); p--; \
  } \
  UNGETC(C); \
  buf[p]=0; \
}

static char buf[1024];

/*** Define handling ***/

struct define
{
  struct define *next;
  char *name;
  int args;
  struct vector *parts;
};

typedef struct define *definep;
definep define_hash[DEFINE_HASHSIZE];

/* argument must be shared string */
static void undefine(char *name)
{
  int h;
  definep d,*prev;

  h=(((int)name) & 0x7fffffff) % DEFINE_HASHSIZE;

  for(prev=define_hash+h;(d=*prev);prev=&(d->next))
  {
    if(d->name==name)
    {
      *prev=d->next;
      free_string(d->name);
      if(d->parts) free_vector(d->parts);
      free((char *)d);
    }
  }
}

/* argument must be shared string */
static definep find_define(char *name)
{
  int h;
  definep d,*prev;

  h=(((int)name) & 0x7fffffff) % DEFINE_HASHSIZE;

  for(prev=define_hash+h;(d=*prev);prev=&(d->next))
  {
    if(d->name==name)
    {
      *prev=d->next;
      d->next=define_hash[h];
      define_hash[h]=d;
      return d;
    }
  }
  return NULL;
}

/* name and as are supposed to be SHARED strings */
static void add_define(char *name,int args,int parts_on_stack)
{
  int h;
  definep d;

  f_aggregate(parts_on_stack,sp-parts_on_stack+1);
  if(sp->type!=T_POINTER)
  {
    yyerror("Define failed for unknown reason.\n");
    free_string(name);
    pop_stack();
    return;
  }
  if(find_define(name))
  {
    char *a;
    a=(char *)alloca(strlen(name)+40);
    sprintf(a,"Redefining '%s'",name);
    yyerror(a);
    free_string(name);
    pop_stack();
    return;
  }
  undefine(name);
  d=(definep)xalloc(sizeof(struct define));
  d->name=name;
  d->args=args;
  d->parts=sp->u.vec;
  SET_TO_ZERO(*sp);
  pop_stack();

  h=(((int)name) & 0x7fffffff) % DEFINE_HASHSIZE;
  d->next=define_hash[h];
  define_hash[h]=d;
}

static void simple_add_define(char *name,char *as)
{
  push_new_shared_string(as);
  add_define(make_shared_string(name),0,1);
}

static void free_all_defines()
{
  int e;
  definep d,next;
  for(e=0;e<DEFINE_HASHSIZE;e++)
  {
    for(d=define_hash[e];d;d=next)
    {
      next=d->next;
      free_string(d->name);
      if(d->parts) free_vector(d->parts);
      free((char *)d);
    }
    define_hash[e]=NULL;
  }
}

static void do_define()
{
  int c,e,t,argc;
  char *s,*s2;
  struct svalue *save_sp=sp;

  SKIPWHITE();
  READBUF(isidchar(C));

  s=make_shared_string(buf);
  
  if(GOBBLE('('))
  {
    argc=0;

    SKIPWHITE();
    READBUF(isidchar(C));
    if(buf[0])
    {
      push_new_shared_string(buf);
      argc++;
      SKIPWHITE();
      while(GOBBLE(','))
      {
        SKIPWHITE();
        READBUF(isidchar(C));
        push_new_shared_string(buf);
        argc++;
      }
    }
    SKIPWHITE();

    if(!GOBBLE(')'))
      yyerror("Missing ')'");
  }else{
    argc=0;
  }

  init_buf();
  t=0;
  push_svalue(&const_empty_string);
  while(1)
  {
    c=GETC();
    if(c=='\\') if(GOBBLE('\n')) continue;
    if( (t!=!!isidchar(c) && argc>0) || c=='\n' || c==EOF)
    {
      s2=free_buf();
      for(e=0;e<argc;e++)
      {
	if(strptr(save_sp+e+1)==s2)
	{
	  free_string(s2);
	  push_number(e);
	  break;
	}
      }
      if(e==argc)
      {
	push_shared_string(s2);
	if(sp[-1].type==T_STRING) f_add();
      }
      if(c=='\n' || c==EOF)
      {
	push_new_shared_string(" ");
	if(sp[-1].type==T_STRING) f_add();
	break;
      }
      t=!!isidchar(c);
      init_buf();
    }
    my_putchar(c);
  }
  UNGETC(c);
  add_define(s,argc,sp-save_sp-argc);
  while(sp>save_sp) pop_stack();
}

/* s is a shared string */
static int expand_define(char *s)
{
  struct svalue *save_sp=sp;
  definep d;
  int len,e,tmp,args;
  d=find_define(s);
  if(!d) return 0;

  if(nexpands>EXPANDMAX)
  {
    yyerror("Macro limit exceeded.");
    return 0;
  }
  SKIPWHITE();
  if(GOBBLE('('))
  {
    int parlvl,quote;
    int c;
    args=0;

    SKIPWHITE();
    init_buf();
    parlvl=1;
    quote=0;
    while(parlvl)
    {
      switch(c=GETC())
      {
      case EOF:
	yyerror("Unexpected end of file.");
	while(sp>save_sp) pop_stack();
	return 0;
      case '"': if(!(quote&2)) quote^=1; break;
      case '\'': if(!(quote&1)) quote^=2; break;
      case '(': if(!quote) parlvl++; break;
      case ')': if(!quote) parlvl--; break;
      case '\\': my_putchar(c); c=GETC(); break;
      case ',':
	if(!quote && parlvl==1)
	{
	  push_shared_string(free_buf());
	  init_buf();
	  args++;
	  continue;
	}
      }
      if(parlvl) my_putchar(c);
    }
    push_shared_string(free_buf());
    if(args==0 && !d->args && !my_strlen(sp))
    {
      pop_stack();
    }else{
      args++;
    }
  }else{
    args=0;
  }
  
  if(args>d->args)
  {
    char *fel=(char *)alloca(strlen(s)+80);
    sprintf(fel,"Too many arguments to marco '%s'.",s);
    yyerror(fel);
    while(sp>save_sp) pop_stack();
    return 0;
  }

  if(args<d->args)
  {
    char *fel=(char *)alloca(strlen(s)+80);
    sprintf(fel,"Too few arguments to marco '%s'.",s);
    yyerror(fel);
    while(sp>save_sp) pop_stack();
    return 0;
  }
  len=0;
  for(e=d->parts->size-1;e>=0;e--)
  {
    switch(d->parts->item[e].type)
    {
    case T_STRING:
      tmp=my_strlen(d->parts->item+e);
      UNGETSTR(strptr(d->parts->item+e),tmp);
      break;

    case T_NUMBER:
      tmp=my_strlen(save_sp+d->parts->item[e].u.number+1);
      UNGETSTR(strptr(save_sp+d->parts->item[e].u.number+1),tmp);
      break;

    default: tmp=0;
    }
    len+=tmp;
  }
  while(sp>save_sp) pop_stack();
  nexpands+=len;
  return 1;
}

/*** Handle include ****/

static void handle_include(char *name)
{
  FILE *f;
  char buf[400],*p;
  int i;

  if(current_file[0]!='/' && !batch_mode)
  {
    buf[0]='/';
    strncpy(buf+1, current_file,sizeof(buf)-1);
    buf[sizeof(buf)-1]=0;
  }else{
    strncpy(buf, current_file,sizeof(buf));
    buf[sizeof(buf)-1]=0;
  }
  
  if ((p = STRRCHR(buf, '/')))
  {
    *p = 0;
  }else{
    buf[0]='/'; buf[1]=0;
  }
  
  p=combine_path(buf,name);

  /* DON'T LOOK!!!!! /Profezzorn  */
  if((f=fopen(p+!batch_mode,"r"))==NULL)
  {
    free(p);

    for (p=STRCHR(name, '.'); p; p = STRCHR(p+1, '.'))
    {
      if (p[1] == '.')
      {
	yyerror("Illigal to have '..' in includes from standard includepath.");
	return;
      }
    }

    for (i=0; i < inc_list_size; i++)
    {
      sprintf(buf,inc_list[i],name);
      if((f = fopen(buf, "r")))
      {
	p=string_copy(buf);
	break;
      }
    }
    if(!p)
    {
      yyerror("Couldn't find file to include.");
      return;
    }
  }

  UNGETSTR("\"",2);
  UNGETSTR(current_file,strlen(current_file));
  sprintf(buf,"\n# %d \"",current_line+1);
  UNGETSTR(buf,strlen(buf));
  total_lines+=current_line-old_line;
  old_line=current_line=1;
  free(current_file);
  current_file=p;
  link_inputstate(file_inputstate(f));
  UNGETSTR("\n",1);
}

/*** Lexical analyzing ***/

static int char_const()
{
  int c;
  switch(c=GETC())
  {
  case '0': case '1': case '2': case '3':
  case '4': case '5': case '6': case '7':
      c-='0';
      if(LOOK()<'0' || LOOK()>'8') return c;
      c=c*8+(GETC()-'0');
      if(LOOK()<'0' || LOOK()>'8') return c;
      c=c*8+(GETC()-'0');
      return c;

    case 'r': return '\r';
    case 'n': return '\n';
    case 't': return '\t';
    case 'b': return '\b';
  }
  return c;
}

static void do_skip()
{
  char *s;
  int lvl,c;
#if LEXDEBUG>3
  fprintf(stderr,"Skipping from %d to ",current_line);
#endif
  lvl=1;
  while(lvl)
  {
    switch(c=GETC())
    {
    case '/':
      if(GOBBLE('*'))
      {
	do{
	  SKIPTO('*');
	}while(!GOBBLE('/'));
      }
      continue;

    case EOF:
      yyerror("Unexpected end of file while skipping.");
      return;

    case '\\':
      GETC();
      continue;
	
    case '\n':
      if(GOBBLE('#'))
      {
	SKIPWHITE();
	READBUF(C!=' ' && C!='\t' && C!='\n');
    
	switch(buf[0])
	{
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	  continue;

	case 'i':
	  if(!strcmp("include",buf)) continue;
	  if(!strcmp("if",buf) || !strcmp("ifdef",buf) || !strcmp("ifndef",buf))
	  {
	    lvl++;
	    continue;
	  }
	  break;

	case 'e':
	  if(!strcmp("endif",buf)) { lvl--; continue; }
	  if(!strcmp("else",buf))
	  {
	    if(lvl==1) lvl=0;
	    continue;
	  }
	  if(!strcmp("elif",buf) || !strcmp("elseif",buf))
	  {
	    if(lvl==1 && calc()) lvl--;
	    continue;
	  }
	  break;

	case 'l':
	  if(!strcmp("line",buf)) continue;
	  break;

	case 'd':
	  if(!strcmp("define",buf)) continue;
	  break;

	case 'u':
	  if(!strcmp("undef",buf)) continue;
	  break;

	case 'p':
	  if(!strcmp("pragma",buf)) continue;
	  break;
	}
    
	s=(char *)alloca(strlen(buf)+80);
	sprintf(s,"Unknown directive #%s.",buf);
	yyerror(s);
	SKIPUPTO('\n');
	continue;
      }
    }
  }
#if LEXDEBUG>3
  fprintf(stderr,"%d in %s.\n",current_line,current_file);
#endif
}

static int do_lex(int literal)
#if LEXDEBUG>4
{
  int t;
  int do_lex2(int literal);
  t=do_lex2(literal);
  if(t<256)
  {
    fprintf(stderr,"do_lex(%d) -> '%c' (%d)\n",literal,t,t);
  }else{
    fprintf(stderr,"do_lex(%d) -> %s (%d)\n",literal,get_f_name(t),t);
  }
  return t;
}

static int do_lex2(int literal)
#endif
{
  int c;
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  while(1)
  {
    switch(c=GETC())
    {
    case '\n':
      if(literal)
      {
	UNGETC('\n');
	return '\n';
      }
      if(GOBBLE('#'))
      {
	if(GOBBLE('!'))
	{
	  SKIPUPTO('\n');
	  continue;
	}
            
	SKIPWHITE();
	READBUF(C!=' ' && C!='\t' && C!='\n');

	switch(buf[0])
	{
	  char *p;

	case 'l':
	  if(strcmp("line",buf)) goto badhash;
	  READBUF(C!=' ' && C!='\t' && C!='\n');

	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	  total_lines+=current_line-old_line;
	  old_line=current_line=atoi(buf)-1;
	  SKIPWHITE();
	  READBUF(C!='\n');

	  p=buf;
	  if(*p=='"' && STRCHR(p+1,'"'))
	  {
	    char *p2;
	    p++;
	    if((p2=STRCHR(p+1,'"')))
	    {
	      *p2=0;
	      p=combine_path("/",p);
	      free(current_file);
	      current_file=p;
	    }
	  }
	  break;

	case 'i':
	  if(!strcmp("include",buf))
	  {
	    SKIPWHITE();
	    c=do_lex(1);
	    if(c=='<')
	    {
	      READBUF(C!='>' && C!='\n');
	      if(!GOBBLE('>'))
	      {
		yyerror("Missing `>`");
		SKIPUPTO('\n');
		continue;
	      }
	    }else{
	      if(c!=F_STRING)
	      {
		yyerror("Couldn't find include filename.\n");
		SKIPUPTO('\n');
		continue;
	      }
	    }
	    handle_include(buf);
	    break;
	  }

	  if(!strcmp("if",buf))
	  {
	    if(!calc()) do_skip();
	    break;
	  }

	  if(!strcmp("ifdef",buf))
	  {
	    SKIPWHITE();
	    READBUF(isidchar(C));
	    p=findstring(buf);
	    if(!p || !find_define(p)) do_skip();
	    break;
	  }

	  if(!strcmp("ifndef",buf))
	  {
	    SKIPWHITE();
	    READBUF(isidchar(C));
	    p=findstring(buf);
	    if(p && find_define(p)) do_skip();
	    break;
	  }
	  goto badhash;

	case 'e':
	  if(!strcmp("endif",buf)) break;
	  if(!strcmp("else",buf))
	  {
	    SKIPUPTO('\n');
	    do_skip();
	    break;
	  }
	  if(!strcmp("elif",buf) || !strcmp("elseif",buf))
	  {
	    SKIPUPTO('\n');
	    do_skip();
	    break;
	  }
	  goto badhash;

	case 'u':
	  if(!strcmp("undef",buf))
	  {
	    SKIPWHITE();
	    READBUF(isidchar(C));
	    if((p=findstring(buf))) undefine(p);
	    break;
	  }
	  goto badhash;

	case 'd':
	  if(!strcmp("define",buf))
	  {
	    do_define();
	    break;
	  }
	  goto badhash;

	case 'p':
	  if(!strcmp("pragma",buf))
	  {
	    SKIPWHITE();
	    READBUF(C!='\n');
	    if (strcmp(buf, "strict_types") == 0)
	    {
	      pragma_strict_types = 1;
	    }else if (strcmp(buf, "unpragma_strict_types") == 0)
	    {
	      pragma_strict_types = 0;
	    } else if (strcmp(buf, "save_types") == 0)
	    {
	      pragma_save_types = 1;
	    } else if (strcmp(buf, "all_inline") == 0)
	    {
	      pragma_all_inline = 1;
	    }
	    break;
	  }

	badhash:
	  p=(char *)alloca(strlen(buf)+80);
	  sprintf(p,"Unknown directive #%s.",buf);
	  yyerror(p);
	  SKIPUPTO('\n');
	  continue;
	  
	}
      }
      continue;

    case ' ':
    case '\t':
      continue;

    case EOF:
      return 0;
  
    case '\'':
      c=GETC();
      if(c=='\\') c=char_const();
      if(GETC()!='\'')
	yyerror("Unterminated character constant.\n");
      yylval.number=c;
      return F_NUMBER;
	
    case '"':
      init_buf();
      while(1)
      {
	c=GETC();
	if(c==-1)
	{
	  yyerror("End of file in string.\n");
	  free(simple_free_buf());
	  return 0;
	}
	if(c=='\n')
	{
	  yyerror("Newline in string.\n");
	  free(simple_free_buf());
	  return 0;
	}
	if(c=='\\')
	{
	  c=char_const();
	}else{
	  if(c=='"') break;
	}
	my_putchar(c);
      }
      if(literal)
      {
	strncpy(buf,return_buf(),sizeof(buf));
	buf[sizeof(buf)-1]=0;
	yylval.string=buf;
      }else{
	yylval.string=free_buf();
      }
      return F_STRING;
  
    case ':':
      if(GOBBLE(':')) return F_COLON_COLON;
      return c;

    case '.':
      if(GOBBLE('.'))
      {
	if(GOBBLE('.')) return F_DOT_DOT_DOT;
	return F_DOT_DOT;
      }
      return c;
  
    case '0':
      if(GOBBLE('x'))
      {
	READBUF(isxdigit(C));
	yylval.number=STRTOL(buf,NULL,16);
	return F_NUMBER;
      }
  
    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    {
      char *p;
      UNGETC(c);
      READBUF(isdigit(C) || C=='.');

      p=STRCHR(buf,'.');
      
      if(p)
      {
	if(p[1]=='.')
	{
	  UNGETSTR(p,strlen(p));
	  *p=0;
	  p=NULL;
	}else if((p=STRCHR(p+1,'.')))
	{
	  UNGETSTR(p,strlen(p));
	  *p=0;
	}
	if((p=STRCHR(buf,'.')))
	{
	  yylval.fnum=STRTOD(buf,NULL);
	  return F_FLOAT;
	}
      }
      yylval.number=STRTOL(buf,NULL,10);
      return F_NUMBER;
    }
  
    case '-':
      if(GOBBLE('=')) return F_SUB_EQ;
      if(GOBBLE('>')) return F_ARROW;
      if(GOBBLE('-')) return F_DEC;
      return '-';
  
    case '+':
      if(GOBBLE('=')) return F_ADD_EQ;
      if(GOBBLE('+')) return F_INC;
      return '+';
  
    case '&':
      if(GOBBLE('=')) return F_AND_EQ;
      if(GOBBLE('&')) return F_LAND;
      return '&';
  
    case '|':
      if(GOBBLE('=')) return F_OR_EQ;
      if(GOBBLE('|')) return F_LOR;
      return '|';

    case '^':
      if(GOBBLE('=')) return F_XOR_EQ;
      return '^';
  
    case '*':
      if(GOBBLE('=')) return F_MULT_EQ;
      return '*';

    case '%':
      if(GOBBLE('=')) return F_MOD_EQ;
      return '%';
  
    case '/':
      if(GOBBLE('*'))
      {
	do{
	  SKIPTO('*');
	}while(!GOBBLE('/'));
	continue;
      }else if(GOBBLE('/'))
      {
	SKIPUPTO('\n');
	continue;
      }
      if(GOBBLE('=')) return F_DIV_EQ;
      return '/';
  
    case '=':
      if(GOBBLE('=')) return F_EQ;
      return '=';
  
    case '<':
      if(GOBBLE('<'))
      {
	if(GOBBLE('=')) return F_LSH_EQ;
	return F_LSH;
      }
      if(GOBBLE('=')) return F_LE;
      return '<';
  
    case '>':
      if(GOBBLE(')')) return F_LIST_END;
      if(GOBBLE('=')) return F_GE;
      if(GOBBLE('>'))
      {
	if(GOBBLE('=')) return F_RSH_EQ;
	return F_RSH;
      }
      return '>';

    case '!':
      if(GOBBLE('=')) return F_NE;
      return F_NOT;

    case '(':
    case '?':
    case ',':
    case '~':
    case '@':
    case ')':
    case '[':
    case ']':
    case '{':
    case ';':
    case '}': return c;
  
    default:
      if(isidchar(c))
      {
	UNGETC(c);
	READBUF(isidchar(C));

	if(!literal)
	{
	  char *s;
	  /* identify indetifier, if it is not a shared string,
	   * then it is neither a define, reserved word or efun
	   */

	  if((s=findstring(buf)))
	  {
	    int i;
	    if(expand_define(s)) continue;

	    i=lookupword(s, reswords, NELEM(reswords));
	    if(i>=0) return i;

	    if(islocal(s)>=0)
	    {
	      yylval.string = copy_shared_string(s);
	      return F_IDENTIFIER;
	    }

	    i=defined_function(s);
	    if(i>=0)
	    {
	      yylval.number=i;
	      return F_FUNCTION_NAME;
	    }

	    i=lookupword(s, predefs, number_predefs);

	    if(i>=0)
	    {
	      if(find_simul_efun(s)<0)
	      {
		yylval.number=i;
		return F_EFUN_NAME;
	      }
	    }
	    s=copy_shared_string(s);
	  }else{
	    s=make_shared_string(buf);
	  }
	  yylval.string=s;
	  return F_IDENTIFIER;
	}
  
	yylval.string=buf;
	return F_IDENTIFIER;
      }else{
	char buff[100];
	sprintf(buff, "Illegal character (hex %02x) '%c'", c, c);
	yyerror(buff);
	return ' ';
      }
    }
  }
}

int yylex()
{
  return do_lex(0);
}

static int lookahead;
static void low_lex()
{
  while(1)
  {
   lookahead=do_lex(1);
    if(lookahead==F_IDENTIFIER)
    {
      if(!strcmp("defined",yylval.string))
      {
	char *s;

	SKIPWHITE();
	if(!GOBBLE('('))
	{
	  yyerror("Missing '(' in defined.\n");
	  return;
	}
	READBUF(isidchar(C));
	if(!GOBBLE(')'))
	{
	  yyerror("Missing ')' in defined.\n");
	  return;
	}
	s=findstring(buf);
	if(s && find_define(s))
	{
	  UNGETSTR(" 1 ",3);
	}else{
	  UNGETSTR(" 0 ",3);
	}
	continue;
      }
      if(!strcmp("efun",yylval.string))
      {
	char *s;

	SKIPWHITE();
	if(!GOBBLE('('))
	{
	  yyerror("Missing '(' in #if efun().\n");
	  return;
	}
	READBUF(isidchar(C));
	if(!GOBBLE(')'))
	{
	  yyerror("Missing ')' in #if efun().\n");
	  return;
	}
	s=findstring(buf);
	if(s && lookupword(s, predefs, number_predefs)>=0)
	{
	  UNGETSTR(" 1 ",3);
	}else{
	  UNGETSTR(" 0 ",3);
	}
	continue;
      }
      if(!expand_define(yylval.string))
      {
	UNGETSTR(" 0 ",3);
	continue;
      }
    }
    break;
  }
}

static void calcC()
{
  switch(lookahead)
  {
    case '(':
      low_lex();
      calc1();
      if(lookahead!=')')
        error("Missing ')'\n");
      break;

    case F_FLOAT:
      push_float(yylval.fnum);
      break;

    case F_STRING:
      push_new_shared_string(yylval.string);
      break;

    case F_NUMBER:
      push_number(yylval.number);
      break;

    default:
      yyerror("Syntax error in #if.");
      return;
  }

  low_lex();

  while(lookahead=='[')
  {
    low_lex();
    calc1();
    f_index();
    if(lookahead!=']')
      error("Missing ']'\n");
    low_lex();
  }
}

static void calcB()
{
  switch(lookahead)
  {
    case F_NOT: low_lex(); calcB(); f_not(); break;
    case '~': low_lex(); calcB(); f_compl(); break;
    default: calcC();
  }
}

static void calcA()
{
  calcB();
  while(1)
  {
    switch(lookahead)
    {
      case '/': low_lex(); calcB(); f_divide(); continue;
      case '*': low_lex(); calcB(); f_multiply(); continue;
      case '%': low_lex(); calcB(); f_mod(); continue;
    }
    break;
  }
}

static void calc9()
{
  calcA();

  while(1)
  {
    switch(lookahead)
    {
      case '+': low_lex(); calcA(); f_add(); continue;
      case '-': low_lex(); calcA(); f_subtract(); continue;
    }
    break;
  }
}

static void calc8()
{
  calc9();

  while(1)
  {
    switch(lookahead)
    {
      case F_LSH: low_lex(); calc9(); f_lsh(); continue;
      case F_RSH: low_lex(); calc9(); f_rsh(); continue;
    }
    break;
  }
}

static void calc7b()
{
  calc8();

  while(1)
  {
    switch(lookahead)
    {
      case '<': low_lex(); calc8(); f_lt(); continue;
      case '>': low_lex(); calc8(); f_gt(); continue;
      case F_GE: low_lex(); calc8(); f_ge(); continue;
      case F_LE: low_lex(); calc8(); f_le(); continue;
    }
    break;
  }
}

static void calc7()
{
  calc7b();

  while(1)
  {
    switch(lookahead)
    {
      case F_EQ: low_lex(); calc7b(); f_eq(); continue;
      case F_NE: low_lex(); calc7b(); f_ne(); continue;
    }
    break;
  }
}

static void calc6()
{
  calc7();

  while(lookahead=='&')
  {
    low_lex();
    calc7();
    f_and();
  }
}

static void calc5()
{
  calc6();

  while(lookahead=='^')
  {
    low_lex();
    calc6();
    f_and();
  }
}

static void calc4()
{
  calc5();

  while(lookahead=='|')
  {
    low_lex();
    calc5();
    f_or();
  }
}

static void calc3()
{
  calc4();

  while(lookahead==F_LAND)
  {
    low_lex();
    if(IS_ZERO(sp))
    {
      calc4();
      pop_stack();
    }else{
      pop_stack();
      calc4();
    }
  }
}

static void calc2()
{
  calc3();

  while(lookahead==F_LOR)
  {
    low_lex();
    if(!IS_ZERO(sp))
    {
      calc3();
      pop_stack();
    }else{
      pop_stack();
      calc3();
    }
  }
}

static void calc1()
{
  calc2();

  if(lookahead=='?')
  {
    low_lex();
    calc1();
    if(lookahead!=':')
      error("Colon expected.\n");
    low_lex();
    calc1();

    if(IS_ZERO(sp-3)) f_swap();
    pop_stack();
    f_swap();
    pop_stack();
  }

  if(lookahead!='\n')
  {
    SKIPUPTO('\n');
    yyerror("Extra characters after #if expression.");
  }else{
    UNGETC('\n');
  }
}

static int calc()
{
  extern int error_recovery_context_exists;
  jmp_buf error_recovery_context;
  struct svalue *save_sp;
  extern struct svalue catch_value;
  int ret;

  save_sp=sp;
  push_control_stack(0);
  push_pop_error_context (1);
  error_recovery_context_exists=2;
  if (setjmp(error_recovery_context))
  {
    ret=0;
    push_pop_error_context(-1);
    pop_control_stack();
    if(catch_value.type==T_STRING)
    {
      yyerror(strptr(&catch_value));
    }else{
      yyerror("Eval error");
    }
    ret=-1;
  }else{
    low_lex();
    calc1();
    pop_control_stack();
    push_pop_error_context(0);
    ret=!IS_ZERO(sp);
  }
  while(sp>save_sp) pop_stack();

  return ret;
}

void set_inc_list(struct svalue *sv)
{
  int i;
  struct vector *v;
  char *p;

  if (sv == 0)
  {
    fprintf(stderr, "There must be a function 'define_include_dirs' in master.c.\n");
    fprintf(stderr, "This function should return an array of all directories to be searched\n");
    fprintf(stderr, "for include files.\n");
    exit(1);
  }
  if (sv->type != T_POINTER)
  {
    fprintf(stderr, "'define_include_dirs' in master.c did not return an array.\n");
    exit(1);
  }
  v = sv->u.vec;
  inc_list = (char **)xalloc(v->size * sizeof (char *));
  inc_list_size = v->size;
  for (i=0; i < v->size; i++)
  {
    if (v->item[i].type != T_STRING)
    {
      fprintf(stderr, "Illegal value returned from 'define_include_dirs' in master.c\n");
      exit(1);
    }
    p = strptr(v->item+i);
    if(!batch_mode)
      if (*p == '/') p++;
    /*
     * Even make sure that the game administrator has not made an error.
     */
    if (!legal_path(p))
    {
      fprintf(stderr, "'define_include_dirs' must give paths without any '..'\n");
      exit(1);
    }
    inc_list[i] = make_shared_string(p);
  }
}


static void start_new()
{
  struct lpc_predef_s *tmpf;
  extern char **local_names;
  extern unsigned short *type_of_locals;
  if(!local_names)
    local_names=(char **)malloc((sizeof(char *))*MAX_LOCAL);
  if(!type_of_locals)
    type_of_locals=(unsigned short *)malloc((sizeof(unsigned short))*MAX_LOCAL);
  free_all_defines();
  
  simple_add_define("LPC4", "1");
  simple_add_define("__LPC4__", "1");
  
  for (tmpf=lpc_predefs; tmpf; tmpf=tmpf->next)
  {
    char namebuf[100],mtext[400];

    *mtext='\0';
    sscanf(tmpf->flag,"%[^=]=%[ -~=]",namebuf,mtext);
    if ( strlen(namebuf) >= sizeof(namebuf) )
      fatal("Define buffer overflow\n");
    if ( strlen(mtext) >= sizeof(mtext) )
      fatal("Define buffer overflow\n");
    simple_add_define(namebuf,mtext);
  }

  free_inputstate(istate);
  istate=NULL;
  link_inputstate(end_inputstate());
  old_line=current_line = 1;
  pragma_all_inline=0;
  pragma_strict_types = 1;
  nexpands=0;
}

void start_new_file(FILE *f)
{
  start_new();
  link_inputstate(file_inputstate(f));
  UNGETSTR("\n",1);
}

void start_new_string(char *s,int len)
{
  start_new();
  UNGETSTR(s,len);
  old_line=current_line = 1;
  UNGETSTR("\n",1);
}

void end_new_file()
{
  free_inputstate(istate);
  istate=NULL;
  free_all_defines();
  total_lines+=current_line-old_line;
}
