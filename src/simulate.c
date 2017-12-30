#include "global.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <setjmp.h>
#include <errno.h>
#if defined(sun)
#include <alloca.h>
#endif
          
/* unistd.h defines _POSIX_VERSION on POSIX.1 systems.  */
#if defined(DIRENT) || defined(_POSIX_VERSION)
#include <dirent.h>
#define NLENGTH(dirent) (strlen((dirent)->d_name))
#else /* not (DIRENT or _POSIX_VERSION) */
#define dirent direct
#define NLENGTH(dirent) ((dirent)->d_namlen)
#ifdef SYSNDIR
#include <sys/ndir.h>
#endif /* SYSNDIR */
#ifdef SYSDIR
#include <sys/dir.h>
#endif /* SYSDIR */
#ifdef NDIR
#include <ndir.h>
#endif /* NDIR */
#endif /* not (DIRENT or _POSIX_VERSION) */

#include "array.h"
#include "simulate.h"
#include "interpret.h"
#include "object.h"
#include "sent.h"
#include "exec.h"
#include "comm.h"
#include "main.h"
#include "dbase_efuns.h"
#include "stralloc.h"
#include "call_out.h"
#include "lex.h"
#include "backend.h"
#include "dynamic_buffer.h"
#include "otable.h"
#include "socket_efuns.h"
#include "simul_efun.h"

extern int errno;
extern int comp_flag;

char *inherit_file;

#ifdef _DCC
#define strcasecmp stricmp
#endif

#ifndef HAVE_UNISTD_H
extern int readlink PROT((char *, char *, int));
extern int symlink PROT((char *, char *));
extern int fchmod PROT((int, int));     
#endif

#ifdef AMIGA_PORT
#define lstat stat
#endif

char *last_verb;

extern int special_parse PROT((char *)),
    legal_path PROT((char *));
extern void pop_stack();

char **lfuns=NULL;
int num_lfuns=0;

void remove_all_players(), end_new_file(),
    print_svalue PROT((struct svalue *)),
    destruct2();

extern int d_flag,current_time;
extern struct svalue *sp;
struct object *obj_list, *obj_list_destruct, *master_ob;
struct object *current_object;      /* The object interpreting a function. */
struct object *command_giver;       /* Where the current command came from. */
int num_parse_error;		/* Number of errors in the parser. */
int new_clone_number;


struct variable *find_status(char *str,int must_find)
{
  int i;
  if((str=findstring(str)))
  {
    for (i=0; i < current_object->prog->num_variables; i++)
    {
      if (current_object->prog->variable_names[i].name==str)
	return &current_object->prog->variable_names[i];
    }
  }
  if (!must_find)
    return 0;
  error("--Status %s not found in prog for %s\n", str,
	current_object->prog->name);
  return 0;
}

/*
 * Give the correct uid and euid to a created object.
 */
void give_uid_to_object(struct object *ob)
{
  struct svalue *ret;
  char *creator_name;

  if (!current_object || !current_object->user)
  {
    /*
     * Only temporary.
     */
    ob->user = make_shared_string("NONAME");
    ob->eff_user = 0;
  }

  if (master_ob == 0)
  {
    ret=apply("get_root_uid",ob,0,1);
    if (ret == 0 || ret->type != T_STRING)
    {
      fprintf(stderr, "get_root_uid() in secure/master.c does not work\n");
      exit(1);
    }
    if(ob->user) free_string(ob->user);
    ob->user = copy_shared_string(strptr(ret));
    if(ob->eff_user) free_string(ob->eff_user);
    ob->eff_user=copy_shared_string(ob->user);
    master_ob=ob;
    add_ref(ob,"master object");
    return;
  }

  /*
   * Ask master.c who the creator of this object is.
   */
  push_shared_string(copy_shared_string(ob->prog->name));
  APPLY_MASTER_OB(ret=,"creator_file", 1);
  if (!ret)
    error("No function 'creator_file' in master.c!\n");
  if (ret->type != T_STRING)
  {
    /* This can be the case for objects in /ftp and /open. */
    destruct_object(ob);
    error("Illegal object to load.\n");
  }
  creator_name = strptr(ret);

  if(ob->user) free_string(ob->user);
  ob->user = copy_shared_string(creator_name);
  if(ob->eff_user) free_string(ob->eff_user);
  ob->eff_user = copy_shared_string(creator_name);
  return;
}

#define IS_POSTFIX(X,Y) (strlen(X)>=strlen(Y) && !strcmp((X)+strlen(X)-strlen(Y),(Y)))


/*
 * Load an object definition from file. If the object wants to inherit
 * from an object that is not loaded, discard all, load the inherited object,
 * and reload again.
 *
 */

struct program *load_object(char *lname,struct inherit_check *inherit_err)
{
  FILE *f;
  extern int total_lines;
  extern int approved_object;

  extern struct program *prog;
  extern char *current_file;
  struct stat c_st;
  int name_length;
  char *name,*real_name;
  struct inherit_check i_check,*cnt;

  if (current_object && current_object->eff_user == 0)
    error("Can't load objects when no effective user.\n");

  /* Truncate possible .c in the object name. */
  /* Remove leading '/' if any. */
  if(!batch_mode)
  {
    while(lname[0] == '/') lname++;
    name_length = strlen(lname);
    name=(char *)alloca(name_length+1);
    strcpy(name, lname);
  }else{
    real_name=combine_path(0,lname);
    name_length=strlen(real_name);
    name=(char *)alloca(name_length+1);
    strcpy(name,real_name);
    free(real_name);
  }

  real_name=(char *)alloca(name_length+strlen(LPC_SUFFIX)+6);
  if (IS_POSTFIX(name,LPC_SUFFIX))
  {
    (void)strcpy(real_name, name);
    if(batch_mode)
    {
      name[name_length-2] = '\0';
      name_length -= 2;
    }
  }else{
    (void)strcpy(real_name, name);
    if(!batch_mode)
      strcat(real_name,LPC_SUFFIX);
  }

  /*
   * First check that the c-file exists.
   */
  if (stat(real_name, &c_st) == -1)
  {
    if(batch_mode)
    {
      strcat(real_name,".c");
      if (stat(real_name, &c_st) == -1) return 0;
    }else{
      return 0;
    }
  }
  for(cnt=inherit_err;cnt;cnt=cnt->next)
  {
    if (!strcmp(name, cnt->name))
    {
      /* hmm, we might loose the memory of inherit_file here */
      error("Illegal to inherit self.\n");
    }
  }

  /*
   * Check if it's a legal name.
   */
  if (!legal_path(real_name))
  {
    fprintf(stderr, "Illegal pathname: %s\n", real_name);
    error("Illegal path name.\n");
    return 0;
  }

  if (comp_flag) fprintf(stderr, " compiling %s ...", real_name);
  f = fopen(real_name, "r");
  if (f == 0)
  {
    perror(real_name);
    error("Couldn't read the file.\n");
  }

  start_new_file(f);
  current_file = string_copy(name); /* This one is freed below */
  compile_file();
  end_new_file();
  if (comp_flag) fprintf(stderr, " done\n");
  update_compile_av(total_lines);
  total_lines = 0;
  (void)fclose(f);
  free(current_file);
  current_file = 0;
  /* Sorry, can't handle objects without programs yet. */
  if (inherit_file == 0 && (num_parse_error > 0 || prog == 0))
  {
    if (prog) free_prog(prog, 1);
    if(num_parse_error == 0 && prog == 0)
      error("No program in object !\n");
    error("Error in loading object\n");
  }
  /*
   * This is an iterative process. If this object wants to inherit an
   * unloaded object, then discard current object, load the object to be
   * inherited and reload the current object again. The global variable
   * "inherit_file" will be set by lang.y to point to a file name.
   */
  if (inherit_file)
  {
    char *tmp = inherit_file;

    if (prog)
    {
      free_prog(prog, 1);
      prog = 0;
    }

    i_check.name=name;
    i_check.next=inherit_err;

    inherit_file = 0;

    if(!load_object(tmp,&i_check))
    {
      error("Couldn't find program '%s' for inherit.\n",tmp);
    }
    free(tmp);
    return load_object(name,0);
  }
  /*
   * Can we approve of this object ?
   */
  prog->flags=ref_cycle;
  if (approved_object || batch_mode)
  {
    prog->flags |= O_APPROVED;
    prog->flags |= O_APPROVES;
  }else{
    struct svalue *ret;
    if(!master_ob)
    {
      prog->flags |= O_APPROVED;
    }else{
      if(!(prog->flags & O_APPROVED))
      {
	push_shared_string(copy_shared_string(prog->name));
	APPLY_MASTER_OB(ret=,"query_approve",1);
	if(ret)
	{
	  if(ret->type==T_NUMBER && ret->u.number>0)
	  {
	    prog->flags |= O_APPROVED;
	    if(ret->u.number>1) prog->flags |= O_APPROVES;
	  }
	}
      }
    }
  }

