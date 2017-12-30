/*
	math.c: this file contains the math efunctions called from
	inside eval_instruction() in interpret.c.
    -- coded by Truilkan 93/02/21
*/

#include <math.h>
#include "efuns.h"
#include "simulate.h"

#define pI 3.1415926535896932384626433832795080

#ifdef MATH

void f_cos(int num_arg,struct svalue *argp)
{
  argp[0].u.fnum= cos(argp[0].u.fnum);
}

void f_sin(int num_arg,struct svalue *argp)
{
  argp[0].u.fnum=sin(argp[0].u.fnum);
}

void f_tan(int num_arg,struct svalue *argp)
{
  float f;

  f=(argp->u.fnum-pI/2) / pI;
  if(f==floor(f+0.5))
  {
    error("Impossible tangent.\n");
    return;
  }
  argp[0].u.fnum= tan(argp[0].u.fnum);
}

void f_asin(int num_arg, struct svalue *argp)
{
  argp[0].u.fnum= asin(argp[0].u.fnum);
}

void f_acos(int num_arg, struct svalue *argp)
{
  argp[0].u.fnum= acos(argp[0].u.fnum);
}

void f_atan(int num_arg, struct svalue *argp)
{
  float f;
  f=(argp->u.fnum-pI/2) / pI;
  if(f==floor(f+0.5))
  {
    error("Impossible tangent.\n");
    return;
  }
  argp[0].u.fnum = atan(argp[0].u.fnum);
}

void f_sqrt(int num_arg, struct svalue *argp)
{
  if(argp[0].type==T_NUMBER)
  {
    int a,b,c,q;
    q=sp->u.number;
    for(a=0,b=0x10000;a+1<b;)
    {
      c=(a+b)/2;
      if(c*c>q) b=c; else a=c;
    }
    sp->u.number=a;
  }else{
    if (argp[0].u.fnum< 0.0)
    {
      error("math: sqrt(x) with (x < 0.0)\n");
      return;
    }
    argp[0].u.fnum = sqrt(argp[0].u.fnum);
  }
}

void f_log(int num_arg,struct svalue *argp)
{
  if (argp[0].u.fnum <= 0.0)
  {
    error("math: log(x) with (x <= 0.0)\n");
    return;
  }
  argp[0].u.fnum = log(argp[0].u.fnum);
}

void f_pow(int num_arg,struct svalue *argp)
{
  argp[0].u.fnum = pow(argp[0].u.fnum,argp[1].u.fnum);
  pop_stack();
}

void f_exp(int num_arg,struct svalue *argp)
{
  argp[0].u.fnum = exp(argp[0].u.fnum);
}

void f_floor(int num_arg,struct svalue *argp)
{
  argp[0].u.fnum = floor(argp[0].u.fnum);
}

void f_ceil(int num_arg,struct svalue *argp)
{
  argp[0].u.fnum = ceil(argp[0].u.fnum);
}

#endif
