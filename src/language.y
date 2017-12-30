
/* this one should make sure that this token has the first number */
%token F_ADD_256

/* This comment inserts extra tokens from make_func after next line
 * the empty lines below will be filled with %token statements,
 * they are left empty so that line numbering will be correct
 * (even if the file will be wrong)
 */

/* MAGIC MARKER */





































































































































































































































































































































/*
 * These are token values that needn't have an associated code for the
 * compiled file
 */

%token F_MAX_OPCODE
%token F_ADD_EQ
%token F_AND_EQ
%token F_ARG_LIST
%token F_ARROW
%token F_BREAK
%token F_CALL_OTHER
%token F_CASE
%token F_CAST
%token F_COLON_COLON
%token F_COMMA
%token F_CONTINUE 
%token F_DEFAULT
%token F_DIV_EQ
%token F_DO
%token F_DOT_DOT
%token F_DOT_DOT_DOT
%token F_EFUN
%token F_EFUN_CALL
%token F_EFUN_NAME
%token F_ELSE
%token F_FLOAT_TOKEN
%token F_FOR
%token F_FUNCTION
%token F_FUNCTION_NAME
%token F_GAUGE
%token F_IDENTIFIER
%token F_IF
%token F_INHERIT
%token F_INLINE
%token F_INT
%token F_LAMBDA
%token F_LIST
%token F_LIST_END
%token F_LOCAL
%token F_LSH_EQ
%token F_LVALUE_LIST
%token F_MAPPING
%token F_MIXED
%token F_MOD_EQ
%token F_MULT_EQ
%token F_NO_MASK
%token F_OBJECT
%token F_OR_EQ
%token F_PRIVATE
%token F_PROTECTED
%token F_PUBLIC
%token F_REGULAR_EXPRESSION
%token F_RSH_EQ
%token F_STATIC
%token F_STATUS
%token F_STRING_DECL
%token F_SUBSCRIPT
%token F_SUB_EQ
%token F_VARARGS 
%token F_VOID
%token F_WHILE
%token F_XOR_EQ

%token F_MAX_INSTR

%right '='
%right '?'
%left F_LOR
%left F_LAND
%left '|'
%left '^'
%left '&'
%left F_EQ F_NE
%left '>' F_GE '<' F_LE  /* nonassoc? */
%left F_LSH F_RSH
%left '+' '-'
%left '*' '%' '/'
%right F_NOT '~'
%nonassoc F_INC F_DEC