  if(!(prog->flags & O_APPROVED))
  {
    free_prog(prog,1);
    error("Object not approved %s\n",name);
  }

  enter_program_hash(prog);  /* add name to fast object lookup table */

  if (function_exists_in_prog("clean_up",prog))
    prog->flags |= O_WILL_CLEAN_UP;

  if (d_flag > 1 && prog)
    debug_message("--%s loaded\n", name);
  return prog;
}

struct program *make_object(char *str)
{
  extern int total_lines;
  extern int approved_object;
	
  extern struct program *prog;
  extern char *current_file;
  char name[200];
  struct inherit_check i_check;
	
  if (current_object && current_object->eff_user == 0)
    error("Can't create objects when no effective user.\n");
  if(batch_mode)
    sprintf(name,"/tmp/%s/%d",current_object->eff_user,++new_clone_number);
  else
    sprintf(name,"tmp/%s/%d",current_object->eff_user,++new_clone_number);

  if (comp_flag) fprintf(stderr, " compiling %s ...", name);

  start_new_string(str,strlen(str));
  current_file = string_copy(name); /* This one is freed below */
  compile_file();
  end_new_file();
  if (comp_flag)
    fprintf(stderr, " done\n");
  update_compile_av(total_lines);
  total_lines = 0;
  free(current_file);
  current_file = 0;
  /* Sorry, can't handle objects without programs yet. */
  if (inherit_file == 0 && (num_parse_error > 0 || prog == 0)) {
    if (prog)
      free_prog(prog, 1);
    if (num_parse_error == 0 && prog == 0)
      error("No program in object !\n");
    error("Error in loading object\n");
  }
  /*
   * This is an iterative process. If this object wants to inherit an
   * unloaded object, then discard current object, load the object to be
   * inherited and reload the current object again. The global variable
   * "inherit_file" will be set by lang.y to point to a file name.
   */
  if (inherit_file)
  {
    char *tmp = inherit_file;

    if (prog)
    {
      free_prog(prog, 1);
      prog = 0;
    }

    i_check.name=name;
    i_check.next=0;

    inherit_file = 0;
    if(!load_object(tmp,&i_check))
    {
      error("Couldn't find program '%s' for inherit.\n",tmp);
    }
    free(tmp);
    return make_object(str);
  }

  prog->flags=ref_cycle;
  /*
   * Can we approve of this object ?
   */
  if (approved_object || batch_mode)
  {
    prog->flags |= O_APPROVED;
    prog->flags |= O_APPROVES;
  }else{
    struct svalue *ret;
    push_shared_string(copy_shared_string(prog->name));
    APPLY_MASTER_OB(ret=,"query_approve",1);
    if(ret)
    {
      if(ret->type==T_NUMBER && ret->u.number>0)
      {
	prog->flags |= O_APPROVED;
	if(ret->u.number>1) prog->flags |= O_APPROVES;
      }
    }
  }

  if(!(prog->flags & O_APPROVED))
  {
    free_prog(prog,1);
    error("Object not approved %s\n",name);
  }
  enter_program_hash(prog);  /* add name to fast object lookup table */

  if (function_exists_in_prog("clean_up",prog))
    prog->flags |= O_WILL_CLEAN_UP;

  if (d_flag > 1 && prog)
    debug_message("--%s loaded\n", name);
  return prog;
}


/*
 * Save the command_giver, because reset() in the new object might change
 * it.
 */
struct object *clone_object(char *str1,int nameit)
{
  struct object *new_ob;
  struct program *p;

  if(current_object && current_object->eff_user == 0)
    error("Illegal to call clone_object() with effective user 0\n");

  p=find_program(str1);
  if(!p)
  {
    struct svalue *ret;
    push_new_shared_string(str1);
    APPLY_MASTER_OB(ret=,"compile_object",1);
    if(!ret || ret->type!=T_OBJECT)
    {
      error("Couldn't clone object '%s' does file exist?\n",str1);
    }
    new_ob=ret->u.ob;
    if(nameit)
    {
      new_ob->obj_index=make_shared_string(str1);
      enter_my_object_hash(new_ob);
    }
    return new_ob;
  }

  new_ob = get_empty_object(p);

  if(nameit)
  {
    new_ob->obj_index=make_shared_string(str1);
    enter_my_object_hash(new_ob);
  }
  give_uid_to_object(new_ob);

  reset_object(new_ob, 0); 
  /* Never know what can happen ! :-( */
  if (new_ob->flags & O_DESTRUCTED) return 0;
  return new_ob;
}

struct object *environment(struct svalue *arg)
{
  struct object *ob = current_object;

  if (arg && arg->type == T_OBJECT)
    ob = arg->u.ob;
  if (ob == 0 || ob->super == 0 || (ob->flags & O_DESTRUCTED))
    return 0;
  if (ob->flags & O_DESTRUCTED)
    error("environment() of destructed object.\n");
  return ob->super;
}

int tot_alloc_sentence=0;

void free_sentence(struct sentences *s,int slot)
{
  free_string(s->sent[slot].match);
  free_svalue(&(s->sent[slot].function));
  s->sent[slot].match=NULL;
  s->used_slots--;
}

#ifdef DEBUG
void test_sentence(struct sentences *s)
{
  int e,num,num2;
  char *pos;
  if(s->used_slots>s->slots || s->slots>tot_alloc_sentence)
    fatal("Not a valid sentence structure.\n");
    
  num2=0;
  for(e=0;e<s->slots;e++)
  {
    if(!s->sent[e].match) continue;
    if(debug_findstring(s->sent[e].match)!=s->sent[e].match)
      fatal("Sentence not in shared string table.\n");
      
    for(num=0,pos=s->sent[e].match-1;(pos=STRCHR(pos+1,'%'));num++);
    if(num!=s->sent[e].num_arg)
    {
      fatal("Error in sentence.\n");
    }
    num2++;
  }
  if(num2!=s->used_slots)
    fatal("Error in sentence.\n");
}
#define TEST_SENTENCE(X) test_sentence(X)
#else
#define TEST_SENTENCE(X)
#endif

void free_sentences(struct sentences *s)
{
  int e;
  for(e=0;e<s->slots;e++)
    if(s->sent[e].match)
      free_sentence(s,e);
  free((char *)s);
}

struct sentences *cleanup_sentences(struct sentences *s)
{
  int e;
  struct sentences *s2;
  int size;
  TEST_SENTENCE(s);
  if(s->used_slots*3>s->slots || s->slots<=6) return s;
  size=s->slots/2;
  tot_alloc_sentence-=s->slots-size;
  s2=(struct sentences *)malloc(sizeof(struct sentences)+
		sizeof(struct sentence)*(size-1));
  s2->slots=size;
  s2->used_slots=0;
  for(e=0;e<s->slots;e++)
  {
    if(!s->sent[e].match) continue;
    s2->sent[s2->used_slots]=s->sent[e];
    s2->used_slots++;
  }
  for(e=s2->used_slots;e<s2->slots;e++)
    s2->sent[e].match=NULL;
  free((char *)s);
  TEST_SENTENCE(s2);
  return s2;
}

struct sentences *remove_sent_for(struct sentences *s,struct object *o)
{
  int e;
  TEST_SENTENCE(s);
  for(e=0;e<s->slots;e++)
  {
    if(!s->sent[e].match) continue;
    if(!s->sent[e].function.u.ob ||
       s->sent[e].function.u.ob==o ||
       (s->sent[e].function.u.ob->flags & O_DESTRUCTED))
      free_sentence(s,e);
  }
  return cleanup_sentences(s);
}

void remove_sent(struct object *ob,struct object *p)
{
  if(!p->sentences)
    return;
  p->sentences=remove_sent_for(p->sentences,ob);
}

int command_modification=0;

