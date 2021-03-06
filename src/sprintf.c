/*
  Sprintf v2.0 By Fredrik Hubinette (Profezzorn@nannymud)
  Should be reasonably compatible and somewhat faster than v1.05+ of Lynscar's
  sprintf. It requires the buffering function provided in dynamic_buffer.c
  It was originally written for lpc4, but should be pretty simple to adapt to
  most lpc-drivers. Fail-safe memory-leak-protection is implemented through a
  stack that can be deallocated at any time. If something fails horribly this
  stack will be deallocated at next call of sprintf. Most operators doesn't
  need this feature though as they allocate their buffers with alloca() or
  simply use pointers into other strings.
  It also has a lot more features:

  Modifyers:
    0  Zero pad numbers (implies right justification)
    !  Toggle truncation
       pad positive integers with a space
    +  pad positive integers with a plus sign
    -  left adjusted within field size (default is right)
    |  centered within field size
    =  column mode if strings are greater than field size
    /  Rough linebreak (break at exactly fieldsize instead of between words)
    #  table mode, print a list of '\n' separated word (top-to-bottom order)
    $  Inverse table mode (left-to-right order)
    n  (where n is a number or *) a number specifies field size
   .n  set precision
   :n  set field size & precision
   ;n  Set column width
    *  if n is a * then next argument is used
   'X'  Set a pad string. ' cannot be a part of the pad_string (yet)
    <  Use same arg again
    ^  repeat this on every line produced
    @  do this format for each entry in argument array
    > Put the string at the bottom end of column instead of top
    _ Set width to size of length of data

 Operators:
   %% percent
   %d signed decimal int
   %u unsigned decimal int
   %o unsigned octal int
   %x lowercase unsigned hexadecimal int
   %X uppercase unsigned hexadecimal int
   %c char
   %f float
   %g heruistically chosen representation of float
   %e exponential notation float
   %s string
   %O any type (prettyprint)
   %D any type (save-object-style)
   %n nop
   %t type of argument
   %<modifyers>{format%}  do a format for every index in an array.

   Most flags and operators are combinable in any fashion, but _really_
   strange results can arise from things like:
      sprintf("%+080#{%s\n%}",get_dir("/"))

   Ideas yet to be implemented:
   Line-break with fill? Lower-case? Upper case? Capitalize?
   Replace? Justify on decimal point?
   Change european/american decimal notation?
   nroff-format? Crack root password and erase all disks?
   Print-optimize? (space to tab, strip trailing spaces)
   '>' Kill this field in all lines but the first one

   examples:

   A short 'people' function (without sort)

   sprintf("%{%-14s %2d %-30s %s\n%}\n",map_array(users(),lambda(object x)
       {
           return ({x->query_real_name(),
		x->query_level(),
		query_ip_name(x),
		file_name(environment(x))
	      });
       }))


   an 'ls'
   sprintf("%-#*{%s\n%}\n",width,get_dir(dir));

   debug-dump
   sprintf("%78*O",width,foo);

   A newspape
   sprintf("%-=*s %-=*s\n",width/2,article1,width/2,article2);

   A 'dotted-line' pricelist row 
   sprintf("%'.'-10s.%'.'4d\n",item,cost);

*/

#include "efuns.h"
#include "array.h"
#include "save_objectII.h"
#include "stralloc.h"
#include "simulate.h"
#include "main.h"
#include "dynamic_buffer.h"

extern int eval_cost;

#define FORMAT_INFO_STACK_SIZE 200
#define RETURN_SHARED_STRING

struct format_info
{
  char *free_string;
  char *b;
  int len;
  int width;
  int precision;
  char *pad_string;
  int pad_length;
  int column_entries;
  short flags;
  short pos_pad;
  short column_width;
  short column_modulo;
};

static struct format_info format_info_stack[FORMAT_INFO_STACK_SIZE];
static struct format_info *fsp;

#define MIN(X,Y) ((X)<(Y)?(X):(Y))

