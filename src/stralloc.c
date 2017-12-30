#include "global.h"
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <signal.h>
#include "array.h"
#include "interpret.h"
#include "object.h"
#include "comm1.h"
#include "exec.h"
#include "call_out.h"
#include "sent.h"
#include "main.h"
#include "stralloc.h"
#include "dynamic_buffer.h"
#include "hash.h"
#include "simulate.h"
#include "socket_efuns.h"
#include "simul_efun.h"
#include "operators.h"
#include "save_objectII.h"

/*
 * stralloc.c - string management.
 *
 * All strings are stored in an extensible hash table, with reference counts.
 * free_string decreases the reference count; if it gets to zero, the string
 * will be deallocated.  add_string increases the ref count if it finds a
 * matching string, or allocates it if it cant.  There is no way to allocate
 * a string of a particular size to fill later (hash wont work!), so you'll
 * have to copy things freom a static (or malloced and later freed) buffer -
 * that is, if you want to avoid space leaks...
 *
 * Current overhead (on a 32 bit system):
 *      next pointer: 4 bytes
 *      length:       4 bytes
 *      references:   4 bytes
 *      sum:         12 bytes + malloc overhead
 */

#ifdef DEBUG
/* #define ALERT "s" */
#endif

extern int d_flag;
#ifdef ALERT
void Alert(char *s)
{
  fprintf(stderr,"Alert: '%s' done.\n",s);
}
void Alert2(char *s)
{
  if(d_flag>6) Alert(s);
}
#endif


/*
 * there is also generic hash table management code, but strings can be shared
 * (that was the point of this code), will be unique in the table,
 * require a reference count, and are malloced, copied and freed at
 * will by the string manager.  Besides, I wrote this code first :-).
 * Look at htable.c for the other code.  It uses the Hash() function
 * defined here, and requires hashed objects to have a pointer to the
 * next element in the chain (which you specify when you call the functions).
 */

static int search_len = 0;
static int num_str_searches = 0;

#ifdef DEBUG
int refchecked=0;
#endif

/*
 * hash table - list of pointers to heads of string chains.
 * Each string in chain has a pointer to the next string and a
 * reference count (char *, int) stored just before the start of the string.
 * HTABLE_SIZE is in config.h, and should be a prime, probably between
 * 1000 and 5000.
 */

static char *base_table[HTABLE_SIZE];

/*
 * generic hash function.  This is probably overkill; I haven't checked the
 * stats for different prime numbers, etc.
 */

INLINE static unsigned int StrHash(const char *s,int len)
{
  return hashmem(s, len, 20) % HTABLE_SIZE;
}

/*
 * Looks for a string in the table.  If it finds it, returns a pointer to
 * the start of the string part, and moves the entry for the string to
 * the head of the pointer chain.
 */

static char *internal_findstring(const char *s,int len,int h)
{
  char * curr, *prev;

#ifdef MALLOC_DEBUG
  check_sfltable();
#endif

  curr = base_table[h];
  prev = 0;
  num_str_searches++;

  while (curr)
  {
    search_len++;
    if (len==SIZE(curr) && !MEMCMP(curr, s,len)) /* found it */
    {
      if (prev)	/* not at head of list */
      {
	NEXT(prev) = NEXT(curr);
	NEXT(curr) = base_table[h];
	base_table[h] = curr;
      }
#if defined(DEBUG) && defined(STRALLOC_REFS)
      if(REFS(curr)<1)
	fatal("String with no references.\n");
#endif
      return curr;		/* pointer to string */
    }
    prev = curr;
    curr = NEXT(curr);
  }
  return 0;		/* not found */
}

char *findstring(const char *foo)
{
  int l;
  char *s;
  l=strlen(foo);
  s=internal_findstring(foo,l,StrHash(foo,l));
#ifdef WARN
  if(foo==s)
    warn(3,"findstring on already shared string.\n");
#endif  
  return s;
}