struct sentences *add_sentence(struct sentences *s,
		char *match,
		struct svalue *fun,
		int pri,
		int args)
{
  int unused,e;
  command_modification++;
  TEST_SENTENCE(s);
  if(s->used_slots==s->slots)
  {
    s=(struct sentences *)realloc((char *)s,sizeof(struct sentences)+
		sizeof(struct sentence)*(s->slots*2-1));
    tot_alloc_sentence+=s->slots;
    for(e=s->slots;e<s->slots*2;e++) s->sent[e].match=NULL;
    s->slots*=2;
  }

  TEST_SENTENCE(s);
  unused=-1;
  for(e=0;e<s->slots;e++)
  {
    if(s->sent[e].match==NULL) { unused=e; continue; }
    if(s->sent[e].pri<=pri) break;
  }
  if(unused==-1)
  {
    for(unused=e+1;s->sent[unused].match;unused++);
    for(;unused>e;unused--) s->sent[unused]=s->sent[unused-1];
  }else{
    e--;
    for(;unused<e;unused++) s->sent[unused]=s->sent[unused+1];
  }
  s->sent[e].pri=pri;
  s->sent[e].num_arg=args;
  s->sent[e].match=match;
  assign_svalue_no_free(&(s->sent[e].function),fun);
  s->used_slots++;
  TEST_SENTENCE(s);
  return s;
}


void add_action(struct svalue *s,struct svalue *fun,int priority)
{
  struct object *ob;
  char *pos;
  int num,e;
  char *str;

  str=copy_shared_string(strptr(s));
  
  if (current_object->flags & O_DESTRUCTED) return;
  ob = current_object;
  if (command_giver == 0 || (command_giver->flags & O_DESTRUCTED))
    return;

  if (ob != command_giver && ob->super != command_giver &&
      ob->super != command_giver->super && ob != command_giver->super)
    error("add_action from object that was not present.\n");
  
  for(num=0,pos=str-1;(pos=STRCHR(pos+1,'%'));num++);
  if(!command_giver->sentences)
  {
    tot_alloc_sentence+=6;
    command_giver->sentences=(struct sentences *)malloc(
	sizeof(struct sentences)+sizeof(struct sentence)*5);
    command_giver->sentences->slots=6;
    command_giver->sentences->used_slots=0;
    for(e=0;e<6;e++)
      command_giver->sentences->sent[e].match=NULL;
  }
  command_giver->sentences=add_sentence(command_giver->sentences,
	str,fun,priority,num);
}


/*
 * Find the sentence for a command from the player.
 * Return success status.
 */
int player_parser(char *cmd)
{
  int e,d;
  struct sentences *se;
  struct sentence *s;

  char *p;
  char verb_copy[100];
  char buff[1100];
  struct svalue *ss,*save_sp;
  extern struct svalue *sp;

  se=command_giver->sentences;
  if(!se) return 0;
  TEST_SENTENCE(se);

  /* skip leading spaces */
  while(*cmd==' ') cmd++;

  strncpy(buff,cmd,1090);

  if (d_flag > 1)
    debug_message("cmd [%s]: %s\n", command_giver->prog->name, buff);

  /* strip trailing spaces. */
  for (p = buff + strlen(buff) - 1; p >= buff && *p==' '; p--)
    *p = '\0';

  if (buff[0] == '\0') return 0;

  save_sp=sp;
  for (e=0;e<se->slots;e++)
  {
    struct svalue *ret;
    int tr;

    s=se->sent+e;
    if(!s->match) continue;

    if(s->function.u.ob->flags & O_DESTRUCTED)
    {
      free_sentence(se,e);
      continue;
    }

    while(save_sp!=sp) pop_stack();
    save_sp=sp;
    if(s->num_arg)
    {
      /* Gotta allocate some phony lvalues to call sscanf() */
      /* Puke, puke, puke */

      ss=sp;
      for(d=0;d<s->num_arg;d++) push_zero();

      push_new_shared_string(buff); /* move outside looop */
      push_shared_string(copy_shared_string(s->match));
	
      for(d=0;d<s->num_arg;d++)
      {
	extern struct lvalue *lsp;
	extern struct lvalue lvalues[LVALUES];
	lsp++;
	if(lsp>=lvalues+LVALUES)
	  error("Lvalue stack overflow.\n");
        ss++;
	lsp->ptr.sval=ss;
	lsp->rttype=T_ANY;
	lsp->type=LVALUE_LOCAL;
        sp++;
        sp->type = T_LVALUE;
        sp->u.lvalue = lsp;
      }
	
      tr=inter_sscanf(s->num_arg+2);
      if(!tr) continue;
      pop_n_elems(s->num_arg*2+2-tr);
    }else{
      if(strcmp(buff,s->match)) continue;
      tr=0;
    }
    /*
     * Now we have found a special sentence !
     */
      
    if (strlen(s->match) >= sizeof verb_copy)
    {
      strncpy(verb_copy, s->match, sizeof verb_copy - 1);
      verb_copy[sizeof verb_copy -1] = '\0';
    }else{
      strcpy(verb_copy, s->match); /* what did the user type anyway? */
    }
    last_verb = verb_copy;
    command_modification=0;
    ret = apply_lambda(&s->function, tr,0);
    last_verb = 0;

    if(!ret)
      error("Function for '%s' not found.\n",s->match);

    if (ret->type == T_NUMBER && ret->u.number == 0)
    {
      if(command_modification)
        error("Command list changed in action that returned 0.\n");
      continue;
    }

/* this used to update wizlist 
    if (s && command_object->user && command_giver->interactive)
    {
      command_object->user->score++;
    }
*/

    while(save_sp!=sp) pop_stack();
    command_modification++;
    return 1;
  }
  while(save_sp!=sp) pop_stack();
  command_modification++;
  return 0;
}


struct vector *query_actions(struct object *foo,struct object *bar)
{
  int num,e;
  struct sentences *s;
  struct vector *v,*r;

  if(!(foo->flags & O_ENABLE_COMMANDS)) return 0;
  if(!(s=foo->sentences))
  {
    return allocate_array(0);
  }
  if(bar)
  {
    for(num=e=0;e<s->slots;e++)
    {
      if(!s->sent[e].match) continue;
      if(s->sent[e].function.u.ob==bar) num++;
    }
  }else{
    num=s->used_slots;
  }
  v=allocate_array_no_init(num,0);

  num=0;
  for(num=e=0;e<s->slots;e++)
  {
    if(!s->sent[e].match) continue;
    if(s->sent[e].function.u.ob->flags & O_DESTRUCTED)
    {
      free_sentence(s,e);
      continue;
    }

    if(bar && s->sent[e].function.u.ob!=bar) continue;
    r=allocate_array_no_init(3,0);
    SET_STR(r->item+0,copy_shared_string(s->sent[e].match));
    assign_svalue_no_free(r->item+1,&(s->sent[e].function));
    r->item[2].type=T_NUMBER;
    r->item[2].subtype=NUMBER_NUMBER;
    r->item[2].u.number=s->sent[e].pri;
    v->item[num].u.vec=r;
    v->item[num].type=T_POINTER;
    num++;
  }
  return v;
}

/*
 * Take a user command and parse it.
 * The command can also come from a NPC.
 * Beware that 'str' can be modified and extended !
 */
int parse_command(char *str,struct object * ob)
{
  struct object *save = command_giver;
  int res;
  /* disallow users to issue commands containing ansi escape codes */
#ifdef NO_ANSI
  char *c;

  for (c = str; *c; c++) {
    if (*c == 27) {
      *c = ' ';			/* replace ESC with ' ' */
    }
  }
#endif
  command_giver = ob;
  res = player_parser(str);
  command_giver = save;
  return(res);
}

/*
 * Execute a command for an object. Copy the command into a
 * new buffer, because 'parse_command()' can modify the command.
 * If the object is not current object, static functions will not
 * be executed. This will prevent forcing players to do illegal things.
 *
 * Return cost of the command executed if success (> 0).
 * When failure, return 0.
 */
int command_for_object(char *str,struct object *ob)
{
  char buff[1000];
  extern int eval_cost;
  int save_eval_cost = eval_cost - 1000;
  struct svalue *t;

  if (strlen(str) > (sizeof(buff) - 1))
    error("Too long command.\n");

  if(ob != current_object)
  {
    push_shared_string(copy_shared_string(str));
    t=apply("query_valid_force",ob,1,0);
    if(IS_ZERO(t)) return 0; 
  }

  if (ob->flags & O_DESTRUCTED)
    return 0;
  strncpy(buff, str, sizeof buff);
  buff[sizeof buff - 1] = '\0';
  if (parse_command(buff, ob))
    return eval_cost - save_eval_cost;
  else
    return 0;
}