#define FIELD_LEFT 1
#define FIELD_CENTER 2
#define PAD_POSITIVE 4 
#define LINEBREAK 8
#define COLUMN_MODE 16
#define ZERO_PAD 32
#define ROUGH_LINEBREAK 64
#define DO_TRUNC 128
#define REPEAT 256
#define SNURKEL 512
#define INVERSE_COLUMN_MODE 1024
#define MULTI_LINE 2048
#define WIDTH_OF_DATA 4096
#define MULTI_LINE_BREAK 8192

#define MULTILINE (LINEBREAK | COLUMN_MODE | ROUGH_LINEBREAK | \
		   INVERSE_COLUMN_MODE | MULTI_LINE | REPEAT)

struct processing
{
  struct processing *next;
  struct vector *v;
};

void describe_svalue(struct svalue *s,int indent,struct processing *p)
{
  char buf[40];
  int e,d;
  struct processing doing;

  indent+=2;
  switch(s->type)
  {
    case T_LVALUE:
      my_strcat("lvalue");
      break;

    case T_NUMBER:
      sprintf(buf,"%d",s->u.number);
      my_strcat(buf);
      break;

    case T_STRING:
      {
        char *a;
        my_putchar('"');
        for(a=strptr(s);*a;a++)
        {
          switch(*a)
          {
            case '"':
            case '\\':
              my_putchar('\\');
            default:
              my_putchar(*a);
          } 
        }
        my_putchar('"');
      }
      break;

#ifdef T_FUNCTION
    case T_FUNCTION:
#endif
    case T_OBJECT:
      /* Here true compatibility with v1.05 isn't possible as
       * describe_svalue isn't allowed to call lpc. (because 
       * the svalue we are describing might be in apply_low's
       * static storage
       */
      if(s->u.ob->flags & O_DESTRUCTED)
      {
        free_svalue(s);
        my_putchar('0');
        break;
      }
      if(s->u.ob->obj_index)
      {
        my_strcat(s->u.ob->obj_index);
      }else{
        my_strcat(s->u.ob->prog->name);
        my_putchar('#');
        sprintf(buf,"%d",s->u.ob->clone_number);
        my_strcat(buf);
      }

#ifdef T_FUNCTION
      if(s->type==T_FUNCTION)
      {
        my_strcat("->");
	my_strcat(FUNC(s->u.ob->prog,s->subtype)->name);
      }
#endif
      break;

    case T_POINTER:
      if(!s->u.vec->size)
      {
        my_strcat("({ })");
        break;
      }
      doing.next=p;
      doing.v=s->u.vec;
      for(e=0;p;e++,p=p->next)
      {
	if(p->v==s->u.vec)
	{
	  sprintf(buf,"@%d",e);
	  my_strcat(buf);
	  return;
	}
      }

      sprintf(buf,"({ /* %d elements */\n",s->u.vec->size);
      my_strcat(buf);
      for(d=0;d<s->u.vec->size;d++)
      {
        if(d) my_strcat(",\n");
        for(e=0;e<indent;e++) my_putchar(' ');
        describe_svalue(s->u.vec->item+d,indent,&doing);
      }
      my_putchar('\n');
      for(e=2;e<indent;e++) my_putchar(' ');
      my_strcat("})");
      break;

#ifdef T_INVALID
    case T_INVALID:
      my_strcat("INVALID");
      break;
#endif

#ifdef T_FLOAT
    case T_FLOAT:
      sprintf(buf,"%f",s->u.fnum);
      my_strcat(buf);
      break;
#endif


#ifdef T_LIST
    case T_LIST:
      if(!s->u.vec->item[0].u.vec->size)
      {
        my_strcat("(< >)");
        break;
      }
      doing.next=p;
      doing.v=s->u.vec;
      for(e=0;p;e++,p=p->next)
      {
	if(p->v==s->u.vec)
	{
	  sprintf(buf,"@%d",e);
	  my_strcat(buf);
	  return;
	}
      }

      sprintf(buf,"(< /* %d elements */\n",s->u.vec->item[0].u.vec->size);
      my_strcat(buf);
      for(d=0;d<s->u.vec->item[0].u.vec->size;d++)
      {
        if(d) my_strcat(",\n");
        for(e=0;e<indent;e++) my_putchar(' ');
        describe_svalue(s->u.vec->item[0].u.vec->item+d,indent,&doing);
      }
      my_putchar('\n');
      for(e=2;e<indent;e++) my_putchar(' ');
      my_strcat(">)");
      break;
#endif

#ifdef T_MAPPING
    /* for dworkin's mappings */
    case T_MAPPING:
      if(!s->u.vec->item[0].u.vec->size)
      {
        my_strcat("([ ])");
        break;
      }
      doing.next=p;
      doing.v=s->u.vec;
      for(e=0;p;e++,p=p->next)
      {
	if(p->v==s->u.vec)
	{
	  sprintf(buf,"@%d",e);
	  my_strcat(buf);
	  return;
	}
      }

      sprintf(buf,"([ /* %d elements */\n",s->u.vec->item[0].u.vec->size);
      my_strcat(buf);
      for(d=0;d<s->u.vec->item[0].u.vec->size;d++)
      {
        if(d) my_strcat(",\n");
        for(e=0;e<indent;e++) my_putchar(' ');
        describe_svalue(s->u.vec->item[0].u.vec->item+d,indent,&doing);
        my_putchar(':');
        describe_svalue(s->u.vec->item[1].u.vec->item+d,indent,&doing);
      }
      my_putchar('\n');
      for(e=2;e<indent;e++) my_putchar(' ');
      my_strcat("])");
      break;
#endif

    default:
      my_strcat("<Unknown>");
  }
}

