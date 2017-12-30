#include "global.h"
#include "interpret.h"
#include "object.h"
#include "exec.h"
#include "main.h"
#include "dynamic_buffer.h"
#include "hash.h"
#include "simulate.h"


/*
 * Object name hash table.  Object names are unique, so no special
 * problems - like stralloc.c.  For non-unique hashed names, we need
 * a better package (if we want to be able to get at them all) - we
 * cant e them to the head of the hash chain, for example.
 *
 * Note: if you change an object name, you must remove it and reenter it.
 */

/*
 * hash table - list of pointers to heads of object chains.
 * Each object in chain has a pointer, next_obj_hash, to the next object.
 * OHASH_SIZE is in config.h, and should be a prime, probably between
 * 100 and 1000.  You can have a quite small table and still get very good
 * performance!  Our database is 8Meg; we use about 500.
 */

static struct object *obj_hash[OHASH_SIZE];

unsigned int Obj_Hash(char *s)
{
  return hashstr(s, 100) % OHASH_SIZE;
}

/*
 * Looks for obj in table, moves it to head.
 */

static int hash_searches = 0, hash_probes = 0, hashes_found = 0;

static struct object * find_hash_n(char *s)
{
	struct object * curr, *prev;

	int h = Obj_Hash(s);

	curr = obj_hash[h];
	prev = 0;

	hash_searches++;

	while (curr) {
	    hash_probes++;
	    if (!strcmp(curr->obj_index, s)) { /* found it */
		if (prev) { /* not at head of list */
		    prev->next_obj_hashed = curr->next_obj_hashed;
		    curr->next_obj_hashed = obj_hash[h];
		    obj_hash[h] = curr;
		    }
		hashes_found++;
		return(curr);	/* pointer to object */
		}
	    prev = curr;
	    curr = curr->next_obj_hashed;
	    }
	
	return(0); /* not found */
}

/*
 * Add an object to the table - can't have duplicate names.
 */

static int objs_in_table = 0;

void enter_my_object_hash(struct object *ob)
{
  struct object * s;
  int h = Obj_Hash(ob->obj_index);

  s = find_hash_n(ob->obj_index);
  if (s) {
    if (s != ob)
      error("Duplicate object \"%s\" in object hash table\n",
	    ob->obj_index);
    else
      error("Entering object \"%s\" twice in object table\n",
	    ob->obj_index);
  }
  if (ob->next_obj_hashed)
    fatal("Object \"%s\" not found in object table but next link not null",
	  ob->obj_index);
  ob->next_obj_hashed = obj_hash[h];
  obj_hash[h] = ob;
  objs_in_table++;
  return;
}

/*
 * Remove an object from the table - generally called when it
 * is removed from the next_all list - i.e. in destruct.
 */

void remove_my_object_hash(struct object *ob)
{
  struct object * s;
  int h = Obj_Hash(ob->obj_index);

  s = find_hash_n(ob->obj_index);

  if (s != ob)
    fatal("Remove object \"%s\": found a different object!",ob->obj_index);
	
  obj_hash[h] = ob->next_obj_hashed;
  ob->next_obj_hashed = 0;
  objs_in_table--;
  return;
}

/*
 * Lookup an object in the hash table; if it isn't there, return null.
 * This is only different to find_object_n in that it collects different
 * stats; more finds are actually done than the user ever asks for.
 */

static int user_hash_lookups = 0, user_hash_found = 0;

struct object * lookup_my_object_hash(s)
char * s;
{
	struct object * ob = find_hash_n(s);
	user_hash_lookups++;
	if (ob) user_hash_found++;
	return(ob);
}

/*
 * Print stats, returns the total size of the object table.  All objects
 * are in table, so their size is included as well.
 */

char *show_ohash_status(int verbose)
{
  char sbuf[200];
  char b[200];
  init_buf();
    if (verbose)
     {
	my_strcat("\nObject index hash table status:\n");
	my_strcat("------------------------------\n");
	sprintf(sbuf, "%.2f", objs_in_table / (float) OHASH_SIZE);
	sprintf(b,"Average hash chain length	           %s\n", sbuf);
	my_strcat(b);
	sprintf(sbuf, "%.2f", (float)hash_probes / hash_searches);
	sprintf(b,"Searches/average search length       %d (%s)\n",
		    hash_searches, sbuf);
	my_strcat(b);
	sprintf(b,"External lookups succeeded (succeed) %d (%d)\n",
		    user_hash_lookups, user_hash_found);
	my_strcat(b);
    }
    sprintf(b,"hash table overhead\t\t\t %8d\n",
	    (int)(OHASH_SIZE * sizeof(struct object *)));
    my_strcat(b);

  return free_buf();

/*    return (OHASH_SIZE * sizeof(struct object *) +
	    objs_in_table * sizeof(struct object)); */
}