/*
 * Remove an object. It is first moved into the destruct list, and
 * not really destructed until later. (see destruct2()).
 */
void destruct_object(struct object *ob)
{
  struct object *super;
  struct object **pp;
  int removed;

  command_modification++;
  if (ob->flags & O_DESTRUCTED)
    return;
  remove_object_from_stack(ob);

  /*
   * check if object has an efun socket referencing it for
   * a callback. if so, close the efun socket.
   */
  if (ob->flags & O_EFUN_SOCKET)
  {
    close_referencing_sockets(ob);
  }

  if (d_flag > 1)
    debug_message("Destruct object %s (ref %d)\n", ob->prog->name, ob->ref);
  super = ob->super;

  while(ob->contains)
  {
    struct object *obj2;
    obj2 = ob->contains;
    push_object(ob->contains);
    /* An error here will not leave destruct() in an inconsistent
     * stage.
     */
    APPLY_MASTER_OB((void),"destruct_environment_of",1);
    if (obj2->super==ob)
      destruct_object(obj2);
  }
 
  set_heart_beat(ob, 0);
  /*
   * Remove us out of this current room (if any).
   * Remove all sentences defined by this object from all objects here.
   */
  if (ob->super) {
    if (ob->super->flags & O_ENABLE_COMMANDS)
      remove_sent(ob, ob->super);
    for (pp = &ob->super->contains; *pp;) {
      if ((*pp)->flags & O_ENABLE_COMMANDS)
	remove_sent(ob, *pp);
      if (*pp != ob)
	pp = &(*pp)->next_inv;
      else
	*pp = (*pp)->next_inv;
    }
  }
  if(ob->sentences)
    free_sentences(ob->sentences);
  ob->sentences=NULL;
  /*
   * Now remove us out of the list of all objects.
   * This must be done last, because an error in the above code would
   * halt execution.
   */
  removed = 0;
  for (pp = &obj_list; *pp; pp = &(*pp)->next_all) {
    if (*pp != ob)
      continue;
    *pp = (*pp)->next_all;
    removed = 1;
    break;
  }
  if (!removed)
    fatal("Failed to delete object.\n");
  if(ob->obj_index)
  {
    remove_my_object_hash(ob);
    free_string(ob->obj_index);
  }
  if(ob==command_giver) command_giver=0;
  ob->super = 0;
  ob->next_inv = 0;
  ob->contains = 0;
  ob->flags &= ~O_ENABLE_COMMANDS;
  ob->next_all = obj_list_destruct;
  obj_list_destruct = ob;
  ob->flags |= O_DESTRUCTED;
}

/*
 * This one is called when no program is executing from the main loop.
 */
void destruct2(struct object *ob)
{
  extern int tot_alloc_object_size;
  struct object **hb;
  int i;
  if (d_flag > 1)
    debug_message("Destruct-2 object %s (ref %d)\n", ob->prog->name, ob->ref);

  if(ob->next_heart_beat)
  {
    extern struct object *hb_list;
    for(hb=&hb_list;*hb!=ob;hb=&((*hb)->next_heart_beat))
      if(!*hb)
	fatal("Dangling hb pointer\n");
    *hb=ob->next_heart_beat;
    ob->next_heart_beat=0;
  }

  /*
   * We must deallocate variables here, not in 'free_object()'.
   * That is because one of the local variables may point to this object,
   * and deallocation of this pointer will also decrease the reference
   * count of this object. Otherwise, an object with a variable pointing
   * to itself, would never be freed.
   * Just in case the program in this object would continue to
   * execute, change string and object variables into the number 0.
   */

  /*
   * Deallocate variables in this object.
   * The space of the variables are not deallocated until
   * the object structure is freed in free_object().
   */
  for (i=0; i<ob->prog->num_variables; i++)
  {
    free_short_svalue(&ob->variables[i],ob->prog->variable_names[i].rttype);
  }

  /* this might not be true, but later we don't know have the program
     so we won't know the number of variables  */
  tot_alloc_object_size -=
    (ob->prog->num_variables - 1) * sizeof (struct svalue);
  free_prog(ob->prog, 1);
  free_string(ob->user);
  free_string(ob->eff_user);
  ob->user=ob->eff_user;
  ob->prog=0;
  free_object(ob, "destruct_object");
}

struct object *first_inventory(struct svalue *arg)
{
  struct object *ob=0;

  if (arg->type == T_OBJECT) ob = arg->u.ob;
  if (ob == 0) error("No object to first_inventory()");
  if (ob->contains == 0) return 0;
  return ob->contains;
}

/*
 * This will enable an object to use commands normally only
 * accessible by players.
 * Also check if the player is a wizard. Wizards must not affect the
 * value of the wizlist ranking.
 */

void enable_commands(int num)
{
  if (current_object->flags & O_DESTRUCTED) return;
  if (d_flag > 1)
    debug_message("Enable commands %s (ref %d)\n",
		  current_object->prog->name, current_object->ref);

  if (num)
  {
    current_object->flags |= O_ENABLE_COMMANDS;
    command_giver = current_object;
  } else {
    if(current_object->sentences)
      free_sentences(current_object->sentences);
    current_object->sentences=0;
    current_object->flags &= ~O_ENABLE_COMMANDS;
    if(current_object == command_giver)
    {
      command_giver = 0;
      command_modification++;
    }
  }
}


#define MAX_LINES 50

/* get dir stolen from MudOS */

/*
 * These are used by qsort in get_dir().
 */
static int pstrcmp(struct svalue *p1,struct svalue *p2)
{
  return my_strcmp(p1, p2);
}
static int parrcmp(struct svalue *p1,struct svalue * p2)
{
  return my_strcmp(p1->u.vec->item, p2->u.vec->item);
}

static void encode_stat(struct svalue *vp,int flags,char *str,struct stat *st)
{
  if (flags == -1)
  {
    struct vector *v = allocate_array_no_init(3,0);

    SET_STR(v->item,make_shared_string(str));
    v->item[1].type = T_NUMBER;
    v->item[1].subtype = NUMBER_NUMBER;
    v->item[1].u.number =
      ((st->st_mode & S_IFDIR) ? -2 : st->st_size);
    v->item[2].type = T_NUMBER; 
    v->item[2].subtype = NUMBER_NUMBER; 
    v->item[2].u.number = st->st_mtime;
    vp->type = T_POINTER;
    vp->u.vec = v;
  } else {
    SET_STR(vp,make_shared_string(str));
  }
}
int match_string(char *match,char * str)
{
  int i;

 again:
  if (*str == '\0' && *match == '\0')
    return 1;
  switch(*match) {
  case '?':
    if (*str == '\0')
      return 0;
    str++;
    match++;
    goto again;
  case '*':
    match++;
    if (*match == '\0')
      return 1;
    for (i=0; str[i] != '\0'; i++)
      if (match_string(match, str+i))
	return 1;
    return 0;
  case '\0':
    return 0;
  case '\\':
    match++;
    if (*match == '\0')
      return 0;
    /* Fall through ! */
  default:
    if (*match == *str) {
      match++;
      str++;
      goto again;
    }
    return 0;
  }
}

/*
 * List files in directory. This function do same as standard list_files did,
 * but instead writing files right away to user this returns an array
 * containing those files. Actually most of code is copied from list_files()
 * function.
 * Differences with list_files:
 *
 *   - file_list("/w"); returns ({ "w" })
 *
 *   - file_list("/w/"); and file_list("/w/."); return contents of directory
 *     "/w"
 *
 *   - file_list("/");, file_list("."); and file_list("/."); return contents
 *     of directory "/"
 *
 * With second argument equal to non-zero, instead of returning an array
 * of strings, the function will return an array of arrays about files.
 * The information in each array is supplied in the order:
 *    name of file,
 *    last update of file,
 *    size of file (-2 means file doesn't exist).
 */
#define MAX_FNAME_SIZE 255
#define MAX_PATH_LEN   1024
typedef int (*cmpfuntyp) (const void *,const void *);

