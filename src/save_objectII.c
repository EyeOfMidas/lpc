#include "global.h"
#include <math.h>
#include <ctype.h>
#include "array.h"
#include "interpret.h"
#include "exec.h"
#include "object.h"
#include "main.h"
#include "stralloc.h"
#include "save_objectII.h"
#include "efun_protos.h"
#include "simulate.h"
#include "regexp.h"

extern int d_flag;

int save_style=0;

struct processing
{
  struct processing *next;
  struct vector *v;
};

static void low_save_svalue(struct svalue *s,struct processing *p);

static void my_error(char *t)
{
  char *s;
  if((s=simple_free_buf())) free(s);
  error(t);
}

static void save_svalue_list(struct vector *v,struct processing *prev)
{
  int e;
  struct processing p;
  p.next=prev;
  p.v=v;
  for(e=0;prev;e++,prev=prev->next)
  {
    if(prev->v==v)
    {
      char buf[20];
      sprintf(buf,"#%d",e);
      my_strcat(buf);
      return;
    }
  }
  for(e=0;e<v->size;e++)
  {
    if(e) my_putchar(',');
    low_save_svalue(&(v->item[e]),&p);
  }
}

void debug_save_svalue_list(struct vector *v)
{
  save_svalue_list(v,0);
}


static void save_mapping(struct vector *v,struct processing *p)
{
  int e;
  struct processing prev;
  prev.next=p;
  prev.v=v;
  for(e=0;p;e++,p=p->next)
  {
    if(p->v==v)
    {
      char buf[20];
      sprintf(buf,"#%d",e);
      my_strcat(buf);
      return;
    }
  }

  for(e=0;e<v->item[0].u.vec->size;e++)
  {
    if(e) my_putchar(',');
    low_save_svalue(&(v->item[0].u.vec->item[e]),&prev);
    my_putchar(':');
    low_save_svalue(&(v->item[1].u.vec->item[e]),&prev);
  }
}

static void save_string(char *s,int len)
{
  char buf[20];
  int e;

  switch(save_style)
  {
  default:
  case SAVE_DEFAULT:
    sprintf(buf,"%dH",len);
    my_strcat(buf);
    my_binary_strcat(s,len);
    break;
    
  case SAVE_AS_ONE_LINE:
    my_putchar('"');
    for(e=0;e<len;e++)
    {
      switch(s[e])
      {
        case '\n':
          my_putchar('\\');
          my_putchar('n');
          break;

        case '\t':
          my_putchar('\\');
          my_putchar('t');
          break;

        case '\b':
          my_putchar('\\');
          my_putchar('b');
          break;

        case '"':
        case '\\':
          my_putchar('\\');
        default:
          my_putchar(s[e]);
      }
    }
    my_putchar('"');
    break;

  case SAVE_OLD_STYLE:
    my_putchar('"');
    for(e=0;e<len;e++)
    {
      switch(s[e])
      {
        case '"':
        case '\n':
        case '\\':
          my_putchar('\\');
        default:
          my_putchar(s[e]);
      }
    }
    my_putchar('"');
    break;
  }
}

void save_object_desc(struct object *ob)
{
  char b[20];
  extern struct object fake_object;
  if(ob==&fake_object) error("*");
  if(ob->obj_index)
  {
    my_strcat("H(");
    save_string(ob->obj_index,strlen(ob->obj_index));
    my_strcat(")");
  }else{
    sprintf(b,"O(%d)",ob->clone_number);
    my_strcat(b);
  }
}