/* this one looks for strings that was already shared only */
static char *find_shared_string(const char *s,int h)
{
  char * curr, *prev;

  curr = base_table[h];
  prev = 0;

  num_str_searches++;
  while(curr)
  {
    search_len++;
    if (curr == s) /* found it */
    {
      if (prev)   /* not at head of list */
      {	
	NEXT(prev) = NEXT(curr);
	NEXT(curr) = base_table[h];
	base_table[h] = curr;
      }

#if defined(DEBUG) && defined(STRALLOC_REFS)
      if(REFS(curr)<1)
	fatal("String with no references.\n");
#endif

      return curr;		/* pointer to string */
    }
    prev = curr;
    curr = NEXT(curr);
  }
  
return 0;			/* not found */
}

char *debug_findstring(const char *foo)
{
  return find_shared_string(foo,StrHash(foo,SIZE(foo)));
}

/* note that begin_shared_string expects the _exact_ size of the string,
 * not the maximum size
 */
char *begin_shared_string(int len)
{
  struct shared *t;
  t=(struct shared *)xalloc(len + sizeof(struct shared));
  t->size=len;
  return t->str;
}

/*
 * Make a space for a string.  This is rather nasty, as we want to use
 * alloc/free, but malloc overhead is generally severe.  Later, we
 * will have to compact strings...
 */

static char * alloc_new_string(const char *string,int len,int h)
{
  char *s;
  s=begin_shared_string(len);
  MEMCPY(s, string,len);
  s[len]=0;

#ifndef STRALLOC_GC
  REFS(s) = 0;

#ifdef DEBUG
  XREF(s)=0;
#endif
#endif
  NEXT(s) = base_table[h];
  base_table[h] = s;
  return s;
}

char *end_shared_string(char *s)
{
  int len,h;
  char *s2;

  len=SIZE(s);
  h=StrHash(s,len);
  
  if((s2=internal_findstring(s,len,h)))
  {
    free((char *)REFERENCE(s,));
    s=s2;
  }else{
    s[len]=0;
#ifndef STRALLOC_GC
    REFS(s) = 0;

#ifdef DEBUG
    XREF(s)=0;
#endif
#endif
    NEXT(s) = base_table[h];
    base_table[h] = s;
  }
#ifndef STRALLOC_GC
  REFS(s)++;

#ifdef DEBUG
  XREF(s)++;
#endif
#endif
  return s;
}

char *make_shared_string(const char *str)
{
  char * s;
  int len=strlen(str);
  int h=StrHash(str,len);

#ifdef ALERT
  if(!strcmp(str,ALERT)) Alert("make_shared_string");
#endif

  s = internal_findstring(str,len,h);
#ifdef WARN
  if(s==str) warn(2,"make_shared_string on already shared string.\n");
#endif  
  if (!s) s = alloc_new_string(str,len,h);
#ifndef STRALLOC_GC
  REFS(s)++;
#ifdef DEBUG
  XREF(s)++;
#endif
#endif
  return s;
}

char * make_shared_binary_string(const char *str,int len)
{
  char * s;
  int h=StrHash(str,len);

#ifdef ALERT
  if(!strcmp(str,ALERT)) Alert("make_shared_binary_string");
#endif

  s = internal_findstring(str,len,h);
#ifdef WARN
  if(s==str) warn(2,"make_shared_binary_string on already shared string.\n");
#endif  
  if (!s) s = alloc_new_string(str,len,h);
#ifndef STRALLOC_GC
  REFS(s)++;
#ifdef DEBUG
  XREF(s)++;
#endif
#endif
  return(s);
}

#ifdef DEBUG
int my_strlen2(const union storage_union u)
{
  if(debug_findstring(strptr2(u))!=strptr2(u))
    fatal("Non shared string to internal.\n");
  return u.string->size;
}

int my_strlen(const struct svalue *s)
{
  if(s->type!=T_STRING)
    fatal("Bad type to internal strlen.\n");

  return my_strlen2(s->u);
}
#endif

INLINE int my_string_is_equal(const struct svalue *a,const struct svalue *b)
{
#ifdef DEBUG
  if(a->type!=T_STRING)
    fatal("Bad type to internal strcmp.\n");
  if(b->type!=T_STRING)
    fatal("Bad type to internal strcmp.\n");
#endif
  return a->u.string == b->u.string;
}

