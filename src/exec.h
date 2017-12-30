/*
 * A compiled program consists of several data blocks, all allocated
 * contiguos in memory to enhance the working set. At the compilation,
 * the blocks will be allocated separately, as the final size is
 * unknow. When compilation is done, the blocks will be copied into
 * the one big area.
 *
 * 1. The program itself. Consists of machine code instructions for a virtual
 *    stack machine.
 * 2. Function headers. This struct exists once for each written function.
 *    It tells where in the program the function is, how many arguments it
 *    takes number of local variables etc. etc.
 * 3. Function pointers. For every function in the program (even inherited
 *    ones) there is a function pointer. It contains information about where
 *    the function struct is, and some flags for the function.
 * 4. Function index. This is an array of shorts that points into the array
 *    of function pointers. It is sorted on the pointer to the name of the
 *    function. There is one short for each function that is callable from
 *    other objects. This area is not present while compiling.
 * 5. String table. All strings used in the program. They are all pointers
 *    into the shared string area. Thus, they are easily found and deallocated
 *    when the object is destructed.
 * 6. Table of variable names. They all point into the shared string table.
 *    Types are also stored here.
 * 7. Line number information. This is used at errors, to find out the
 *    line number of the error.
 * 8. List of inherited objects.
 * 9. A list of constants used by the program. Only arrays, mappings
 *    and lists apply.
 * 10. Switch information. (stored in mappings)
 * 11. a list of 'fast lookup' lfun pointers.
 */

/*
 * When an new object inherits from another, all function definitions
 * are copied, and all variable definitions.
 * Flags below can't explicitly declared. Flags that can be declared,
 * are found with TYPE_ below.
 *
 * When an object is compiled with type testing NAME_STRICT_TYPES, all
 * types are saved of the arguments for that function during compilation.
 * If the #pragma save_types is specified, then the types are saved even
 * after compilation, to be used when the object is inherited.
 */
#define NAME_INHERITED		0x1	/* Defined by inheritance */
#define NAME_UNDEFINED		0x2	/* Not defined yet */
#define NAME_STRICT_TYPES	0x4	/* Compiled with type testing */
#define NAME_HIDDEN		0x8	/* Not visible for inheritance */
#define NAME_PROTOTYPE		0x10	/* Defined by a prototype only */


#define LF_CATCH_TELL 0
#define LF_HEART_BEAT 1
#define LF_INIT 2
#define LF_EXIT 3

extern char **lfuns;
extern int num_lfuns;

struct function {
  char *name;
  unsigned int offset;	/* Address of function */

  unsigned short num_local;	/* Number of local variables */
  unsigned short num_arg;	/* Number of arguments needed.
				   -1 arguments means function not defined
				   in this object. Probably inherited */
};

struct function_p
{
/* Access like this:
 * prog->inherit[this->prog].prog->functions[this->fun]
 */
  unsigned short prog;
  unsigned short fun;
  unsigned short flags;	/* NAME_ . See above. */
  unsigned short type;	/* Return type of function. See below. */
};

#define FUNC(PROG,X) \
 ((PROG)->inherit[(PROG)->function_ptrs[X].prog].prog->functions+ \
  (PROG)->function_ptrs[X].fun)

struct inherit
{
  unsigned short inherit_level;
  struct program *prog;
  unsigned short function_index_offset;
  unsigned short variable_index_offset;
};

struct variable
{
  char *name;
  unsigned short type;		/* Type of variable. See below. TYPE_ */
  unsigned char flags;		/* Facts found by the compiler. NAME_ */
  unsigned char rttype;         /* run-time type */
};


struct program
{
  int ref;			/* Reference count */
#ifdef DEBUG
  int extra_ref;		/* Used to verify ref count */
#endif
  int id;
  unsigned short flags;
  struct program *next_hash;

