#include "efuns.h"
#include <signal.h>
#include <sys/param.h>
#include "main.h"
#include "dynamic_buffer.h"
#include "simulate.h"
#include "stralloc.h"

/* fixa: simplify_path(), asynkron input */

#ifdef F_SLEEP
void f_sleep(int num_arg,struct svalue *argp)
{
  if(!batch_mode &&
     (!current_object->eff_user || current_object->eff_user!=master_ob->user))
    error("Function sleep() only supported in batchmode\n");
  sleep(sp->u.number);
  pop_stack();
}
#endif

#ifdef F_GETCHAR
void f_getchar(int num_arg,struct svalue *argp)
{
  if(!batch_mode &&
     (!current_object->eff_user || current_object->eff_user!=master_ob->user))
    error("Function getchar() only supported in batchmode\n");
  push_number(getchar());
}
#endif

#ifdef F_EXIT
void f_exit(int num_arg,struct svalue *argp)
{
  extern do_sync_database();
  if(!batch_mode &&
     (!current_object->eff_user || current_object->eff_user!=master_ob->user))
    error("Function exit() only supported in batchmode\n");
  do_sync_database();
  exit(sp->u.number);
}
#endif

#ifdef F_GETS
void f_gets(int num_arg,struct svalue *argp)
{
  char buffer[READ_FILE_MAX_SIZE],*c;
  if(!batch_mode &&
     (!current_object->eff_user || current_object->eff_user!=master_ob->user))
    error("Function gets() only supported in batchmode\n");
  c=fgets(buffer,READ_FILE_MAX_SIZE,stdin);
  if(!c)
  {
    push_number(0);
  }else{
    push_new_shared_string(c);
  }
}
#endif

#ifdef F_POPEN
void f_popen(int num_arg,struct svalue *argp)
{
  char foo[1000];
  void sig_child(int s);
  FILE *fooo;
  int c;

  if(!batch_mode &&
     (!current_object->eff_user || current_object->eff_user!=master_ob->user))
    error("Function popen() only supported in batchmode\n");
  init_buf();
  signal(SIGCHLD, SIG_DFL);
  fooo=popen(strptr(argp),"r");
  
  pop_stack();
  if(fooo)
  {
    do{
      c=fread(foo,1,sizeof(foo),fooo);
      my_binary_strcat(foo,c);
    }while(!feof(fooo));
    pclose(fooo);
    push_shared_string(free_buf());
  }else{
    push_zero();
  }
  signal(SIGCHLD, sig_child);
}
#endif

#ifdef F_PERROR
void f_perror(int num_arg,struct svalue *argp)
{
  if(!batch_mode &&
     (!current_object->eff_user || current_object->eff_user!=master_ob->user))
    error("Function perror() only supported in batchmode\n");
  fprintf(stderr,"%s",strptr(argp));
  pop_stack();
}
#endif

#ifdef F_GETENV
void f_getenv(int num_arg,struct svalue *argp)
{
  char *fuu;
  if(!batch_mode &&
     (!current_object->eff_user || current_object->eff_user!=master_ob->user))
    error("Function getenv() only supported in batchmode\n");
  fuu=getenv(strptr(argp));
  pop_stack();
  if(fuu)
  {
    push_new_shared_string(fuu);
  }else{
    push_zero();
  }
}
#endif

#ifdef F_CD
void f_cd(int num_arg,struct svalue *argp)
{
  int e;
  if(!batch_mode &&
     (!current_object->eff_user || current_object->eff_user!=master_ob->user))
    error("Function cd() only supported in batchmode\n");
  e=chdir(strptr(argp+0));
  pop_stack();
  push_number(e);
}
#endif

#ifdef F_GETCWD
void f_getcwd(int num_arg,struct svalue *argp)
{
  char *e;
  if(!batch_mode &&
     (!current_object->eff_user || current_object->eff_user!=master_ob->user))
    error("Function getcwd() only supported in batchmode\n");
#ifdef HAVE_GETCWD
  e=(char *)getcwd(0,1000); 
#else
  e=(char *)getwd((char *)malloc(MAXPATHLEN+1));
  if(!e)
    fatal("Couldn't fetch current path.\n");
#endif
  push_new_shared_string(e);
  free(e);
}
#endif

#ifdef F_PUTENV
void f_putenv(int num_arg,struct svalue *argp)
{
  if(!batch_mode &&
     (!current_object->eff_user || current_object->eff_user!=master_ob->user))
    error("Function putenv() only supported in batchmode\n");
  putenv(strptr(argp));
  pop_stack();
}
#endif


#ifdef F_FORK
void f_fork(int num_arg,struct svalue *argp)
{
  if(!batch_mode &&
     (!current_object->eff_user || current_object->eff_user!=master_ob->user))
    error("Function fork() only supported in batchmode\n");
  push_number(fork());
}
#endif

#ifdef F_SETUGID
void f_setugid(int num_arg,struct svalue *argp)
{
  int a,b;
  if(!batch_mode &&
     (!current_object->eff_user || current_object->eff_user!=master_ob->user))
    error("setuguid() may only be called by Root.\n");
  a=argp[0].u.number;
  b=argp[1].u.number;
  pop_stack();
  pop_stack();
  b=setgid((gid_t)b); /* gid_t == long */
  a=setuid((uid_t)a); /* uid_t == long */
  push_number(!!(a+b));
}
#endif
