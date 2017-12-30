#ifndef GLOBAL_H
#define GLOBAL_H
/*
 * Some structure forward declarations are needed.
 */

/* This is needed for linux */
#ifdef MALLOC_smalloc
#define NO_FIX_MALLOC
#endif

struct program;
struct function;
struct svalue;
struct sockaddr;
struct object;
struct vector;
struct svalue;

#include "config.h"

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif

#include "port.h"

#if defined(__GNUC__) && !defined(DEBUG) && !defined(lint)
#define INLINE inline
#else
#define INLINE
#endif

#ifdef BUFSIZ
#define PROT_STDIO(x) PROT(x)
#else
#define PROT_STDIO(x) ()
#endif

#ifdef __STDC__
#define PROT(x) x
#else
#define PROT(x) ()
#endif

#ifdef MALLOC_DECL_MISSING
char *malloc PROT((int));
char *realloc PROT((char *,int));
void free PROT((char *));
char *calloc PROT((int,int));
#endif

#ifdef GETPEERNAME_DECL_MISSING
int getpeername PROT((int, struct sockaddr *, int *));
#endif

#ifdef GETHOSTNAME_DECL_MISSING
void gethostname PROT((char *,int));
#endif

#ifdef POPEN_DECL_MISSING
FILE *popen PROT((char *,char *));
#endif

#ifdef GETENV_DECL_MISSING
char *getenv PROT((char *));
#endif

#ifndef MSDOS

#if !defined(AMIGA_UNIX) && !defined(__STDC__)
int read PROT((int, char *, int));
#endif
#if !defined(sgi) && !defined(AMIGA_UNIX) && !defined(linux)
/* int mkdir PROT((const char *, int)); */
#endif
int fclose PROT_STDIO((FILE *));
int pclose PROT_STDIO((FILE *));
#if !defined(_DCC) && !defined(__STDC__)
int chdir PROT((char *));
#endif
void abort PROT((void));
int fflush PROT_STDIO((FILE *));
#ifndef __STDC__
int rmdir PROT((char *));
#ifndef _DCC
int unlink PROT((char *));
#endif
#endif
int fclose PROT_STDIO((FILE *));
int fseek PROT_STDIO((FILE *, long, int));
#ifndef __STDC__
int fork PROT((void));
#endif
#ifndef AMIGA_UNIX
int wait PROT((int *));
/* int execl PROT((char *, char *, ... )); */
#endif
int pipe PROT((int *));
int dup2 PROT((int, int));
#ifndef __STDC__
int vfork PROT((void));
#endif
void exit PROT((int));
#ifndef AMIGA_UNIX
/* int _exit PROT((int)); */
#endif
unsigned int alarm PROT((unsigned int));
#if ! defined(AMIGA_UNIX) && !defined(pyr) && !defined(linux) && !defined(__STDC__)
int ioctl PROT((int, ... ));
#endif
int close PROT((int));
#if !defined(AMIGA_UNIX) && !defined(__STDC__)
int write PROT((int, char *, int));
#endif
int _filbuf();
#ifdef sun
char *_crypt PROT((char *, char *));
#endif

long random PROT((void));

#ifndef __STDC__
int link PROT((char *, char *));
#endif
#endif

void yyerror PROT((char *));
struct object *environment PROT((struct svalue *));
struct object *first_inventory PROT((struct svalue *));
struct object *dump_trace PROT((int));
void init_num_args PROT((void));
void tell_object PROT((char *));
void startshutdowngame PROT((void));
int parse PROT((char *, struct svalue *, char *, struct svalue *, int));
int indent_program PROT((char *));
struct variable *find_status PROT((char *, int));
void free_prog PROT((struct program *, int));
char *heart_beat_status PROT((int verbose));
void slow_shut_down PROT((int));
void load_first_objects PROT((void));
int random_number PROT((int));
void reset_object PROT((struct object *, int));
int get_current_time PROT((void));
char *time_string PROT((int));
void remove_object_from_stack PROT((struct object *ob));
void compile_file PROT((void));
struct program *find_program(char *);
struct program *find_program2(char *);
struct object *find_hashed(char *);
int data_size(struct object *);
void set_inc_list PROT((struct svalue *sv));
struct object *lookup_my_object_hash(char *);

#if !defined(ultrix) && !defined(M_UNIX) && !defined(sgi)\
 && !defined(AMIGA_UNIX)
extern int rename PROT((const char *,const char *));
#endif


#define BASEOF(ptr, str_type, field)  \
((struct str_type *)((char*)ptr - (char*)& (((struct str_type *)0)->field)))

#define NELEM(a) (sizeof (a) / sizeof ((a)[0]))

typedef void (*func_t) PROT((int,struct svalue *));
typedef void (*opcode_t) PROT((void));


#ifdef MALLOC_DEBUG
void check_sfltable();
#endif
void fsort(void *base,long elms,long elmSize,int (*cmpfunc)(const void *, const void *));
void msort(void *base,long elms,long elmSize,int (*cmpfunc)(const void *, const void *));

#endif