INLINE int my_strcmp(const struct svalue *a,const struct svalue *b)
{
  int tmp;
#ifdef DEBUG
  if(a->type!=T_STRING)
    fatal("Bad type to internal strcmp.\n");
  if(b->type!=T_STRING)
    fatal("Bad type to internal strcmp.\n");
#endif
  if(my_string_is_equal(a,b)) return 0;

  if(my_strlen(a)>my_strlen(b))
  {
    tmp=MEMCMP(strptr(a),strptr(b),my_strlen(b));
    if(tmp) return tmp;
    return 1;
  }else if(my_strlen(a)<my_strlen(b)){
    tmp=MEMCMP(strptr(a),strptr(b),my_strlen(a));
    if(tmp) return tmp;
    return -1;
  }else{
    return MEMCMP(strptr(a),strptr(b),my_strlen(a));
  }
}

#ifndef copy_shared_string
char * copy_shared_string(char *str)
{
#ifdef ALERT
  if(!strcmp(str,ALERT)) Alert("copy_shared_string");
#endif

#ifdef DEBUG
   if(debug_findstring(str)!=str)
    fatal("Copy shared string on non shared string.\n");
#endif

#ifndef STRALLOC_GC
  REFS(str)++;
#ifdef DEBUG
  XREF(str)++;
#endif
#endif

  return(str);
}
#endif

INLINE static void terminally_free_string(char **prev,char *s)
{
  /* It will be at the head of the hash chain */
  *prev = NEXT(s);

  free((char *)REFERENCE(s,));
}

/*
 * free_string - reduce the ref count on a string.  Various sanity
 * checks applied, the best of them being that a add_string allocated
 * string will point to a word boundary + sizeof(char *)+sizeof(int),
 * since malloc always allocates on a word boundary.
 * On systems where a short is 1/2 a word, this gives an easy check
 * to see whather we did in fact allocate it.
 */

#ifdef DEBUG
void free_string(char *str)
{
#if defined(DEBUG) || defined(STRALLOC_REFS)
  char * s;
  int h;
#endif

#ifdef ALERT
  if(!strcmp(str,ALERT)) Alert("free_string");
#endif


#ifndef	DEBUG
#ifndef STRALLOC_GC
  if(--REFS(str)) return;
#endif

#ifdef STRALLOC_REFS
  h=StrHash(str,SIZE(str));
  s = find_shared_string(str,h); /* moves it to head of table if found */
#endif

#else

  h=StrHash(str,SIZE(str));
  s = find_shared_string(str,h); /* moves it to head of table if found */
  if (!s)
  {
    fatal("Free string: not found in string table! (%s)", str);
    return;
  }
  if (s != str)
  {
    fatal("Free string: string didnt hash to the same spot! (%s)", str);
    return;
  }

#ifndef STRALLOC_GC
  XREF(s)--;
  REFS(s)--;

  if (REFS(s)<0)
  {
    fatal("Freeing string too many times. (%s)", str);
    return;
  }
/*
  if(REFS(s) != XREF(s) && refchecked)
  {
    fprintf(stderr,"Freeing string with strange xrefs '%s'!\n",s);
  }
*/
  if (REFS(s) > 0) return;
#endif

#endif /* DEBUG */

#ifdef STRALLOC_REFS
  terminally_free_string(base_table+h,s);
#endif
}

#else

#ifndef STRALLOC_GC
INLINE void really_free_string(char *str)
{
  int h;
  h=StrHash(str,SIZE(str));
  terminally_free_string(base_table+h,find_shared_string(str,h));
}
#endif

#endif

/*
 * you think this looks bad!  and we didn't even tell them about the
 * GNU malloc overhead!  tee hee!
 */

