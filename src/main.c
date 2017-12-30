#include "global.h"
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <ctype.h>
#if defined(sun)
#include <alloca.h>
#endif
#ifdef STDC_HEADERS
#include <stdarg.h>
#endif

#include "simulate.h"
#include "interpret.h"
#include "object.h"
#include "exec.h"
#include "lex.h"
#include "main.h"
#include "array.h"
#include "stralloc.h"
#include "backend.h"
#include "simul_efun.h"

#ifdef LACIP
#include "lacip/op.h"
#endif

#ifdef GETRUSAGE_THROUGH_PROCFS
#include <sys/procfs.h>
#include <sys/fcntl.h>
int proc_fd;

void open_proc_fd()
{
  char proc_name[20];

  sprintf(proc_name, "/proc/%05ld", (long)getpid());
  proc_fd = open(proc_name, O_RDONLY);
  if(proc_fd < 0)
  {
    fprintf(stderr, "Couldn't open %s\n", proc_name);
  }
}
#endif

extern char *prog;
extern int current_line;

extern int lasdebug;
int d_flag = 0;	/* Run with debug */
int t_flag = 0;	/* Disable heart beat and reset */
int comp_flag = 0; /* Trace compilations */
int T_flag = 0;
#ifdef WARN
int W_flag = 0;
#endif
int batch_mode=0; /* he he */

void init_stdin_handler();
extern int stdin_closed,stdinisatty,stdin_is_sock;

#ifdef YYDEBUG
extern int yydebug;
#endif

char *reserved_area=NULL;	/* reserved for malloc() */
struct svalue const_empty_string;

char *hostname;

double consts[NUM_CONSTS];

extern jmp_buf error_recovery_context;
extern int error_recovery_context_exists;

extern struct object *master_ob;
extern int eval_cost;


unsigned long signals;
RETSIGTYPE sig_lpc(int s)
{
  signal(s,sig_lpc);
  signals|=1<<s;
}

RETSIGTYPE sig_child(int s)
{
  wait(&s);
  signal(s,sig_child);
}

/* don't forget to change LF_MAX in exec.h */
char *initial_lfun_list[]=
{
  "catch_tell",
  "heart_beat",
  "init",
  "exit",
} ;

extern void init_stacks();
extern void init_sockets();

