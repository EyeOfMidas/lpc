struct call_out_s
{
  int time;
  struct object *command_giver;
  struct vector *args;
};

typedef struct call_out_s call_out;

extern call_out **pending_calls; /* pointer to first busy pointer */
extern int num_pending_calls; /* no of busy pointers in buffer */

void do_call_outs();
char *print_call_out_usage(int verbose);
void free_all_call_outs();
#ifdef DEBUG
void count_ref_from_call_outs();
#endif
