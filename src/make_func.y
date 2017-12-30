%{
#include "global.h"
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>

#include "interpret.h"
#define _YACC_

extern int yydebug;
#ifndef YYDEBUG
#define YYDEBUG
#endif


char *func_spec,*language,*the_lang,*efun_protos;
char *opcode_statistics,*opcode_lang,*opc_cost;


#ifndef BUFSIZ
#define BUFSIZ 		1024
#endif

#define NELEMS(arr) 	(sizeof arr / sizeof arr[0])

int num_buff;
int report_newline=0;

char my_tolower(char s)
{
  if(isupper(s)) return tolower(s);
  return s;
}

char my_toupper(char s)
{
  if(islower(s)) return toupper(s);
  return s;
}

/* For quick sort purposes : */
struct instruction
{
 /* input from func_spec */
  char *key;
  char *buf;
  char has_token;
  char is_efun;

 /* input opcode_statistics */
  int nr;
  char *name;
  int used;
  int compiled;
  float cost;
  float time;

/* output data */
  int eval_cost;
};

struct instruction instrs[MAX_FUNC];

int min_arg = -1, limit_max = 0;

/*
 * arg_types is the types of all arguments. A 0 is used as a delimiter,
 * marking next argument. An argument can have several types.
 */
int arg_types[MAX_FUNC*MAX_LOCAL], last_current_type;

/*
 * Store the types of the current efun. They will be copied into the
 * arg_types list if they were not already there (to save memory).
 */
int curr_arg_types[MAX_LOCAL], curr_arg_type_size;

void yyerror PROT((char *));
int yylex();
int yyparse();
int ungetc PROT((int c, FILE *f));
char *type_str PROT((int)), *etype PROT((int)), *etype1 PROT((int)),
   *ctype PROT((int));
#ifndef toupper
int toupper PROT((int));
#endif

void fatal(char *str, ...) ATTRIBUTE((noreturn,format (printf,1,2)));
void fatal(char *str, ...)
{
  va_list args;

  va_start(args,str);
  VFPRINTF(stderr, str, args);
  va_end(args);
  exit(1);
}

%}
%union {
    int number;
    char *string;
}

%token ID TOKEN NEWLINE

%token VOID INT STRING OBJECT MAPPING LIST MIXED UNKNOWN FLOAT FUNCTION
%token DEFAULT CONST SIDE_FX REGULAR_EXPRESSION

%type <number> type VOID INT STRING OBJECT MAPPING LIST MIXED UNKNOWN REGULAR_EXPRESSION
%type <number> arg_list basic typel CONST cons
%type <number> arg_type typel2 FLOAT FUNCTION

%type <string> ID optional_ID optional_default

%%

funcs: /* empty */ | funcs func ;

optional_ID: ID | /* empty */ { $$ = ""; } ;

optional_default: DEFAULT ':' ID { $$ = $3; } | /* empty */ { $$="0"; } ;

func: type ID optional_ID '(' arg_list optional_default ')' ';'
    {
      char buff[500];
      char f_name[500];
      int i;
      if (min_arg == -1)  min_arg = $5;
      if ($3[0] == '\0')
      {
	int len;
	if (strlen($2) + 1 + 2 > sizeof f_name)
	  fatal("A local buffer was too small!(1)\n");
	sprintf(f_name, "F_%s", $2);
	len = strlen(f_name);
	for (i=0; i < len; i++)
        {
	  if (islower(f_name[i]))  f_name[i] = toupper(f_name[i]);
	}
        instrs[num_buff].has_token=1;
      }else{
	if (strlen($3) + 1 > sizeof f_name)
	  fatal("A local buffer was too small(2)!\n");
	strcpy(f_name, $3);
	instrs[num_buff].has_token=0;
      }
      for(i=0; i < last_current_type; i++)
      {
	int j;
	for (j = 0; j+i<last_current_type && j < curr_arg_type_size; j++)
	{
	  if (curr_arg_types[j] != arg_types[i+j])
  	    break;
	}
	if (j == curr_arg_type_size) break;
      }
      if (i == last_current_type)
      {
	int j;
	for (j=0; j < curr_arg_type_size; j++)
        {
	  arg_types[last_current_type++] = curr_arg_types[j];
	  if (last_current_type == NELEMS(arg_types))
	    yyerror("Array 'arg_types' is too small");
	}
      }
      sprintf(buff, "{\"%s\",%s,%d,%d,%s,%s,%s,%d,%s,f_%s},\n",
	      $2,
	      f_name,
	      min_arg,
	      limit_max ? -1 : $5,
	      ctype($1),
	      etype(0),
	      etype(1),
	      i,
	      $6,
	      $2);
      if (strlen(buff) > sizeof buff)
        fatal("Local buffer overwritten !\n");

      instrs[num_buff].key = (char *) malloc(strlen($2)+1 );
      strcpy(instrs[num_buff].key, $2);

      instrs[num_buff].buf = (char *) malloc(strlen(buff) + 1);
      strcpy(instrs[num_buff].buf, buff);

      instrs[num_buff].is_efun=1;
      num_buff++;
      min_arg = -1;
      limit_max = 0;
      curr_arg_type_size = 0;
    }
   | TOKEN token_list NEWLINE
   ;

