#ifndef SIMUL_EFUN_H
#define SIMUL_EFUN_H

#include "interpret.h"

#define SIMUL_EFUN_ON 1

struct simul_efun_entry
{
  short flags;
  char *name;
  struct svalue fun;
};

int find_simul_efun PROT((char *));
void free_simul_efun();

extern struct simul_efun_entry *simul_efuns;
extern int num_simul_efuns;
#endif