static void low_save_svalue(struct svalue *s,struct processing *p)
{
  char b[30];
  switch(s->type)
  {
  case T_LVALUE:
    my_strcat("Lvalue");
    break;

  case T_REGEXP:
    my_strcat("R(");
    save_string(s->u.regexp->str,strlen(s->u.regexp->str));
    my_strcat(")");
    break;
    
  case T_STRING:
    save_string(strptr(s),my_strlen(s));
    break;

  case T_NUMBER:
    sprintf(b,"%d",s->u.number);
    my_strcat(b);
    break;

  case T_FLOAT:
    sprintf(b,"F%0.8f",s->u.fnum);
    my_strcat(b);
    break;

  case T_POINTER:
    sprintf(b,"(%d{",s->u.vec->size);
    my_strcat(b);
    save_svalue_list(s->u.vec,p);
    my_strcat("})");
    break;

  case T_ALIST_PART:
    sprintf(b,"(%d*",s->u.vec->size);
    my_strcat(b);
    save_svalue_list(s->u.vec,p);
    my_strcat("*)");
    break;

  case T_LIST:
    sprintf(b,"(%d<",s->u.vec->item[0].u.vec->size);
    my_strcat(b);
    save_svalue_list(s->u.vec->item[0].u.vec,p);
    my_strcat(">)");
    break;

  case T_MAPPING:
    sprintf(b,"(%d[",s->u.vec->item[0].u.vec->size);
    my_strcat(b);
    save_mapping(s->u.vec,p);
    my_strcat("])");
    break;

  case T_OBJECT:
    save_object_desc(s->u.ob);
    break;

  case T_FUNCTION:
    save_object_desc(s->u.ob);
    my_strcat("->");
    my_strcat(FUNC(s->u.ob->prog,s->subtype)->name);
    break;
     
  default:
    fatal("Unknown type %d in save_object.\n",s->type);
  }
}

void save_svalue(struct svalue *s)
{
  low_save_svalue(s,0);
}

static char *restore_string(char **t)
{
  init_buf();
  (*t)++;
  while(**t!='"')
  {
    switch(**t)
    {
      case 0:
        my_error("Unexpected end of string in restore.\n");

      case '\\':
      {
        (*t)++;
        switch(**t)
        {
          case 0:
            my_error("Unexpected end of string in restore.\n");

          case 'n': my_putchar('\n'); break;
          case 't': my_putchar('\t'); break;
          case 'b': my_putchar('\b'); break;
          case '0':
          case '1':
          case '2':
          {
	    char c;
            c=(**t-'0')*8*8;
            (*t)++;
            if(**t<'0' || **t>'8') 
              my_error("Error in restore string.\n");
            c+=(**t-'0')*8;
            (*t)++;
            if(**t<'0' || **t>'8') 
              my_error("Error in restore string.\n");
            c+=(**t-'0');
            my_putchar(c);
          }
          default:  my_putchar(**t);  break;
        }
        (*t)++;
        break;
      }
      default:
        my_putchar(*((*t)++));
    }
  }
  (*t)++;
  return free_buf();
}

struct processing2
{
  struct processing2 *next;
  struct svalue *s;
};

