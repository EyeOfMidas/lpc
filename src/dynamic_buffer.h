#ifndef DYNAMIC_BUFFER_H
#define DYNAMIC_BUFFER_H

#define BUFFER_BEGIN_SIZE 4080
struct string_s
{
  char *str;
  int len;
};

typedef struct string_s string;

struct dynamic_buffer_s
{
  string s;
  int bufsize;
};

typedef struct dynamic_buffer_s dynamic_buffer;

void init_buf_buf_with_string(string);
string complex_free_buf();
void my_putchar(char b);
void my_put_int(long);
void my_strcat(const char *b);
void my_binary_strcat(const char *b,int len);
void init_buf();
char *simple_free_buf();
/* int my_get_buf_size(); */
char *free_buf();
void init_buf_with_string(string s);
char *return_buf();
#endif
