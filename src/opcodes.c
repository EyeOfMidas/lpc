#include <math.h>
#include "efuns.h"
#include "array.h"
#include "simul_efun.h"
#include "stralloc.h"
#include "mapping.h"
#include "list.h"
#include "main.h"
#include "opcodes.h"
#include "simulate.h"
#include "regexp.h"

extern struct svalue start_of_stack[EVALUATOR_STACK_SIZE];

void f_push_array()
{
  struct vector *v;
  v=sp[0].u.vec;
  if(sp+v->size >= &start_of_stack[EVALUATOR_STACK_SIZE])
    error("Array does not fit on stack.\n");
  check_vector_for_destruct(v);
  copy_svalues_raw(sp,v->item,v->size);
  sp+=v->size-1;
  free_vector(v);
}

extern char *get_relative_offset();

#define BT_VECTOR_MASK (BT_POINTER | BT_MAPPING | BT_ALIST_PART)

struct lvalue lvalues[LVALUES];
struct lvalue *lsp=lvalues-1;

struct lvalue *push_indexed_lvalue(struct svalue *s,struct svalue *i)
{
  struct lvalue *l;
  if(!s) error("Illigal indexing.\n");

  if(s->type==T_LVALUE)
  {
    l=s->u.lvalue;
#ifdef DEBUG
    if(l!=lsp)  fatal("Error in interpreter.\n");
#endif
  }else{
    lsp++;
    if(lsp>=lvalues+LVALUES) error("Lvalue stack overflow.\n");
    lsp->type=LVALUE_LOCAL;
    lsp->rttype=T_ANY;
    lsp->ptr.sval=s;
    l=lsp;
  }

  lsp++;
  if(lsp>=lvalues+LVALUES) error("Lvalue stack overflow.\n");
  lsp->type=LVALUE_INDEX;
  lsp->ptr.sval=NULL;
  assign_svalue_no_free(&(lsp->ind),i);
  return lsp;
}

/* Assume resolved */
static int get_lvalue_type(struct lvalue *l)
{
  unsigned short type;
  type=l->rttype;
  if(type==T_ANY)
  {
    type=l->ptr.sval->type;
  }else if(type<=MAX_REF_TYPE){
    if(!l->ptr.uval->ref) type=T_NUMBER;
  }
  return type;
}

/* Assume resolved and approperiate type */
static struct vector *get_vector_from_lvalue(struct lvalue *l)
{
  if(l->rttype==T_ANY)
  {
    return l->ptr.sval->u.vec;
  }else{
    return l->ptr.uval->vec;
  }
}

static void resolv_lvalue(struct lvalue *l)
{
  struct vector *v;
  int ind,len,type;

  if(l->ptr.sval) return;
  switch(l->type)
  {
  case LVALUE_INDEX:
    resolv_lvalue(l-1);

    type=get_lvalue_type(l-1);
    switch(type)
    {
    case T_POINTER:
      v=get_vector_from_lvalue(l-1);
      if(l->ind.type!=T_NUMBER) error("Indexing array with non-integer.\n");
      ind=l->ind.u.number;
      len=v->size;
      if(ind<0) ind+=len;
      if(ind<0 || ind>=len)  error("Index out of range.\n");

      l->ptr.sval=v->item+ind;
      l->rttype=T_ANY;
      break;

    case T_MAPPING:
      v=get_vector_from_lvalue(l-1);
      ind=assoc(&(l->ind),v->item[0].u.vec);
      if(ind<0)
      {
	insert_alist(&(l->ind),v->item+2,v);
	ind=assoc(&(l->ind),v->item[0].u.vec);
      }
      l[-1].ptr.sval=v->item+1;
      l[-1].rttype=T_ANY;
      l->ptr.sval=v->item[1].u.vec->item+ind;
      l->rttype=T_ANY;
    }
  }
}

