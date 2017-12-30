#include "efuns.h"
#include "patchlevel.h"

void f_version(int num_arg,struct svalue *argp)
{
  char buff[20];
  sprintf(buff, "%s%02d", GAME_VERSION, PATCH_LEVEL);
  push_new_shared_string(buff);
}
