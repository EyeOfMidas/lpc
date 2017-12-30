#include <stdio.h>
#include "patchlevel.h"
#include "global.h"

int main(int argv, char ** argc)
{
  switch(argv)
  {
  case 1:
    printf("#define PATCH_LEVEL %d\n",PATCH_LEVEL+1);
    break;

  case 2:
    printf("lpc%s%02d\n", GAME_VERSION, PATCH_LEVEL);
    break;
  }
  exit(0);
}