token_list:
          | token_list ID
          {
            int e;
            char buff[500];
            char funname[500];

            for(e=0;$2[e];e++) funname[e]=my_tolower($2[e]);

            funname[e]=0;

            instrs[num_buff].key=malloc(strlen(funname)-1);
            strcpy(instrs[num_buff].key,funname+2);

	    sprintf(buff, "{\"%s\",%s,-1,0,0,0,0,0,0,NULL},\n",
		    funname+2,
		    $2);

            instrs[num_buff].is_efun=0;
            instrs[num_buff].buf = (char *) malloc(strlen(buff)+1);
            strcpy(instrs[num_buff].buf, buff);
            instrs[num_buff].has_token=1;
            num_buff++;
          };
          

cons:       { $$=0; }
    | CONST { $$=0x20000; }
    | SIDE_FX { $$=0x40000; }

type: cons basic     { $$ = $1 | $2; }
    | cons basic '*' { $$ = $1 | $2 | 0x10000; };

basic: VOID | FLOAT | FUNCTION | INT | STRING | MIXED | UNKNOWN | OBJECT
   | MAPPING | LIST | REGULAR_EXPRESSION;

arg_list: /* empty */		{ $$ = 0; }
	| typel2		{ $$ = 1; if ($1) min_arg = 0; }
	| arg_list ',' typel2 	{ $$ = $1 + 1; if ($3) min_arg = $$ - 1; } ;

typel2: typel
    {
	$$ = $1;
	curr_arg_types[curr_arg_type_size++] = 0;
	if (curr_arg_type_size == NELEMS(curr_arg_types))
	    yyerror("Too many arguments");
    } ;

arg_type: type
    {
	if ($1 != VOID) {
	    curr_arg_types[curr_arg_type_size++] = $1;
	    if (curr_arg_type_size == NELEMS(curr_arg_types))
		yyerror("Too many arguments");
	}
	$$ = $1;
    } ;

typel: arg_type			{ $$ = ($1 == VOID && min_arg == -1); }
     | typel '|' arg_type 	{ $$ = (min_arg == -1 && ($1 || $3 == VOID));}
     | '.' '.' '.'		{ $$ = min_arg == -1 ; limit_max = 1; } ;

%%

struct type {
    char *name;
    int num;
} types[] = {
{ "void", VOID },
{ "int", INT },
{ "string", STRING },
{ "object", OBJECT },
{ "mapping", MAPPING },
{ "regular_expression", REGULAR_EXPRESSION },
{ "mixed", MIXED },
{ "unknown", UNKNOWN },
{ "float", FLOAT},
{ "function", FUNCTION},
{ "list", LIST },
{ "const", CONST },
{ "side_fx", SIDE_FX },
{ "default", DEFAULT },
};

FILE *f;
int current_line = 1;

typedef int (*cmpfuntyp) (const void *,const void *);

int time_cmp(const struct instruction *a,const struct instruction *b)
{
  return a->time-b->time;
}