static char *low_restore_svalue(char *t,struct svalue *s,struct processing2 *p)
{
  extern struct svalue *sp;
  free_svalue(s);
  switch(*t)
  {
  case '#':
  {
    int e;
    e=(int)STRTOL(t,&t,10);
    while(e-- && p) p=p->next;
    if(!p || !p->s) my_error("Error in restore object");
    assign_svalue(s,p->s);
    return t;
  }

  case '-': 
  case '0': case '1': case '2': case '3': case '4':
  case '5': case '6': case '7': case '8': case '9':
    s->u.number=(int)STRTOL(t,&t,10);
    if(*t=='H')
    {
      int tmp;
      tmp=s->u.number;
      t++;
      SET_STR(s,make_shared_binary_string(t,tmp));
      t+=tmp;
    }else{
      s->type=T_NUMBER;
      s->subtype=NUMBER_NUMBER;
    }
    return t;

  case 'R':
    if(strncmp("R(",t,2)) my_error("Error in restore object");
    t+=2;
    sp++;
    SET_TO_ZERO(*sp);
    t=low_restore_svalue(t,s,p);
    if(*t!=')') my_error("Error in restore object");
    t++;
    f_cast_to_regexp();
    assign_svalue(s,sp);
    pop_stack();
    break;

  case 'O':
    s->type=T_NUMBER;
    s->subtype=NUMBER_DESTRUCTED_OBJECT;
    s->u.number=0;
    if(strncmp("O(",t,2)) my_error("Error in restore object");
    t+=2;
    while(*t>='0' && *t<='9') t++;
    if(*t!=')') my_error("Error in restore object");
    t++;
    return t;

  case 'F':
    s->type=T_FLOAT;
    t++;
    s->u.fnum=STRTOD(t,&t);
    return t;

  case 'H':
    if(strncmp("H(",t,2)) my_error("Error in restore object");
    t+=2;
    sp++;
    SET_TO_ZERO(*sp);
    t=low_restore_svalue(t,sp,p);
    if(*t!=')') my_error("Error in restore object");
    t++;
    f_cast_to_object();
    if(*t=='-')
    {
      char *tmp;
      if(strncmp("->",t,2)) my_error("Error in restore object.\n");
      t+=2;
      if(*t=='"') t++;
      tmp=t;
      while(isalnum(*t) || *t=='_') t++;
      if(*t=='"') t++;
      push_shared_string(make_shared_binary_string(tmp,t-tmp));
      f_get_function(2,sp-1);
    }
    assign_svalue(s,sp);
    pop_stack();
    return t;
    
  case '"':
    SET_STR(s,restore_string(&t));
    return t;

  case '(':
  {
    struct processing2 curr;
    curr.next=p;
    curr.s=s;
    switch(*++t)
    {
      int e,size;

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      size=(int)STRTOL(t,&t,10);
      switch(*(t++))
      {
      case '{':
	s->u.vec=allocate_array(size);
	if(!s) my_error("Out of memory in restore_object.\n");
	s->type=T_POINTER;
	for(e=0;e<size;e++)
	{
	  t=low_restore_svalue(t,s->u.vec->item+e,&curr);
	  if(*t==',') t++;
	}
	if(t[0]!='}' || t[1]!=')')
	  my_error("Error while restoring array.\n");
	t+=2;
	return t;

      case '<':
	s->u.vec=allocate_array(1);
	if(!s) my_error("Out of memory in restore_object.\n");
	s->type=T_POINTER;
	s->u.vec->item[0].u.vec=allocate_array(size);
	if(!s->u.vec->item[0].u.vec)
	  my_error("Out of memory in restore_object.\n");
	s->u.vec->item[0].type=T_ALIST_PART;
	s->type=T_LIST;

	for(e=0;e<size;e++)
	{
	  t=low_restore_svalue(t,s->u.vec->item[0].u.vec->item+e,&curr);
	  if(*t==',') t++;
	}
	if(t[0]!='>' || t[1]!=')')
	  my_error("Error while restoring list.\n");
	t+=2;
	order_alist(s->u.vec);
	return t;

      case '[':
	s->u.vec=allocate_array(3);
	if(!s) my_error("Out of memory in restore_object.\n");
	s->type=T_POINTER;
	s->u.vec->item[0].u.vec=allocate_array(size);
	if(!s->u.vec->item[0].u.vec)
	  my_error("Out of memory in restore_object.\n");
	s->u.vec->item[0].type=T_ALIST_PART;
	s->u.vec->item[1].u.vec=allocate_array(size);
	if(!s->u.vec->item[0].u.vec)
	  my_error("Out of memory in restore_object.\n");
	s->u.vec->item[1].type=T_ALIST_PART;
	s->type=T_MAPPING;

	for(e=0;e<size;e++)
	{
	  t=low_restore_svalue(t,s->u.vec->item[0].u.vec->item+e,&curr);
	  if(*t!=':') my_error("Error when restoring mapping.\n");
	  t++;
	  t=low_restore_svalue(t,s->u.vec->item[1].u.vec->item+e,&curr);
	  if(*t==',') t++;
	}
	if(t[0]!=']' || t[1]!=')')
	  my_error("Error while restoring list.\n");
	t+=2;
	order_alist(s->u.vec);
	return t;
      }

    case '{':
      t++; e=0;
      while(*t!='}' && *t)
      {
	sp++;
	SET_TO_ZERO(*sp);
	t=low_restore_svalue(t,sp,&curr);
	if(*t==',') t++;
	e++;
      }
      if(!*t) my_error("Unexpected end of string in restore object.\n");
      if(*++t==')') t++;
      f_aggregate(e,sp-e+1);
      assign_svalue(s,sp);
      pop_stack();
      return t;

    case '<':
      t++; e=0;
      while(*t!='>' && *t)
      {
	sp++;
	SET_TO_ZERO(*sp);
	t=low_restore_svalue(t,sp,&curr);
	if(*t==',') t++;
	e++;
      }
      if(!*t) my_error("Unexpected end of string in restore object.\n");
      if(*++t==')') t++;
      f_l_aggregate(e,sp-e+1);
      assign_svalue(s,sp);
      pop_stack();
      return t;

    case '[':
      t++; e=0;

      while(*t!=']' && *t)
      {
	sp++;
	SET_TO_ZERO(*sp);
	t=low_restore_svalue(t,sp,&curr);
	if(*t!=':') my_error("Error while restoring mapping.\n");
	t++;
	e++;
	sp++;
	SET_TO_ZERO(*sp);
	t=low_restore_svalue(t,sp,&curr);
	if(*t==',') t++;
	e++;
      }
      if(!*t) my_error("Unexpected end of string in restore object.\n");
      if(*++t==')') t++;
      f_m_aggregate(e,sp-e+1);
      assign_svalue(s,sp);
      pop_stack();
      return t;
    }
  }
  }
  return t;
}