%{
/*
 * This is the grammar definition of LPC. The token table is built
 * automatically by make_func. The lang.y is constructed from this file,
 * the generated token list and post_lang.y. The reason of this is that there
 * is no #include-statment that yacc recognizes.
 */
#include "global.h"
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#if defined(sun)
#include <alloca.h>
#endif

#include <setjmp.h>
#include "simul_efun.h"
#include "interpret.h"
#include "array.h"
#include "object.h"
#include "exec.h"
#include "instrs.h"
#include "incralloc.h"
#include "operators.h"
#include "stralloc.h"
#include "main.h"
#include "las.h"
#include "simulate.h"
#include "interpret.h"
#include "lex.h"

extern struct program fake_prog;

#define YYMAXDEPTH	600

int islocal PROT((char *));

extern struct mem_block mem_block[NUMAREAS];


#define align(x) (((x) + 3) & ~3)
#define FUN_IS_VARARGS 0x40000000

/*
 * If the type of the function is given, then strict types are
 * checked and required.
 */
int exact_types;
extern int pragma_strict_types;	/* Maintained by lex.c */
extern int pragma_save_types;	/* Also maintained by lex.c */
int approved_object;		/* How I hate all these global variables */

int total_num_prog_blocks, total_prog_block_size;

extern int lasdebug;
extern struct vector *current_switch_mapping;
extern int *break_stack;
extern int current_break,break_stack_size;
extern int *continue_stack;
extern int current_continue,continue_stack_size;

extern int comp_stackp;
extern int num_parse_error;
extern int d_flag;
static int current_type;
node *init_node=0;


static struct program NULL_program; /* marion - clean neat empty struct */

void push_locals();
void pop_locals();
void push_break_stack();
void pop_break_stack(int);
void push_continue_stack();
void pop_continue_stack(int);

void epilog();
int check_declared PROT((char *str));
static void prolog();
void free_all_local_names(),
    add_local_name PROT((char *, int)), smart_log PROT((char *, int, char *));
extern int yylex();
int verify_declared PROT((char *));
static void copy_variables();
static void copy_functions PROT((struct program *, int type,int offset));
void type_error PROT((char *, int));
int insert_efun_call(int f,int args);
void insert_pop_stack();
void pop_local_names();
char *get_type_name(int);

extern int current_line;
/*
 * 'inherit_file' is used as a flag. If it is set to a string
 * after yyparse(), this string should be loaded as an object,
 * and the original object must be loaded again.
 */
extern char *current_file, *inherit_file;

/*
 * The names and types of arguments and auto variables.
 */
char **local_names;
unsigned short *type_of_locals;
int current_number_of_locals = 0;

struct program *prog;	/* Is returned to the caller of yyparse */

/*
 * Return the index of the function found, otherwise -1.
 */
int defined_function(char *s)
{
  int e;
  struct function_p *funp;
  struct function *fun;

#ifdef DEBUG
  if(s!=debug_findstring(s))
    fatal("defined_function on nonshared string.\n");
#endif

  for (e=mem_block[A_FUNCTION_PTRS].current_size/sizeof(struct function_p)-1
       ;e>=0;e--)
  {
    funp=FUNCTIONP(e);
    if (funp->flags & NAME_HIDDEN) continue;
    fun=INHERIT(funp->prog)->prog->functions+funp->fun;
    /* Pointer comparison is possible since we use unique strings */
    if (fun->name == s) return e;
  }
  return -1;
}

int low_reference_inherited_function(int e,char *function_name)
{
  struct function_p funp;
  struct program *p;
  int i,d;

  p=fake_prog.inherit[e].prog;
  i=find_shared_string_function(function_name,p);
  if(i==-1) return i;

  if(p->function_ptrs[i].flags & (NAME_UNDEFINED|NAME_HIDDEN))
    return -1;

  funp=p->function_ptrs[i];
  funp.prog=e;
  funp.flags|=NAME_HIDDEN;

  for(d=0;d<fake_prog.num_function_ptrs;d++)
  {
    struct function_p *fp;
    fp=fake_prog.function_ptrs+d;

    if(!MEMCMP((char *)fp,(char *)&funp,sizeof funp)) return d;
  }

  add_to_mem_block(A_FUNCTION_PTRS,(char *)&funp,sizeof funp);
  return fake_prog.num_function_ptrs;
}

int reference_inherited_function(char *super_name,char *function_name)
{
  struct program *p;
  int e,i;

#ifdef DEBUG
  if(function_name!=debug_findstring(function_name))
    fatal("defined_function on nonshared string.\n");
#endif

  setup_fake_program();

  for(e=fake_prog.num_inherited-1;e>0;e--)
  {
    if(fake_prog.inherit[e].inherit_level!=1) continue;
    p=fake_prog.inherit[e].prog;
    if(super_name)
    {
      int l;
      l=strlen(p->name);
      if(l<strlen(super_name)) continue;
      if(strncmp(super_name,p->name+l-strlen(super_name),strlen(super_name)))
	continue;
    }

    i=low_reference_inherited_function(e,function_name);
    if(i==-1) continue;
    return i;
  }
  return -1;
}

/*
 * Define a new function. Note that this function is called at least twice
 * for alll function definitions. First as a prototype, then as the real
 * function. Thus, there are tests to avoid generating error messages more
 * than once by looking at (flags & NAME_PROTOTYPE).
 */
static int define_new_function(char *name,
			       int num_arg,
			       int num_local,
			       int offset,
			       int flags,
			       int type)
{
  int num;
  struct function fun,*func;
  struct function_p fun_p,*funp;
  unsigned short argument_start_index;
  extern int pragma_all_inline;

#ifdef DEBUG
  if(name!=debug_findstring(name))
    fatal("defined_new_function on nonshared string.\n");
#endif

  num = defined_function(name);

  if(pragma_all_inline)
    type|=TYPE_MOD_INLINE;
  if (num >= 0)
  {
    /*
     * The function was already defined. It may be one of several reasons:
     *
     * 1.	There has been a prototype.
     * 2.	There was the same function defined by inheritance.
     * 3.	This function has been called, but not yet defined.
     * 4.	The function is doubly defined.
     * 5.	A "late" prototype has been encountered.
     */
    funp = FUNCTIONP(num);
    if (!(funp->flags & NAME_UNDEFINED) &&
	!(flags & NAME_PROTOTYPE) &&
	!(funp->flags & NAME_INHERITED))
    {
      char *p = (char *)alloca(80 + strlen(name));
      sprintf(p, "Redeclaration of function %s.", name);
      yyerror(p);
      return num;
    }

    /*
     * It was either an undefined but used funtion, or an inherited
     * function. In both cases, we now consider this to be THE new
     * definition. It might also have been a prototype to an already
     * defined function.
     *
     * Check arguments only when types are supposed to be tested,
     * and if this function really has been defined already.
     *
     * 'nomask' functions may not be redefined.
     */
    if ((funp->type & TYPE_MOD_NO_MASK) &&
	!(funp->flags & NAME_PROTOTYPE) &&
	!(flags & NAME_PROTOTYPE))
    {
      char *p = (char *)alloca(80 + strlen(name));
      sprintf(p, "Illegal to redefine 'nomask' function \"%s\"",name);
      yyerror(p);
    }

    func=INHERIT(funp->prog)->prog->functions+funp->fun;

    if (exact_types && funp->type != TYPE_UNKNOWN)
    {
      int i;
      if (func->num_arg != num_arg && !(funp->type & TYPE_MOD_VARARGS))
      {
	yyerror("Incorrect number of arguments.");
      }else if (!(funp->flags & NAME_STRICT_TYPES)){
	yyerror("Called function not compiled with type testing.");
      }else{
	/* Now check that argument types wasn't changed. */
	for (i=0; i < num_arg; i++)
	{
	  /* oops, gotta do something here */
	}
      }
    }

    /* If it was yet another prototype, then simply return. */
    if (flags & NAME_PROTOTYPE)
      return num;

#ifdef DEBUG
    if(lasdebug)
    {
      fprintf(stderr,"Defining function '%s'\n",name);
    }
#endif

    if(funp->prog!=0)
    {
      fun.name=copy_shared_string(name);
      funp->fun=mem_block[A_FUNCTIONS].current_size/sizeof(struct function);
      funp->prog=0;
      add_to_mem_block(A_FUNCTIONS,(char *)&fun,sizeof fun);
      func=INHERIT(funp->prog)->prog->functions+funp->fun;
    }

    func->num_arg=num_arg;
    func->num_local=num_local;
    func->offset=offset;

    funp->flags = flags;
    funp->type = type;
    funp->prog=0;

    if (exact_types) funp->flags |= NAME_STRICT_TYPES;
    return num;
  }
  
  fun.name = copy_shared_string(name);
  fun.offset = offset;
  fun.num_arg = num_arg;
  fun.num_local = num_local;

  num = mem_block[A_FUNCTIONS].current_size / sizeof fun;
  /* Number of local variables will be updated later */
  add_to_mem_block(A_FUNCTIONS, (char *)&fun, sizeof fun);

  fun_p.flags = flags;
  fun_p.type = type;
  fun_p.fun=num;
  fun_p.prog=0;
  if (exact_types)
    fun_p.flags |= NAME_STRICT_TYPES;

  num = mem_block[A_FUNCTION_PTRS].current_size / sizeof fun_p;
  /* Number of local variables will be updated later */
  add_to_mem_block(A_FUNCTION_PTRS, (char *)&fun_p, sizeof fun_p);

  if (exact_types == 0 || num_arg == 0)
  {
    argument_start_index = INDEX_START_NONE;
  } else {
    int i;

    /*
     * Save the start of argument types.
     */
    argument_start_index =
      mem_block[A_ARGUMENT_TYPES].current_size /
	sizeof (unsigned short);
    for (i=0; i < num_arg; i++)
    {
      add_to_mem_block(A_ARGUMENT_TYPES, (char *)&type_of_locals[i],
		       sizeof type_of_locals[i]);
    }
  }
  add_to_mem_block(A_ARGUMENT_INDEX, (char *)&argument_start_index,
		   sizeof argument_start_index);
  return num;
}

unsigned short compile_type_to_runtime_type(unsigned short type)
{
  if(type & TYPE_MOD_POINTER) return T_POINTER;
  switch(type & TYPE_MOD_MASK)
  {
  case TYPE_NUMBER: return T_NUMBER;
  case TYPE_STRING: return T_STRING;
  case TYPE_OBJECT: return T_OBJECT;
  case TYPE_MAPPING: return T_MAPPING;
  case TYPE_LIST: return T_LIST;
  case TYPE_FUNCTION: return T_FUNCTION;
  case TYPE_FLOAT: return T_FLOAT;
  case TYPE_REGULAR_EXPRESSION: return T_REGEXP;
  default: return T_ANY;
  }
}

/* argument must be a shared string */
static void define_variable(char *name,int type,int flags)
{
  struct variable dummy;
  int n;

#ifdef DEBUG
  if(name!=debug_findstring(name))
    fatal("define_variable on nonshared string.\n");
#endif

  if(type==TYPE_VOID) return;
  n = check_declared(name);
  if (n != -1 && (VARIABLE(n)->type & TYPE_MOD_NO_MASK))
  {
    char *p = (char *)alloca(80 + strlen(name));
    sprintf(p, "Illegal to redefine 'nomask' variable \"%s\"", name);
    yyerror(p);
  }
  dummy.name = copy_shared_string(name);
  dummy.type = type;
  dummy.flags = flags;
  dummy.rttype=compile_type_to_runtime_type(type);
  switch(dummy.rttype)
  {
  case T_FUNCTION:
    dummy.rttype=T_ANY;
  case T_ANY:
    add_to_mem_block(A_VARIABLES, (char *)&dummy, sizeof dummy);
    /* add a dummy variable to allocate some extra space */
    dummy.name=make_shared_string("dummy");
    dummy.type=TYPE_VOID;
    dummy.flags=NAME_HIDDEN;
    dummy.rttype=T_NOTHING;
    add_to_mem_block(A_VARIABLES, (char *)&dummy, sizeof dummy);
    break;

  default:
    add_to_mem_block(A_VARIABLES, (char *)&dummy, sizeof dummy);
  }
}

void fix_comp_stack(int sp)
{
  if(comp_stackp>sp)
  {
    yyerror("Compiler stack fixed.");
    comp_stackp=sp;
  }else if(comp_stackp<sp){
    fatal("Compiler stack frame underflow.");
  }
}

%}
%union
{
  int number;
  float fnum;
  unsigned int address;		/* Address of an instruction */
  char *string;
  unsigned short type;
  struct node_s *n;
}