char *add_string_status(int verbose)
{
  char b[200];

  init_buf();

  if (verbose)
  {
#ifndef STRALLOC_GC
    int allocd_strings=0,allocd_bytes=0;
#endif
    int num_distinct_strings=0,bytes_distinct_strings=0;
    int overhead_bytes=0;
    int e;
    char *p;
    for(e=0;e<HTABLE_SIZE;e++)
    {
      for(p=base_table[e];p;p=NEXT(p))
      {
	num_distinct_strings++;
	bytes_distinct_strings+=((SIZE(p)+3)&~3);
#ifndef STRALLOC_GC
	allocd_strings+=REFS(p);
	allocd_bytes=REFS(p)*((SIZE(p)+3)&~3);
#endif
      }
    }
    overhead_bytes=(sizeof(struct shared)-1)*num_distinct_strings;
    my_strcat("\nShared string hash table:\n");
    my_strcat("-------------------------\t Strings    Bytes\n");

#ifndef STRALLOC_GC
    sprintf(b,"Total asked for\t\t\t%8d %8d\n",
	    allocd_strings, allocd_bytes);
    my_strcat(b);
#endif
    sprintf(b,"Strings malloced\t\t%8d %8d + %d overhead\n",
	    num_distinct_strings, bytes_distinct_strings, overhead_bytes);
    my_strcat(b);
    sprintf(b,"Space actually required/total string bytes %d%%\n",
	    (bytes_distinct_strings + overhead_bytes)*100 / allocd_bytes);
    my_strcat(b);
  }
  sprintf(b,"Searches: %d    Average search length: %6.3f\n",
	  num_str_searches, (double)search_len / num_str_searches);
  my_strcat(b);
  return free_buf();
  /*  return(bytes_distinct_strings + overhead_bytes); */
}

/* only used in debug mode, or when stralloc uses garbage collect */
int ref_cycle=0;

#ifdef DEBUG
static void *string_to_find;
int check_ref_cycle=0;
#endif

#ifdef DEBUG
#define RETTYPE int
#define RETDECL RETTYPE ret=0
#define CCALL(X,Y) if X { ret=1; Y; }
#define RETURN return ret
#else
#define RETTYPE void
#define RETDECL
#define CCALL(X,Y) X
#define RETURN return
#endif
static RETTYPE check_svalues(struct svalue *s,int number);
static RETTYPE check_short_svalue(union storage_union *u,int type);

INLINE static RETTYPE check_string(char *c)
{
  char *t;
  RETDECL;
  if(c)
  {
    t=debug_findstring(c);
    if(!t) fatal("Shared string not shared.\n");
    if(t!=c) fatal("Shared string not same as one in hashtable.\n");
#ifdef DEBUG
    if(check_ref_cycle) RETURN;
    if(string_to_find)
    {
      if(string_to_find==c)
      {
	fprintf(stderr,"Found '%s'\n",c);
	ret=1;
      }
    }else
#endif
    {
#ifdef STRALLOC_GC
      if(SIZE(c)<0) SIZE(c)=-SIZE(c); /* mark */
#else
#ifdef DEBUG
      XREF(c)++;
      ret=0;
#endif
#endif
    } 
  }
  RETURN;
}

static RETTYPE check_array(struct vector *v)
{
  RETDECL;

#ifdef DEBUG
  if(v==string_to_find)
  {
    fprintf(stderr,"Array found...\n");
    ret=1;
  }
  if(v->ref<1) fatal("Zero refs array.\n");
  if(v->size>MAX_ARRAY_SIZE && !batch_mode)
    fatal("Too large array.\n");

  if(v->malloced_size<v->size)
    fatal("Impossible array.\n");

  if(check_ref_cycle)
  {
    if((v->flags & O_REF_CYCLE) != ref_cycle)
      fatal("Error in ref cycle of program.\n");
    if(!v->extra_ref) RETURN;
    v->extra_ref=0;
  }else
#endif
  {
#ifdef DEBUG
    if(!string_to_find)
      v->extra_ref++;
#endif
    if((v->flags & O_REF_CYCLE) == ref_cycle) RETURN;
    v->flags&=~O_REF_CYCLE;
    v->flags|=ref_cycle;
  }

  if(v->size>0 && v->item[0].type==T_ALIST_PART)
    check_alist_for_destruct(v);

  CCALL((check_svalues(v->item,v->size)), );
  RETURN;
}