char *restore_svalue(char *t,struct svalue *s)
{
  return low_restore_svalue(t,s,0);
}

char *save_object()
{
  int i;

  if (current_object->flags & O_DESTRUCTED)
    error("Save_object on destructed object.\n");

  init_buf();
  for (i=0;i < current_object->prog->num_variables; i++)
  {
    struct svalue tmp;
    if(current_object->prog->variable_names[i].rttype==T_NOTHING)
      continue;

    if(current_object->prog->variable_names[i].rttype==T_ANY)
    {
      tmp=*(struct svalue *)(current_object->variables+i);
    }else{
      tmp.type=current_object->prog->variable_names[i].rttype;
      tmp.u=current_object->variables[i];
      if(!tmp.u.number && tmp.type<=MAX_REF_TYPE)
	tmp.type=T_NUMBER;
    }

    if (current_object->prog->variable_names[i].type & TYPE_MOD_STATIC)
      continue;
    my_strcat(current_object->prog->variable_names[i].name);
    my_putchar(' ');
    save_svalue(&tmp);
    my_putchar('\n');
  }
  return free_buf();
}


int restore_object(char *t)
{
  char *var,q[150];
  struct variable *p;

  if (current_object->flags & O_DESTRUCTED)
    error("Restore object on destructed object.\n");

  while(*t)
  {
    int i;
    struct svalue *v,tmp;

    SET_TO_ZERO(tmp);
    if(*t=='\n') { t++; continue; }
    t=STRCHR(var=t, ' ');
    if (t == 0 || t-var>=sizeof(q)-1)
      error("Illegal format when restore.\n");
    strncpy(q,var,t-var);
    q[t-var]=0;
    p = find_status(q, 0);
    t++;
    t=restore_svalue(t,&tmp);
    if (p == 0 || (p->type & TYPE_MOD_STATIC))
    {
      free_svalue(&tmp);
    }else if(p->rttype==T_ANY){
      i=p - current_object->prog->variable_names;
      v = (struct svalue *)&current_object->variables[i];
      assign_svalue(v,&tmp);
      free_svalue(&tmp);
    }else if(p->rttype==tmp.type){
      i=p - current_object->prog->variable_names;
      assign_short_svalue(current_object->variables+i,&tmp,tmp.type);
      free_svalue(&tmp);
    }
    if(*t=='\n') t++;
  }
  if (d_flag > 1)
    debug_message("Object %s restored.\n", current_object->prog->name);
  return 1;
}