struct vector *get_dir(char *path,int flags)
{
    struct vector *v;
    int i, count = 0;
    DIR *dirp;
    int namelen, do_match = 0;
    struct dirent *de;
    struct stat st;
    char *endtemp;
	char temppath[MAX_FNAME_SIZE + MAX_PATH_LEN + 2];
	char regexp[MAX_FNAME_SIZE + MAX_PATH_LEN + 2];
    char *p;

    if (!path) return 0;

    if (strlen(path)<2) {
	temppath[0]=path[0]?path[0]:'.';
	temppath[1]='\000';
	p = temppath;
    } else {
	strncpy(temppath, path, MAX_FNAME_SIZE + MAX_PATH_LEN + 2);
	/*
	 * If path ends with '/' or "/." remove it
	 */
	if ((p = STRRCHR(temppath, '/')) == 0)
	    p = temppath;
	if (p[0] == '/' && ((p[1] == '.' && p[2] == '\0') || p[1] == '\0'))
	  *p = '\0';
    }

    if (stat(temppath, &st) < 0) {
	if (*p == '\0')
	    return 0;
	if (p != temppath) {
	    strcpy(regexp, p + 1);
	    *p = '\0';
	} else {
	    strcpy(regexp, p);
	    strcpy(temppath, ".");
	}
	do_match = 1;
    } else if (*p != '\0' && strcmp(temppath, ".")) {
	if (*p == '/' && *(p + 1) != '\0')
	    p++;
	v = allocate_array(1);
        encode_stat(&v->item[0], flags, p, &st);
	return v;
    }

    if ((dirp = opendir(temppath)) == 0)
	return 0;

    /*
     *  Count files
     */
    for (de = readdir(dirp); de; de = readdir(dirp))
    {
      namelen = NLENGTH(de);

      if (!do_match && (strcmp(de->d_name, ".") == 0 ||
			strcmp(de->d_name, "..") == 0))
	continue;
      if (do_match && !match_string(regexp, de->d_name))
	continue;
      count++;
      if (count >= MAX_ARRAY_SIZE)
	break;
    }
    /*
     * Make array and put files on it.
     */
    v = allocate_array(count);
    if (count == 0) {
	/* This is the easy case :-) */
	closedir(dirp);
	return v;
    }
    rewinddir(dirp);
    endtemp = temppath + strlen(temppath);
    strcat(endtemp++, "/");
    for(i = 0, de = readdir(dirp); i < count; de = readdir(dirp))
    {
      namelen = NLENGTH(de);
      if (!do_match && (strcmp(de->d_name, ".") == 0 ||
			strcmp(de->d_name, "..") == 0))
	continue;
      if (do_match && !match_string(regexp, de->d_name))
	continue;
      de->d_name[namelen] = '\0';
      if (flags == -1) {
	/* We'll have to .... sigh.... stat() the file to
	   get some add'tl info.  */
	strcpy(endtemp, de->d_name);
	stat(temppath, &st);	/* We assume it works. */
      }
      encode_stat(&v->item[i], flags, de->d_name, &st);
      i++;
    }
    closedir(dirp);
    /* Sort the names. */
    qsort((char *)v->item, count, sizeof v->item[0],
	  (cmpfuntyp)(  (flags == -1) ? parrcmp : pstrcmp ));
    return v;
}

/* Profezzorn */
void print_svalue(struct svalue *arg)
{
  char buf[300];

#ifdef DEBUG
  if (!arg)
    fatal("Null argument to print svalue.\n");
#endif
  switch(arg->type)
  {
  case T_STRING:
    push_svalue(arg);
    break;

  case T_FLOAT:
    sprintf(buf,"%0.8f", arg->u.fnum);
    push_new_shared_string(buf);
    break;

  case T_OBJECT:
    sprintf(buf,"OBJ(%d)", arg->u.ob->clone_number);
    push_new_shared_string(buf);
    break;

  case T_NUMBER:
    sprintf(buf,"%d", arg->u.number);
    push_new_shared_string(buf);
    break;

  case T_POINTER:
    sprintf(buf,"ARRAY[%d]",arg->u.vec->size);
    push_new_shared_string(buf);
    break;

  case T_LIST:
    sprintf(buf,"LIST[%d]",arg->u.vec->size);
    push_new_shared_string(buf);
    break;

  case T_MAPPING:
    sprintf(buf,"MAPPING[%d]",arg->u.vec->size);
    push_new_shared_string(buf);
    break;

  default:
    push_new_shared_string("<UNKNOWN>");
  }
  if(!command_giver)
  {
    fwrite(strptr(sp),1,my_strlen(sp),stdout);
    fflush(stdout);
    pop_stack();
    return;
  }
  (void)apply_lfun(LF_CATCH_TELL,command_giver,1,0);
}

void do_write(struct svalue *arg)
{
  struct object *save_command_giver = command_giver;
  print_svalue(arg);
  command_giver = save_command_giver;
}

struct object *find_hashed(char *name)
{
  struct object *ob;
  extern struct object *lookup_my_object_hash(char *);

  if(batch_mode)
  {
    name=combine_path(0,name);
    if(!(ob=lookup_my_object_hash(name)))
      ob=clone_object(name,1);
    free(name);
  }else{
    if(name[0]=='/') name++;
    if(!(ob=lookup_my_object_hash(name)))
      ob=clone_object(name,1);
  }
  return ob;
}

struct program *find_program(char *str)
{
  struct program *p;

  /* Remove leading '/' if any. */
  if(!batch_mode)
    while(str[0] == '/') str++;

  p = find_program2(str);
  if(!p)
    p = load_object(str,0);
  return p;
}

/* Look for a loaded object. Return 0 if non found. */
struct program *find_program2(char *str)
{
  register struct program *p;
  register int length;

  /* Remove leading '/' if any. */
  if(!batch_mode)
  {
    while(str[0] == '/') str++;
    /* Truncate possible .c in the object name. */
    length = strlen(str);
    if (IS_POSTFIX(str,LPC_SUFFIX))
    {
      /* A new writreable copy of the name is needed. */
      char *p;
      p = (char *)alloca(strlen(str)+1);
      strcpy(p, str);
      str = p;
      str[length-strlen(LPC_SUFFIX)] = '\0';
    }
    p=lookup_program_hash(str);
    return p;
  }else{
    str=combine_path(0,str);
    length=strlen(str);
    if (IS_POSTFIX(str,LPC_SUFFIX))
      str[length-strlen(LPC_SUFFIX)]=0;
    p=lookup_program_hash(str);
    free(str);
    return p;
  }
}

/*
 * Transfer an object.
 * The object has to be taken from one inventory list and added to another.
 * The main work is to update all command definitions, depending on what is
 * living or not. Note that all objects in the same inventory are affected.
 *
 * There are some boring compatibility to handle. When -o flag is specified,
 * several functions are called in some objects. This is dangerous, as
 * object might self-destruct when called.
 */