%type <fnum> F_FLOAT
%type <number> F_FUNCTION_NAME F_EFUN_NAME argument_list 
%type <number> assign F_NUMBER F_LOCAL
%type <number> optional_varargs optional_varargs2
%type <number> argument type basic_type optional_star cast
%type <number> type_modifier type_modifier_list opt_basic_type
%type <string> F_IDENTIFIER F_STRING string_constant
%type <string> any_identifier ident_or_function_name

/* The following symbos return type information */

%type <n> function_call string expr01 expr00 comma_expr fnumber
%type <n> expr2 expr1 expr3 number expr0 expr4 catch lvalue_list
%type <n> lambda for_expr block  assoc_pair new_local_name
%type <n> expr_list2 m_expr_list m_expr_list2 statement gauge sscanf
%type <n> for do cond optional_else_part while statements
%type <n> local_name_list call_other expr_list3 swap
%type <n> unused2 foreach unused switch case return expr_list default
%type <n> continue break block_or_semi
%%

all: program;

program: program def possible_semi_colon
       |	 /* empty */ ;

possible_semi_colon: /* empty */
                   | ';' { yyerror("Extra ';'. Ignored."); };

string_constant: F_STRING { $$=string_copy($1); free_string($1); }
	   | string_constant '+' F_STRING
	{
	    $$ = xalloc( strlen($1) + strlen($3) + 1 );
	    strcpy($$, $1);
	    strcat($$, $3);
	    free($1);
	    free_string($3);
	};

inheritance: type_modifier_list F_INHERIT string_constant ';'
	{
	  struct program *p;
	  struct inherit inherit;
	  int variable_index_offset;
	  int function_index_offset;
	  int inherit_offset,e;
	  char *s;

	  p = find_program2($3);
	  if (p == 0)
	  {
	    inherit_file = $3;
	    /* Return back to load_object() */
	    YYACCEPT;
	  }
	  free($3);
	  if (p->flags & O_APPROVES) approved_object = 1;

	  inherit_offset =
	    mem_block[A_INHERITS].current_size /
	      sizeof (struct inherit);

	  function_index_offset =
	    mem_block[A_FUNCTION_PTRS].current_size /
	      sizeof (struct function_p);

	  variable_index_offset =
	    mem_block[A_VARIABLES].current_size /
	      sizeof (struct variable);

	  for(e=0;e<p->num_inherited;e++)
	  {
	    inherit=p->inherit[e];
	    inherit.inherit_level++;
	    inherit.function_index_offset+=function_index_offset;
	    inherit.variable_index_offset+=variable_index_offset;
	    add_to_mem_block(A_INHERITS,(char *) &inherit, sizeof inherit);
	  }
	  copy_variables(p, $1);
	  copy_functions(p, $1, inherit_offset);
	  
	  if((s=findstring("__INIT")))
	  {
	    if(-1!=find_shared_string_function(s,p))
	    {
	      e=reference_inherited_function(NULL,s);
	      init_node=mknode('{',init_node,mkcastnode(TYPE_VOID,
			mkefunnode(F_CALL_FUNCTION,mkconstfunnode(e))));
	    }
	  }
	};

fnumber: F_FLOAT { $$=mkfloatnode($1); }

number: F_NUMBER { $$=mkintnode($1); } ;

optional_star: /* empty */ { $$ = 0; }
             | '*' { $$ = TYPE_MOD_POINTER; }
             ;

block_or_semi: block { $$ = mknode('{',$1,mknode(F_RETURN,mkintnode(0),0)); }
             | ';' { $$ = NULL;} ;

def: type optional_star any_identifier
	{
	  if ($1 & TYPE_MOD_MASK)
	  {
	    exact_types = $1 | $2;
	  }else{
	    if (pragma_strict_types)
	      yyerror("No return type given for function.");
	    exact_types = 0;
	  }
	}
	'(' argument ')'
	{
	    /*
	     * Define a prototype. If it is a real function, then the
	     * prototype will be replaced below.
	     */
	    define_new_function($3, $6 & ~FUN_IS_VARARGS , 0, 0,
				NAME_UNDEFINED|NAME_PROTOTYPE, $1 | $2 );
	}
        block_or_semi
	{
	  int tmp,args;
	  tmp=mem_block[A_PROGRAM].current_size;
	  args=$6 & ~FUN_IS_VARARGS;
	  /* Either a prototype or a block */
	  if ($9!=NULL)
	  {
	    dooptcode($9,args);
	    define_new_function($3,args,
				current_number_of_locals-args,
				tmp, 0,	$1 | $2 | get_opt_info() |
				(($6 & FUN_IS_VARARGS)?TYPE_MOD_VA_ARGS:0)
				);
	  }
	  free_all_local_names();
	  free_string($3);	/* Value was copied above */
	}
   | type name_list ';' { if ($1 == 0) yyerror("Missing type"); }
   | inheritance
   | error 
   {
     if(num_parse_error>5) YYACCEPT;
   } ;

new_arg_name: type optional_star any_identifier
            {
#if 0
	      if(islocal($3))
	      {
		yyerror("Illigal to redefine local name.\n");
	      }
#endif
              if (exact_types && $1 == 0) {
                yyerror("Missing type for argument");
                add_local_name($3, TYPE_ANY);	/* Supress more errors */
              } else {
                add_local_name($3, $1 | $2);
              }
            };

optional_varargs: ',' F_DOT_DOT_DOT { $$=FUN_IS_VARARGS; }
                | /* empty */ { $$=0; }
                ;

optional_varargs2: F_DOT_DOT_DOT { $$=FUN_IS_VARARGS; }
                 | /* empty */ { $$=0; }
                 ;

argument: optional_varargs2
        | argument_list optional_varargs { $$= $1 | $2; }
        ;

argument_list: new_arg_name { $$ = 1; }
	     | argument_list ',' new_arg_name { $$ = $1 + 1; } ;

type_modifier: F_NO_MASK { $$ = TYPE_MOD_NO_MASK; }
	     | F_STATIC { $$ = TYPE_MOD_STATIC; }
	     | F_PRIVATE { $$ = TYPE_MOD_PRIVATE; }
	     | F_PUBLIC { $$ = TYPE_MOD_PUBLIC; }
	     | F_VARARGS { $$ = TYPE_MOD_VARARGS; }
	     | F_PROTECTED { $$ = TYPE_MOD_PROTECTED; }
	     | F_INLINE { $$ = TYPE_MOD_INLINE; }
             ;

type_modifier_list: /* empty */ { $$ = 0; }
		  | type_modifier type_modifier_list { $$ = $1 | $2; } ;

type: type_modifier_list opt_basic_type { $$ = $1 | $2; current_type = $$; } ;

cast: '(' basic_type optional_star ')' { $$ = $2 | $3; } ;

opt_basic_type: basic_type | /* empty */ { $$ = TYPE_UNKNOWN; } ;

