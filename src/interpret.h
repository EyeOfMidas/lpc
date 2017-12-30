#ifndef INTERPRET_H
#define INTERPRET_H

struct shared;
struct regexp;

union storage_union
{
  float fnum;
  int number;
  struct shared *string;
  struct object *ob;
  struct vector *vec;
  struct lvalue *lvalue;
  struct regexp *regexp;
  int *ref;
};

/*
 * The value stack element.
 * If it is a string, then the way that the string has been allocated differ,
 * wich will affect how it should be freed.
 */
struct svalue
{
  unsigned short type;
  unsigned short subtype;
  union storage_union u;
};

/* 0 must be without need to be freed */
#define T_POINTER	0
#define T_OBJECT	1
#define T_MAPPING	2
#define T_LIST		3
#define T_ALIST_PART    4
#define T_FUNCTION      5
#define T_REGEXP        6
#define T_STRING	7
#define T_FLOAT         8
#define T_LVALUE	9
#define T_NUMBER	10
#define T_NOTHING       254
#define T_ANY           255

#ifdef STRALLOC_GC
#define MAX_REF_TYPE T_REGEXP
#else
#define MAX_REF_TYPE T_STRING
#endif

#define T_INT           T_NUMBER

#define BT_LVALUE       (1<<T_LVALUE)
#define BT_NUMBER       (1<<T_NUMBER)
#define BT_STRING	(1<<T_STRING)
#define BT_POINTER	(1<<T_POINTER)
#define BT_OBJECT	(1<<T_OBJECT)
#define BT_MAPPING	(1<<T_MAPPING)
#define BT_LIST		(1<<T_LIST)
#define BT_ALIST_PART   (1<<T_ALIST_PART)
#define BT_FUNCTION     (1<<T_FUNCTION)
#define BT_FLOAT        (1<<T_FLOAT)
#define BT_REGEXP       (1<<T_REGEXP)
#define BT_INT          (1<<T_INT)
#define BT_ANY          0xffff
/* these types uses the .u.vec */
#define BT_VECTOR (BT_ALIST_PART | BT_POINTER | BT_MAPPING | BT_LIST)

#define IS_TYPE(X,Y) ((1<<(X).type) & (Y))

/* different types of zeros */
#define NUMBER_NUMBER 0 /* normal number */
#define NUMBER_UNDEFINED 1 /* value not present */
#define NUMBER_DESTRUCTED_OBJECT 2 /* a destructed object */
#define NUMBER_DESTRUCTED_FUNCTION 3 /* a function in a destructed object */

#define LVALUE_INDEX 0
#define LVALUE_RANGE 1
#define LVALUE_LOCAL 2
#define LVALUE_GLOBAL 3
#define LVALUE_SHORT_GLOBAL 4

#define LVALUES 800

struct lvalue
{
  short type;
  short rttype;
  union ptr
  {
    struct svalue *sval;
    union storage_union *uval;
  } ptr;
  struct svalue ind;
};

struct lnode_def;
void free_vector PROT((struct vector *));
void free_all_values();

/*
 * Control stack element.
 * 'prog' is usually same as 'ob->prog' (current_object), except when
 * when the current function is defined by inheritance.
 * The pointer, csp, will point to the values that will be used at return.
 */
struct control_stack
{
  struct object *comgiver;	/* Always save the command giver... */
  struct object *ob;		/* Current object */
  struct program *prog;		/* Current program */
  int num_local_variables;	/* Local + arguments */
  int num_of_arguments;		/* Local + arguments */
  char *pc;
  struct svalue *fp;
  struct function *funp;	/* Only used for tracebacks */
  int func;			/* funp==prog->funcions[func] */
  int function_index_offset;	/* Used when executing functions in inherited
				   programs */
  int variable_index_offset;	/* Same */
  struct vector *va_args;	/* remaining arguments */
};

#define IS_ZERO(x) (!(x) || (((x)->type == T_NUMBER) && ((x)->u.number == 0)))
#define SET_TO_ZERO(x) ((x).type=T_NUMBER,(x).subtype=NUMBER_NUMBER,(x).u.number=0)
#define SET_TO_ONE(x) ((x).type=T_NUMBER,(x).subtype=NUMBER_NUMBER,(x).u.number=1)
#define SET_TO_UNDEFINED(x) ((x).type=T_NUMBER,(x).subtype=NUMBER_UNDEFINED,(x).u.number=0)
#define SET_STR(X,Y) ((X)->type=T_STRING,(X)->u.string=BASE(Y))

