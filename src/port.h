#ifndef HAVE_STRTOL
long STRTOL(char *str,char **ptr,int base);
#else
#define STRTOL strtol
#endif

#ifndef HAVE_STRTOD
double STRTOD(char * nptr, char **endptr);
#else
#define STRTOD strtod
#endif

#ifndef HAVE_STRCSPN
int STRCSPN(const char *s,const char * set);
#else
#define STRCSPN strcspn
#endif

#ifndef HAVE_MEMSET
char *MEMSET (char *s,int c,int n);
#else
#define MEMSET memset
#endif

#ifndef HAVE_MEMCPY
char *MEMCPY(char *b,const char *a,int s);
#define __builtin_memcpy MEMCPY
#else
#define MEMCPY memcpy
#endif

#ifndef HAVE_MEMCMP
int MEMCMP(const char *b,const char *a,int s);
#else
#define MEMCMP memcmp
#endif

#ifndef HAVE_MEMCHR
char *MEMCHR(char *p,char c,int e);
#else
#define MEMCHR memchr
#endif

#ifndef HAVE_MEMMEM
char *MEMMEM(const char *needle, int needlelen,
		char *haystack,int haystacklen);
#else
#define MEMMEM memmem
#endif

#ifndef HAVE_STRCHR
#ifdef HAVE_RINDEX
#define STRCHR(X,Y) ((char *)index(X,Y))
#else
char *STRCHR(char *s,char c);
#endif
#else
#define STRCHR strchr
#ifdef STRCHR_DECL_MISSING
char *STRCHR(char *s,char c);
#endif
#endif

#ifndef HAVE_STRRCHR
#ifdef HAVE_RINDEX
#define STRRCHR(X,Y) ((char *)rindex(X,Y))
#else
char *STRRCHR(char *s,int c);
#endif
#else
#define STRRCHR strrchr
#endif

#ifndef HAVE_STRSTR
char *STRSTR(char *s1,const char *s2);
#else
#define STRSTR strstr
#endif

#ifndef HAVE_STRTOK
char *STRTOK(char *s1,char *s2);
#else
#define STRTOK strtok
#endif

#if !defined(HAVE_VFPRINTF) || !defined(HAVE_VSPRINTF)
#include <stdarg.h>
#endif

#ifndef HAVE_VFPRINTF
int VFPRINTF(FILE *f,char *s,va_list args);
#else
#define VFPRINTF vfprintf
#endif

#ifndef HAVE_VSPRINTF
int VSPRINTF(char *buf,char *fmt,va_list args);
#else
#define VSPRINTF vsprintf
#endif


int my_rand();
void my_srand(int seed);