basic_type: F_STATUS { $$=current_type=TYPE_NUMBER; }
	| F_INT { $$=current_type=TYPE_NUMBER; }
	| F_STRING_DECL { $$=current_type=TYPE_STRING; }
	| F_OBJECT { $$=current_type=TYPE_OBJECT; }
	| F_VOID { $$=current_type=TYPE_VOID; }
	| F_MIXED { $$=current_type=TYPE_ANY; } 
	| F_MAPPING { $$=current_type=TYPE_MAPPING; }
	| F_REGULAR_EXPRESSION { $$=current_type=TYPE_REGULAR_EXPRESSION; }
	| F_LIST { $$=current_type=TYPE_LIST; }
	| F_FUNCTION { $$=current_type=TYPE_FUNCTION; }
	| F_FLOAT_TOKEN { $$=current_type=TYPE_FLOAT; }
        ;

name_list: new_name
	 | name_list ',' new_name;

new_name: optional_star F_IDENTIFIER
	{
          define_variable($2, current_type | $1, 0);
	  free_string($2);
	}
        | optional_star F_IDENTIFIER '='
        {
          define_variable($2, current_type | $1, 0);
        }
        expr0
	{
          int var_num;
          var_num = verify_declared($2);
          init_node=mknode('{',init_node,
		mkcastnode(TYPE_VOID,mknode(F_ASSIGN,$5,mkglobalnode(var_num))));
          free_string($2);
	} ;


new_local_name: optional_star ident_or_function_name
                {
                  add_local_name($2, current_type | $1);
		  $$=0;
                }
              | optional_star ident_or_function_name '=' expr0
                {
                  add_local_name($2, current_type | $1);
                  $$=mkcastnode(TYPE_VOID,
				mknode(F_ASSIGN,$4,
				       mklocalnode(islocal($2))));
                  
                };


block:'{'
     {
       $<number>$=current_number_of_locals;
     } 
     statements '}'
     {
       int e;
       for(e=($<number>2)+1;e<current_number_of_locals;e++)
       {
	 if(local_names[e]) free_string(local_names[e]);
	 local_names[e]=NULL; 
       }
       $$=$3;
     } ;

local_name_list: new_local_name
               | local_name_list ',' new_local_name
	       {
		 $$=mknode('{',$1,$3);
	       }
               ;

statements: { $$=0; }
          | statements statement { $$=mknode('{',$1,$2); }
          ;

statement: unused2 ';' { $$=$1; }
         | basic_type local_name_list ';' { $$=$2; }
         | cond
         | while
         | do
         | for
         | switch
         | case
         | default
         | return ';'
	 | block {}
         | foreach
         | break
         | continue
         | error ';' { $$=0; }
  	 | ';' { $$=0; } 
         ;


break: F_BREAK { $$=mknode(F_BREAK,0,0); } ;
default: F_DEFAULT ':'  { $$=mknode(F_DEFAULT,0,0); } ;
continue: F_CONTINUE { $$=mknode(F_CONTINUE,0,0); } ;

lambda: F_LAMBDA
	{
	  push_locals();
	  exact_types=1;
	  $<number>$=comp_stackp;
	}
       '(' argument ')' block
	{
	  char buf[40],*name;
	  int f,i,args;
	  fix_comp_stack($<number>2);
	  args=$4 & ~FUN_IS_VARARGS;
	  i=mem_block[A_PROGRAM].current_size;
          dooptcode(mknode('{',$6,mknode(F_RETURN,mkintnode(0),0)),args);
	  sprintf(buf,"__lambda_%d",
		  (int)(mem_block[A_FUNCTION_PTRS].current_size/
		  sizeof(struct function_p)));
	  name=make_shared_string(buf);
	  f=define_new_function(name,args, current_number_of_locals - args,
				i, 0,
				TYPE_ANY | get_opt_info() |
				(($4 & FUN_IS_VARARGS)?TYPE_MOD_VA_ARGS:0)
				);
	  free_string(name);
	  pop_locals();
          $$=mkconstfunnode(f);
	} ;


cond: F_IF '(' comma_expr ')' 
      statement
      optional_else_part
      {
        $$=mkcastnode(TYPE_VOID,mknode('?',$3,mknode(':',$5,$6)));
      }
    ;

optional_else_part: { $$=0; }
    | F_ELSE statement { $$=$2; }
    ;      

foreach: F_FOREACH '(' expr0 ',' expr4 ')'
         statement
	 {
           $$=mknode(F_FOREACH,mknode(' ',$3,$5),$7);
	 } ;

do: F_DO statement F_WHILE '(' comma_expr ')' ';'
    {
      $$=mknode(F_DO,$2,$5);
    } ;


for: F_FOR '(' unused  ';' for_expr ';' unused ')'
     statement
     {
       $$=mknode('{',$3,mknode(F_FOR,$5,mknode(':',$9,$7)));
     } ;


while:  F_WHILE '(' comma_expr ')'
        statement
	{
	  $$=mknode(F_FOR,$3,mknode(':',$5,NULL));
        } ;

for_expr: /* EMPTY */ { $$=mkintnode(1); }
        | comma_expr;

switch:	F_SWITCH '(' comma_expr ')'
        statement
        {
          $$=mknode(F_SWITCH,$3,$5);
        } ;


case: F_CASE comma_expr ':'
    {
      $$=mknode(F_CASE,$2,0);
    }
    | F_CASE comma_expr F_DOT_DOT comma_expr ':'
    {
      $$=mknode(F_CASE,$2,$4);
    }
    ;

return: F_RETURN
	{
          $$=mknode(F_RETURN,mkintnode(0),0);
	}
      | F_RETURN comma_expr
	{
          $$=mknode(F_RETURN,$2,0);
	};
	
unused: { $$=0; }
      | unused2
      ;

unused2: comma_expr
       {
         $$=mkcastnode(TYPE_VOID,$1);
       }
       ;

comma_expr: expr0
          | unused2 ',' expr0
          {
	    $$ = mknode(F_ARG_LIST,mkcastnode(TYPE_VOID,$1),$3); 
	  }
          ;

expr00: expr0
      | '@' expr0
      {
	$$=mknode(F_PUSH_ARRAY,$2,0);
      };

expr0: expr01
     | expr4 '=' expr0
       {
         $$=mknode(F_ASSIGN,$3,$1);
       }
     | expr4 assign expr0
       {
	 $$=mknode(F_ASSIGN,mknode($2,copy_node($1),$3),$1);
       }
     | error assign expr01
       {
          $$=0;
       };

expr01: expr1 { $$ = $1; }
     | expr1 '?' expr01 ':' expr01
	{
          $$=mknode('?',$1,mknode(':',$3,$5));
	};

assign: F_AND_EQ  { $$=F_AND; }
      | F_OR_EQ   { $$=F_OR ; }
      | F_XOR_EQ  { $$=F_XOR; }
      | F_LSH_EQ  { $$=F_LSH; }
      | F_RSH_EQ  { $$=F_RSH; }
      | F_ADD_EQ  { $$=F_ADD; }
      | F_SUB_EQ  { $$=F_SUBTRACT; }
      | F_MULT_EQ  { $$=F_MULTIPLY; }
      | F_MOD_EQ  { $$=F_MOD; }
      | F_DIV_EQ  { $$=F_DIVIDE; }
      ;

optional_comma: | ',' ;

expr_list: { $$=0; }
         | expr_list2 optional_comma { $$=$1; }
         ;
         

expr_list2: expr00
          | expr_list2 ',' expr00 { $$=mknode(F_ARG_LIST,$1,$3); }
          ;

m_expr_list: { $$=0; }
           | m_expr_list2 optional_comma { $$=$1; }
           ;

m_expr_list2: assoc_pair
            | m_expr_list2 ',' assoc_pair { $$=mknode(F_ARG_LIST,$1,$3); }
            ;