/* Position a string inside a field with fill */

INLINE static void fix_field(
	char *b,
        int len,
	int flags,
	int width,
	char *pad_string,
	int pad_length,
	char pos_pad)
{
  int e,d;
  if(!width)
  {
    if(pos_pad && b[0]!='-') my_putchar(pos_pad);
    my_binary_strcat(b,len);
    return;
  }

  d=0;
  if(!(flags & DO_TRUNC) && len+(pos_pad && b[0]!='-')>=width)
  {
    if(pos_pad && b[0]!='-') my_putchar(pos_pad);
    my_binary_strcat(b,len);
    return;
  }
  if(flags & ZERO_PAD) /* zero pad is kind of special... */
  {
    if(b[0]=='-')
    {
      my_putchar('-');
      b++;
      len--;
      width--;
    }else{
      if(pos_pad)
      {
        my_putchar(pos_pad);
        width--;
      }
    }
    for(;width>len;width--) my_putchar('0');
    while(len--) my_putchar(*(b++));
    return;
  }

  if(flags & FIELD_CENTER)
  {
    e=len;
    if(pos_pad && b[0]!='-') e++;
    for(e=(width-e)/2;e-->0;d++)
    {
      if(d==pad_length) d=0;
      my_putchar(pad_string[d]);
      width--;
    }
    flags|=FIELD_LEFT;
  }

  if(flags & FIELD_LEFT)
  {
    if(pos_pad && b[0]!='-') { my_putchar(pos_pad); width--; d++; }
    d+=MIN(width,len);
    while(len-- && width--) my_putchar(*(b++));
    for(d%=pad_length;width--;d++)
    {
      if(d>=pad_length) d=0;
      my_putchar(pad_string[d]);
    }
    return;
  }

  /* Right-justification */

  if(pos_pad && b[0]!='-') len++;
  for(;width>len;d++)
  {
    if(d>=pad_length) d=0;
    my_putchar(pad_string[d]);
    width--;
  }
  if(pos_pad && b[0]!='-' && len==width)
  {
    my_putchar(pos_pad);
    len--;
    width--;
  }
  b+=len-width;
  while(width--) my_putchar(*(b++));
}

static void free_sprintf_strings(void)
{
  for(;fsp>=format_info_stack;fsp--)
  {
    if(fsp->free_string)
      free(fsp->free_string);
    fsp->free_string=0;
  }
}

static void sprintf_error(char *s,...) ATTRIBUTE((noreturn,format (printf, 1, 2)));
static void sprintf_error(char *s,...)
{
  char buf[100];
  va_list args;
  va_start(args,s);
  free_sprintf_strings();

  sprintf(buf,"Sprintf: %s",s);
  va_error(buf,args);
  va_end(args);
}