void assign(struct lvalue *l,struct svalue *s)
{
  switch(l->type)
  {
  case LVALUE_SHORT_GLOBAL:
    assign_short_svalue(l->ptr.uval,s,l->rttype);
    return;

  case LVALUE_GLOBAL:
  case LVALUE_LOCAL:
    assign_svalue(l->ptr.sval,s);
    break;
    
  case LVALUE_INDEX:
  {
    int ind,len,type;
    struct svalue *sv;
    struct vector *v;
    resolv_lvalue(l-1);
    sv=l[-1].ptr.sval;

    if(!sv) error("Error in assign.\n");

    type=get_lvalue_type(l-1);
    switch(type)
    {
    case T_POINTER:
      if(l->ind.type!=T_NUMBER)	error("Index isn't an integer.\n");
      v=get_vector_from_lvalue(l-1);
      ind=l->ind.u.number;
      len=v->size;
      if(ind<0) ind+=len;
      if(ind<0 || ind>=len) error("Index out of range.\n");

      v->types|=1<<s->type;
      assign_svalue(v->item+ind,s);
      return;

    case T_MAPPING:
      insert_alist(&(l->ind),s,get_vector_from_lvalue(l-1));
      return;

    case T_LIST:
      v=get_vector_from_lvalue(l-1);
      if(IS_ZERO(s))
      {
	ind=assoc(&(l->ind),v->item[0].u.vec);
	if(ind>=0) donk_alist_item(v,ind);
      }else{
	struct svalue const1;
	const1.type=T_INT;
	const1.subtype=NUMBER_NUMBER;
	const1.u.number=0;
	insert_alist(&(l->ind),&const1,v);
      }
      return;

    case T_STRING:
    {
      char *s1,*s2;
      int ch;

      if(s->type!=T_INT)
	error("Wrong type to assign (expected int)\n");

      ch=s->u.number;
      if(l->ind.type!=T_NUMBER)
	error("Tried to index string with something other than int.\n");

      ind=l->ind.u.number;

      if(l[-1].rttype==T_ANY)
      {
	s2=strptr(l[-1].ptr.sval);
	len=my_strlen(l[-1].ptr.sval);
      }else{
	s2=strptr2(l[-1].ptr.uval[0]);
	len=my_strlen2(l[-1].ptr.uval[0]);
      }

      if(ind<0) ind+=len;
      if(ind<0 || ind>=len) error("Index out of range.\n");
	
      s1=begin_shared_string(len);
      MEMCPY(s1,s2,len);
      s1[ind]=ch;
      s1=end_shared_string(s1);
      free_svalue(sv);

      if(l[-1].rttype==T_ANY)
      {
	SET_STR(l[-1].ptr.sval,s1);
      }else{
	l[-1].ptr.uval->string=BASE(s1);
      }
      return;
    }
    }
  }

  default:
    error("Unknown assignment type.\n");
  }
}

void lvalue_to_svalue_no_free(struct svalue *s,struct lvalue *l)
{
  switch(l->type)
  {
  case LVALUE_SHORT_GLOBAL:
    if(l->rttype!=T_ANY)
    {
      assign_svalue_from_short_no_free(s,l->ptr.uval,l->rttype);
      return;
    }
    
  case LVALUE_LOCAL:
  case LVALUE_GLOBAL:
    assign_svalue_no_free(s,l->ptr.sval);
    return;

  case LVALUE_INDEX:
  {
    struct vector *v;
    struct svalue *sv;
    int ind,len;

    resolv_lvalue(l-1);
    sv=l[-1].ptr.sval;
    if(!sv) error("Error in indexing.\n");
    

    switch(get_lvalue_type(l-1))
    {
    case T_POINTER:
      if(l->ind.type!=T_NUMBER) error("Index isn't an integer.\n");
      v=get_vector_from_lvalue(l-1);
      ind=l->ind.u.number;
      len=v->size;

      if(ind<0) ind+=len;
      if(ind<0 || ind>=len) error("Index out of range.\n");
      assign_svalue_no_free(s,v->item+ind);
      return;

    case T_MAPPING:
      v=get_vector_from_lvalue(l-1);
      ind=assoc(&(l->ind),v->item[0].u.vec);
      if(ind<0)
      {
	assign_svalue_no_free(s,v->item+2);
      }else{
	assign_svalue_no_free(s,v->item[1].u.vec->item+ind);
	if(IS_ZERO(s) && s->subtype==NUMBER_UNDEFINED)
	  s->subtype=NUMBER_NUMBER;
      }
      return;

    case T_LIST:
      v=get_vector_from_lvalue(l-1);
      if(assoc(&(l->ind),v->item[0].u.vec)<0)
	SET_TO_UNDEFINED(*s);
      else
	SET_TO_ONE(*s);
      return;

    case T_STRING:
    {
      char *s2;
      if(l->ind.type!=T_NUMBER)
	error("Tried to index string with something other than int.\n");

      ind=l->ind.u.number;

      if(l[-1].rttype==T_ANY)
      {
	s2=strptr(l[-1].ptr.sval);
	len=my_strlen(l[-1].ptr.sval);
      }else{
	s2=strptr2(l[-1].ptr.uval[0]);
	len=my_strlen2(l[-1].ptr.uval[0]);
      }

      if(ind<0) ind+=len;
	
      s->type=T_NUMBER;
      s->subtype=NUMBER_NUMBER;
      if(ind<0 || ind>=len)
	s->u.number=0;
      else
	s->u.number=EXTRACT_UCHAR(s2+ind);
      return;

    default:
      error("Indexing a basic type.\n");
    }
    }
  }
  }
  error("Error when referencing lvalue.\n");
}

