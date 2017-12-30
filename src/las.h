#define MAX_GLOBAL_VARIABLES 1000

struct node_s
{
  unsigned short token;
  unsigned short type;
  long opt_info;
  short lnum;
  union 
  {
    struct svalue sval;
    struct
    {
      union
      {
	struct object *ob;
	char *str;
	struct node_s *node;
	struct vector *vec;
	int number;
	float fnum;
      } a,b;
    } s;
  } u;
};

typedef struct node_s node;

#define OPT_A_IS_NODE         0x1
#define OPT_B_IS_NODE         0x2
#define OPT_CONST             0x4
#define OPT_SIDE_EFFECT       0x8
#define OPT_LOCAL_ASSIGNMENT  0x10
#define OPT_GLOBAL_ASSIGNMENT 0x20
#define OPT_RESTORE_OBJECT    0x40
#define OPT_TRY_OPTIMIZE      0x80
#define OPT_OPT_COMPUTED      0x100
#define OPT_CASE              0x200
#define OPT_CONT              0x400
#define OPT_BREAK             0x800
#define OPT_RETURN            0x1000
#define OPT_GLOBAL_DEPEND     0x2000
#define OPT_LVALUE_CHANGE     0x4000
#define OPT_LVALUE_OVERWRITE  0x8000
#define OPT_COULD_BE_CONST    0x10000
#define OPT_OPTIMIZED         0x20000

#define OPT_NOT_THROWABLE (OPT_SIDE_EFFECT | OPT_LOCAL_ASSIGNMENT | \
			   OPT_GLOBAL_ASSIGNMENT | OPT_RESTORE_OBJECT | \
			   OPT_CASE | OPT_CONT | OPT_BREAK | OPT_RETURN | \
			   OPT_LVALUE_CHANGE | OPT_LVALUE_OVERWRITE )

/* NUMPAREAS areas are saved with the program code after compilation.
 */
#define A_PROGRAM		0
#define A_FUNCTIONS		1
#define A_FUNCTION_PTRS         2
#define A_STRINGS		3
#define A_VARIABLES		4
#define A_LINENUMBERS		5
#define A_INHERITS		6
#define A_ARGUMENT_TYPES	7
#define A_ARGUMENT_INDEX	8
#define A_SWITCH_MAPPINGS	9
#define A_CONSTANTS             10
#define NUMAREAS		11

void dooptcode(node *n,int args);
void push_explicit(int);
void push_address();
int pop_address();
void setup_fake_program();
INLINE void add_to_mem_block(int n,char *data,int size);

node *mknode(short,node *,node *);
node *mkintnode(int);
node *mkstrnode(char *);
node *mkcastnode(int,node *);
node *mkfloatnode(float);
node *mkconstfunnode(int);
node *mkglobalnode(int);
node *mklocalnode(int);
node *mksimulefunnode(int);
node *mkefunnode(int,node *);

node *mksvaluenode(struct svalue *);
node *copy_node(node *);
int get_opt_info(void);
void start_line_numbering(void);

#define FUNCTION(n) ((struct function *)mem_block[A_FUNCTIONS].block + (n))
#define FUNCTIONP(n) ((struct function_p *)mem_block[A_FUNCTION_PTRS].block + (n))
#define VARIABLE(n) ((struct variable *)mem_block[A_VARIABLES].block + (n))
#define INHERIT(n) ((struct inherit *)mem_block[A_INHERITS].block + (n))

/*
 * Some good macros to have.
 */

#define MASK(x) ((x) & TYPE_MOD_MASK)

#define BASIC_TYPE(e,t) ((e) == TYPE_ANY ||\
			 (e) == (t) ||\
			 (t) == TYPE_ANY)

#define SAME_TYPE(e,t) ( MASK(e) != TYPE_ANY && \
			(MASK(e) == MASK(t) || \
			 ((e) & (t) & TYPE_MOD_POINTER)) )

#define TYPE(e,t) (BASIC_TYPE(MASK(e), MASK(t)) ||\
		   ((e) & (t) & TYPE_MOD_POINTER))