static RETTYPE check_program(struct program *p)
{
  int e;
  RETDECL;

  if(!p) RETURN;

#ifdef DEBUG
  if(p==string_to_find)
  {
    fprintf(stderr,"Program found...\n");
    ret=1;
  }
  if(check_ref_cycle)
  {
    if((p->flags & O_REF_CYCLE) != ref_cycle)
      fatal("Error in ref cycle of program.\n");
    if(!p->extra_ref) RETURN;
    p->extra_ref=0;
  }else
#endif
  {
#ifdef DEBUG
    if(!string_to_find)
      p->extra_ref++;
#endif
    if((p->flags & O_REF_CYCLE) == ref_cycle) RETURN;
    p->flags&=~O_REF_CYCLE;
    p->flags|=ref_cycle;
  }

  CCALL((check_string(p->name)),fprintf(stderr,"Was the name of a program.\n"));

  for(e=0;e<p->num_variables;e++)
  {
    CCALL((check_string(p->variable_names[e].name)),fprintf(stderr,"It's the name of a variable in %s.\n",p->name));
  }

  for(e=0;e<p->num_functions;e++)
  {
    CCALL((check_string(p->functions[e].name)),fprintf(stderr,"It's the name of a function in %s.\n",p->name));
  }

  for(e=0;e<p->num_strings;e++)
  {
    CCALL((check_string(p->strings[e])),fprintf(stderr,"It's a string in %s.\n",p->name));
  }

  if(p->inherit[0].prog!=p) fatal("p->inherited[0] != p.\n");

  for(e=1;e<p->num_inherited;e++)
  {
    CCALL((check_program(p->inherit[e].prog)),fprintf(stderr,"Was inherited by '%s'.\n",p->name));
  }

  for(e=0;e<p->num_switch_mappings;e++)
  {
    CCALL((check_array(p->switch_mappings[e])),fprintf(stderr,"Was in a switch mapping of %s.\n",p->name));
  }

  CCALL((check_svalues(p->constants,p->num_constants)),fprintf(stderr,"Was in a program constant of %s.\n",p->name));
  RETURN;
}

static RETTYPE check_object(struct object *ob)
{
  int e;
  RETDECL;

  if(!ob) RETURN;
#ifdef DEBUG
  if(ob==string_to_find)
  {
    fprintf(stderr,"Object found...\n");
    ret=1;
  }
  if(check_ref_cycle)
  {
    if((ob->flags & O_REF_CYCLE) != ref_cycle)
      fatal("Error in ref cycle of object.\n");
    if(!ob->extra_ref) return 0;
    ob->extra_ref=0;
  }else
#endif
  {
#ifdef DEBUG
    if(!string_to_find)
      ob->extra_ref++;
#endif
    if((ob->flags & O_REF_CYCLE) == ref_cycle) RETURN;
    ob->flags&=~O_REF_CYCLE;
    ob->flags|=ref_cycle;
  }

  if(ob->flags & O_DESTRUCTED) RETURN;

  for(e=0;e<ob->prog->num_variables;e++)
  {
    CCALL((check_short_svalue(ob->variables+e, ob->prog->variable_names[e].rttype)),fprintf(stderr,"Found in variable\n"));
  }

  CCALL((check_string(ob->obj_index)),fprintf(stderr,"It's the index of an object.\n"));
  CCALL((check_string(ob->user)),fprintf(stderr,"Found in uid.\n"));
  CCALL((check_string(ob->eff_user)),fprintf(stderr,"Found in euid.\n"));

  if(ob->sentences)
  {
    for(e=0;e<ob->sentences->slots;e++)
    {
      if(!ob->sentences->sent[e].match) continue;
      
      CCALL((check_string(ob->sentences->sent[e].match)),fprintf(stderr,"It's match in an add_action\n"));
      CCALL((check_svalues(&(ob->sentences->sent[e].function),1)),fprintf(stderr,"It's the function name in an add_action\n"));
    }
  }
  CCALL((check_program(ob->prog)),fprintf(stderr,"Found in program %s\n",ob->prog->name));

  RETURN;
}