void lvalue_to_svalue(struct svalue *s,struct lvalue *l)
{
  free_svalue(s);
  lvalue_to_svalue_no_free(s,l);
}

int *lvalue_to_intp(struct lvalue *l,char *fel)
{
  resolv_lvalue(l);
  if(!l->ptr.sval) return 0;
  if(l->rttype==T_ANY)
  {
    if(l->ptr.sval->type==T_NUMBER)  return & l->ptr.sval->u.number;
  }else if(l->rttype==T_NUMBER)
  {
    return & l->ptr.uval->number;
  }
  error(fel);
  return 0;
}

void f_index()
{
  struct svalue s;
  struct lvalue *l;
  l=push_indexed_lvalue(sp-1,sp);
  lvalue_to_svalue_no_free(&s,l);
  free_lvalue(l);
  pop_stack();
  pop_stack();
  sp++;
  *sp=s;
}

void f_indirect()
{
#ifdef DEBUG
  if (sp->type != T_LVALUE)
    fatal("Bad type to F_INDIRECT\n");
#endif
  sp++;
  lvalue_to_svalue(sp,sp[-1].u.lvalue);
  f_swap();
  pop_stack();
}

void f_swap()
{
  struct svalue a;
  a=*(sp-1);
  *(sp-1)=*sp;
  *sp=a;
}

void f_swap_variables()
{
  sp++;
  lvalue_to_svalue(sp,sp[-2].u.lvalue);
  sp++;
  lvalue_to_svalue(sp,sp[-2].u.lvalue);
  assign(sp[-2].u.lvalue,sp-1);
  assign(sp[-3].u.lvalue,sp);
  pop_n_elems(4);
}



void f_cast_to_object()
{
  struct object *v;
  switch(sp->type)
  {
  case T_OBJECT:
    break;
  case T_FUNCTION:
    sp->type=T_OBJECT;
    break;
  case T_STRING:
    v=find_hashed(strptr(sp));
    if(v)
    {
      pop_stack();
      push_object(v);
    }else{
      error("Couldn't find object '%s'\n",strptr(sp));
    }
    break;

  case T_INT:
  {
    /* Here I try to use a somewhat odd 'skiplist' /Profezzorn */
    int i;
    i=sp->u.number;
    pop_stack();
    for(v=obj_list;v && v->clone_number>i;)
    {
#define DOCHECK(X) if(v->X && v->X->clone_number<v->clone_number && v->X->clone_number>=i) { v=v->X; continue; }
      DOCHECK(next_heart_beat);
      DOCHECK(next_obj_hashed);
      DOCHECK(next_inv);
      DOCHECK(contains);
      DOCHECK(super);
      v=v->next_all;
    }
    if(!v || v->clone_number!=i)
    {
      push_zero();
    }else{
      push_object(v);
    }
    break;
  }
    
  default:
    error("Casting a strange value.\n");
    break;
  }
}

void f_cast_to_function()
{
  int function;
  switch(sp->type)
  {
  case T_FUNCTION:
    break;

  default:
    error("Casting a strange value.\n");
    break;

  case T_STRING:
    function=find_shared_string_function(strptr(sp),current_object->prog);
    
    pop_stack();
    if(function>=0)
    {
      sp++;
      sp->type=T_FUNCTION;
      sp->u.ob=current_object;
      add_ref(current_object, "(function)");
      sp->subtype=function;
    }else{
      push_number(0);
    }
  }
}
    
