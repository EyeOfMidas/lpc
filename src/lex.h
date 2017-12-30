#define DEFINE_HASHSIZE 57

struct lpc_predef_s
{
    char *flag;
    struct lpc_predef_s *next;
};

extern struct lpc_predef_s * lpc_predefs;
void start_new_file(FILE *f);
void start_new_string(char *s,int len);
char *get_f_name PROT((int));
char *get_instruction_name PROT((int));
int lookup_predef PROT((char *));
void free_reswords(void);
void free_memory_in_lex_at_exit(void);