void read_opc_statistics()
{
  FILE *ff;
  int e,d;
  int nr,compiled,used,post_cost;
  float time,cost;
  char name[1000];
  char buffer[1000];
  char buffer2[1000];
  char buffer3[1000];

  float sum_time;
  float x;
  int a,b,c;

  for(e=0;e<MAX_FUNC;e++) instrs[e].eval_cost=AVERAGE_COST;
  f=fopen(opcode_statistics,"r");
  ff=fopen(opcode_lang,"r");
  if(f && ff)
  {
    fgets(name,sizeof(name),f);
    for(e=0;!feof(f);e++)
    {
      fgets(buffer2,sizeof(buffer2),f);
      sscanf(buffer2,"%d %d %d %d %f %f %s",&nr,
	     &post_cost,&used,&compiled,&cost,&time,name);
      fseek(ff,0,0);
      d=-1;
      while(!feof(ff))
      {
	fgets(buffer,sizeof(buffer),ff);
	if(!strncmp(buffer,"#define",7))
	{
	  sscanf(buffer+7,"%s %d",buffer3,&d);
	  if(d==nr+F_OFFSET) break;
	}
      }
      if(d!=nr+F_OFFSET) continue;
      for(a=0;a<strlen(buffer3);a++) buffer3[a]=my_tolower(buffer3[a]);

      for(a=0;a<num_buff;a++)
        if(!strcmp(buffer3+2,instrs[a].key)) break;

      instrs[a].nr=nr;
      if(!instrs[a].key)
      {
        instrs[a].key=(char *)malloc(strlen(buffer3+2)+1);
        strcpy(instrs[a].key,buffer3+2);
      }
      instrs[a].used=used;
      instrs[a].compiled=compiled;
      instrs[a].cost=cost;  
      instrs[a].time=time;
    }
  }
  if(f) fclose(f);
  if(ff) fclose(ff);

  qsort((char *)instrs,num_buff,sizeof(instrs[0]),(cmpfuntyp)time_cmp);
  for(a=0;a<num_buff;a++)
    if(instrs[a].time!=0.0)
      break;
  sum_time=instrs[a+(num_buff-a)/2].time;

  for(e=0;e<num_buff;e++)
  {
    if(instrs[e].used)
    {
      x=instrs[e].time * (float)AVERAGE_COST / sum_time;
      if(x>(float)MAX_COST_PER_INSTR)
      {
	x=(float)MAX_COST_PER_INSTR;
	a=1;
      }
      if(x<=1.0)
      {
	x=1.0;
      }
      instrs[e].eval_cost=(int)x;
    }else{
      instrs[e].eval_cost=AVERAGE_COST;
    }
  }

  if(!(f=fopen(opc_cost,"w")))
  {
    fprintf(stderr,"Failed to open file opc_cost.h\n");
    exit(10);
  }
  fprintf(f,"/* This file was automatically generated by make_func, */\n");
  fprintf(f,"/* DO NOT CHANGE IT! -Profezzorn */\n\n");
  
  fprintf(f,"int func_cost_alist[] = {\n");
  b=c=0;
  for(e=0;e<num_buff;e++)
  {
    if(instrs[e].used>0)
    {
      if(!strcmp(instrs[e].key,"return") &&
	 !strcmp(instrs[e].key,"dumb_return"))
	instrs[e].eval_cost=AVERAGE_COST;

      for(d=0;d<strlen(instrs[e].key);d++)
	name[d]=my_toupper(instrs[e].key[d]);
      name[d]=0;
      c+=instrs[e].eval_cost;
      b++;
      fprintf(f,"  F_%-28s, %d,\n",name,instrs[e].eval_cost);
    }
  }
  fprintf(f,"} ;\n");
  fprintf(f,"/* Actual average %f */\n",(float)c/(float)b);
  fclose(f);
}

int used_cmp(struct instruction *a,struct instruction *b)
{
  return b->used-a->used;
}