int main(int argc,char ** argv,char **env)
{
  struct svalue *ret;
  extern int game_is_being_shut_down;
  extern int current_time;
  int i, new_mudlib = 0;
  char *p;

#ifdef GETRUSAGE_THROUGH_PROCFS
  open_proc_fd();
#endif


#ifdef MALLOC_gc
  gc_init();
#endif
  SET_STR(&const_empty_string,make_shared_string(""));

  lfuns=(char **)xalloc(sizeof(char **)*NELEM(initial_lfun_list));
  for(i=0;i<NELEM(initial_lfun_list);i++)
    lfuns[i]=make_shared_string(initial_lfun_list[i]);
  num_lfuns=i;

  init_stacks();
  p=(char *)malloc(100);
  gethostname(p,100);
  p[100-1]=0;
  hostname=make_shared_string(p);
  free(p);
    
#ifdef debugmalloc
  malloc_debug(2);
#endif

  /*
   * Check that the definition of EXTRACT_UCHAR() is correct.
   */
  p = (char *)&i;
  *p = -10;
  if (EXTRACT_UCHAR(p) != 0x100 - 10) {
    fprintf(stderr, "Bad definition of EXTRACT_UCHAR() in machine.h.\n");
    exit(1);
  }
  my_srand(get_current_time());
  current_time = get_current_time();
  for (i=0; i < NELEM(consts); i++)
    consts[i] = exp(- i / 900.0);
  init_num_args();
  reset_machine(1);

  /*
   * The flags are parsed twice !
   * The first time, we only search for the -m flag, which specifies
   * another mudlib, and the D-flags, so that they will be available
   * when compiling master.c.
   */
  for (i=1; i < argc; i++)
  {
    if (argv[i][0] != '-') continue;

    switch(argv[i][1])
    {
    case 'D':
      if (argv[i][2])
      {
	/* Amylaar : allow flags to be passed down to
	   the LPC preprocessor */
	struct lpc_predef_s *tmp;
	  
	tmp = (struct lpc_predef_s *) malloc(sizeof(struct lpc_predef_s));
	if (!tmp) fatal("malloc failed");
	tmp->flag = argv[i]+2;
	tmp->next = lpc_predefs;
	lpc_predefs = tmp;
	continue;
      }
      fprintf(stderr, "Illegal flag syntax: %s\n", argv[i]);
      exit(1);
      break;

    case 'm':
      if (chdir(argv[i]+2) == -1)
      {
	fprintf(stderr, "Bad mudlib directory: %s\n", argv[i]+2);
	exit(1);
      }
      new_mudlib = 1;
      break;

    case 'I':
      stdin_is_sock=1;
      break;
      
    case 'C':
      batch_mode=1;
      i=argc;			/* the rest of the flags are 'mud only' */
    }
  }

  if(!batch_mode)
  {
    if (!new_mudlib && chdir(MUD_LIB) == -1)
    {
      fprintf(stderr, "Bad mudlib directory: %s\n", MUD_LIB);
      exit(1);
    }
  }

  if (RESERVED_SIZE > 0 && !batch_mode)
    reserved_area = malloc(RESERVED_SIZE);

  if(stdin_is_sock)
  {
    stdin_closed=1;
    stdinisatty=0;
  }else{
    init_stdin_handler();
  }

  if (setjmp(error_recovery_context))
  {
    clear_state();
    fprintf(stderr,"Fatal failiure when loading master object!\n");
  }else{
    error_recovery_context_exists = 1;
    assert_master_ob_loaded();
  }

  if((ret=apply_master_ob("get_cached_functions",0)))
  {
    if(!IS_ZERO(ret))
    {
      if(ret->type==T_POINTER)
      {
	int t;
	lfuns=(char **)realloc((char *)lfuns,sizeof(char **)*(ret->u.vec->size+num_lfuns));
	if(!lfuns)
	{
	  fprintf(stderr,"Couldn't allocate space for cached functions.\n");
	  exit(10);
	}
	for(i=0;i<ret->u.vec->size;i++)
	{
	  if(ret->u.vec->item[i].type!=T_STRING)
	  {
	    fprintf(stderr,"get_cached_function() returned an array containing non-strings.\n");
	    exit(10);
	  }
	  for(t=0;t<num_lfuns;t++)
	    if(lfuns[t]==strptr(ret->u.vec->item+i))
	      break;
	  if(t==num_lfuns)
	  {
	    lfuns[num_lfuns]=copy_shared_string(strptr(ret->u.vec->item+i));
	    num_lfuns++;
	  }
	}
      }else{
	fprintf(stderr,"Warning: get_cached_functions() in master.c doesn't return an array or zero.\n");
      }
    }
  }

  error_recovery_context_exists = 0;

  if(batch_mode)
  {
    push_new_shared_string(BINDIR);
    push_new_shared_string(MUD_LIB);
    set_inc_list(apply_master_ob("define_include_dirs", 2));
  }else{
    set_inc_list(apply_master_ob("define_include_dirs", 0));
  }

  init_sockets();		/* initialize efun sockets           */

  signal(SIGCHLD, sig_child);
  signal(SIGINT, sig_lpc);
  signal(SIGQUIT, sig_lpc);
  signal(SIGTERM, sig_lpc);
  signal(SIGUSR1, sig_lpc);
  signal(SIGUSR2, sig_lpc);
  signal(SIGPIPE, SIG_IGN);
#if 0
#ifndef DEBUG
  if(-1!=find_function("signal",master_ob->prog))
  {
    signal(SIGSEGV, sig_lpc);
    signal(SIGFPE, sig_lpc);
    signal(SIGBUS, sig_lpc);
  }
#endif
#endif

  for (i=1; i < argc; i++)
  {
    if (argv[i][0] != '-')
    {
      fprintf(stderr, "Bad argument %s\n", argv[i]);
      exit(1);
    } else {
      /*
       * Look at flags. -m and -o has already been tested.
       */
      switch(argv[i][1])
      {
      case 'f':
	eval_cost=0;
	push_new_shared_string(argv[i]+2);
	APPLY_MASTER_OB((void),"flag", 1);
	if (game_is_being_shut_down) {
	  fprintf(stderr, "Shutdown by master object.\n");
	  exit(0);
	}
	continue;

      case 'C':
      {
	struct vector *fo,*env_fo;
	int e;
	batch_mode=1;
	eval_cost=0;
	push_new_shared_string(argv[i]+2);
	i++;
	fo=allocate_array_no_init(argc-i,0);
	for(e=0;i<argc;i++,e++)
	{
	  SET_STR(fo->item+e,make_shared_string(argv[i]));
	}
	for(e=0;env[e];e++);

	env_fo=allocate_array_no_init(e,0);
	for(e=0;env[e];e++)
	{
	  SET_STR(env_fo->item+e,make_shared_string(env[e]));
	}
	  
	push_vector(fo);
	push_vector(env_fo);
	fo->ref--;
	env_fo->ref--;
	(void)apply_master_ob("batch", 3);
	if (game_is_being_shut_down) {
	  exit(0);
	}
	i=argc;			/* the rest of the flags are 'mud only' */
      }
      case 'I':
	push_number(batch_mode);
	(void)apply_master_ob("stdin_is_sock",1);
	continue;

      case 'D':
	continue;
      case 'N':
	continue;
      case 'm':
	continue;
#define OPTION(flag,variable) case flag: if(isdigit(argv[i][2])) variable=atoi(argv[i]+2); else variable++; break

        OPTION('l',lasdebug);
        OPTION('d',d_flag);
        OPTION('c',comp_flag);
        OPTION('t',t_flag);
        OPTION('T',T_flag);
#ifdef WARN
        OPTION('W',W_flag);
#endif
#ifdef YYDEBUG
        OPTION('y',yydebug);
#endif
      default:
	fprintf(stderr, "Unknown flag: %s\n", argv[i]);
	exit(1);
      }
    }
  }
    
  if (game_is_being_shut_down)
    exit(1);
#ifdef OPCPROF
  check_cost_for_instr(-1);
#endif
  backend();

  return 0;
}

