/* 92/04/18 - cleaned up stylistically by Sulam@TMI */
#include "global.h"
#include <signal.h>
#include <setjmp.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/times.h>
#include <math.h>
#include "interpret.h"
#include "object.h"
#include "exec.h"
#include "comm.h"
#include "debug.h"
#include "dbase_efuns.h"
#include "call_out.h"
#include "backend.h"
#include "dynamic_buffer.h"
#include "simulate.h"
#include "main.h"

#ifdef LACIP
#include "lacip/op.h"
#endif

jmp_buf	error_recovery_context;
int error_recovery_context_exists = 0;

/*
 * The 'current_time' is updated at every heart beat.
 */
int current_time;

int heart_beat_flag = 0;

static void cycle_hb_list PROT((void));
extern struct object *command_giver, *obj_list_destruct;
extern struct object  *master_ob;
extern int num_user, d_flag;

extern INLINE void make_selectmasks();
extern int process_user_command();

struct object *current_heart_beat;

void call_heart_beat();
void init_user_conn(), init_sockets(),
    destruct2 PROT((struct object *));
RETSIGTYPE sigalrm_handler();
extern int t_flag;


/*
 * There are global variables that must be zeroed before any execution.
 * In case of errors, there will be a LONGJMP(), and the variables will
 * have to be cleared explicitely. They are normally maintained by the
 * code that use them.
 *
 * This routine must only be called from top level, not from inside
 * stack machine execution (as stack will be cleared).
 */
void clear_state()
{
  current_object = 0;
  command_giver = 0;
  current_prog = 0;
  error_recovery_context_exists = 1;
  reset_machine(0);	/* Pop down the stack. */
}


/* All destructed objects are moved into a sperate linked list,
 * and deallocated after program execution.  */

INLINE void remove_destructed_objects()
{
  struct object *ob, *next;

  for(ob=obj_list_destruct; ob; ob = next)
  {
    next = ob->next_all;
    destruct2(ob);
  }
  obj_list_destruct = 0;
}

/*
 * This is the backend. We will stay here for ever (almost).
 */
int eval_cost,max_eval_cost;
extern int game_is_being_shut_down;

RETSIGTYPE sig_hup(int arg)
{
  game_is_being_shut_down = 1;
}

void check_signals()
{
  extern unsigned long signals;
  unsigned long tmp;
  if(signals)
  {
    struct svalue *ret;
    tmp=signals;
    signals=0;
    push_number(tmp);
    APPLY_MASTER_OB(ret=,"signal",1);

    if(IS_ZERO(ret))
    {
      if(tmp &((1<<SIGFPE) |
	       (1<<SIGQUIT) |
	       (1<<SIGBUS) |
	       (1<<SIGSEGV)))
      {
	do_close_database();
	fflush(stdout);
	abort();
      }

      if(tmp & ((1<<SIGTERM) |
		(1<<SIGINT)))
      {
	do_close_database();
	fflush(stdout);
	exit(0);
      }

      if(tmp & (1<<SIGUSR1))
	APPLY_MASTER_OB((void),"shut",0);

      if(tmp & (1<<SIGUSR2))
	startshutdowngame();
    }
  }
}

#ifndef HAVE_UALARM
unsigned ualarm(register unsigned usecs,register unsigned reload);
#endif

extern void stralloc_gc();

void backend()
{
#ifdef LACIP
  extern int refused;
#endif
  extern fd_set readmask, writemask;
  extern int slow_shut_down_to_do;
  struct timeval timeout;
  int nb;

  signal(SIGHUP, sig_hup);
  signal(SIGALRM, sigalrm_handler);
  ualarm(HEARTBEAT_INTERVAL,0);

  setjmp(error_recovery_context);
  error_recovery_context_exists = 1;
  while(1){
    /*
     * The call of clear_state() should not really have to be done
     * once every loop. However, there seem to be holes where the
     * state is not consistent. If these holes are removed,
     * then the call of clear_state() can be moved to just before the
     * while() - statement. *sigh* /Lars
     */
    clear_state();
    eval_cost = 0;
    
    remove_destructed_objects();

    /*
     * shut down mud if game_is_being_shut_down is set.
     */
    if(game_is_being_shut_down)
      shutdowngame();
    if(slow_shut_down_to_do)
    {
      APPLY_MASTER_OB((void),"shut", 0);
      slow_shut_down_to_do = 0;
    }
    /*
     * select
     */
#ifdef LACIP
    if(refused) acceptmouse();
    refused=0;
    not_disp();
#endif
    make_selectmasks();
    if (heart_beat_flag) { /* use zero timeout if a heartbeat is pending. */
       timeout.tv_sec = 0; /* this should avoid problems with longjmp's too */
       timeout.tv_usec = 0;
    } else {
       /* not using infinite timeout so that we'll have insurance in the
          unlikely event a heartbeat happens between now and the select().
          Note that SIGALRMs (for heartbeats) do make select() drop through.
       */
       timeout.tv_sec = 0; 
       timeout.tv_usec = 100000;
    }

    stralloc_gc(0);

    nb = select(FD_SETSIZE,&readmask,&writemask,(fd_set *)0, &timeout);
#ifdef MALLOC_DEBUG
    check_sfltable();
#endif
    check_signals();
    /*
     * process I/O if necessary.
     */
    if(nb > 0){
      process_io();
    }
#ifdef MALLOC_DEBUG
    check_sfltable();
#endif
    /*
     * call heartbeat if appropriate.
     */
    if(heart_beat_flag){
      debug(512,("backend: HEARTBEAT\n"));
      call_heart_beat();
    }
    do_sync_database();
#ifdef MALLOC_DEBUG
    check_sfltable();
#endif
  }
}

