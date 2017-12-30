#ifndef __STDLIB_H
#define __STDLIB_H

inherit "/inherit/stdlib";
#define qsort(X,Y) { sort_array((X),(Y),this_object()); }

#endif
