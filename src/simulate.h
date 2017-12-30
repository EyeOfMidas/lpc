#ifndef SIMULATE_H
#define SIMULATE_H

#include <stdarg.h>


struct inherit_check
{
  struct inherit_check *next;
  char *name;
};

struct program *load_object (char *,struct inherit_check *);

void add_action PROT((struct svalue *,struct svalue *, int));
void destruct_object PROT((struct object *));
void fatal(char *,...) ATTRIBUTE((noreturn,format (printf, 1, 2)));
void error(char *,...) ATTRIBUTE((noreturn,format (printf, 1, 2)));
void va_error(char *,va_list) ATTRIBUTE((noreturn));
void enable_commands PROT((int));
struct object *clone_object PROT((char *,int));
struct program *make_object PROT((char *));
void tell_room PROT((struct object *, struct svalue *, struct vector *));
int command_for_object PROT((char *, struct object *));
char *check_valid_path PROT((char *,int, char *, char *, int));
char *simple_check_valid_path(struct svalue *s,char *fun,int writeflag);
int legal_path PROT((char *path));
struct vector *get_dir PROT((char *path,int flags));
char *read_bytes PROT((char *file, int, int));
int write_bytes PROT((char *file, int, struct svalue *));
char *read_file PROT((char *file, int, int));
struct object *object_present PROT((struct svalue *, struct object *));
char *print_file PROT((char *, int, int));
void do_write PROT((struct svalue *));
struct vector *query_actions(struct object *on,
			     struct object *by);

char *combine_path(char *cwd,char *file);
void throw_error PROT((void));
void strip_trailing_slashes (char *path);
int isdir (char *path);
int do_move (char *from,char * to);
void shutdowngame(void);
#endif