assoc_pair:  expr0 ':' expr1
             {
               $$=mknode(F_ARG_LIST,$1,$3);
             } ;

expr1: expr2
     | expr1 F_LOR expr1  { $$=mknode(F_LOR,$1,$3); }
     | expr1 F_LAND expr1 { $$=mknode(F_LAND,$1,$3); }
     | expr1 '|' expr1    { $$=mknode(F_OR,$1,$3); }
     | expr1 '^' expr1    { $$=mknode(F_XOR,$1,$3); }
     | expr1 '&' expr1    { $$=mknode(F_AND,$1,$3); }
     | expr1 F_EQ expr1   { $$=mknode(F_EQ,$1,$3); }
     | expr1 F_NE expr1   { $$=mknode(F_NE,$1,$3); }
     | expr1 '>' expr1    { $$=mknode(F_GT,$1,$3); }
     | expr1 F_GE expr1   { $$=mknode(F_GE,$1,$3); }
     | expr1 '<' expr1    { $$=mknode(F_LT,$1,$3); }
     | expr1 F_LE expr1   { $$=mknode(F_LE,$1,$3); }
     | expr1 F_LSH expr1  { $$=mknode(F_LSH,$1,$3); }
     | expr1 F_RSH expr1  { $$=mknode(F_RSH,$1,$3); }
     | expr1 '+' expr1    { $$=mknode(F_ADD,$1,$3); }
     | expr1 '-' expr1    { $$=mknode(F_SUBTRACT,$1,$3); }
     | expr1 '*' expr1    { $$=mknode(F_MULTIPLY,$1,$3); }
     | expr1 '%' expr1    { $$=mknode(F_MOD,$1,$3); }
     | expr1 '/' expr1    { $$=mknode(F_DIVIDE,$1,$3); }
     ;

expr2: expr3
     | cast expr2         { $$=mkcastnode($1,$2); }
     | F_INC expr4       { $$=mknode(F_INC,$2,0); }
     | F_DEC expr4       { $$=mknode(F_DEC,$2,0); }
     | F_NOT expr2        { $$=mknode(F_NOT,$2,0); }
     | '~' expr2          { $$=mknode(F_COMPL,$2,0); }
     | '-' expr2          { $$=mknode(F_NEGATE,$2,0); }
     ;

expr3: expr4
     | expr4 F_INC       { $$=mknode(F_POST_INC,$1,0); }
     | expr4 F_DEC       { $$=mknode(F_POST_DEC,$1,0); }
     ;

expr4: function_call
     | string
     | number
     | fnumber
     | catch
     | gauge
     | sscanf
     | swap
     | call_other
     | lambda
     | F_IDENTIFIER
     {
       int i;
       if((i=islocal($1))>=0)
       {
         $$=mklocalnode(i);
       }else if((i=check_declared($1))>=0){
         $$=mkglobalnode(i);
       }else if((i=find_simul_efun($1))>=0){
         $$=mksimulefunnode(i);
       }else{
         char *e=(char *)alloca(strlen($1)+20);
         sprintf(e,"'%s' undefined.",$1);
         yyerror(e);
         $$=0;
       }
       free_string($1);
     }
     | expr4 '[' expr0 ']'
     {
       $$=mknode(F_INDEX,$1,$3);
     }
     | expr4 '['  comma_expr F_DOT_DOT comma_expr ']'
     {
       $$=mkefunnode(F_RANGE,mknode(F_ARG_LIST,$1,mknode(F_ARG_LIST,$3,$5)));
     }
     | '(' comma_expr ')' { $$=$2; }
     | '(' '{' expr_list '}' ')' { $$=mkefunnode(F_AGGREGATE,$3); }
     | '(' '[' m_expr_list ']' ')' { $$=mkefunnode(F_M_AGGREGATE,$3); };
     | '(' '<' expr_list F_LIST_END { $$=mkefunnode(F_L_AGGREGATE,$3); }
     | F_FUNCTION_NAME
     {
       struct function_p *fun;
       fun=FUNCTIONP($1);
       if((fun->flags & NAME_UNDEFINED) &&
	   !(fun->flags & NAME_PROTOTYPE))
       {
	 char *p,*name;
	 name=INHERIT(fun->prog)->prog->functions[fun->fun].name;
	 p=(char *)alloca(80+strlen(name));
	 sprintf(p,"Undefined function '%s'",name);
	 yyerror(p);
       }

       $$=mkconstfunnode($1);
     } 
     | expr4 F_ARROW any_identifier
     {
       $$=mkefunnode(F_GET_FUNCTION,mknode(F_ARG_LIST,$1,mkstrnode($3)));
     }
     | any_identifier F_COLON_COLON any_identifier
     {
       int f;
       struct function_p *funp;

       f=reference_inherited_function($1,$3);

       if (f<0 || (funp=FUNCTIONP(f))->flags & NAME_UNDEFINED)
       {
         char *buff;
	 buff=(char *)alloca(80+strlen($1)+strlen($3));
         sprintf(buff, "Undefined function %s::%s", $1,$3);
         yyerror(buff);
       }
       free_string($1);
       free_string($3);
       $$=mkconstfunnode(f);
     }
     | F_COLON_COLON any_identifier
     {
       int e,i;

       $$=0;
       setup_fake_program();
       for(e=1;e<fake_prog.num_inherited;e++)
       {
	 if(fake_prog.inherit[e].inherit_level!=1) continue;
	 i=low_reference_inherited_function(e,$2);
	 if(i==-1) continue;
	 if($$)
	 {
	   $$=mknode(F_ARG_LIST,$$,mkconstfunnode(i));
	 }else{
	   $$=mkconstfunnode(i);
	 }
       }
       if(!$$)
       {
	 $$=mkintnode(0);
       }else{
	 if($$->token==F_ARG_LIST)
	   $$=mkefunnode(F_AGGREGATE,$$);
       }
       free_string($2);
     }
     ;

expr_list3: { $$=0; }
          | ',' expr_list { $$=$2; }
          ;

call_other: F_CALL_OTHER '(' expr0 ',' expr0 expr_list3 ')'
          {
	    $$=mkefunnode(F_CALL_FUNCTION,mknode(F_ARG_LIST,
				 mkefunnode(F_GET_FUNCTION,
					    mknode(F_ARG_LIST,
					    mkcastnode(TYPE_OBJECT,$3),
						   $5)),$6));
          }
  
gauge: F_GAUGE '(' unused ')' { $$=mknode(F_GAUGE,$3,0); } ;
catch: F_CATCH '(' unused ')'
     {
       $$=mknode(F_CATCH,$3,NULL);
     } ;

sscanf: F_SSCANF '(' expr0 ',' expr0 lvalue_list ')'
      {
        $$=mknode(F_SSCANF,mknode(F_ARG_LIST,$3,$5),$6);
      }


swap: F_SWAP_VARIABLES '(' expr4 ',' expr4 ')'
    {
      $$=mknode(F_SWAP_VARIABLES,mknode(F_LVALUE_LIST,$3,$5),0);
    }
  

lvalue_list: /* empty */ { $$ = 0; }
	   | ',' expr4 lvalue_list
           {
             $$ = mknode(F_LVALUE_LIST,$2,$3);
           } ;

string: F_STRING { $$=mkstrnode($1); } ;