void move_object(struct object *item,struct object *dest)
{
  struct object **pp, *ob, *next_ob;
  struct object *save_cmd = command_giver;

  command_modification++;
  /* Recursive moves are not allowed. */
  for (ob = dest; ob; ob = ob->super)
    if (ob == item)
      error("Can't move object inside itself.\n");

  /*
   * Objects must have inherited std/object if they are to be allowed to
   * be moved.
   */
  if(!(item->flags & O_APPROVED) ||
     !(dest->flags & O_APPROVED)) {
    error("Trying to move object where src or dest not inherit std/object\n");
    return;
  }

  if (item->super) {
    int okey = 0;
		
    push_object(item);
    (void)apply_lfun(LF_EXIT, item->super, 1,0);
    if (item->flags & O_DESTRUCTED || dest->flags & O_DESTRUCTED)
      return;			/* Give up */

    remove_sent(item->super, item);

    if (item->super->flags & O_ENABLE_COMMANDS)
      remove_sent(item, item->super);
    for (pp = &item->super->contains; *pp;) {
      if (*pp != item) {
	if ((*pp)->flags & O_ENABLE_COMMANDS)
	  remove_sent(item, *pp);
	if (item->flags & O_ENABLE_COMMANDS)
	  remove_sent(*pp, item);
	pp = &(*pp)->next_inv;
	continue;
      }
      *pp = item->next_inv;
      okey = 1;
    }
    if (!okey)
      fatal("Failed to find object %s in super list of %s.\n",
	    item->prog->name, item->super->prog->name);
  }
  item->next_inv = dest->contains;
  dest->contains = item;
  item->super = dest;
  /*
   * Setup the new commands. The order is very important, as commands
   * in the room should override commands defined by the room.
   * Beware that init() in the room may have moved 'item' !
   *
   * The call of init() should really be done by the object itself
   * (except in the -o mode). It might be too slow, though :-(
   */
  if (item->flags & O_ENABLE_COMMANDS) {
    command_giver = item;
    (void)apply_lfun(LF_INIT, dest, 0,0);
    if ((dest->flags & O_DESTRUCTED) || item->super != dest) {
      command_giver = save_cmd; /* marion */
      return;
    }
  }
   /*
    * Run init of the item once for every present player, and
    * for the environment (which can be a player).
    */
  for (ob = dest->contains; ob && !(item->flags & O_DESTRUCTED); ob=next_ob) {
    next_ob = ob->next_inv;
    if (ob == item)
      continue;
    if (ob->flags & O_DESTRUCTED)
      error("An object was destructed at call of init()\n");
    if (ob->flags & O_ENABLE_COMMANDS) {
      command_giver = ob;
      (void)apply_lfun(LF_INIT, item, 0,0);
      if (dest != item->super) {
	command_giver = save_cmd; /* marion */
	return;
      }
    }
    if (item->flags & O_DESTRUCTED) /* marion */
      error("The object to be moved was destructed at call of init()\n");
    if (item->flags & O_ENABLE_COMMANDS) {
      command_giver = item;
      (void)apply_lfun(LF_INIT, ob, 0,0);
      if (dest != item->super) {
	command_giver = save_cmd; /* marion */
	return;
      }
    }
  }
  if (dest->flags & O_DESTRUCTED) /* marion */
    error("The destination to move to was destructed at call of init()\n");
  if (item->flags & O_DESTRUCTED) /* Profezzorn */
    error("The moved object was destructed.\n");
  if (dest->flags & O_ENABLE_COMMANDS) {
    command_giver = dest;
    (void)apply_lfun(LF_INIT, item, 0,0);
  }
  command_giver = save_cmd;
}

char debug_parse_buff[50]; /* Used for debugging */

/*
 * Hard coded commands, that will be available to all players. They can not
 * be redefined, so the command name should be something obscure, not likely
 * to be used in the game.
 */
int special_parse(char *buff)
{
#ifdef DEBUG
  strncpy(debug_parse_buff, buff, sizeof debug_parse_buff);
  debug_parse_buff[sizeof debug_parse_buff - 1] = '\0';
#endif
#if defined(MALLOC_smalloc)
  if (strcmp(buff, "debugmalloc") == 0) {
    extern int debugmalloc;
    debugmalloc = !debugmalloc;
    if (debugmalloc)
      vtell_object("On.\n");
    else
      vtell_object("Off.\n");
    return 1;
  }
#endif
  if (strcmp(buff, "status") == 0 || strcmp(buff, "status tables") == 0)
  {
    int tot, res, verbose = 0;
    extern char *reserved_area;
    extern int tot_alloc_sentence, tot_alloc_object,
    tot_alloc_object_size;
    extern int num_arrays, total_array_size;
    extern int total_num_prog_blocks, total_prog_block_size;

    if (strcmp(buff, "status tables") == 0)
      verbose = 1;
    if (reserved_area)
      res = RESERVED_SIZE;
    else
      res = 0;

    if (!verbose) {
      vtell_object("Sentences:\t\t\t%8d %8d\n", tot_alloc_sentence,
		   (int)(tot_alloc_sentence * sizeof (struct sentence)));
      vtell_object("Objects:\t\t\t%8d %8d\n",
		   tot_alloc_object, tot_alloc_object_size);
      vtell_object("Arrays:\t\t\t\t%8d %8d\n", num_arrays,
		   total_array_size);
      vtell_object("Prog blocks:\t\t\t%8d %8d\n",
		   total_num_prog_blocks, total_prog_block_size);
      vtell_object("Memory reserved:\t\t\t %8d\n", res);
    }
    tot = total_prog_block_size +
      total_array_size +
	tot_alloc_object_size +
	  res;

    if (!verbose) {
      vtell_object("\t\t\t\t\t --------\n");
      vtell_object("Total:\t\t\t\t\t %8d\n", tot);
    }
    return 1;
  }
  return 0;
}


void fatal(char *fmt, ...)
{
  extern void debug_message_va(char *a,va_list args);
  va_list args;
  static int in_fatal = 0;

  va_start(args,fmt);
  /* Prevent double fatal. */
  if (in_fatal) abort();

  in_fatal = 1;
  (void)VFPRINTF(stderr, fmt, args);
  fflush(stderr);
  if (current_object)
    (void)fprintf(stderr, "Current object was %s\n",
		  current_object->prog->name);
  debug_message_va(fmt, args);
  if (current_object)
    debug_message("Current object was %s\n", current_object->prog->name);
  debug_message("Dump of variables:\n");

  (void)dump_trace(1); 
  abort();
}

int num_error = 0;

/*
 * Error() has been "fixed" so that users can catch and throw them.
 * To catch them nicely, we really have to provide decent error information.
 * Hence, all errors that are to be caught
 * (error_recovery_context_exists == 2) construct a string containing
 * the error message, which is returned as the
 * thrown value.  Users can throw their own error values however they choose.
 */

/*
 * This is here because throw constructs its own return value; we dont
 * want to replace it with the system's error string.
 */

void throw_error()
{
  extern int error_recovery_context_exists;
  extern jmp_buf error_recovery_context;
  extern struct svalue catch_value;
  if (error_recovery_context_exists > 1) {
    longjmp(error_recovery_context, 1);
    fatal("Throw_error failed!");
  }
  if(catch_value.type==T_STRING)
  {
    error("Throw: %s.\n",strptr(&catch_value));
  }
}

static char emsg_buf[2000];

void va_error(char *fmt, va_list args)
{
  extern int error_recovery_context_exists;
  extern jmp_buf error_recovery_context;
  extern struct object *current_heart_beat;
  extern struct svalue catch_value;
  extern int eval_cost,max_eval_cost;
  struct object *ob,*init_ob;
  char buf[200];
  extern struct control_stack control_stack[MAX_TRACE];

#ifdef OPCPROF
  check_cost_for_instr(-1);
#endif
  VSPRINTF(emsg_buf+1, fmt, args);

  emsg_buf[0] = '*';	/* all system errors get a * at the start */
  if (error_recovery_context_exists==2) /* user catches this error */
  {
    struct svalue v;
    
    SET_STR(&v,make_shared_string(emsg_buf));
    assign_svalue(&catch_value, &v);
    longjmp(error_recovery_context, 1);
    fatal("Catch() longjump failed");
  }

  eval_cost=0;
  max_eval_cost=MAX_COST;
  num_error++;
  if (num_error > 1)
    fatal("Too many simultaneous errors.\n");
  debug_message("%s", emsg_buf+1);
  if (current_object)
  {
    debug_message("program: %s, object: %s line %d\n",
		  current_prog ? current_prog->name : "",
		  current_object->prog->name,
		  get_line_number_if_any());
  }
  fprintf(stderr,"%s",emsg_buf+1);

  if (current_object)
  {
    fprintf(stderr,"program: %s, object: %s line %d\n",
	    current_prog ? current_prog->name : "",
	    current_object->prog->name,
	    get_line_number_if_any());
  }
  ob=dump_trace(0);
  fflush(stderr);
  if (ob && ob->flags & O_DESTRUCTED)
  {
    if(command_giver)
      vtell_object("error when executing program in destroyed object %s\n",
		   ob->prog->name);
    debug_message("error when executing program in destroyed object %s\n",
		  ob->prog->name);
  }

  if (error_recovery_context_exists==3) /* user catches this error */
  {
    num_error--;
    free_svalue(&catch_value);
    longjmp(error_recovery_context, 1);
    fatal("Catch() longjump failed");
  }


  if(!control_stack[1].ob || control_stack[1].ob->flags & O_DESTRUCTED)
  {
    if(!current_object || current_object->flags & O_DESTRUCTED)
    {
      init_ob=NULL;
    }else{
      init_ob=current_object;
    }
  }else{
    init_ob=control_stack[1].ob;
  }

  reset_machine (0);
  push_new_shared_string(emsg_buf);
  if(current_object && !(current_object->flags & O_DESTRUCTED))
  {
    push_object(current_object);
  }else{
    push_zero();
  }
  if(current_prog)
  {
    push_shared_string(copy_shared_string(current_prog->name));
  }else{
    push_zero();
  }
  if(current_object)
  {
    if(current_object->obj_index)
    {
      push_new_shared_string(current_object->prog->name);
    }else{
      sprintf(buf,"%s(%d)",current_object->prog->name,
	      current_object->clone_number);
      push_new_shared_string(buf);
    }
  }else{
    push_zero();
  }
  if(init_ob) push_object(init_ob);
  else push_zero();

  current_object=NULL;
  push_number(get_line_number_if_any());
  APPLY_MASTER_OB((void),"handle_error",6);

  if (current_heart_beat)
  {
    set_heart_beat(current_heart_beat, 0);
    debug_message("Heart beat in %s turned off.\n",
		  current_heart_beat->prog->name);
    push_object(current_heart_beat);
    APPLY_MASTER_OB((void),"kill_heart_beat",1);
    current_heart_beat = 0;
  }
  num_error--;
  if (error_recovery_context_exists)
    longjmp(error_recovery_context, 1);
  if(batch_mode)
    exit(10);
  abort();
}