static RETTYPE check_svalues(struct svalue *s,int number)
{
  int e;
  RETDECL;

  for(e=0;e<number;e++,s++)
  {
    switch(s->type)
    {
    case T_STRING:
      CCALL((check_string(strptr(s))),fprintf(stderr,"In shared string svalue.\n"));
      break;

    case T_MAPPING:
    case T_LIST:
    case T_POINTER:
    case T_ALIST_PART:
      CCALL((check_array(s->u.vec)),
	switch(s->type)
	{
	case T_MAPPING: fprintf(stderr,"In mapping.\n"); break;
	case T_LIST:  fprintf(stderr,"In list.\n"); break;
	case T_POINTER: fprintf(stderr,"In array.\n"); break;
	case T_ALIST_PART: fprintf(stderr,"In alist part.\n"); break;
	}
	    );
      break;

    case T_LVALUE:
      CCALL((check_svalues(&(s->u.lvalue->ind),1)),fprintf(stderr,"Found in lvalue index\n"));

    case T_OBJECT:
    case T_FUNCTION:
      CCALL((check_object(s->u.ob)),fprintf(stderr,"Found in object %s\n",s->u.ob->prog->name));
      if(s->u.ob->flags & O_DESTRUCTED)
      {
	short tmp;
	tmp=s->type=T_OBJECT;
        free_svalue(s);
	s->subtype=tmp?NUMBER_DESTRUCTED_OBJECT:NUMBER_DESTRUCTED_FUNCTION;
      }
    }
  }
  RETURN;
}

static RETTYPE check_short_svalue(union storage_union *u,int type)
{
  RETDECL;

  if(u->number)
  {
    switch(type)
    {
    case T_ANY:
      CCALL((check_svalues((struct svalue *)u,1)),);
    case T_NOTHING:
      break;

    case T_STRING:
      CCALL((check_string(strptr2(*u))),fprintf(stderr,"In shared string svalue.\n"));
      break;

    case T_MAPPING:
    case T_LIST:
    case T_POINTER:
    case T_ALIST_PART:
      CCALL((check_array(u->vec)),
	    switch(type)
	  {
	  case T_MAPPING: fprintf(stderr,"In mapping.\n"); break;
	  case T_LIST:  fprintf(stderr,"In list.\n"); break;
	  case T_POINTER: fprintf(stderr,"In array.\n"); break;
	  case T_ALIST_PART: fprintf(stderr,"In alist part.\n"); break;
	  }
	    );
      break;

    case T_OBJECT:
    case T_FUNCTION:
      CCALL((check_object(u->ob)),fprintf(stderr,"Found in object %s\n",u->ob->prog->name));
      if(u->ob->flags & O_DESTRUCTED)
      {
	free_short_svalue(u,type);
      }
    }
  }
  RETURN;
}

static RETTYPE check_str_ref_from_call_outs()
{
  int e;
  RETDECL;
  for(e=0;e<num_pending_calls;e++)
  {
    CCALL((check_object(pending_calls[e]->command_giver)),fprintf(stderr,"Was command giver in call_out.\n"));
    CCALL((check_array(pending_calls[e]->args)),fprintf(stderr,"Was in argument list for call_out.\n"));
  }
 RETURN;
}

extern struct svalue start_of_stack[EVALUATOR_STACK_SIZE];
extern struct lpc_socket lpc_socks[MAX_EFUN_SOCKS];
extern struct svalue *sp;
extern struct object *obj_list;
extern struct object *obj_list_destruct;
extern char **inc_list;
extern int inc_list_size;
extern struct svalue ret_value;
extern struct program *obj_table[OTABLE_SIZE];
extern struct keyword predefs[],reswords[];
extern int real_number_predefs,number_reswords;
extern char *last_file;
extern struct object *master_ob;
extern struct vector null_vector;
extern int batch_mode;

