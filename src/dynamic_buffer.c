#include "global.h"
#include "dynamic_buffer.h"
#include "stralloc.h"
#include "simulate.h"
#include "main.h"

static dynamic_buffer buff;

void low_my_putchar(char b,dynamic_buffer *buf)
{
#ifdef DEBUG
  if(!buf->s.str)
    fatal("Error in internal buffering.\n");
#endif
  if(buf->s.len>=buf->bufsize)
  {
    buf->bufsize*=2;
    buf->s.str=(char *)realloc(buf->s.str,buf->bufsize);
  }
  buf->s.str[buf->s.len++]=b;
}

void low_my_binary_strcat(const char *b,int l,dynamic_buffer *buf)
{
#ifdef DEBUG
  if(!buf->s.str)
    fatal("Error in internal bufferint.\n");
#endif

  while(buf->s.len+l>=buf->bufsize)
  {
    buf->bufsize*=2;
    buf->s.str=realloc(buf->s.str,buf->bufsize);
  }
  MEMCPY(buf->s.str+buf->s.len,b,l);
  buf->s.len+=l;
}

void low_init_buf(dynamic_buffer *buf)
{
  if(!buf->s.str)
    buf->s.str=(char *)xalloc((buf->bufsize=BUFFER_BEGIN_SIZE));
  if(!buf->s.str)
    error("Out of memory.\n");
  *(buf->s.str)=0;
  buf->s.len=0;
}

void low_init_buf_with_string(string s,dynamic_buffer *buf)
{
  if(buf->s.str) { free(buf->s.str); buf->s.str=NULL; } 
  buf->s=s;
  if(!buf->s.str) init_buf();
  /* if the string is an old buffer, this realloc will set the old
     the bufsize back */
  for(buf->bufsize=BUFFER_BEGIN_SIZE;buf->bufsize<buf->s.len;buf->bufsize*=2);
  buf->s.str=realloc(buf->s.str,buf->bufsize);
}

string complex_free_buf()
{
  string tmp;
  if(!buff.s.str) return buff.s;
  my_putchar(0);
  buff.s.len--;
  tmp=buff.s;
  buff.s.str=0;
  return tmp;
}

char *simple_free_buf()
{
  if(!buff.s.str) return NULL;
  return complex_free_buf().str;
}

char *low_free_buf(dynamic_buffer *buf)
{
  char *q;
  if(!buf->s.str) return 0;
  q=make_shared_binary_string(buf->s.str,buf->s.len);
  free(buf->s.str);
  buf->s.str=0;
  buf->s.len=0;
  return q;
}

char *free_buf() { return low_free_buf(&buff); }
void my_putchar(char b) { low_my_putchar(b,&buff); }
void my_binary_strcat(const char *b,int l) { low_my_binary_strcat(b,l,&buff); }
void my_strcat(const char *b) { my_binary_strcat(b,strlen(b)); }
void init_buf() { low_init_buf(&buff); }
void init_buf_with_string(string s) { low_init_buf_with_string(s,&buff); }
char *return_buf()
{
  my_putchar(0);
  return buff.s.str;
}
/* int my_get_buf_size() {  return buff->s.len; } */