void f_cast_to_float()
{
  float f;
  switch(sp->type)
  {
  case T_NUMBER:
    f=(float)sp->u.number;
    pop_stack();
    push_float(f);
    break;

  case T_FLOAT:
    break;

  case T_STRING:
    f=(float)atof(strptr(sp));
    pop_stack();
    push_float(f);
    break;

  default:
    error("Casting a strange value.\n");
  }
}

void f_cast_to_int()
{
  int i;
  switch(sp->type)
  {
  case T_FLOAT:
    i=sp->u.fnum;
    pop_stack();
    push_number((int)i);
    break;

  case T_NUMBER:
    break;

  case T_STRING:
    i=atoi(strptr(sp));
    pop_stack();
    push_number(i);
    break;

  default:
    error("Casting a strange value.\n");
  }
}

void f_cast_to_regexp()
{
  regexp *r;
  switch(sp->type)
  {
  case T_REGEXP:
    break;

  case T_STRING:
    r=regcomp(strptr(sp),0);
    r->str=string_copy(strptr(sp));
    pop_stack();
    sp++;
    sp->u.regexp=r;
    sp->type=T_REGEXP;
    r->ref=1;
    break;

  default:
    error("Casting a strange value.\n");
  }
}

void f_cast_to_string()
{
  char buf[30];
  switch(sp->type)
  {
  case T_FLOAT:
    sprintf(buf,"%f",sp->u.fnum);
    pop_stack();
    push_new_shared_string(buf);
    break;

  case T_NUMBER:
    sprintf(buf,"%d",sp->u.number);
    pop_stack();
    push_new_shared_string(buf);
    break;

  case T_STRING:
    break;

  default:
    error("Casting a strange value.\n");
  }
}

/*
  flags:
   *
  operators:
  %d
  %s
  %f
  %c
  %n
  %[
  %%
*/

int read_set(char *match,int cnt,char *set,int match_len)
{
  int init;
  int last=0;
  int e;

  if(cnt>=match_len)
    error("Error in sscanf format string.\n");

  if(match[cnt]=='^')
  {
    for(e=0;e<256;e++) set[e]=1;
    init=0;
    cnt++;
    if(cnt>=match_len)
      error("Error in sscanf format string.\n");
  }else{
    for(e=0;e<256;e++) set[e]=0;
    init=1;
  }
  if(match[cnt]==']')
  {
    set[last=']']=init;
    cnt++;
    if(cnt>=match_len)
      error("Error in sscanf format string.\n");
  }

  for(;match[cnt]!=']';cnt++)
  {
    if(match[cnt]=='-')
    {
      cnt++;
      if(cnt>=match_len)
	error("Error in sscanf format string.\n");
      if(match[cnt]==']')
      {
	set['-']=init;
	break;
      }
      for(e=last;e<match[cnt];e++) set[e]=init;
    }
    set[last=EXTRACT_UCHAR(match+cnt)]=init;
  }
  return cnt;
}