static void check_even_more_ref_counts2()
{
  int e,q;
  struct object *ob;
  struct program *p;
#ifdef DEBUG
  struct vector *v;
#endif
  RETDECL;

  check_str_ref_from_call_outs();
  q=sp-start_of_stack+1;

  for(e=0;e<num_lfuns;e++)
  {
    CCALL((check_string(lfuns[e])),fprintf(stderr,"Was an lfun\n"));
  }
  
  CCALL((check_svalues(start_of_stack,q)),fprintf(stderr,"Was found on stack\n"));

#ifdef YYOPT
  for(e=0;e<real_number_predefs;e++)
  {
    CCALL((check_string(predefs[e].word)),fprintf(stderr,"Was efun name\n"));
  }

  for(e=0;e<number_reswords;e++)
  {
    CCALL((check_string(reswords[e].word)),fprintf(stderr,"Was a reserved word\n"));
  }
#endif
  for(e=0;e<OTABLE_SIZE;e++)
  {
    for(p=obj_table[e];p;p=p->next_hash)
    {
      CCALL((check_program(p)),fprintf(stderr,"Found in prog %s\n",p->name));
    }
  }

  for(ob=obj_list; ob; ob = ob->next_all)
  {
    CCALL((check_object(ob)),fprintf(stderr,"Found in object %s\n",ob->prog->name));
  }

  for(ob=obj_list_destruct; ob; ob = ob->next_all)
  {
    CCALL((check_object(ob)),fprintf(stderr,"Found in destructed object %s\n",ob->prog->name));
  }

  CCALL((check_object(master_ob)),fprintf(stderr,"Found master object\n"));

  for(e=0;e<inc_list_size;e++)
  {
    CCALL((check_string(inc_list[e])),fprintf(stderr,"Found in include path.\n"));
  }

  for(e=0;e<num_simul_efuns;e++)
  {
    CCALL((check_string(simul_efuns[e].name)),fprintf(stderr,"Is the name of a simul_efun.\n"));
    CCALL((check_svalues(&(simul_efuns[e].fun),1)),fprintf(stderr,"Found in a simul efun svalue.\n"));
  }

  CCALL((check_svalues(&ret_value,1)),fprintf(stderr,"Was last returned value.\n"));
    
  CCALL((check_svalues(&const_empty_string,1)),fprintf(stderr,"Was the empty string.\n"));
  CCALL((check_string(hostname)),fprintf(stderr,"Was hostname.\n"));
  CCALL((check_string(last_file)),fprintf(stderr,"Was filename of last database.\n"));

#ifdef ADD_CACHE_SIZE
  for(e=0;e<ADD_CACHE_SIZE;e++)
  {
    CCALL((check_string(add_cache[e].a)),fprintf(stderr,"Was in add cache.\n"));
    CCALL((check_string(add_cache[e].b)),fprintf(stderr,"Was in add cache.\n"));
    CCALL((check_string(add_cache[e].sum)),fprintf(stderr,"Was in add cache.\n"));
  }
#endif
  for(e=0;e<MAX_EFUN_SOCKS;e++)
  {
    CCALL((check_svalues(&(lpc_socks[e].read_callback),1)),fprintf(stderr,"Was read callback svalue.\n"));
    CCALL((check_svalues(&(lpc_socks[e].write_callback),1)),fprintf(stderr,"Was write callback svalue.\n"));
    CCALL((check_svalues(&(lpc_socks[e].close_callback),1)),fprintf(stderr,"Was close callback svalue.\n"));
  }
  CCALL((check_array(&null_vector)),fprintf(stderr,"Was null vector.\n"));
#ifdef DEBUG
  for(v=&null_vector;v;v=v->next)
  {
    CCALL((check_array(v)),fprintf(stderr,"Was in an array.\n"));
  }
#endif
}

static void check_even_more_ref_counts()
{
  if(ref_cycle)
  {
    ref_cycle=0;
  }else{
    ref_cycle=O_REF_CYCLE;
  }
  check_even_more_ref_counts2();
}


static last_gc;