/* 
 * Despite the name, this routine takes care of several things.
 * It will loop through all objects once every 10 minutes.
 *
 * If an object is found in a state of not having done reset, and the
 * delay to next reset has passed, then reset() will be done.
 *
 * If the object has a existed more than the time limit given for swapping,
 * then 'clean_up' will first be called in the object, after which it will
 * be swapped out if it still exists.
 *
 * There are some problems if the object self-destructs in clean_up, so
 * special care has to be taken of how the linked list is used.
*/
static void look_for_objects_to_swap()
{
  static int next_time;
  struct object  * VOLATILE ob;
  struct object * VOLATILE next_ob;
  jmp_buf save_error_recovery_context;
  int save_rec_exists;
  struct object *save;

  if(current_time < next_time)
    return;			/* Not time to look yet */
  next_time = current_time + 15 * 60; /* Next time is in 15 minutes */
  MEMCPY((char *) save_error_recovery_context,
	 (char *) error_recovery_context, sizeof error_recovery_context);
  save_rec_exists = error_recovery_context_exists;
  
  /* prevent error messages from objects going to whoever happens to
   * be this_user() (usually this isn't the user of the object).  */
  save = command_giver;
  command_giver = (struct object *)0;
  /* Objects object can be destructed, which means that
   * next object to investigate is saved in next_ob. If very unlucky,
   * that object can be destructed too. In that case, the loop is simply
   * restarted.  */
  ob=obj_list;
  next_ob=ob->next_all;
  if(setjmp(error_recovery_context))
  {
    extern void clear_state();
    ob=next_ob;
    clear_state();
    debug_message("Error in look_for_objects_to_swap.\n");
  }else{
    error_recovery_context_exists=1;
    while(ob)
    {
      next_ob=ob->next_all;
      /* Should this object have reset(1) called ? */
      if ((ob->next_reset < current_time) && !(ob->flags & O_RESET_STATE))
      {
	if(d_flag)
	{
	  fprintf(stderr, "RESET %s\n", ob->prog->name);
	}
	reset_object(ob, 1);
      }

      if(TIME_TO_CLEAN_UP > 0)
      {
	/* Has enough time passed, to give the object a chance
	 * to self-destruct ? Save the O_RESET_STATE, which will be cleared.
	 *
	 * Only call clean_up in objects that has defined such a function.
	 *
	 * Only if the clean_up returns a non-zero value, will it be called
	 * again.  */

	if (current_time - ob->time_of_ref > TIME_TO_CLEAN_UP
	    && (ob->flags & O_WILL_CLEAN_UP))
	{
	  int save_reset_state = ob->flags & O_RESET_STATE;
	  struct svalue *svp;
	      
	  if(d_flag)
	    fprintf(stderr, "clean up %s\n", ob->prog->name);

	  /* Supply a flag to the object that says if this program
	   * is inherited by other objects. Cloned objects might as well
	   * believe they are not inherited. Swapped objects will not
	   * have a ref count > 1 (and will have an invalid ob->prog
	   * pointer).  */

	  svp = apply("clean_up", ob, 0,1);
	  if(ob->flags & O_DESTRUCTED)
	    continue;
	  if(!svp || (svp->type == T_NUMBER && svp->u.number == 0))
	    ob->flags &= ~O_WILL_CLEAN_UP;
	  ob->flags |= save_reset_state;
	}
      }
      ob=next_ob;
    }
  }
    
  /* restore command_giver to its previous value */
  command_giver = save;
  MEMCPY((char *) error_recovery_context,(char *) save_error_recovery_context,
	 sizeof error_recovery_context);
  error_recovery_context_exists = save_rec_exists;
}

/* Call all heart_beat() functions in all objects.  Also call the next reset,
 * and the call out.
 * We do heart beats by moving each object done to the end of the heart beat
 * list before we call its function, and always using the item at the head
 * of the list as our function to call.  We keep calling heart beats until
 * a timeout or we have done num_heart_objs calls.  It is done this way so
 * that objects can delete heart beating objects from the list from within
 * their heart beat without truncating the current round of heart beats.
 *
 * Set command_giver to current_object if it is a living object. If the object
 * is shadowed, check the shadowed object if living. There is no need to save
 * the value of the command_giver, as the caller resets it to 0 anyway.  */