int main(int argc,char ** argv)
{
  FILE *fdr,*fdw;
  int i;
  char buffer[BUFSIZ + 1],*c;
  void make_efun_table();

#ifdef YYDEBUG
  while(argc>1 &&  argv[1][0]=='-' && argv[1][1]=='y' && argv[1][2]==0)
  {
    argv++;
    argc--;
    yydebug++;
  }
#endif
  if(argc!=8)
  {
    fprintf(stderr,"Wrong number of arguments to make_func.\n");
    return 11;
  }
  func_spec=argv[1];
  language=argv[2];
  the_lang=argv[3];
  efun_protos=argv[4];
  opcode_statistics=argv[5];
  opcode_lang=argv[6];
  opc_cost=argv[7];

  if ((f = fopen(func_spec, "r")) == NULL) 
  {
    perror(func_spec);
    exit(1);
  }
  yyparse();
  fclose(f);

  read_opc_statistics();
  /* Now sort the main_list */
  qsort((char *)instrs,num_buff,sizeof(instrs[0]),(cmpfuntyp)used_cmp);
  make_efun_table();
  /* Now display it... */

  printf("{\n");
  for (i = 0; i < num_buff; i++)
  {
    if(instrs[i].buf)
      printf("%s", instrs[i].buf);
  }

  printf("\n};\nint efun_arg_types[] = {\n");

  for (i=0; i < last_current_type; i++)
  {
    if (arg_types[i] == 0)
      printf("0,\n");
    else
      printf("%s,", ctype(arg_types[i]));
  }
  printf("};\n");
  /*
   * Write all the tokens out.  Do this by copying the
   * pre-include portion of lang.y to lang.y, appending
   * this information, then appending the post-include
   * portion of lang.y.  It's done this way because I don't
   * know how to get YACC to #include %token files.  *grin*
   */
  if (!(fdr = fopen(language, "r"))) {
    perror(language);
    exit(1);
  }

  if (!(fdw = fopen(the_lang, "w")))
  {
    perror(the_lang);
    exit(1);
  }

  while((c=fgets(buffer,BUFSIZ,fdr)))
  {
    fwrite(buffer,1,strlen(buffer),fdw);
    if(!strcmp(c,"/* MAGIC MARKER */\n")) break;
  }
  
  for (i = 0; i < num_buff; i++)
  {
    int ch;
    if (instrs[i].has_token)
    {
      char *str;		/* It's okay to mung instrs[*] now */
      for (str = instrs[i].key; *str; str++)
	*str = my_toupper(*str);
      fprintf(fdw, "%%token F_%s\n", instrs[i].key);
      if(c)
      {
	ch=getc(fdr);
	if(ch!='\n') ungetc(ch,fdr);
      }
    }
  }

  while((c=fgets(buffer,BUFSIZ,fdr)))
    fwrite(buffer,1,strlen(buffer),fdw);

  fclose(fdr);
  fclose(fdw);
  return 0;
}

void yyerror(char *str)
{
  fprintf(stderr, "%s:%d: %s\n", func_spec, current_line, str);
  exit(1);
}

int ident(int c)
{
  char buff[100];
  int len, i;

  for (len=0; isalnum(c) || c == '_' || c=='%'; c = getc(f))
  {
    buff[len++] = c;
    if (len + 1 >= sizeof buff)
      fatal("Local buffer in ident() too small!\n");
    if (len == sizeof buff - 1)
    {
      yyerror("Too long indentifier");
      break;
    }
  }
  (void)ungetc(c, f);
  buff[len] = '\0';
  if(!strcmp(buff,"%token"))
  {
    report_newline=1;
    return TOKEN;
  }
     
  for (i=0; i < NELEMS(types); i++)
  {
    if (strcmp(buff, types[i].name) == 0)
    {
      yylval.number = types[i].num;
      return types[i].num;
    }
  }
  yylval.string = (char *)malloc(strlen(buff)+1);
  strcpy(yylval.string, buff);
  return ID;
}

char *type_str(int n)
{
  int i, type = n & 0xffff;

  for (i=0; i < NELEMS(types); i++)
  {
    if (types[i].num == type)
    {
      if (n & 0x10000)
      {
	static char buff[100];
	if (strlen(types[i].name) + 3 > sizeof buff)
	  fatal("Local buffer too small in type_str()!\n");
	sprintf(buff, "%s *", types[i].name);
	return buff;
      }
      return types[i].name;
    }
  }
  return "What?";
}

