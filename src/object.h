#ifndef OBJECT_H
#define OBJECT_H
#include "interpret.h"

/*
 * Definition of an object.
 * If the object is inherited, then it must not be destructed !
 *
 * The reset is used as follows:
 * 0: There is an error in the reset() in this object. Never call it again.
 * 1: Normal state.
 * 2 or higher: This is an interactive player, that has not given any commands
 *		for a number of reset periods.
 */
/* Soon I'll need more flags /Profezzorn */

#define O_HEART_BEAT		0x01  /* Does it have an heart beat ? */
#define O_ENABLE_COMMANDS	0x04  /* Can it execute commands ? */
#define O_DESTRUCTED		0x08  /* Is it destructed ? */
#define O_APPROVED		0x10  /* Is std/object.c inherited ? */
#define O_RESET_STATE		0x20 /* Object in a 'reset':ed state ? */
#define O_WILL_CLEAN_UP		0x40 /* clean_up will be called next time */
#define O_APPROVES		0x100 /* Does inheriting this object set approved? */
#define O_REF_CYCLE             0x200

#define O_EXPUNGE               0x800
#define O_EFUN_SOCKET           0x1000

struct object
{
  int ref;			/* Reference count. */
  unsigned short flags;		/* Bits or'ed together from above */
  int next_reset;		/* Time of next reset of this object */
  int time_of_ref;		/* Time when last referenced. */
#ifdef DEBUG
  int extra_ref;		/* Used to check ref count. */
#endif
  struct program *prog;
  struct object *next_all;
  struct object *next_inv;
  struct object *next_heart_beat;
  struct object *contains;
  struct object *super;		/* Which object surround us ? */
  struct object *next_obj_hashed; /*shiver*/
  struct sentences *sentences;
  char *user;                   /* What wizard defined this object */
  char *eff_user;	        /* Used for permissions */
  int clone_number;		/* The clone number of this object */
  char *obj_index;		/* */
  int cpu,created;
  union storage_union variables[1];	/* All variables to this program */
  /* The variables MUST come last in the struct */
};

struct object *get_empty_object PROT((struct program *));
struct program *find_program PROT((char *));
struct program *find_program2 PROT((char *));
struct object *current_object, *command_giver;

extern struct object *obj_list;
extern struct object *obj_list_destruct;

struct value;
void remove_destructed_objects(),
    move_object PROT((struct object *, struct object *)),
    tell_object PROT((char *)),
    add_ref PROT((struct object *, char *)),
    reference_prog PROT((struct program *, char *));


void vtell_object(char *, ...) ATTRIBUTE((format (printf, 1,2)));
void remove_all_objects();

#ifndef DEBUG
#define add_ref(ob,str) ((ob)->ref++)
#define free_object(ob,str) { if(((ob)->ref--<=1)) real_free_object(ob,str); }
void real_free_object PROT((struct object *, char *));
#else
void free_object PROT((struct object *, char *));
#endif

#endif