int inter_sscanf(int num_arg)
{
  char *input;
  int input_len;
  char *match;
  int match_len;
  struct svalue sval;
  int e,cnt,matches,eye,arg;
  int no_assign;
  char set[256];
  struct svalue *argp;
  
  argp=sp-num_arg+1;

  check_argument(0,T_STRING,F_SSCANF);
  check_argument(1,T_STRING,F_SSCANF);

  input=strptr(argp+0);
  input_len=my_strlen(argp+0);

  match=strptr(argp+1);
  match_len=my_strlen(argp+1);

  arg=eye=matches=0;

  for(cnt=0;cnt<match_len;cnt++)
  {
    for(;cnt<match_len;cnt++)
    {
      if(match[cnt]=='%')
      {
        if(match[cnt+1]=='%')
        {
          cnt++;
        }else{
          break;
        }
      }
      if(eye>=input_len ||input[eye]!=match[cnt])
	return matches;
      eye++;
    }
    if(cnt>=match_len) return matches;

#ifdef DEBUG
    if(match[cnt]!='%' || match[cnt+1]=='%')
    {
      fatal("Error in sscanf.\n");
    }
#endif

    cnt++;
    if(cnt>=match_len)
      error("Error in sscanf format string.\n");

    if(match[cnt]=='*')
    {
      no_assign=1;
      cnt++;
      if(cnt>=match_len)
        error("Error in sscanf format string.\n");
    }else{
      no_assign=0;
    }

    switch(match[cnt])
    {
    case 'c':
      if(eye>=input_len) return matches;
      sval.type=T_NUMBER;
      sval.subtype=NUMBER_NUMBER;
      sval.u.number=EXTRACT_UCHAR(input+eye);
      eye++;
      break;

    case 'd':
      if(eye>=input_len) return matches;

      if(isdigit(input[eye]) || input[eye]=='-')
      {
	char *t;          
	sval.type=T_NUMBER;
	sval.subtype=NUMBER_NUMBER;
	sval.u.number=STRTOL(input+eye,&t,0);
	eye=t-input;
      }else{
	return matches;
      }
      break;

    case 's':
      if(cnt+1>=match_len)
      {
	SET_STR(&sval,make_shared_binary_string(input+eye,input_len-eye));
	break;
      }else{
	char *end_str_start;
	char *end_str_end;
	char *s=0;		/* make gcc happy */
	char *p=0;		/* make gcc happy */
	int start,contains_percent_percent;

	start=eye;
	end_str_start=match+cnt+1;
          
	s=match+cnt+1;
      test_again:
	if(*s=='%')
	{
	  s++;
	  if(*s=='*') s++;
	  switch(*s)
	  {
	  case 'n':
	    s++;
	    goto test_again;
	      
	  case 's':
	    error("Illigal to have to adjecent %%s.\n");
	    return 0;		/* make gcc happy */
	      
	  case 'd':
	    for(e=0;e<256;e++) set[e]=1;
	    for(e='0';e<='9';e++) set[e]=0;
	    set['-']=0;
	    goto match_set;

	  case 'f':
	    for(e=0;e<256;e++) set[e]=1;
	    for(e='0';e<='9';e++) set[e]=0;
	    set['.']=set['-']=0;
	    goto match_set;

	  case '[':		/* oh dear */
	    read_set(match,s-match+1,set,match_len);
	    for(e=0;e<256;e++) set[e]=!set[e];
	    goto match_set;
	  }
	}

	contains_percent_percent=0;

	for(e=cnt;e<match_len;e++)
	{
	  if(match[e]=='%')
	  {
	    if(match[e+1]=='%')
	    {
	      contains_percent_percent=1;
	    }else{
	      break;
	    }
	  }
	}
	   
	end_str_end=match+e;

	if(!contains_percent_percent)
	{
	  s=MEMMEM(end_str_start,
		   end_str_end-end_str_start,
		   input+eye,
		   input_len-eye);
	  if(!s) return matches;
	  eye=s-input;
	}else{
	  for(;eye<input_len;eye++)
	  {
	    p=input+eye;
	    for(s=end_str_start;s<end_str_end;s++,p++)
	    {
	      if(*s!=*p) break;
	      if(*s=='%') s++;
	    }
	    if(s==end_str_end)
	      break;
	  }
	  if(eye==input_len)
	    return matches;
	}
	SET_STR(&sval,make_shared_binary_string(input+start,eye-start));

	cnt=end_str_end-match-1;
	eye+=end_str_end-end_str_start;
	break;
      }

    case '[':
      cnt=read_set(match,cnt+1,set,match_len);
      /* I want to be able to match the empty set as well.
	 if(!set[input[eye]]) return matches;
	 */

    match_set:
      for(e=eye;eye<input_len && set[EXTRACT_UCHAR(input+eye)];eye++);
      SET_STR(&sval,make_shared_binary_string(input+e,eye-e));
      break;

    case 'n':
      sval.type=T_NUMBER;
      sval.subtype=NUMBER_NUMBER;
      sval.u.number=eye;
      break;
    
    case 'f':
      if(eye>=input_len) return matches;

      if(isdigit(input[eye]) || input[eye]=='-' || input[eye]=='.')
      {
	char *t;
	sval.u.fnum=(float)STRTOD(input+eye,&t);
	sval.type=T_FLOAT;
	eye=t-input;
      }else{
	return matches;
      }
      break;        

    default:
      error("Unknown sscanf token %%%c\n",match[cnt]);
    }
    matches++;

    if(!no_assign)
    {
      arg++;
      if(num_arg-2<arg)
      {
	free_svalue(&sval);
	error("Too few arguments to sscanf.\n");
      }
      assign(argp[1+arg].u.lvalue,&sval);
    }
    free_svalue(&sval);
  }
  return matches;
}


