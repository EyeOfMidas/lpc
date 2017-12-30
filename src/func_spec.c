#include "config.h"
/*
 * This file specifies types and arguments for efuns.
 * An argument can have two different types with the syntax 'type1 | type2'.
 * An argument is marked as optional if it also takes the type 'void'.
 */

/*
 * These values are used by the stack machine, and can not be directly
 * called from LPC.
 */
%token F_ADD_256 F_ADD_512 F_ADD_768 F_ADD_1024 F_ADD_256X
%token F_POP_VALUE F_DUP F_MARK F_WRITE_OPCODE F_POP_N_ELEMS
%token F_PUSH_INDEXED_LVALUE F_INDIRECT F_INDEX
%token F_PUSH_LTOSVAL F_PUSH_LTOSVAL2 F_PUSH_COST F_SWAP
%token F_BRANCH F_BRANCH_WHEN_ZERO F_BRANCH_WHEN_NON_ZERO
%token F_SHORT_BRANCH F_SHORT_BRANCH_WHEN_ZERO F_SHORT_BRANCH_WHEN_NON_ZERO
%token F_PUSH_ARRAY

/*
 * Basic value pushing
 */

%token F_PUSH_SIMUL_EFUN
%token F_CONSTANT_FUNCTION F_SHORT_CONSTANT_FUNCTION
%token F_GLOBAL F_LOCAL
%token F_PUSH_GLOBAL_LVALUE F_PUSH_LOCAL_LVALUE
%token F_CONSTANT F_SHORT_CONSTANT
%token F_FLOAT
%token F_STRING F_SHORT_STRING
%token F_NUMBER F_CONST_1 F_BYTE F_NEG_BYTE F_SHORT F_CONST0 F_CONST1 

/*
 * These are the predefined functions that can be accessed from LPC.
 */

%token F_INC F_DEC F_POST_INC F_POST_DEC F_INC_AND_POP F_DEC_AND_POP
%token F_RETURN F_DUMB_RETURN F_TAILRECURSE

%token F_ASSIGN F_ASSIGN_AND_POP
%token F_ASSIGN_LOCAL F_ASSIGN_LOCAL_AND_POP
%token F_ASSIGN_GLOBAL F_ASSIGN_GLOBAL_AND_POP
%token F_ADD F_ADD_INT
%token F_SUBTRACT F_MULTIPLY F_DIVIDE F_MOD

%token F_LT F_GT F_EQ F_GE F_LE F_NE
%token F_NEGATE F_NOT F_COMPL
%token F_AND F_OR F_XOR
%token F_LSH F_RSH
%token F_LAND F_LOR

%token F_SWITCH F_SSCANF F_SWAP_VARIABLES F_CATCH
%token F_CAST_TO_OBJECT F_CAST_TO_REGEXP F_CAST_TO_STRING
%token F_CAST_TO_INT F_CAST_TO_FLOAT F_CAST_TO_FUNCTION
%token F_INC_LOOP F_DEC_LOOP
%token F_INC_NEQ_LOOP F_DEC_NEQ_LOOP
%token F_FOREACH

/* Object handling */
object this_object();
side_fx object clone_object(string);
side_fx void destruct(object default: F_THIS_OBJECT);
side_fx void move_object(object);
side_fx string make_object(string);
object first_inventory(object default: F_THIS_OBJECT);
object next_inventory(object default: F_THIS_OBJECT);
object *all_inventory(object default: F_THIS_OBJECT);
object *deep_inventory(object default: F_THIS_OBJECT);
object environment(object default: F_THIS_OBJECT);
object next_clone(object|string);
int created(object);
int object_cpu(object);
int clone_number(object);
object *top_cpu(void);
object master();
object previous_object();
object *previous_objects();
object initiating_object();
string file_name(object default: F_THIS_OBJECT);

/* Object hash */
string hash_name(object);
object find_object(string);

/* Program handling */
string function_exists(string, object default: F_THIS_OBJECT);
string *inherit_list(object default: F_THIS_OBJECT);
side_fx void add_simul_efun(string,function|void);  /* name, function */
int function_args(function);
function *get_function_list(object);
side_fx int load(string);
side_fx int update(string);

/* String functions */
const string *explode(string, string);
const string implode(string *, string);
#ifdef USE_SPRINTF
string sprintf(string, ...);
#endif
const string lower_case(string);
const string capitalize(string);
const int strstr(string,string,int default: F_CONST0);
const string upper_case(string);
const mixed replace(string|mixed *,mixed,mixed);
const int strlen(string);
const int hash(string,int|void);
const int fuzzymatch(string,string);

/* Array efuns */
const string *regexp(string *, regular_expression);
const int sizeof(mixed *);
const int member_array(mixed, mixed *);
const mixed *allocate(int,int|void,...);
const mixed *array_of_index(mixed *,int);
const mixed *aggregate(mixed|void,mixed|void,...);
const int *find_shortest_path(mixed *,int,int);
side_fx mixed *sum_arrays(function,mixed *,...);
side_fx mixed *sort_array(mixed *,function|int|void,...); /* use zero for smartsort */
side_fx mixed *filter_array(mixed *, function|int|string, ...);
side_fx mixed *map_array(mixed *, function|int|string, ...);
side_fx int search_array(mixed *,function|int|string, ...);

/* save/restore functions */
side_fx int restore_object(string);
string save_object();
const mixed decode_value(string);
const string code_value(mixed,int default: F_CONST0);

