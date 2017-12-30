#include "efuns.h"
#include "main.h"
#include "simulate.h"
#include "simul_efun.h"
#include "stralloc.h"

/* void add_simul_efun(string,function|void); */  /* name, function */

struct simul_efun_entry *simul_efuns=0;
int num_simul_efuns=0;
static int simul_efun_space=0;

void f_add_simul_efun(int num_arg,struct svalue *argp)
{
  int e;
  struct svalue *ret;
  push_object(current_object);
  push_svalue(argp);
  if(num_arg==2) push_svalue(argp+1);
  APPLY_MASTER_OB(ret=,"valid_add_simul_efun",num_arg+1);
  if(IS_ZERO(ret))
  {
    error("Invalid add_simul_efun.\n");
  }else{
    for(e=0;e<num_simul_efuns;e++)
    {
      if(simul_efuns[e].name==strptr(argp+0))
      {
        if(num_arg==2)
        {
          free_svalue(&(simul_efuns[e].fun));
          simul_efuns[e].fun=argp[1];
          simul_efuns[e].flags|=SIMUL_EFUN_ON;
          sp--;
        }else{
          simul_efuns[e].flags&=~SIMUL_EFUN_ON;
        }
        pop_stack();
        return;
      }
    }
    if(num_arg==2)
    {
      if(simul_efun_space<=num_simul_efuns)
      {
        if(!simul_efuns)
        {
          simul_efun_space=42;
          simul_efuns=(struct simul_efun_entry *)
	    xalloc(sizeof(struct simul_efun_entry)*simul_efun_space);
        }else{
          simul_efun_space*=2;
          simul_efuns=(struct simul_efun_entry *)
	    realloc((char *)simul_efuns,
		    sizeof(struct simul_efun_entry)*simul_efun_space);
          if(!simul_efuns) fatal("OUT OF MEMORY.\n");        
        }
      }
      simul_efuns[num_simul_efuns].fun=argp[1];
      simul_efuns[num_simul_efuns].name=strptr(argp+0);
      simul_efuns[num_simul_efuns].flags=SIMUL_EFUN_ON;
      num_simul_efuns++;
      sp-=2;
    }else{
      pop_stack();
    }
  }
}

/* requires shared string argument */
int find_simul_efun(char *s)
{
  int e;
  for(e=0;e<num_simul_efuns;e++)
  {
    if(simul_efuns[e].name==s)
    {
      if(simul_efuns[e].flags & SIMUL_EFUN_ON) return e;
      return -1;
    }
  }
  return -1;
}

void free_simul_efun()
{
  int e;
  for(e=0;e<num_simul_efuns;e++)
  {
    free_svalue(&(simul_efuns[e].fun));
    free_string(simul_efuns[e].name);
  }
  if(simul_efuns) free((char *)simul_efuns);
  simul_efun_space=0;
  num_simul_efuns=0;
}

