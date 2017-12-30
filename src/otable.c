#include "global.h"

#include "interpret.h"
#include "object.h"
#include "exec.h"
#include "main.h"
#include "otable.h"
#include "dynamic_buffer.h"
#include "hash.h"
#include "simulate.h"

/*
 * program name hash table.  program names are unique, so no special
 * problems - like stralloc.c.  For non-unique hashed names, we need
 * a better package (if we want to be able to get at them all) - we
 * cant e them to the head of the hash chain, for example.
 *
 * Note: if you change an program name, you must remove it and reenter it.
 */

/*
 * hash table - list of pointers to heads of program chains.
 * Each program in chain has a pointer, next_hash, to the next program.
 * OTABLE_SIZE is in config.h, and should be a prime, probably between
 * 100 and 1000.  You can have a quite small table and still get very good
 * performance!  Our database is 8Meg; we use about 500.
 */

struct program *obj_table[OTABLE_SIZE];

/*
 * program hash function, ripped off from stralloc.c.
 */

unsigned int ObjHash(char *s)
{
  return hashstr(s, 100) % OTABLE_SIZE;
}

#if 0
void check_sfltable()
{
  int e;
  struct program *p,*next;
  for(e=0;e<OTABLE_SIZE;e++)
  {
    for(p=obj_table[e];p;p=next)
    {
      next=p->next_hash;
      if(p->ref==1)
      {
	if(ObjHash(p->name)!=e)
	{
	  fatal("Program hashed in wrong place.\n",0,0,0,0,0,0,0,0,0);
	}
      }
    }
  }

}
#endif
/*
 * Looks for obj in table, moves it to head.
 */

static int obj_searches = 0, obj_probes = 0, objs_found = 0;

static struct program * find_obj_n(s)
char * s;
{
	struct program * curr, *prev;

	int h = ObjHash(s);

	curr = obj_table[h];
	prev = 0;

	obj_searches++;

	while (curr) {
	    obj_probes++;
	    if (!strcmp(curr->name, s)) { /* found it */
		if (prev) { /* not at head of list */
		    prev->next_hash = curr->next_hash;
		    curr->next_hash = obj_table[h];
		    obj_table[h] = curr;
		    }
		objs_found++;
		return(curr);	/* pointer to program */
		}
	    prev = curr;
	    curr = curr->next_hash;
	    }
	
	return(0); /* not found */
}

/*
 * Add an program to the table - can't have duplicate names.
 */

static int objs_in_table = 0;

void enter_program_hash(struct program *ob)
{
	struct program * s;
	int h = ObjHash(ob->name);

	s = find_obj_n(ob->name);
	if (s)
	{
	  if (s != ob)
	    fatal("Duplicate program \"%s\" in program hash table",ob->name);
	  else
	    fatal("Entering program \"%s\" twice in program table",ob->name);
	}
        if (ob->next_hash)
	    fatal("program \"%s\" not found in program table but next link not null",
			ob->name);
	ob->next_hash = obj_table[h];
	obj_table[h] = ob;
	objs_in_table++;
	return;
}

/*
 * Remove an program from the table - generally called when it
 * is removed from the next_all list - i.e. in destruct.
 */

void remove_program_hash(struct program *ob)
{
	struct program * s;
	int h = ObjHash(ob->name);

	s = find_obj_n(ob->name);

	if (s != ob)
		fatal("Remove program \"%s\": found a different program!",
			ob->name);
	
	obj_table[h] = ob->next_hash;
	ob->next_hash = 0;
	objs_in_table--;
	return;
}

/*
 * Lookup an program in the hash table; if it isn't there, return null.
 * This is only different to find_program_n in that it collects different
 * stats; more finds are actually done than the user ever asks for.
 */

static int user_obj_lookups = 0, user_obj_found = 0;

struct program * lookup_program_hash(s)
char * s;
{
	struct program * ob = find_obj_n(s);
	user_obj_lookups++;
	if (ob) user_obj_found++;
	return(ob);
}

/*
 * Print stats, returns the total size of the program table.  All programs
 * are in table, so their size is included as well.
 */



char *show_otable_status(int verbose)
{
  char sbuf[200];
  char b[200];
  init_buf();

  if (verbose)
  {
    my_strcat("\nProgram name hash table status:\n");
    my_strcat("------------------------------\n");
    sprintf(sbuf, "%.2f", objs_in_table / (float) OTABLE_SIZE);
    sprintf(b,"Average hash chain length	           %s\n", sbuf);
    my_strcat(b);
    sprintf(sbuf, "%.2f", (float)obj_probes / obj_searches);
    sprintf(b,"Searches/average search length       %d (%s)\n",
		    obj_searches, sbuf);
    my_strcat(b);
    sprintf(b,"External lookups succeeded (succeed) %d (%d)\n",
		    user_obj_lookups, user_obj_found);
    my_strcat(b);
  }
  sprintf(b,"hash table overhead\t\t\t %8d\n",
		(int)(OTABLE_SIZE * sizeof(struct program *)));
  my_strcat(b);
  return free_buf();

/*    return (OTABLE_SIZE * sizeof(struct program *) +
	    objs_in_table * sizeof(struct program)); */
}