/* This is called once for every '%' on every ouputted line
 * it takes care of linebrak and column mode. It returns 1
 * if there is more for next line.
 */

INLINE static int do_one(struct format_info *f)
{
  char *rest;
  int e,d,lastspace;

  rest=NULL;
  if(f->flags & (LINEBREAK|ROUGH_LINEBREAK))
  {
    if(!f->width)
      sprintf_error("Must have field width for linebreak.\n");
    lastspace=-1;
    for(e=0;e<f->len && e<=f->width;e++)
    {
      if(f->b[e]=='\n')
      {
        lastspace=e;
        rest=f->b+e+1;
        break;
      }
      if(f->b[e]==' ' && (f->flags & LINEBREAK))
      {
        lastspace=e;
        rest=f->b+e+1;
      }
    }
    if(e==f->len && f->len<=f->width)
    {
      lastspace=e;
      rest=f->b+lastspace;
    }else if(lastspace==-1){
      lastspace=MIN(f->width,f->len);
      rest=f->b+lastspace;
    }
    fix_field(f->b,lastspace,f->flags,f->width,f->pad_string,
	      f->pad_length,f->pos_pad);
  }else if(f->flags & INVERSE_COLUMN_MODE){
    if(!f->width)
      sprintf_error("Must have field width for column mode.\n");
    e=f->width/(f->column_width+1);
    if(!f->column_width || e<1) e=1;

    rest=f->b;
    for(d=0;rest[d] && e;d++)
    {
      while(rest[d] && rest[d]!='\n') d++;
      fix_field(rest,d,f->flags,f->column_width,f->pad_string,
		f->pad_length,f->pos_pad);
      e--;
      rest+=d;
      d=-1;
      if(*rest) rest++;
    }
  }else if(f->flags & COLUMN_MODE){
    int mod,col;
    if(!f->width)
      sprintf_error("Must have field width for column mode.\n");
    mod=f->column_modulo;
    col=f->width/(f->column_width+1);
    if(!f->column_width || col<1) col=1;
    rest=f->b;
    for(d=0;*rest && d<col;d++)
    {
      for(e=0;rest[e] && rest[e]!='\n';e++);
      fix_field(rest,e,f->flags,f->column_width,
		f->pad_string,f->pad_length,f->pos_pad);

      for(e=0;*rest && e<mod;e++)
      {
	char *s;
	s=STRCHR(rest,'\n');
	if(s)
	  rest=s+1;
	else
 	  rest+=strlen(rest);
      }
      f->column_entries--;
    }
    if(f->column_entries>0)
    {
      for(rest=f->b;*rest && *rest!='\n';rest++);
      if(*rest) rest++;
    }else{
      rest=NULL;
    }
  }else{
    fix_field(f->b,f->len,f->flags,f->width,f->pad_string,f->pad_length,f->pos_pad);
  }
  if(f->flags & REPEAT) return 0;
  if(rest)
  {
    f->len-=rest-f->b;
    f->b=rest;
  }else{
    f->len=0;
    f->b="";
  }
  return f->len>0;
}

char *get_name_of_type(int t)
{
  switch(t)
  {
#ifdef T_INVALID
    case T_INVALID: return "invalid";
#endif
    case T_LVALUE: return "lvalue";
    case T_NUMBER: return "int";
    case T_STRING: return "string";
    case T_POINTER: return "array";
    case T_OBJECT: return "object";
#ifdef T_MAPPING
    case T_MAPPING: return "mapping";
#endif
#ifdef T_LIST
    case T_LIST: return "list";
#endif
#ifdef T_ALIST_PART
    case T_ALIST_PART: return "alist part";
#endif
#ifdef T_FUNCTION
    case T_FUNCTION: return "function";
#endif
#ifdef T_REAL
    case T_REAL: return "real";
#endif
#ifdef T_FLOAT
    case T_FLOAT: return "float";
#endif
    default: return "unknown";
  }
}

