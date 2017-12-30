#ifndef STRALLOC_H
#define STRALLOC_H
#include "interpret.h"

struct shared
{
#ifndef STRALLOC_GC
  int refs;
#ifdef DEBUG
  int extra_ref;
#endif
#endif
  char *next;
  int size;
  char str[1]; /* Must be last */
};

#define REFERENCE(X,Y) (BASEOF(X,shared,str)Y)

#define BASE(X) REFERENCE(X,)
#define SIZE(X) REFERENCE(X,->size)
#define REFS(X) REFERENCE(X,->refs)
#define STR(X) REFERENCE(X,->str)
#define XREF(X) REFERENCE(X,->extra_ref)
#define NEXT(X) REFERENCE(X,->next)
#define MEMORY_SIZE(X) (SIZE(X)+sizeof(shared)-1)

#ifndef DEBUG
#define my_strlen2(u) ((u).string->size)
#define my_strlen(s) my_strlen2(((s)->u))

#ifdef STRALLOC_GC
#define copy_shared_string(str) (str)
#define free_string(str) 
#else
#define copy_shared_string(str) (REFS(str)++,str)

#ifdef STRALLOC_REFS
#define free_string(X) { char *str=(X); if(!--REFS(str))  really_free_string(str); }
#else
#define free_string(X) --REFS(str)
#endif /* STRALLOC_REFS */

#endif /* STRALLOC_GC */

#else
int my_strlen2(const union storage_union u);
int my_strlen(const struct svalue *s);
void free_string PROT((char *));
char *copy_shared_string PROT((char *));
#endif /* DEBUG */

char *make_shared_string PROT((const char *));
INLINE int my_strcmp(const struct svalue *a,const struct svalue *b);
INLINE int my_string_is_equal(const struct svalue *a,const struct svalue *b) ATTRIBUTE((const));

char *findstring(const char *);
char *make_shared_binary_string(const char *str,int len);
char *debug_findstring(const char *foo);
char *begin_shared_string(int len);
char *end_shared_string(char *s);
char *add_string_status PROT((int verbose));
void uninit_strings(void);
void stralloc_gc(int);
extern int ref_cycle;
void not_backend_stralloc_gc();
#define strptr2(u) (&((u).string->str[0]))
#define strptr(s) (&((s)->u.string->str[0]))
#endif /* STRALLOC_H */
