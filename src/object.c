#include "global.h"
#include <sys/types.h>
#include <sys/stat.h>

#include "simulate.h"
#include "interpret.h"
#include "object.h"
#include "sent.h"
#include "exec.h"
#include "main.h"
#include "stralloc.h"
#include "array.h"


extern int d_flag;
extern int total_num_prog_blocks, total_prog_block_size;

int tot_alloc_object, tot_alloc_object_size;

/*
 * Send a message to an object.
 */
void vtell_object(char *str,...)
{
  va_list args;
  char buf[10000];
  struct object *save_command_giver;

  if(command_giver && !(command_giver->flags && O_DESTRUCTED))
  {
    if(current_object->flags & O_DESTRUCTED)
      return;

    va_start(args,str);
    VSPRINTF(buf,str,args);
    va_end(args);

    if(strlen(buf)>9500)
      error("Too long message\n");
    save_command_giver = command_giver;
    push_new_shared_string(buf);
    apply_lfun(LF_CATCH_TELL,command_giver,1,0);
    command_giver = save_command_giver;
  }
}

#ifdef DEBUG
void free_object(struct object *ob, char *from)
{
  ob->ref--;
  if (d_flag > 1)
    fprintf(stderr,"Subtr ref to ob %s: %d (%s)\n",
	    ob->prog?ob->prog->name:"DESTRUCTED",
	   ob->ref, from);
#else
void real_free_object(struct object *ob,char *from)
{
#endif
  if(ob->ref==1 && (ob->flags & O_EXPUNGE) && !(ob->flags & O_DESTRUCTED))
  {    
    destruct_object(ob);
    return;
  }
  if (ob->ref > 0) return;

  if (d_flag) fprintf(stderr,"free_object: %s.\n",
		      ob->prog?ob->prog->name:"DESTRUCTED");

  if (!(ob->flags & O_DESTRUCTED))
  {
    /* This is fatal, and should never happen. */
    fatal("FATAL: Object %p %s ref count 0, but not destructed (from %s).\n",
	  ob, 
	  ob->prog?ob->prog->name:"DESTRUCTED",
	  from);
  }
  tot_alloc_object_size -= sizeof (struct object);

  tot_alloc_object--;
  free((char *)ob);
}

#ifdef DEBUG
void add_ref(struct object *ob,char *from)
{
  ob->ref++;
  if (d_flag > 1)
    fprintf(stderr,"Add reference to object %s: %d (%s)\n", ob->prog->name,
	    ob->ref, from);
}
#endif

/*
 * Allocate an empty object, and set all variables to 0. Note that a
 * 'struct object' already has space for one variable. So, if no variables
 * are needed, we allocate a space that is smaller than 'struct object'. This
 * unused (last) part must of course (and will not) be referenced.
 */
struct object *get_empty_object(struct program *p)
{
  extern int new_clone_number,current_time;
  struct object *ob;
  int size = sizeof (struct object) + (p->num_variables- 1) * sizeof(union storage_union);
  int e;

  tot_alloc_object++;
  tot_alloc_object_size += size;
  ob = (struct object *)calloc(size,1);
  ob->ref = 1;
  ob->clone_number=(++new_clone_number);
  ob->flags = p->flags & ( O_APPROVED | O_WILL_CLEAN_UP | O_REF_CYCLE) ;
  ob->created =current_time;
  ob->prog = p;
  reference_prog (p, "new_object");
  ob->next_all = obj_list;
  obj_list = ob;

  for(e=0;e<p->num_variables;e++)
  {
    if(p->variable_names[e].rttype==T_ANY)
    {
      SET_TO_ZERO(*(struct svalue *)(ob->variables+e));
      e++;
    }else{
      ob->variables[e].number=0;
    }
  }
  return ob;
}

void remove_all_objects()
{
  struct object *ob;

  while(obj_list)
  {
    ob = obj_list;
    destruct_object(ob);
    if (ob == obj_list) break;
  }
  remove_destructed_objects();
}

void reference_prog (struct program *progp,char *from)
{
  progp->ref++;
  if (d_flag)
    printf("reference_prog: %s ref %d (%s)\n",
	   progp->name, progp->ref, from);
}

/*
 * Decrement reference count for a program. If it is 0, then free the prgram.
 * The flag free_sub_strings tells if the propgram plus all used strings
 * should be freed.
 */
void free_prog(struct program *progp, int free_sub_strings)
{
  progp->ref--;
  if (d_flag)
    printf("free_prog: %s (%d)\n", progp->name,progp->ref);
  if (progp->ref > 0)
    return;
  if (progp->ref < 0)
    fatal("Negative ref count for prog ref.\n");
  total_prog_block_size -= progp->total_size;
  total_num_prog_blocks -= 1;
  if (free_sub_strings) {
    int i;

    /* Free all function names. */
    for (i=0; i < progp->num_functions; i++)
      if (progp->functions[i].name)
	free_string(progp->functions[i].name);

    /* Free all strings */
    for (i=0; i < progp->num_strings; i++)
      free_string(progp->strings[i]);

    /* Free all variable names */
    for (i=0; i < progp->num_variables; i++)
      free_string(progp->variable_names[i].name);

    /* Free all inherited objects */
    /* Remember that the first index is just ourselves */
    for (i=1; i < progp->num_inherited; i++)
      free_prog(progp->inherit[i].prog, 1);

    /* Free all switch mappings */
    for (i=0; i < progp->num_switch_mappings; i++)
      free_vector(progp->switch_mappings[i]);

    /* Free all constants */
    for (i=0; i < progp->num_constants; i++)
      free_svalue(progp->constants+i);

    /* Free the name of the program */
    free_string(progp->name);
  }
  free((char *)progp);
}

void reset_object(struct object *ob,int arg)
{
  extern int current_time;
  extern void push_shared_string(char *);

  /* Be sure to update time first ! */
  ob->next_reset = current_time + TIME_TO_RESET/2 +
    random_number(TIME_TO_RESET/2);

  if(!arg)
  {
    apply("__INIT", ob, 0,1);
    if(!(ob->flags & O_DESTRUCTED)) apply("create", ob, 0,1);
  } else {
    apply("reset", ob, 0,1);
  }
  ob->flags |= O_RESET_STATE;
}