#define GET_SVALUE(VAR) \
  if(arg) \
  { \
    VAR=arg; \
    arg=0; \
  }else{ \
    if(!(num_arg--)) \
    { \
      sprintf_error("Too few arguments to sprintf.\n"); \
      break; /* make gcc happy */ \
    } \
    VAR=lastarg=(argp++); \
  }

#define GET(VAR,LPC_TYPE,TYPE_NAME,EXTENSION) \
  { \
    struct svalue *tmp_; \
    GET_SVALUE(tmp_); \
    if(tmp_->type!=LPC_TYPE) \
    { \
      sprintf_error("Expected %s, got %s.\n",TYPE_NAME, \
	get_name_of_type(tmp_->type)); \
      break; /* make gcc happy */ \
    } \
    VAR=tmp_->u.EXTENSION; \
  }

#define GET_STRING(VAR) \
  { \
    struct svalue *tmp_; \
    GET_SVALUE(tmp_); \
    if(tmp_->type!=T_STRING) \
    { \
      sprintf_error("Expected string, got %s.\n",\
		    get_name_of_type(tmp_->type)); \
      break; /* make gcc happy */ \
    } \
    VAR=strptr(tmp_); \
  }

#define GET_INT(VAR) GET(VAR,T_NUMBER,"integer",number)
#define GET_FLOAT(VAR) GET(VAR,T_FLOAT,"float",fnum)
#define GET_ARRAY(VAR) GET(VAR,T_POINTER,"array",vec)
#define GET_OBJECT(VAR) GET(VAR,T_OBJECT,"object",ob)

#define DO_OP() \
   if(fsp->flags & SNURKEL) \
   { \
     if(!v) \
     { \
       GET_ARRAY(v); \
       if(!v->size) { fsp--; break; } \
       arg=v->item; \
       pos=1; \
     }else{ \
       if(pos>=v->size) { fsp--; break; } \
       arg=v->item+pos; \
       pos++; \
     } \
   }

/* This is the main lpc_sprintf function, note that it calls itself
 * recursively during the '%{ %}' parsing. The string is stored in
 * the buffer in save_objectII.c
 */

