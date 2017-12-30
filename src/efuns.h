/* efuns.h:  this file should be included by any .c file that wants to
   define f_* efuns to be called by eval_instruction() in interpret.c
*/


#include "global.h" /* must be included before the #ifdef TIMES */

#include <sys/types.h>
#ifdef TIMES
#include <sys/times.h>
#endif
#include <sys/stat.h>
#if !defined(hp68k) && !defined(vax)
#include <time.h>
#endif /* !hp68k */
#if defined(sun) || defined(apollo) || defined(__386BSD__) || defined(hp68k) || defined(vax)
#include <sys/time.h>
#endif /* sun, etc */

#include <sys/resource.h>
#include <sys/ioctl.h>
#ifdef HAVE_FNCTL_H
#include <fcntl.h>
#endif
#include <netdb.h>
#include <errno.h>
#include <ctype.h>

#include <setjmp.h>

#include "interpret.h"
#include "object.h"
#include "exec.h"
#include "efun_protos.h"
#include "lang.h"
#include "sent.h"

extern int d_flage;
extern int tracedepth;
extern int current_time;
extern char *last_verb;
extern struct object *previous_ob;
extern struct object *master_ob;
extern struct svalue *expected_stack;
extern struct object *current_heart_beat, *current_interactive;
extern struct svalue catch_value;	/* Used to throw an error to a catch */
extern struct control_stack *csp;	/* Points to last element pushed */

extern struct svalue *sp;
extern int eval_cost;
extern struct control_stack control_stack[MAX_TRACE];

void do_trace PROT((char *, char *, char *));
int strpref PROT((char *, char *));
void pop_push_conditional(int f);
void push_float(float f);

#define check_argument(X,Y,Z) if (argp[X].type != Y) bad_arg(X+1,Z)
