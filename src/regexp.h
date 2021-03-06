#ifndef REGEXP_H
#define REGEXP_H
/*
 * Definitions etc. for regexp(3) routines.
 *
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 */

#define NSUBEXP  10
typedef struct regexp
{
  int ref;  /* For lpc use only */
  char *str; /* original regexp, must be a shared string */
  char *startp[NSUBEXP];
  char *endp[NSUBEXP];
  char regstart;		/* Internal use only. */
  char reganch;			/* Internal use only. */
  char *regmust;		/* Internal use only. */
  int regmlen;			/* Internal use only. */
  char program[1];		/* Unwarranted chumminess with compiler. */
} regexp;



extern regexp *regcomp();
extern int regexec();
extern char *regsub();
extern void regerror();
#endif