char *string_copy(char *str)
{
  char *p;

  p = xalloc(strlen(str)+1);
  (void)strcpy(p, str);
  return p;
}


void debug_message_va(char *a,va_list args)
{
  static FILE *fp = NULL;
  char deb[100];

  /* nej tack, jag k|r */
  if(batch_mode)
    return;

  if (fp == NULL)
  {
    sprintf(deb,"%s.debug.log",hostname);
    fp = fopen(deb, "w");
    if (fp == NULL)
    {
      perror(deb);
      abort();
    }
  }
  (void)VFPRINTF(fp, a, args);
  (void)fflush(fp);
}

void debug_message(char *a, ...)
{
  va_list args;
  va_start(args,a);
  debug_message_va(a,args);
  va_end(args);
}

#ifdef WARN
void warn(int w_level,char *a, ...)
{
  va_list args;
  if(W_flag<w_level) return;
  va_start(args,a);
  VFPRINTF(stderr,a,args);
  va_end(args);
}
#else
void warn(int w, char *a, ...) {}
#endif
void debug_message_svalue(struct svalue *v)
{
  if (v == 0)
  {
    debug_message("<NULL>");
    return;
  }
  switch(v->type)
  {
  case T_NUMBER:
    debug_message("%d", v->u.number);
    return;
  case T_STRING:
    debug_message("\"%s\"", strptr(v));
    return;
  case T_OBJECT:
    debug_message("OBJ(%s)", v->u.ob->prog->name);
    return;
  case T_LVALUE:
    debug_message("LVALUE");
    return;
  default:
    debug_message("<INVALID>");
    return;
  }
}

#ifdef malloc
#undef malloc
#endif

int slow_shut_down_to_do = 0;

char *xalloc(int size)
{
  extern void do_gc(int);
  char *p;
  static int going_to_exit;

  if (going_to_exit) exit(3);
  if (size == 0) fatal("Tried to allocate 0 bytes.\n");

  if((p = malloc(size))) return p;

  do_gc(10000);
  if((p = malloc(size))) return p;
  
  if (reserved_area)
  {
    free(reserved_area);
    p = "Temporary out of MEMORY. Freeing reserve.\n";
    write(1, p, strlen(p));
    reserved_area=0;
    slow_shut_down_to_do++;
    if((p = malloc(size))) return p;
  }
  going_to_exit = 1;
  p = "Totally out of MEMORY.\n";
  write(1, p, strlen(p));
  (void)dump_trace(0);
  exit(2);
  return 0;
}