function_call: expr4 '(' expr_list ')'
             {
               $$=mkefunnode(F_CALL_FUNCTION,mknode(F_ARG_LIST,$1,$3));
             }
             | F_EFUN F_COLON_COLON any_identifier '(' expr_list ')'
             {
               int i;
               i=lookup_predef($3);
               if(i<=0)
               {
                 char *p=(char *)alloca(strlen($3)+20);
                 sprintf(p,"Unknown efun: %s.",$3);
                 yyerror(p);
                 i=0;
               }
	       free_string($3);
               $$=mkefunnode(i,$5);
             }
             | F_EFUN_NAME '(' expr_list ')'
             {
               $$=mkefunnode($1,$3);
             }
             ;


ident_or_function_name: F_IDENTIFIER
                      | F_FUNCTION_NAME
                      {
			struct function_p *funp;
			struct function *fun;
			funp=FUNCTIONP($1);
			fun=INHERIT(funp->prog)->prog->functions+funp->fun;
			$$=copy_shared_string(fun->name);
		      }

any_identifier: ident_or_function_name
              | F_EFUN_NAME
              {
                $$=copy_shared_string(instrs[$1-F_OFFSET].name);
              } ;



%%

void yyerror(char *str)
{
    extern int num_parse_error;

    if (num_parse_error > 5)
	return;
    (void)fprintf(stderr, "%s:%d: %s\n", current_file,current_line,str);
    fflush(stderr);
    smart_log(current_file, current_line, str);
/* This should be done in some other way in the future /Profezzorn
    if (num_parse_error == 0)
	save_error(str, current_file, current_line);
*/
    num_parse_error++;
}

/* argument must be a shared string */
int check_declared(char *str)
{
  struct variable *vp;
  int offset;

#ifdef DEBUG
  if(str!=debug_findstring(str))
    fatal("check_declared on nonshared string.\n");
#endif

  for (offset=0;
       offset < mem_block[A_VARIABLES].current_size;
       offset += sizeof (struct variable))
  {
    vp = (struct variable *)&mem_block[A_VARIABLES].block[offset];
    if (vp->flags & NAME_HIDDEN) continue;
    /* Pointer comparison is possible since we use unique strings */
    if (vp->name == str)
      return offset / sizeof (struct variable);
  }
  return -1;
}

int verify_declared(char *str)
{
    int r;

    r = check_declared(str);
    if (r < 0)
    {
	char buff[100];
        (void)sprintf(buff, "Variable %s not declared !", str);
        yyerror(buff);
	return -1;
    }
    return r;
}

/* argument must be a shared string (no need to free it) */
void add_local_name(char *str,int type)
{
  if (current_number_of_locals == MAX_LOCAL)
  {
    yyerror("Too many local variables");
  }else {
    type_of_locals[current_number_of_locals] = type;
    local_names[current_number_of_locals++] = str;
  }
}

/* argument must be a shared string */
int islocal(char *str)
{
  int e;
  for(e=current_number_of_locals-1;e>=0;e--)
    if(local_names[e]==str)
      return e;
  return -1;
}

void free_all_local_names()
{
  int i;

  for (i=0; i<current_number_of_locals; i++)
  {
    if(local_names[i])
      free_string(local_names[i]);
    local_names[i] = 0;
  }
  current_number_of_locals = 0;
}

/*
 * Copy all function definitions from an inherited object. They are added
 * as undefined, so that they can be redefined by a local definition.
 * If they are not redefined, then they will be updated, so that they
 * point to the inherited definition. See epilog(). Types will be copied
 * at that moment (if available).
 *
 * A call to an inherited function will not be
 * done through this entry (because this entry can be replaced by a new
 * definition). If an function defined by inheritance is called, then one
 * special definition will be made at first call.
 */
static void copy_functions(struct program *from,int type,int offset)
{
  int i;

  for (i=0; i < from->num_function_ptrs; i++)
  {
    /* Do not call define_new_function() from here, as duplicates would
     * be removed.
     */
    struct function_p fun;
    int new_type;
    char *name;

    fun = from->function_ptrs[i];	/* Make a copy */
    /* Prepare some data to be used if this function will not be
     * redefined.
     */

    name=from->inherit[fun.prog].prog->functions[fun.fun].name;

    fun.prog+=offset;
    if (fun.type & TYPE_MOD_NO_MASK)
    {
      int n;
      n = defined_function(name);
      if (n != -1 && !(FUNCTIONP(n)->flags & NAME_UNDEFINED))
      {
	char *p = (char *)alloca(80 + strlen(name));
	sprintf(p, "Illegal to redefine 'nomask' function \"%s\"",name);
	yyerror(p);
      }
    }
    /*
     * public functions should not become private when inherited
     * 'private'
     */
    new_type = type;
    if (fun.type & TYPE_MOD_PUBLIC) new_type &= ~TYPE_MOD_PRIVATE;
    fun.type |= new_type;

    if(fun.type & TYPE_MOD_PRIVATE) fun.flags|=NAME_HIDDEN;
    fun.flags |= NAME_INHERITED;
    add_to_mem_block(A_FUNCTION_PTRS, (char *)&fun, sizeof fun);
  }
}

/*
 * Copy all variabel names from the object that is inherited from.
 * It is very important that they are stored in the same order with the
 * same index.
 */
static void copy_variables(struct program *from,int type)
{
  int i;

  for (i=0; i<from->num_variables; i++)
  {
    int new_type = type;
    int n = check_declared(from->variable_names[i].name);

    if (n != -1 && (VARIABLE(n)->type & TYPE_MOD_NO_MASK))
    {
      char *p = (char *)alloca(80+strlen(from->variable_names[i].name));
      sprintf(p, "Illegal to redefine 'nomask' variable \"%s\"",
	      VARIABLE(n)->name);
      yyerror(p);
    }
    /*
     * 'public' variables should not become private when inherited
     * 'private'.
     */
    if (from->variable_names[i].type & TYPE_MOD_PUBLIC)
      new_type &= ~TYPE_MOD_PRIVATE;
    define_variable(from->variable_names[i].name,
		    from->variable_names[i].type | new_type,
		    from->variable_names[i].type & TYPE_MOD_PRIVATE ?
		    NAME_HIDDEN : 0);
  }
}

char *get_type_name(int type)
{
    static char buff[100];
    static char *type_name[] =
    {
      "unknown",
      "mixed",
      "void",
      "int", 
      "string",
      "object", 
      "mapping",
      "list",
      "function",
      "float",
      "regular_expression"
    };
    int pointer = 0;

    buff[0] = 0;
    if (type & TYPE_MOD_STATIC)
	strcat(buff, "static ");
    if (type & TYPE_MOD_NO_MASK)
	strcat(buff, "nomask ");
    if (type & TYPE_MOD_PRIVATE)
	strcat(buff, "private ");
    if (type & TYPE_MOD_PROTECTED)
	strcat(buff, "protected ");
    if (type & TYPE_MOD_PUBLIC)
	strcat(buff, "public ");
    if (type & TYPE_MOD_VARARGS)
	strcat(buff, "varargs ");
    type &= TYPE_MOD_MASK;
    if (type & TYPE_MOD_POINTER) {
	pointer = 1;
	type &= ~TYPE_MOD_POINTER;
    }
    if (type >= sizeof type_name / sizeof type_name[0]) fatal("Bad type\n");
    strcat(buff, type_name[type]);
    strcat(buff," ");
    if (pointer)
	strcat(buff, "* ");
    return buff;
}

void type_error(str, type)
    char *str;
    int type;
{
    static char buff[100];
    char *p;
    p = get_type_name(type);
    if (strlen(str) + strlen(p) + 5 >= sizeof buff) {
	yyerror(str);
    } else {
	strcpy(buff, str);
	strcat(buff, ": \"");
	strcat(buff, p);
	strcat(buff, "\"");
	yyerror(buff);
    }
}