void error(char *fmt,...)
{
  va_list args;
  va_start(args,fmt);
  va_error(fmt,args);
  va_end(args);
}

/*
 * Check that it is an legal path. No '..' are allowed.
 */
int legal_path(char *path)
{
    char *p;
    if(batch_mode) return 1;
    if (path == NULL || STRCHR(path, ' '))
	return 0;
    if (path[0] == '/')
        return 0;
#ifdef MSDOS
    if (!valid_msdos(path)) return(0);
#endif
    for(p = STRCHR(path, '.'); p; p = STRCHR(p+1, '.')) {
	if (p[1] == '.')
	    return 0;
    }
    return 1;
}

/*
 * There is an error in a specific file. Ask the game driver to log the
 * message somewhere.
 */
void smart_log(char *error_file,int line,char * what)
{
  char buff[500];

  if(!batch_mode)
  {
    if (error_file == 0)
      return;
    if (strlen(what) + strlen(error_file) > sizeof buff - 100)
      what = "...[too long error message]...";
    if (strlen(what) + strlen(error_file) > sizeof buff - 100)
      error_file = "...[too long filename]...";
    sprintf(buff, "%s line %d:%s\n", error_file, line, what);
    push_new_shared_string(error_file);
    push_new_shared_string(buff);
    APPLY_MASTER_OB((void),"log_error", 2);
  }
}


/*
 * Check that a path to a file is valid for read or write.
 * This is done by functions in the master object.
 * The path is always treated as an absolute path, and is returned without
 * a leading '/'.
 * If the path was '/', then '.' is returned.
 * The returned string may or may not be residing inside the argument 'path',
 * so don't deallocate arg 'path' until the returned result is used no longer.
 * Otherwise, the returned path is temporarily allocated by apply(), which
 * means it will be dealocated at next apply().
 */

char *check_valid_path(char *path,int shared_string,
		       char *eff_user,
		       char *call_fun,
		       int writeflg)
{
  struct svalue *v;

  if(batch_mode)		/* who cares in batchmode */
  {
    return path;
  }else{
    if (eff_user == 0)
      return 0;
    if(shared_string)
      push_shared_string(copy_shared_string(path));
    else
      push_new_shared_string(path);
    push_shared_string(copy_shared_string(eff_user));
    push_new_shared_string(call_fun); /* fix? */

    if (writeflg)
    {
      APPLY_MASTER_OB(v=,"valid_write", 3);
    } else{
      APPLY_MASTER_OB(v=,"valid_read", 3);
    }
    if (!v || (v->type == T_NUMBER && v->u.number == 0))
      return 0;

    if(v->type==T_STRING) path=strptr(v);

    if (path[0] == '/')
      path++;
    if (path[0] == '\0')
      path = ".";
    if (legal_path(path))
      return path;
    return 0;
  }
}

char *simple_check_valid_path(struct svalue *s,char *fun,int writeflag)
{
  return check_valid_path(strptr(s),
			  1,
			  current_object->eff_user,
			  fun,writeflag);
}

/*
 * This one is called from HUP.
 */
int game_is_being_shut_down;

void startshutdowngame() {   game_is_being_shut_down = 1;  }

/*
 * This one is called from the command "shutdown".
 * We don't call it directly from HUP, because it is dangerous when being
 * in an interrupt.
 */
#define DEALLOCATE_MEMORY_AT_SHUTDOWN

void shutdowngame()
{
#ifdef ADD_CACHE_SIZE
  extern void free_add_cache();
#endif
  extern void do_gc(int);
  extern char *reserved_area;
  extern struct svalue catch_value;
  char *s;
  int e;

  APPLY_MASTER_OB((void),"game_shutting_down",0);
#ifdef OPCPROF
  dump_opcode_info();
#endif
  do_close_database();
#ifdef DEALLOCATE_MEMORY_AT_SHUTDOWN
  free_svalue(&catch_value);
  free_all_call_outs();
  remove_all_objects();
  free_object(master_ob,"shutdown");
  free_string(hostname); hostname=NULL;
  do_gc(4711);
  for(e=0;e<num_lfuns;e++) free_string(lfuns[e]);
  free((char*)lfuns);
  free_simul_efun();
  free_reswords();
  if(reserved_area) free(reserved_area);
  free_memory_in_lex_at_exit();
  free_svalue(&const_empty_string);
  if((s=simple_free_buf())) free(s);
#ifdef ADD_CACHE_SIZE
  free_add_cache();
#endif
  if(d_flag)
  {
    int e;
    struct object *o;
    struct program *p;
    extern void dump_stralloc_strings();
    extern struct program *obj_table[OTABLE_SIZE];
    for(o=obj_list;o;o=o->next_all)
    {
      printf("Object not destructed: %s(%d) %d refs\n",
	     o->prog ? o->prog->name : "<program freed>",
	     o->clone_number,
	     o->ref);
    }
    for(e=0;e<OTABLE_SIZE;e++)
    {
      for(p=obj_table[e];p;p=p->next_hash)
      {
	printf("Program not freed: %s %d refs\n", p->name, p->ref);
      }
    }
    
    dump_stralloc_strings();
  }
#endif
  exit(0);
}

/*
 * Credits for some of the code below goes to Free Software Foundation
 * Copyright (C) 1990 Free Software Foundation, Inc.
 * See the GNU General Public License for more details.
 */
#ifndef S_ISDIR
#define	S_ISDIR(m)	(((m)&S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISREG
#define	S_ISREG(m)	(((m)&S_IFMT) == S_IFREG)
#endif

int isdir (char *path)
{
  struct stat stats;

  return stat (path, &stats) == 0 && S_ISDIR (stats.st_mode);
}

void strip_trailing_slashes (char *path)
{
  int last;

  last = strlen (path) - 1;
  while (last > 0 && path[last] == '/')
    path[last--] = '\0';
}

struct stat to_stats, from_stats;