/* uid functions */
side_fx int export_uid(object);
string geteuid(object default: F_THIS_OBJECT);
string getuid(object default: F_THIS_OBJECT);
side_fx int seteuid(string|int);

/* "command" functions */
object this_player();
side_fx object set_this_player(object);
string query_verb();
side_fx int command(string, object default: F_THIS_OBJECT);
side_fx void add_action(string,function, int default: F_CONST0);
side_fx void disable_commands();
side_fx void enable_commands();
int living(object);
mixed *query_actions(object, object|void);

/* Output functions */
side_fx void write(mixed);

/* File efuns */
string combine_path(string,string|void);
int file_length(string);
side_fx int mkdir(string);
side_fx int write_bytes(string, int, string);
side_fx int write_file(string, string);
side_fx int rename(string, string);
int file_size(string);
string *get_dir(string, int default: F_CONST0);
string read_bytes(string, void|int, void|int);
string read_file(string, void|int, void|int);
side_fx int rm(string);
side_fx int rmdir(string);
mixed *file_stat(string,int default: F_CONST0);

/* Mapping efuns */
side_fx mapping m_set_default(mapping,mixed);
mixed m_get_default(mapping);
side_fx mapping sum_mappings(mapping,mapping,function|int,function|int,function|int);
side_fx mapping m_cleanup(mapping);
side_fx mapping filter_mapping(mapping, function|int|string, ...);
side_fx mapping m_delete(mapping, mixed);
const mapping m_aggregate(mixed|void,mixed|void,...);
const mixed *m_indices(mapping);
const int m_sizeof(mapping);
const mixed *m_values(mapping);
const list m_list(mapping);
side_fx mapping map_mapping(mapping, function|int|string, ...);
const int mappingp(mixed);
mapping mkmapping(mixed *, mixed *);
const mapping solidify_mapping(mapping);

/* lists */
const list mklist(mixed *); 
const mixed *indices(list|mapping);
const list l_delete(list,mixed);
const int l_sizeof(list);
const int listp(mixed);
const list l_aggregate(mixed|void,mixed|void,...);

/* Function pointers */
side_fx mixed call_function(function *|function|int,mixed|void,...);
function get_function(object,string);
function get_lfun(object,int); /* this function is faster than get_function,
				  you don't have to use it though, the compiler
				  will optimize get_function to get_lfun when
				  possible */

function this_function();
string function_name(function);
object function_object(function);

/* floating point stuff */
#ifdef MATH
const float cos(float);
const float sin(float);
const float tan(float);
const float asin(float);
const float acos(float);
const float atan(float);
const mixed sqrt(float|int);
const float log(float);
const float pow(float, float);
const float exp(float);
const float floor(float);
const float ceil(float);
#endif

/* socket efuns */
side_fx int socket_create(int, function|void, function|void);
side_fx int socket_bind(int, int);
side_fx int socket_listen(int, function);
side_fx int socket_accept(int, function, function);
side_fx int socket_connect(int, string, function, function);
side_fx int socket_write(int, string, string|void);
side_fx int socket_close(int);
side_fx int socket_release(int, function);
side_fx int socket_acquire(int, function, function, function);
side_fx int set_socket_read(int,function);
side_fx int set_socket_write(int,function);
side_fx int set_socket_close(int,function);
side_fx int set_socket_mode(int,int);
const string socket_error(int);
string socket_address(int);
string socket_port(int);
string dump_socket_status();
side_fx int socket_from_stdin();

/* Phew, status functions */
string dumpallobj();
string dump_malloc_data();
string dump_string_data(int);
string dump_heart_beat_data(int);
string dump_prog_table_data(int);
string dump_index_table_data(int);

/* Delay efuns */
side_fx void call_out(function, int, ...);
mixed *call_out_info();
int find_call_out(function);
side_fx int remove_call_out(function);
side_fx int set_heart_beat(int);

/* Identifying functions */
const int intp(mixed);
const int objectp(mixed);
const int pointerp(mixed);
const int functionp(mixed);
const int stringp(mixed);
const int floatp(mixed);
const int zero_type(mixed);
const int regular_expressionp(mixed);

/* Unix-type functions */
const string crypt(string, string|int);	/* An int as second argument ? */
const string ctime(int);
const string query_host_name();
int time();
#ifdef HAVE_GETRUSAGE
mixed *rusage();
#endif
side_fx void shutdown();

/* Misc */
const mixed copy_value(mixed);
const mixed sum(mixed,mixed|void,...);
const mixed range(mixed,int,int);
int random(int);
side_fx void expunge();
side_fx void gc(int);
side_fx void break_point();
const int size(mixed);
const string version();
mixed debug_info(int, mixed|void, ...);
string query_load_average();
side_fx void throw(mixed);
int query_num_arg(void);
mixed *get_varargs();
side_fx void set_max_eval_cost(int);
side_fx void reset_eval_cost();
int is_equal(mixed,mixed);

/* batch mode */
side_fx string gets();
side_fx int getchar();
side_fx void exit(int);
side_fx void sleep(int);
side_fx string popen(string);
side_fx int fork();
string getenv(string);
#ifdef HAVE_PUTENV
side_fx void putenv(string);
#endif
side_fx void perror(string);
side_fx int cd(string);
side_fx string getcwd();
side_fx int setugid(int,int);

/* database support efuns */
mixed db_get(string,string);
side_fx int db_set(string,string,string);
side_fx int db_delete(string,string);
side_fx mixed db_indices(string);
side_fx void db_flush();