#ifdef DEBUG
void dump_program_desc(struct program *p)
{
  int e,d,q;
  fprintf(stderr,"Program '%s':\n",p->name);

  fprintf(stderr,"All inherits:\n");
  for(e=0;e<p->num_inherited;e++)
  {
    fprintf(stderr,"%3d:",e);
    for(d=0;d<p->inherit[e].inherit_level;d++) fprintf(stderr,"  ");
    fprintf(stderr,"%s\n",p->inherit[e].prog->name);
  }

  fprintf(stderr,"All functions:\n");
  for(e=0;e<p->num_function_ptrs;e++)
  {
    fprintf(stderr,"%3d:",e);
    for(d=0;d<p->inherit[p->function_ptrs[e].prog].inherit_level;d++)
      fprintf(stderr,"  ");
    fprintf(stderr,"%s::%s();\n",
	    p->inherit[p->function_ptrs[e].prog].prog->name,
	    FUNC(p,e)->name);
  }
  fprintf(stderr,"All sorted functions:\n");
  for(q=0;q<p->num_funindex;q++)
  {
    e=p->funindex[q];
    fprintf(stderr,"%3d (%3d):",e,q);
    for(d=0;d<p->inherit[p->function_ptrs[e].prog].inherit_level;d++)
      fprintf(stderr,"  ");
    fprintf(stderr,"%s::%s();\n",
	    p->inherit[p->function_ptrs[e].prog].prog->name,
	    FUNC(p,e)->name);
  }
}
#endif

/*
 * Compile an LPC file.
 */
void compile_file()
{
  int yyparse();

  prolog();
  yyparse();
  epilog();
}

int funcmp(const void *a,const void *b)
{
  struct function_p *fpa,*fpb;
  struct function *fa,*fb;
  fpa=prog->function_ptrs+*(unsigned short *)a;
  fpb=prog->function_ptrs+*(unsigned short *)b;
  fa=prog->inherit[fpa->prog].prog->functions+fpa->fun;
  fb=prog->inherit[fpb->prog].prog->functions+fpb->fun;
  return fa->name - fb->name;
}

/*
 * The program has been compiled. Prepare a 'struct program' to be returned.
 */