int
copy (from, to)
     char *from, *to;
{
  int ifd;
  int ofd;
  char buf[1024 * 8];
  int len;			/* Number of bytes read into `buf'. */
  
  if (!S_ISREG (from_stats.st_mode))
    {
      error ("cannot move `%s' across filesystems: Not a regular file\n", from);
      return 1;
    }
  
  if (unlink (to) && errno != ENOENT)
    {
      error ("cannot remove `%s'\n", to);
      return 1;
    }

  ifd = open (from, O_RDONLY, 0);
  if (ifd < 0)
    {
      error ("%s: open failed\n", from);
      return errno;
    }
  ofd = open (to, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (ofd < 0)
    {
      error ("%s: open failed\n", to);
      close (ifd);
      return 1;
    }
#ifdef HAVE_FCHMOD
  if (fchmod (ofd, from_stats.st_mode & 0777))
    {
      error ("%s: fchmod failed\n", to);
      close (ifd);
      close (ofd);
      unlink (to);
      return 1;
    }
#endif
  
  while ((len = read (ifd, buf, sizeof (buf))) > 0)
    {
      int wrote = 0;
      char *bp = buf;
      
      do
	{
	  wrote = write (ofd, bp, len);
	  if (wrote < 0)
	    {
	      error ("%s: write failed\n", to);
	      close (ifd);
	      close (ofd);
	      unlink (to);
	      return 1;
	    }
	  bp += wrote;
	  len -= wrote;
	} while (len > 0);
    }
  if (len < 0)
    {
      error ("%s: read failed\n", from);
      close (ifd);
      close (ofd);
      unlink (to);
      return 1;
    }

  if (close (ifd) < 0)
    {
      error ("%s: close failed", from);
      close (ofd);
      return 1;
    }
  if (close (ofd) < 0)
    {
      error ("%s: close failed", to);
      return 1;
    }
  
#ifndef HAVE_FCHMOD
  if (chmod (to, from_stats.st_mode & 0777))
    {
      error ("%s: chmod failed\n", to,0,0,0,0,0,0,0,0);
      return 1;
    }
#endif

  return 0;
}

/* Move FROM onto TO.  Handles cross-filesystem moves.
   If TO is a directory, FROM must be also.
   Return 0 if successful, 1 if an error occurred.  */

int do_move (char *from,char * to)
{
  if (lstat (from, &from_stats) != 0)
    {
      error ("%s: lstat failed\n", from);
      return 1;
    }

  if (lstat (to, &to_stats) == 0)
    {
#ifndef MSDOS
      if (from_stats.st_dev == to_stats.st_dev
	  && from_stats.st_ino == to_stats.st_ino)
#else
      if (same_file(from,to))
#endif
	{
	  error ("`%s' and `%s' are the same file", from, to);
	  return 1;
	}

      if (S_ISDIR (to_stats.st_mode))
	{
	  error ("%s: cannot overwrite directory", to);
	  return 1;
	}

    }
  else if (errno != ENOENT)
    {
      error ("%s: unknown error\n", to);
      return 1;
    }
#ifdef SYSV
  if (isdir(from)) {
      char cmd_buf[100];
      sprintf(cmd_buf, "/usr/lib/mv_dir %s %s", from, to);
      return system(cmd_buf);
  } else
#endif /* SYSV */      
  if (rename (from, to) == 0)
    {
      return 0;
    }

  if (errno != EXDEV)
    {
      error ("cannot move `%s' to `%s'", from, to);
      return 1;
    }

  /* rename failed on cross-filesystem link.  Copy the file instead. */

  if (copy (from, to))
      return 1;
  
  if (unlink (from))
    {
      error ("cannot remove `%s'", from);
      return 1;
    }

  return 0;

}

char *read_file(char *file,int start,int len)
{
  struct stat st;
  FILE *f;
  char *str,*p,*p2,*end,c;
  int size;

  if (len < 0) return 0;

  if (!file)
    return 0;
  f = fopen(file, "r");
  if (f == 0)
    return 0;
  if (fstat(fileno(f), &st) == -1)
    fatal("Could not stat an open file.\n");
  size = st.st_size;
  if (size > READ_FILE_MAX_SIZE) {
    if ( start || len ) size = READ_FILE_MAX_SIZE;
    else {
      fclose(f);
      return 0;
    }
  }
  if (!start) start = 1;
  if (!len) len = READ_FILE_MAX_SIZE;
  str = xalloc(size + 1);
  str[size] = '\0';
  do {
    if (size > st.st_size)
      size = st.st_size;
    if (fread(str, size, 1, f) != 1) {
      fclose(f);
      free(str);
      return 0;
    }
    st.st_size -= size;
    end = str+size;
    for (p=str; ( p2=MEMCHR(p,'\n',end-p) ) && --start; ) p=p2+1;
  } while ( start > 1 );
  for (p2=str; p != end; ) {
    c = *p++;
    if (!c) c=' ';
    *p2++=c;
    if ( c == '\n' )
      if (!--len) break;
  }
  if ( len && st.st_size ) {
    size -= ( p2-str) ; 
    if (size > st.st_size)
      size = st.st_size;
    if (fread(p2, size, 1, f) != 1) {
      fclose(f);
      free(str);
      return 0;
    }
    st.st_size -= size;
    end = p2+size;
    for (; p2 != end; ) {
      c = *p2;
      if (!c) *p2=' ';
      p2++;
      if ( c == '\n' )
	if (!--len) break;
    }
    if ( st.st_size && len ) {
      /* tried to read more than READ_MAX_FILE_SIZE */
      fclose(f);
      free(str);
      return 0;
    }
  }
  *p2='\0';
  fclose(f);
#if 0				/* caller immediately frees the string again,
				 * so there's no use to make it smaller now
				 */
  if ( st.st_size > (p2-str) ) {
    /* can't allocate shared string when string type isn't passed to the caller */
    p2=strdup(str);
    free(str);
    return p2;
  }
#endif
  return str;
}

/* returns a shared string */
char *read_bytes(char *file,int start,int len)
{
  struct stat st;

  char *str,*p;
  int size, f;
#ifndef HAVE_UNISTD_H
  int lseek();
#endif

  if (len < 0) return 0;
  if(!batch_mode && len > MAX_BYTE_TRANSFER)
    len=MAX_BYTE_TRANSFER;
  if (!file)
    return 0;
  f = open(file, O_RDONLY);
  if (f < 0)
    return 0;

  if (fstat(f, &st) == -1)
    fatal("Could not stat an open file.\n");
  size = st.st_size;
  if(start < 0) 
    start = size + start;

  if (start >= size) {
    close(f);
    return 0;
  }
  if ((start+len) > size) 
    len = (size - start);

  if ((size = lseek(f,start, 0)) < 0)
    return 0;

  str = xalloc(len + 1);

  size = read(f, str, len);

  close(f);

  if (size <= 0) {
    free(str);
    return 0;
  }

  /*
   * The string has to end to '\0'!!!
   */
  str[size] = '\0';

  p = make_shared_binary_string(str,size);
  free(str);

  return p;
}

int write_bytes(char *file,int start,struct svalue *string)
{
  struct stat st;

  int size, f,len;
#ifndef HAVE_UNISTD_H
  int lseek();
#endif
  char *str;
  str=strptr(string);
  len=my_strlen(string);

  if (!file)return 0;
  if(!batch_mode && len > MAX_BYTE_TRANSFER)
    return 0;
  f = open(file, O_WRONLY | O_CREAT);
  if (f < 0)
    return 0;

  if (fstat(f, &st) == -1)
    fatal("Could not stat an open file.\n");
  size = st.st_size;

  if(start < 0) start += size;
  if(start < 0) return 0;

  if ((size = lseek(f,start, 0)) < 0)
    return 0;

  size = write(f, str, len);

  close(f);

  if (size <= 0) return 0;
  return size;
}
    
#include <sys/param.h>

char *combine_path(char *cwd,char *file)
{
  /* cwd is supposed to be combined already */
  register char *p,*pp,*ppp,*pppp;
  char *my_cwd;
  my_cwd=0;

  if(file[0]=='/')
  {
    cwd="/";
    file++;
  }else{
    
    if(!cwd)
    {
#ifdef HAVE_GETWD
      cwd=(char *)getwd(my_cwd=(char *)malloc(MAXPATHLEN+1));
      if(!cwd)
	fatal("Couldn't fetch current path.\n");
#else
#ifdef HAVE_GETCWD
      my_cwd=cwd=(char *)getcwd(0,1000); 
#else
      /* maybe autoconf was wrong.... if not, insert your method here */
      my_cwd=cwd=(char *)getcwd(0,1000); 
#endif
#endif
    }
  }
  if(cwd[strlen(cwd)-1]=='/')
  {
    p=(char *)xalloc(strlen(cwd)+strlen(file)+1);
    strcpy(p,cwd);
    strcat(p,file);
  }else{
    p=(char *)xalloc(strlen(cwd)+strlen(file)+2);
    strcpy(p,cwd);
    strcat(p,"/");
    strcat(p,file);
  }
  for(pp=STRCHR(p,'/');pp && *pp;)
  {
    if(pp[1]=='.')
    {
      if(pp[2]=='.' && (pp[3]=='/' || pp[3]==0))
      {
	pppp=pp+3;
	for(ppp=pp-1;ppp>=p && *ppp!='/';ppp--);

	pp=ppp;
	/* note: assignment, not comparisment */
	while((*(ppp++)=*(pppp++)));

	continue;
      }else if(pp[2]=='/' || pp[2]==0){
	ppp=pp;
	pppp=pp+2;
	while((*(ppp++)=*(pppp++))); /* note: assignment, not comparisment */
	continue;
      }
    }
    pp=STRCHR(pp+1,'/');
  }
  if(my_cwd) free(my_cwd);
  return p;
}