int yylex1()
{
  register int c;
    
  for(;;)
  {
    switch(c = getc(f))
    {
    case ' ':
    case '\t': continue;

    case '#':
    {
      int line;
      /* does any operating system support longer pathnames? */
      char aBuf[2048];

      fgets(aBuf, 2047, f);
      if (sscanf(aBuf, "%d", &line) == 2) current_line = line;
      current_line++;
      continue;
    }
    case '\n':
      current_line++;
      if(report_newline)
      {
	report_newline=0;
	return NEWLINE;
      }
      continue;

    case EOF: return -1;
    default:
      if (isalpha(c) || c=='%' || c=='_') return ident(c);
      return c;
    }
  }
}

int yylex() { return yylex1(); }

char *etype1(int n)
{
  if (n & 0x10000) return "BT_POINTER";
  switch(n)
  {
  case FLOAT: return "BT_FLOAT";
  case FUNCTION: return "BT_FUNCTION";
  case INT: return "BT_NUMBER";
  case OBJECT: return "BT_OBJECT";
  case MAPPING: return "BT_MAPPING";
  case REGULAR_EXPRESSION: return "BT_REGEXP";
  case LIST: return "BT_LIST";
  case STRING: return "BT_STRING";
  case MIXED:  return "0";		/* 0 means any type */
  default: yyerror("Illegal type for argument");
  }
  return "What?";
}

char *etype(int n)
{
  int i;
  int local_size = 100;
  char *buff = (char *)malloc(local_size);

  for (i=0; i < curr_arg_type_size; i++)
  {
    if (n == 0) break;
    if (curr_arg_types[i] == 0) n--;
  }
  if (i == curr_arg_type_size) return "0";
  buff[0] = '\0';
  for(; curr_arg_types[i] != 0; i++)
  {
    char *p;
    if (curr_arg_types[i] == VOID) continue;
    if (buff[0] != '\0') strcat(buff, "|");
    p = etype1(curr_arg_types[i]);
    /*
     * The number 2 below is to include the zero-byte and the next
     * '|' (which may not come).
     */
    if (strlen(p) + strlen(buff) + 2 > local_size)
    {
      fprintf(stderr, "Buffer overflow!\n");
      exit(1);
    }
    strcat(buff, etype1(curr_arg_types[i]));
  }
  if (!strcmp(buff, ""))  strcpy(buff, "BT_ANY");
  return buff;
}

char *ctype(int n)
{
  static char buff[200];	/* 100 is such a comfortable size :-) */
  char *p;

  buff[0] = '\0';

  if(n & 0x20000) strcat(buff,"TYPE_MOD_CONSTANT|");
  if(n & 0x40000) strcat(buff,"TYPE_MOD_SIDE_EFFECT|");
  if (n & 0x10000) strcat(buff, "TYPE_MOD_POINTER|");

  n &= 0xffff;
  switch(n)
  {
  case FLOAT: p = "TYPE_FLOAT"; break;
  case FUNCTION: p = "TYPE_FUNCTION"; break;
  case VOID: p = "TYPE_VOID"; break;
  case STRING: p = "TYPE_STRING"; break;
  case INT: p = "TYPE_NUMBER"; break;
  case OBJECT: p = "TYPE_OBJECT"; break;
  case MAPPING: p = "TYPE_MAPPING"; break;
  case REGULAR_EXPRESSION: p = "TYPE_REGULAR_EXPRESSION"; break;
  case LIST:   p = "TYPE_LIST"; break;
  case MIXED: p = "TYPE_ANY"; break;
  case UNKNOWN: p = "TYPE_UNKNOWN"; break;
  default:
    yyerror("Bad type!");
    return "";
  }
  strcat(buff, p);
  if (strlen(buff) + 1 > sizeof buff)
    fatal("Local buffer overwritten in ctype()");
  return buff;
}

void make_efun_table()
{
  FILE *fp;
  int i;

  fp = fopen(efun_protos,"w");
  if (!fp)
  {
    fprintf(stderr,"make_func: unable to open %s\n",efun_protos);
    exit(-1);
  }
  fprintf(fp,"/*\n\tThis file is automatically generated by make_func.\n");
  fprintf(fp,"\tdo not make any manual changes to this file.\n*/\n\n");
  for (i = 0; i < num_buff; i++)
  {
    if(instrs[i].is_efun)
      fprintf(fp,"void f_%s PROT((int, struct svalue *));\n",instrs[i].key);
    else
      fprintf(fp,"void f_%s PROT((void));\n",instrs[i].key);
  }
  fclose(fp);

}