struct svalue *apply_lfun(int fun,struct object *ob,int num_arg,int ignorestatic);
struct svalue *apply PROT((char *, struct object *, int,int));
struct svalue *apply_shared PROT((char *, struct object *, int,int));
int apply_lambda_low(struct object *,int,int,int);
struct svalue *apply_lambda(struct svalue *,int,int);
struct svalue *apply_master_ob PROT((char *fun, int num_arg));
struct svalue *safe_apply (char *fun,struct object * ob,int num_arg);
struct svalue *safe_apply_lambda(struct svalue *lambda,int num_arg);
struct svalue *apply_numbered_fun(struct object *o,int fun,int num_arg,int ignorestatic);

void push_string PROT((char *));
void push_number PROT((int));
void push_object PROT((struct object *));
int inter_sscanf PROT((int));
void assert_master_ob_loaded();
void check_eval_cost();
char *function_exists PROT((char *, struct object *));
char *function_exists_in_prog(char *fun,struct program *prog);
int get_line_number_if_any PROT((void));
void check_a_lot_ref_counts PROT((struct program *));
void clear_state PROT((void));
void reset_machine PROT((int));
void push_svalue PROT((struct svalue *));
void push_new_shared_string(char *p);
void call_function PROT((struct program *, struct function *));
int find_function(char *fun,struct program *prog);
int find_shared_string_function(char *fun,struct program *prog);
void pop_stack();
void push_zero();
void push_shared_string(char *);
void push_malloced_string(char *);
void pop_n_elems(int);
void bad_arg(int arg,int instruction)  ATTRIBUTE((noreturn));
void push_vector(struct vector *v);
void push_one();
struct svalue *find_value(int num);
void eval_instruction(char *p);
INLINE void push_mapping(struct vector *v);

void assign_svalue PROT((struct svalue *, struct svalue *));
void move_svalue PROT((struct svalue *, struct svalue *));
INLINE void assign_svalue_no_free PROT((struct svalue *to, struct svalue *from));
#ifdef DEBUG
void assign_svalue_raw PROT((struct svalue *to, struct svalue *from));
#else

#define assign_svalue_raw(TO,FROM) \
  { struct svalue t; *(TO)=t=*(FROM); if(t.type<=MAX_REF_TYPE) t.u.ref[0]++; }
     
#endif
INLINE void free_svalue PROT((struct svalue *));
INLINE void free_svalues PROT((struct svalue *,int));
INLINE void free_lvalue PROT((struct lvalue *));
INLINE void push_control_stack(struct function *funp);
void pop_control_stack();
void push_pop_error_context (int push);
INLINE void push_conditional(int f);
INLINE void copy_svalue(struct svalue *dest,struct svalue *src);
void copy_svalues_no_free(struct svalue *to,struct svalue *from,int num);
void copy_svalues(struct svalue *,struct svalue *,int);
void copy_svalues_raw(struct svalue *,struct svalue *,int);
INLINE void push_list(struct vector *v);

INLINE void assign_short_svalue_no_free(union storage_union *to,
					struct svalue *from,
					unsigned short type);
INLINE void assign_short_svalue(union storage_union *to,
				struct svalue *from,
				unsigned short type);
INLINE void free_short_svalue(union storage_union *u,
			      unsigned short type);
INLINE void assign_svalue_from_short_no_free(struct svalue *to,
				     union storage_union *from,
				     unsigned short type);

#define APPLY_MASTER_OB(RET,FUN,ARGS) \
{ \
  static int fun_,master_cnt=0; \
  extern struct object *master_ob; \
  assert_master_ob_loaded(); \
  if(master_cnt!=master_ob->prog->id) \
  { \
    fun_=find_function(FUN,master_ob->prog); \
    master_cnt=master_ob->prog->id; \
  } \
  RET apply_numbered_fun(master_ob,fun_,ARGS,1); \
}
extern struct svalue const_empty_string;
#endif

