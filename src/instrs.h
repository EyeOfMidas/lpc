/*
 * Information about all instructions. This is not really needed as the
 * automatially generated efun_arg_types[] should be used.
 */

struct instr
{
  short max_arg, min_arg;	/* Can't use char to represent -1 */
  unsigned short type[2];
  short Default;
  short ret_type;
  char *name;
  int arg_index;
  func_t efunc;
  int eval_cost;
#ifdef OPCPROF
  int used;
  int compiled;
  int avg_time;
  int avg_cost;
#endif
};

#ifdef F_MAX_INSTR
extern struct instr instrs[F_MAX_INSTR-F_OFFSET];
#else
extern struct instr instrs[1000];
#endif


int dump_opcode_info();
void check_cost_for_instr(int);
void add_compiled(int instr);