void stralloc_gc(int force)
{
#if !defined(STRALLOC_REFS) || defined(DEBUG)
  char *c;
  int e;
#endif
  extern int current_time;
#ifdef DEBUG
  struct program *p;
  struct object *ob;
  struct vector *v;
#endif

  if(d_flag<3 && !force && current_time-last_gc>STRALLOC_GC_TIME) return;
  last_gc=current_time;
#ifdef ADD_CACHE_SIZE
  clean_add_cache();
#endif
#ifdef DEBUG
  for(e=0;e<OTABLE_SIZE;e++)
    for(p=obj_table[e];p;p=p->next_hash)
      p->extra_ref=-1;

  for(ob=obj_list; ob; ob = ob->next_all)
    ob->extra_ref=-1;

  for(v=&null_vector; v; v = v->next)
    v->extra_ref=-1;

  check_ref_cycle=1;
  check_even_more_ref_counts2();
  check_ref_cycle=0;

  string_to_find=0;
  refchecked=1;
#endif

#ifdef STRALLOC_GC
  for(e=0;e<HTABLE_SIZE;e++)
  {
    char **prev;
    for(prev=base_table+e;(c=*prev);prev=&(NEXT(c)))
      SIZE(c)=-SIZE(c);
  }
#endif

#if defined(DEBUG) && !defined(STRALLOC_GC)
  for(e=0;e<HTABLE_SIZE;e++)
  {
    char **prev;
    for(prev=base_table+e;(c=*prev);prev=&(NEXT(c)))
      XREF(c)=0;
  }
#endif

#if !defined(STRALLOC_GC) && !defined(DEBUG)
  if(force)
#endif
    check_even_more_ref_counts();

#ifdef DEBUG

  for(e=0;e<OTABLE_SIZE;e++)
  {
    for(p=obj_table[e];p;p=p->next_hash)
    {
      if(p->extra_ref!=p->ref)
      {
        fprintf(stderr,"Warning: program '%s' had %d refs, expected %d\n",p->name,p->ref,p->extra_ref);
	string_to_find=p;
	check_even_more_ref_counts();
	if(d_flag>4) fatal("Error in object refs.\n");
      }
    }
  }


  for(ob=obj_list; ob; ob = ob->next_all)
  {
    if(ob->extra_ref!=ob->ref)
    {
      fprintf(stderr,"Warning: object %s#%d had %d refs, expected %d\n",ob->prog->name,ob->clone_number,ob->ref,ob->extra_ref);
      string_to_find=ob;
      check_even_more_ref_counts();
      if(d_flag>4) fatal("Error in object refs.\n");
    }
  }

  for(v=&null_vector; v; v = v->next)
  {
    if(v->extra_ref-1!=v->ref)
    {
      fprintf(stderr,"Warning: array had %d refs, expected %d\n",v->ref,v->extra_ref-1);
      init_buf();
      my_strcat("({");
      debug_save_svalue_list(v);
      my_strcat("})");
      fprintf(stderr,"%s\n",return_buf());
      string_to_find=v;
      check_even_more_ref_counts();
      if(d_flag>4) fatal("Error in array refs.\n");
    }
  }


#ifndef STRALLOC_GC
  for(e=0;e<HTABLE_SIZE;e++)
  {
    for(c=base_table[e];c;c=NEXT(c))
    {
      if(XREF(c)!=REFS(c))
      {
        fprintf(stderr,"Warning: '%s' had %d refs, expected %d\n",c,REFS(c),XREF(c));
	string_to_find=c;
	check_even_more_ref_counts();
	if(d_flag>4) fatal("Error in string refs.\n");
      }
    }
  }
#endif
#endif


#ifndef STRALLOC_REFS
  for(e=0;e<HTABLE_SIZE;e++)
  {
    char **prev;
    for(prev=base_table+e;(c=*prev);prev=&(NEXT(c)))
    {
#ifdef STRALLOC_HYBRID
      if(!REFS(c))
#else
      if(SIZE(c)<0)
#endif
      {
	terminally_free_string(prev,c);
      }else{
	prev=&(NEXT(c));
      }
    }
  }
#endif
}

void not_backend_stralloc_gc()
{
  /* we can't run gc here because pointers might be in local
   * variables and such. *sigh*
   */

  last_gc=0;
}

void dump_stralloc_strings()
{
  int e;
  char *p;
  for(e=0;e<HTABLE_SIZE;e++)
    for(p=base_table[e];p;p=NEXT(p))
      printf("%d refs \"%s\"",REFS(p),p);
}