static string low_lpc_sprintf(
	char *format,
	int format_len,
	struct svalue *argp,
	int num_arg,
        string prefix)
{
  int tmp,setwhat,pos,d,e;
  char *a;
  char buffer[40];
  struct format_info *f,*start;
  float tf;
  struct svalue *arg; /* pushback argument */
  struct svalue *lastarg=0;
  struct vector *v;

  start=fsp;
  for(a=format;a<format+format_len;a++)
  {
    eval_cost++;
    fsp++;
    if(fsp-format_info_stack==FORMAT_INFO_STACK_SIZE)
      sprintf_error("Sprintf stack overflow.\n");
    fsp->pad_string=" ";
    fsp->pad_length=1;
    fsp->free_string=NULL;
    fsp->column_width=0;
    fsp->pos_pad=fsp->flags=fsp->width=fsp->precision=0;

    if(*a!='%')
    {
      for(e=0;a[e] && a[e]!='%';e++);
      fsp->b=a;
      fsp->len=e;
      fsp->width=e;
      a+=e-1;
      continue;
    }
    arg=NULL;
    v=NULL;
    setwhat=pos=0;

    for(a++;;a++)
    {
      switch(*a)
      {
        default:
          sprintf_error("Error in format string.\n");
          fatal("Foo, you shouldn't be here!\n");

        /* First the modifyers */

        case '0': fsp->flags|=ZERO_PAD; continue;
        case '1': case '2': case '3':
        case '4': case '5': case '6':
        case '7': case '8': case '9':
          tmp=STRTOL(a,&a,10);
	  a--;
          goto got_arg;

        case '*':
          GET_INT(tmp);

        got_arg:
          if(tmp<1 || tmp>4096)
            sprintf_error("Illigal width.\n");
          switch(setwhat)
          {
            case 0: fsp->width=tmp; break;
            case 1: fsp->width=tmp;
            case 2: fsp->precision=tmp; break;
            case 3: fsp->column_width=tmp; break;
          }
          continue;

        case ';': setwhat=3; continue;
        case '.': setwhat=2; continue;
        case ':': setwhat=1; continue;

        case '=': fsp->flags|=LINEBREAK; continue;
        case '/': fsp->flags|=ROUGH_LINEBREAK; continue;
        case '#': fsp->flags|=COLUMN_MODE; continue;
        case '$': fsp->flags|=INVERSE_COLUMN_MODE; continue;

        case '-': fsp->flags|=FIELD_LEFT; continue;
        case '|': fsp->flags|=FIELD_CENTER; continue;
        case ' ': fsp->pos_pad=' '; continue;
        case '+': fsp->pos_pad='+'; continue;
        case '!': fsp->flags^=DO_TRUNC; continue;
        case '^': fsp->flags|=REPEAT; continue;
        case '@': fsp->flags|=SNURKEL; continue;
        case '>': fsp->flags|=MULTI_LINE_BREAK; continue;
        case '_': fsp->flags|=WIDTH_OF_DATA; continue;

        case '\'':
          tmp=0;
          for(a++;a[tmp]!='\'';tmp++)
            if(!a[tmp])
              sprintf_error("Unfinished pad string in format string.\n");
          if(tmp)
          {
            fsp->pad_string=a;
            fsp->pad_length=tmp;
          }
          a+=tmp;
          continue;

        case '<':
	  if(!lastarg)
             sprintf_error("No last argument.\n");
	  arg=lastarg;
	  continue;

        /* now the real operators */

        case '{':
	{
	  struct vector *w;
	  string b;
	  DO_OP();
	  for(e=1,tmp=1;tmp;e++)
	  {
	    if(!a[e])
	    {
	      sprintf_error("Missing %%} in format string.\n");
	      break;		/* UNREACHED */
	    }
	    if(a[e]=='%')
	    {
	      switch(a[e+1])
	      {
	      case '%': e++; break;
	      case '}': tmp--; break;
	      case '{': tmp++; break;
	      }
	    }
	  }
            
	  GET_ARRAY(w);
	  if(!w->size)
	  {
	    fsp->b="";
	    fsp->len=0;
	  }else{
	    b.str=NULL;
	    b.len=0;
	    for(tmp=0;tmp<w->size;tmp++)
	    {
	      check_eval_cost();
	      if(w->item[tmp].type==T_POINTER)
	      {
		b=low_lpc_sprintf(a+1,e-2,w->item[tmp].u.vec->item,
				  w->item[tmp].u.vec->size,b);
	      }else{
		b=low_lpc_sprintf(a+1,e-2,w->item+tmp,1,b);
	      }
	    }
	    fsp->b=b.str;
	    fsp->len=b.len;
	    fsp->free_string=fsp->b;
	  }
	  a+=e;
	  break;
	}

        case '%':
          fsp->b="%";
          fsp->len=fsp->width=1;
          break;

        case 'n':
          DO_OP();
          fsp->b="";
          fsp->len=0;
          break;

        case 't':
          {
            struct svalue *t;
            DO_OP();
            GET_SVALUE(t);
            fsp->b=get_name_of_type(t->type);
            fsp->len=strlen(fsp->b);
            break;
          }

        case 'c':
          DO_OP();
          fsp->b=(char *)alloca(1);
          GET_INT(fsp->b[0]);
          fsp->len=1;
          break;

        case 'o':
        case 'd':
        case 'u':
        case 'x':
        case 'X':
          DO_OP();
          GET_INT(tmp);
          buffer[0]='%';
          buffer[1]=*a;
          buffer[2]=0;
          fsp->b=(char *)alloca(100);
          sprintf(fsp->b,buffer,tmp);
          fsp->len=strlen(fsp->b);
          break;

        case 'e':
        case 'f':
        case 'g':
          DO_OP();
          fsp->b=(char *)xalloc(fsp->width+10);
          sprintf(buffer,"%%*.*%c",*a);
          GET_FLOAT(tf)
          sprintf(fsp->b,buffer,
		  fsp->width?fsp->width:8,
		  fsp->precision?fsp->precision:3,tf);
          fsp->len=strlen(fsp->b);
          fsp->free_string=fsp->b;
          break;

        case 'D':
          {
	    string s;
            struct svalue *t;
            DO_OP();
            GET_SVALUE(t);
            init_buf();
	    save_style=SAVE_AS_ONE_LINE;
            save_svalue(t);
	    s=complex_free_buf();
	    fsp->b=s.str;
	    fsp->len=s.len;
            fsp->free_string=fsp->b;
            break;
          }

        case 'O':
          {
	    string s;
            struct svalue *t;
            DO_OP();
            GET_SVALUE(t);
            if(argp->type!=T_STRING)
            {
              init_buf();
              describe_svalue(t,0,0);
	      s=complex_free_buf();
              fsp->b=s.str;
	      fsp->len=s.len;
              fsp->free_string=fsp->b;
              break;
            }else{
              arg=t;
            }
          }

        case 's':
          DO_OP();
          GET_STRING(fsp->b);
          fsp->len=strlen(fsp->b);
	  break;
      }
      if((fsp->flags & SNURKEL) && v->size>pos)
      {
        eval_cost++;
        check_eval_cost();
        fsp++;
        if(fsp-format_info_stack==FORMAT_INFO_STACK_SIZE)
          sprintf_error("Format stack overflow.\n");
        fsp[0]=fsp[-1];
        fsp->free_string=NULL; /* we don't want to free it twice */
        a--;
        continue;
      }
      break;
    }
  }

  for(f=fsp;f>start;f--)
  {
    if(f->flags & WIDTH_OF_DATA) f->width=f->len;
    if(((f->flags & INVERSE_COLUMN_MODE) && !f->column_width) ||
       (f->flags & COLUMN_MODE))
    {
      int max_len,nr,columns;
      for(max_len=nr=tmp=e=0;e<f->len;e++)
      {
	if(f->b[e]=='\n')
	{
	  nr++;
	  if(max_len<tmp) max_len=tmp;
	  tmp=0;
	}
	tmp++;
      }
      nr++;
      if(max_len<tmp) max_len=tmp;
      if(!f->column_width) f->column_width=max_len;
      f->column_entries=nr;
      columns=f->width/(f->column_width+1);
      if(f->column_width<1 || columns<1) columns=1;
      f->column_modulo=(nr+columns-1)/columns;
    }
  }

  /* Here we do some DWIM */
  for(f=fsp-1;f>start;f--)
  {
    if((f[1].flags & MULTILINE) &&
       !(f[0].flags & (MULTILINE|MULTI_LINE_BREAK)))
    {
      for(e=0;e<f->len && f->b[e]!='\n';e++);
      if(e==f->len) f->flags|=MULTI_LINE;
    }
  }
  init_buf_with_string(prefix);
  for(f=start+1;f<=fsp;)
  {
    for(;f<=fsp && !(f->flags&MULTILINE);f++) do_one(f);
    do
    {
      d=0;
      for(e=0;f+e<=fsp && (f[e].flags & MULTILINE);e++) d|=do_one(f+e);
      if(d) my_putchar('\n');
    }while(d);
    for(;f<=fsp && (f->flags&MULTILINE);f++);
  }
  while(fsp>start)
  {
    if(fsp->free_string) free(fsp->free_string);
    fsp->free_string=0;
    fsp--;
  }
  return complex_free_buf();
}

/* An C-callable lpc_sprintf */
string lpc_sprintf(char *format,struct svalue *argp,int num_arg)
{
  string prefix;
  prefix.str=0;
  prefix.len=0;

  free_sprintf_strings();
  fsp=format_info_stack-1;
  return low_lpc_sprintf(format,strlen(format),argp,num_arg,prefix);
}

/* The efun */
void f_sprintf(int num_arg,struct svalue *argp)
{
  string s,prefix;
  prefix.str=0;
  prefix.len=0;

  free_sprintf_strings();
  fsp=format_info_stack-1;

  s=low_lpc_sprintf(strptr(argp+0),my_strlen(argp),argp+1,num_arg-1,prefix);
  pop_n_elems(num_arg);
  push_shared_string(make_shared_binary_string(s.str,s.len));
  free(s.str);
}

/* func_spec.c entry: */
/* side_fx string sprintf(string,mixed|void,...); */