  char *program;		/* The binary instructions */
  char *name;			/* Name of file that defined prog */
  signed char *line_numbers;	        /* Line number information */
  struct function *functions;
  struct function_p *function_ptrs;
  unsigned short *funindex;
  char **strings;		/* All strings uses by the program */
  struct variable *variable_names; /* All variables defined */
  struct inherit *inherit;	/* List of inherited prgms */
  int total_size;		/* Sum of all data in this struct */
  struct vector **switch_mappings;
  struct svalue *constants;
  short *lfuns;

  /*
   * The types of function arguments are saved where 'argument_types'
   * points. It can be a variable number of arguments, so allocation
   * is done dynamically. To know where first argument is found for
   * function 'n' (number of function), use 'type_start[n]'.
   * These two arrays will only be allocated if '#pragma save_types' has
   * been specified. This #pragma should be specified in files that are
   * commonly used for inheritance. There are several lines of code
   * that depends on the type length (16 bits) of 'type_start' (sorry !).
   */
  unsigned short *argument_types;
#define INDEX_START_NONE		65535
  unsigned short *type_start;
  /*
   * And now some general size information.
   */
  unsigned short num_constants;
  unsigned short num_switch_mappings;
  unsigned short program_size;	/* size of this instruction code */
  unsigned short num_functions;
  unsigned short num_funindex;
  unsigned short num_function_ptrs;
  unsigned short num_strings;
  unsigned short num_variables;
  unsigned short num_inherited;
  unsigned short num_line_numbers;
  unsigned short num_lfuns;
};

extern struct program *current_prog;

/*
 * Types available. The number '0' is valid as any type. These types
 * are only used by the compiler, when type checks are enabled. Compare with
 * the run-time types, named T_ interpret.h.
 */

#define TYPE_UNKNOWN	0	/* This type must be casted */
#define TYPE_ANY	1	/* Will match any type */
#define TYPE_VOID	2
#define TYPE_NUMBER	3
#define TYPE_STRING	4
#define TYPE_OBJECT	5
#define TYPE_MAPPING	6
#define TYPE_LIST	7
#define TYPE_FUNCTION   8
#define TYPE_FLOAT      9
#define TYPE_REGULAR_EXPRESSION    10

/*
 * These are or'ed in on top of the basic type.
 */
#define TYPE_MOD_STATIC		0x0100	/* Static function or variable */
#define TYPE_MOD_NO_MASK	0x0200	/* The nomask => not redefineable */
#define TYPE_MOD_POINTER	0x0400	/* Pointer to a basic type */
#define TYPE_MOD_PRIVATE	0x0800	/* Can't be inherited */
#define TYPE_MOD_PROTECTED	0x1000
#define TYPE_MOD_PUBLIC		0x2000  /* Force inherit through private */
#define TYPE_MOD_VARARGS	0x4000	/* may take less args */
#define TYPE_MOD_CONSTANT	0x8000	/* Used for optimization */
#define TYPE_MOD_SIDE_EFFECT	0x80	/* Used for optimization */
#define TYPE_MOD_INLINE         0x40	/* Used for optimization */
#define TYPE_MOD_VA_ARGS        0x20	/* may take more args */

#define TYPE_MOD_MASK		(~(TYPE_MOD_STATIC | TYPE_MOD_NO_MASK |\
				   TYPE_MOD_PRIVATE | TYPE_MOD_PROTECTED |\
				   TYPE_MOD_PUBLIC | TYPE_MOD_VARARGS |\
                                   TYPE_MOD_CONSTANT | TYPE_MOD_SIDE_EFFECT|\
				   TYPE_MOD_INLINE | TYPE_MOD_VA_ARGS))
struct keyword
{
  char *word;
  short token;
  short min_args;	/* Minimum number of arguments. */
  short max_args;	/* Maximum number of arguments. */
  short ret_type;	/* The return type used by the compiler. */
  short arg_type1;	/* Type of argument 1 */
  short arg_type2;	/* Type of argument 2 */
  int arg_index;	/* Index pointing to where to find arg type */
  short Default;	/* an efun to use as default for last argument */
  func_t efunc;		/* what do we call? */
  opcode_t opfunc;	/* who you gonna call? */
};



int defined_function(char *s);
int islocal(char *str);