struct object *hb_list = 0; /* head */
struct object *hb_tail = 0; /* for sane wrap around */

static int num_hb_objs = 0;  /* so we know when to stop! */
static int num_hb_calls = 0; /* stats */
static float perc_hb_probes = 100.0; /* decaying avge of how many complete */

void call_heart_beat()
{
  struct object *ob;
  struct object *save_current_object = current_object;
  struct object *save_command_giver = command_giver;
  int num_done = 0;

  heart_beat_flag = 0;
  signal(SIGALRM, sigalrm_handler);
  ualarm(HEARTBEAT_INTERVAL,0);

  debug(256,("."));

  current_time = get_current_time();

  if(hb_list)
  {
    num_hb_calls++;
    while(hb_list && !heart_beat_flag  && (num_done < num_hb_objs))
    {
      if(!(hb_list->flags & O_HEART_BEAT) || hb_list->flags & O_DESTRUCTED)
      { 
        ob=hb_list;
	hb_list=ob->next_heart_beat;
        ob->next_heart_beat=0;
        continue;
      }
      num_done++;
      cycle_hb_list();
      ob = hb_tail; /* now at end */

      /* move ob to end of list, do ob */
      current_heart_beat = ob;
      if(ob->flags & O_ENABLE_COMMANDS)
	command_giver = ob;
      else
	command_giver = 0;

      eval_cost = 0;
      apply_lfun(LF_HEART_BEAT,ob,0,1);
    }
    if(num_hb_objs)
      perc_hb_probes = 100 * (float) num_done / num_hb_objs;
    else
      perc_hb_probes = 100.0;
  }
  current_object = save_current_object;
  current_heart_beat = 0;
  look_for_objects_to_swap();
  do_call_outs(); /* some things depend on this, even without users! */
  command_giver = save_command_giver;
}

/* Take the first object off the heart beat list, place it at the end
 */
static void cycle_hb_list()
{
  struct object *ob;

  if(!hb_list)
    fatal("Cycle heart beat list with empty list!");
  if(hb_list == hb_tail)
    return; /* 1 object on list */
  ob = hb_list;
  hb_list = hb_list->next_heart_beat;
  hb_tail->next_heart_beat = ob;
  hb_tail = ob;
  ob->next_heart_beat = 0;
}

int set_heart_beat(struct object *ob,int to)
{
  if(ob->flags & O_DESTRUCTED) return 0;

  if((!!to) ^ !(ob->flags & O_HEART_BEAT)) return 0; /* no change */
  if(to)
  {
    ob->flags |= O_HEART_BEAT;
    if(!ob->next_heart_beat) /* hasn't been unlinked yet */
    {
      ob->next_heart_beat = hb_list;
      hb_list = ob;
    }
    if(!hb_tail) hb_tail = ob;
    num_hb_objs++;
  }else{
    /* let call_heart_beat unlink it */
    ob->flags &= ~O_HEART_BEAT;
    num_hb_objs--;
  }
  return 1;
}

char *heart_beat_status(int verbose)
{
  char b[100];
  char buf[100];
  init_buf();
  my_strcat("\nHeart beat information:\n");
  my_strcat("-----------------------\n");
  sprintf(b,",Number of objects with heart beat: %d, starts: %d\n",
		num_hb_objs, num_hb_calls);
  my_strcat(b);

  sprintf(buf, "%.2f", perc_hb_probes);
  sprintf(b,"Percentage of HB calls completed last time: %s\n", buf);
  my_strcat(b);

  return free_buf();
}

static double load_av = 0.0;

void update_load_av()
{
  extern double consts[];
  static int last_time;
  int n;
  double c;
  static int acc = 0;

  acc++;
  if(current_time == last_time)
    return;
  n = current_time - last_time;
  if(n < NUM_CONSTS)
    c = consts[n];
  else
    c = exp(- n / 900.0);
  load_av = c * load_av + acc * (1 - c) / n;
  last_time = current_time;
  acc = 0;
}

static double compile_av = 0.0;

void update_compile_av(lines)
     int lines;
{
  extern double consts[];
  static int last_time;
  int n;
  double c;
  static int acc = 0;

  acc += lines;
  if(current_time == last_time)
    return;
  n = current_time - last_time;
  if(n < NUM_CONSTS)
    c = consts[n];
  else
    c = exp(- n / 900.0);
  compile_av = c * compile_av + acc * (1 - c) / n;
  last_time = current_time;
  acc = 0;
}

char *query_load_av()
{
  static char buff[100];

  sprintf(buff, "%.2f cmds/s, %.2f comp lines/s", load_av, compile_av);
  return(buff);
}
