#ifndef __CTYPE_H
#define __CTYPE_H

inherit "inherit/ctype";

#define __CF_ISALNUM	0x01
#define __CF_ISALPHA	0x02
#define __CF_ISCNTRL	0x04
#define __CF_ISDIGIT	0x08
#define __CF_ISGRAPH	0x10
#define __CF_ISLOWER	0x20
#define __CF_ISSPACE	0x40
#define __CF_ISUPPER	0x80
#define __CF_ISPUNCT	0x100
#define __CF_ISXDIGIT	0x200
#define __CF_ISPRINT	0x400

#define isalnum(C)  (__CAry[C] & __CF_ISALNUM)
#define isalpha(C)  (__CAry[C] & __CF_ISALPHA)
#define iscntrl(C)  (__CAry[C] & __CF_ISCNTRL)
#define isdigit(C)  (__CAry[C] & __CF_ISDIGIT)
#define isgraph(C)  (__CAry[C] & __CF_ISGRAPH)
#define islower(C)  (__CAry[C] & __CF_ISLOWER)
#define isspace(C)  (__CAry[C] & __CF_ISSPACE)
#define isupper(C)  (__CAry[C] & __CF_ISUPPER)
#define ispunct(C)  (__CAry[C] & __CF_ISPUNCT)
#define isxdigit(C) (__CAry[C] & __CF_ISXDIGIT)
#define isprint(C)  (__CAry[C] & __CF_ISPRINT)

#endif
