#include "global.h"
#ifdef OPCPROF
#include "lang.h"
#include "lex.h"
#include "instrs.h"

#if 0
int get_time_since_last_time()
{
  static long u_secs,u_usec;
  static long s_secs,s_usec;
  struct rusage rus;
  int t;
  if (getrusage(RUSAGE_SELF, &rus) < 0) return 0;
  t= (rus.ru_utime.tv_sec-u_secs)*1000000 + rus.ru_utime.tv_usec-u_usec;
  t+=(rus.ru_stime.tv_sec-s_secs)*1000000 + rus.ru_stime.tv_usec-s_usec;

  u_secs= rus.ru_utime.tv_sec;
  u_usec= rus.ru_utime.tv_usec;
  s_secs= rus.ru_stime.tv_sec;
  s_usec= rus.ru_stime.tv_usec

  return t;
}
#else
long get_time_since_last_time()
{
  static long last_time;
  long t,tt;
  t=clock();
  tt=t-last_time;
  last_time=t;
  return tt;
}
#endif

void add_compiled(int instr)
{
  if(instr>=0 && instr<F_MAX_OPCODE-F_OFFSET)
    instrs[instr].compiled++;
}

int get_cost_since_last_time()
{
  extern int eval_cost;
  static long last_cost;
  int t;
  t=eval_cost-last_cost;
  last_cost=eval_cost;
  return t;
}

void check_cost_for_instr(int instr)
{
  static int last_instr;
  if(instr>=0 && instr<=F_MAX_INSTR-F_OFFSET)
  {
    if(last_instr>=0 && last_instr<=F_MAX_INSTR-F_OFFSET)
    {
      instrs[last_instr].used++;
      instrs[last_instr].avg_time+=get_time_since_last_time();
      instrs[last_instr].avg_cost+=get_cost_since_last_time();
    }
    last_instr=instr;
  }else{
    last_instr=-1;
  }
}

int dump_opcode_info()
{
  int e;
  FILE *f;
  f=fopen("opcode_statistics","w");
  if(!f) return;
  fprintf(f,"#nr eval_cost runned compiled average_cost average_time name\n");
  for(e=0;e<F_MAX_OPCODE-F_OFFSET;e++)
  {
    fprintf(f,"%3d %4d %6d %6d %10.4f %10.4f %s\n",
           e,
	   instrs[e].eval_cost,
	   instrs[e].used,
	   instrs[e].compiled,
	    instrs[e].used ? 
	   ((float)instrs[e].avg_cost)/((float)instrs[e].used) : 0.0,
	    instrs[e].used ?
	   ((float)instrs[e].avg_time)/((float)instrs[e].used) : 0.0,
	    get_f_name(e+F_OFFSET)) ;
  }
  fclose(f);
}
#endif