void epilog()
{
  static int current_program_id=1;

  int size, i,e,t;
  char *p;
#if 0
#ifdef DEBUG
  if (num_parse_error == 0 && type_of_argumentp != 0)
    fatal("Failed to deallocate argument type stack\n",0,0,0,0,0,0,0,0,0);
#endif
#endif
  /*
   * Define the __INIT function, but only if there was any code
   * to initialize.
   */
  if (init_node)
  {
    char *s;
    int pc=mem_block[A_PROGRAM].current_size;
    s=make_shared_string("__INIT");
    define_new_function(s,0,0,0,NAME_UNDEFINED | NAME_PROTOTYPE,
			TYPE_VOID|TYPE_MOD_STATIC);
    dooptcode(mknode('{',init_node,mknode(F_RETURN,mkintnode(0),0)),0);
    define_new_function(s, 0, 0,pc, 0,
			TYPE_VOID|TYPE_MOD_STATIC | get_opt_info());
    free_string(s);
  }

  if (num_parse_error > 0)
  {
    char **grog;
    struct function *grfun;
    struct variable *grvar; 
    int grogs;
    struct vector **foo;
    struct svalue *s;

    prog = 0;
    grog=(char **)(mem_block[A_STRINGS].block);
    grogs=mem_block[A_STRINGS].current_size/sizeof(char *);
    for(i=0;i<grogs;i++) free_string(grog[i]);

    foo=(struct vector **)(mem_block[A_SWITCH_MAPPINGS].block);
    grogs=mem_block[A_SWITCH_MAPPINGS].current_size/sizeof(struct vector *);
    for(i=0;i<grogs;i++) free_vector(foo[i]);

    grfun=(struct function *)(mem_block[A_FUNCTIONS].block);
    grogs=mem_block[A_FUNCTIONS].current_size/sizeof(struct function);
    for(i=0;i<grogs;i++) free_string(grfun[i].name);
      
    grvar=(struct variable *)(mem_block[A_VARIABLES].block);
    grogs=mem_block[A_VARIABLES].current_size/sizeof(struct variable);
    for(i=0;i<grogs;i++) free_string(grvar[i].name);

    s=(struct svalue *)(mem_block[A_CONSTANTS].block);
    grogs=mem_block[A_CONSTANTS].current_size/sizeof(struct svalue);
    for(i=0;i<grogs;i++) free_svalue(s+i);
      
    for (i=0; i<NUMAREAS; i++)
    {
      free(mem_block[i].block);
      mem_block[i].current_size=0;
    }
    return;
  }

  size = align(sizeof (struct program));
  for (i=0; i<NUMAREAS; i++)
    size += align(mem_block[i].current_size);
  size+=align(sizeof(short)*num_lfuns);
  size+=align((mem_block[A_FUNCTION_PTRS].current_size/
	       sizeof (struct function_p))* sizeof(unsigned short));

  p = (char *)xalloc(size);
  prog = (struct program *)p;
  *prog = NULL_program;
  prog->total_size = size;
  prog->ref = 0;
  prog->id=current_program_id++;
  prog->name = fake_prog.name;
  total_prog_block_size += prog->total_size;
  total_num_prog_blocks += 1;

  p += align(sizeof (struct program));

#define INS_BLOCK(PTR,PTRS,TYPE,AREA) \
  if((prog->PTRS = mem_block[AREA].current_size/sizeof(TYPE))) \
  { \
    prog->PTR = (TYPE *)p; \
    MEMCPY(p,mem_block[AREA].block,mem_block[AREA].current_size); \
    p+=align(mem_block[AREA].current_size); \
  }else{ \
    prog->PTR = (TYPE *) NULL; \
  }

  INS_BLOCK(program,program_size,char,A_PROGRAM);
  INS_BLOCK(line_numbers,num_line_numbers,unsigned char,A_LINENUMBERS);
  INS_BLOCK(functions,num_functions,struct function,A_FUNCTIONS);
  INS_BLOCK(function_ptrs,num_function_ptrs,struct function_p,A_FUNCTION_PTRS);
  INS_BLOCK(strings,num_strings,char *,A_STRINGS);
  INS_BLOCK(variable_names,num_variables,struct variable,A_VARIABLES);
  INS_BLOCK(switch_mappings,num_switch_mappings,struct vector *,A_SWITCH_MAPPINGS);
  INS_BLOCK(inherit,num_inherited,struct inherit,A_INHERITS);

  /* Ok, sort for binsearch */
  prog->funindex=(unsigned short *)p;
  for(e=i=0;i<prog->num_function_ptrs;i++)
  {
    struct function_p *funp;
    struct function *fun;
    funp=prog->function_ptrs+i;
    if(funp->flags & (NAME_HIDDEN|NAME_UNDEFINED)) continue;
    if(funp->flags & NAME_INHERITED)
    {
      if(funp->type & TYPE_MOD_PRIVATE) continue;
      fun=prog->inherit[funp->prog].prog->functions+funp->fun;
      for(t=0;t>=0 && t<prog->num_function_ptrs;t++)
      {
	struct function_p *funpb;
	struct function *funb;

	if(t==i) continue;
	funpb=prog->function_ptrs+t;
	if(funpb->flags & (NAME_HIDDEN|NAME_UNDEFINED)) continue;
	if((funpb->flags & NAME_INHERITED) && t<i) continue;
	funb=prog->inherit[funpb->prog].prog->functions+funpb->fun;
	if(fun->name==funb->name) t=-10;
      }
      if(t<0) continue;
    }
    prog->funindex[e]=i;
    e++;
  }
  prog->num_funindex=e;
  fsort((void *)prog->funindex, e,sizeof(unsigned short),funcmp);

  p+=align(prog->num_funindex*sizeof(unsigned short));

  prog->constants = (struct svalue *)p;
  prog->num_constants = mem_block[A_CONSTANTS].current_size /
    sizeof (struct svalue);
  if (mem_block[A_CONSTANTS].current_size)
  {
    int e;
    MEMCPY(p, mem_block[A_CONSTANTS].block,
	   mem_block[A_CONSTANTS].current_size);
    /* see to it that all strings in constants are shared */
    /* for speed and memory effinciency reasons /Hubbe */
    /* hmm, I wonder why I copy the svalues.... /Hubbe */
    for(e=0;e<prog->num_constants;e++)
    {
      struct svalue tmp;
      copy_svalue(&tmp,prog->constants+e);
      free_svalue(prog->constants+e);
      prog->constants[e]=tmp;
    }
  }
  p += align(mem_block[A_CONSTANTS].current_size);

  prog->lfuns=(short *)p;
  for(e=0;e<num_lfuns;e++)
    prog->lfuns[e]=find_function(lfuns[e],prog);
  prog->num_lfuns=num_lfuns;

  p += align(sizeof(short)*num_lfuns);

  prog->argument_types = 0;	/* For now. Will be fixed someday */

  prog->type_start = 0;
  for (i=0; i<NUMAREAS; i++)
    free((char *)mem_block[i].block);

  /*  marion
      Do referencing here - avoid multiple referencing when an object
      inherits more than one object and one of the inherited is already
      loaded and not the last inherited
      */
  reference_prog (prog, "epilog");

  /* profezzorn
     The first item in the inherit_list should be ourselves,
     so don't reference that one
     */
  prog->inherit[0].prog=prog;
  for (i = 1; i < prog->num_inherited; i++)
    reference_prog (prog->inherit[i].prog, "inheritance");


#ifdef DEBUG
  if(d_flag>3 && !inherit_file)
  {
    fprintf(stderr,"Functions in program '%s':\n",prog->name);
    for(e=0;e<prog->num_function_ptrs;e++)
    {
      struct function_p *funp;
      struct function *fun;
      funp=prog->function_ptrs+e;
      fun=prog->inherit[funp->prog].prog->functions+funp->fun;
      
      fprintf(stderr,"Function [%c%c%c%c%c] %10s [%c%c%c%c%c%c%c%c] %s %d %d\n",
	      funp->flags&NAME_INHERITED?'I':' ',
	      funp->flags&NAME_HIDDEN?'H':' ',
	      funp->flags&NAME_UNDEFINED?'U':' ',
	      funp->flags&NAME_STRICT_TYPES?'S':' ',
	      funp->flags&NAME_PROTOTYPE?'P':' ',
	      get_type_name(funp->type&TYPE_MOD_MASK),
	      funp->type&TYPE_MOD_STATIC?'S':' ',
	      funp->type&TYPE_MOD_NO_MASK?'N':' ',
	      funp->type&TYPE_MOD_POINTER?'*':' ',
	      funp->type&TYPE_MOD_PRIVATE?'-':' ',
	      funp->type&TYPE_MOD_PROTECTED?'p':' ',
	      funp->type&TYPE_MOD_PUBLIC?'+':' ',
	      funp->type&TYPE_MOD_VARARGS?'V':' ',
	      funp->type&TYPE_MOD_CONSTANT?'C':' ',
	      fun->name,
	      fun->num_arg,
	      fun->num_local
	      );
    }
    printf("SORTED Functions in program '%s':\n",prog->name);
    for(i=0;i<prog->num_funindex;i++)
    {
      struct function_p *funp;
      struct function *fun;
      e=prog->funindex[i];
      funp=prog->function_ptrs+e;
      fun=prog->inherit[funp->prog].prog->functions+funp->fun;
      
      fprintf(stderr,"Function [%c%c%c%c%c] %10s [%c%c%c%c%c%c%c%c] %s %d %d\n",
	      funp->flags&NAME_INHERITED?'I':' ',
	      funp->flags&NAME_HIDDEN?'H':' ',
	      funp->flags&NAME_UNDEFINED?'U':' ',
	      funp->flags&NAME_STRICT_TYPES?'S':' ',
	      funp->flags&NAME_PROTOTYPE?'P':' ',
	      get_type_name(funp->type&TYPE_MOD_MASK),
	      funp->type&TYPE_MOD_STATIC?'S':' ',
	      funp->type&TYPE_MOD_NO_MASK?'N':' ',
	      funp->type&TYPE_MOD_POINTER?'*':' ',
	      funp->type&TYPE_MOD_PRIVATE?'-':' ',
	      funp->type&TYPE_MOD_PROTECTED?'p':' ',
	      funp->type&TYPE_MOD_PUBLIC?'+':' ',
	      funp->type&TYPE_MOD_VARARGS?'V':' ',
	      funp->type&TYPE_MOD_CONSTANT?'C':' ',
	      fun->name,
	      fun->num_arg,
	      fun->num_local
	      );
    }
    if(d_flag>4)
      dump_program_desc(prog);
  }
#endif
}

/*
 * Initialize the environment that the compiler needs.
 */
static void prolog()
{
  int i;
  struct inherit inherit;
  extern struct program fake_prog;

  start_line_numbering();
  approved_object = 0;
  prog = 0;			/* 0 means fail to load. */
  comp_stackp = 0;		/* Local temp stack used by compiler */
  num_parse_error = 0;
  free_all_local_names();	/* In case of earlier error */
  /* Initialize memory blocks where the result of the compilation
   * will be stored.
   */
  for (i=0; i < NUMAREAS; i++)
  {
    mem_block[i].block = xalloc(START_BLOCK_SIZE);
    mem_block[i].current_size = 0;
    mem_block[i].max_size = START_BLOCK_SIZE;
  }
  inherit.prog=&fake_prog;
  inherit.inherit_level=0;
  inherit.function_index_offset=0;
  inherit.variable_index_offset=0;
  add_to_mem_block(A_INHERITS,(char *)&inherit,sizeof inherit);
  fake_prog.name=make_shared_string(current_file);
  setup_fake_program();
  init_node=0;
}

void push_locals()
{
  push_explicit(current_type);
  push_explicit(exact_types);
  push_explicit((int)local_names);
  push_explicit((int)type_of_locals);
  push_explicit(current_number_of_locals);
  local_names=(char **)malloc(sizeof(char *)*MAX_LOCAL);
  type_of_locals=(unsigned short *)malloc(sizeof(unsigned short)*MAX_LOCAL);
  current_number_of_locals=0;
  exact_types=0;
}

void pop_locals()
{
  int i;

  for (i=0; i<current_number_of_locals; i++)
  {
    if(local_names[i])
      free_string(local_names[i]);
    local_names[i] = 0;
  }
  free((char *)type_of_locals);
  free((char *)local_names);
  current_number_of_locals=(int)pop_address();
  type_of_locals=(unsigned short *)pop_address();
  local_names=(char **)pop_address();
  exact_types=pop_address();
  current_type=pop_address();
}
