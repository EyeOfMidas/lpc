extern int batch_mode;
extern char *hostname;
char *string_copy PROT((char *));
char *xalloc PROT((int));
void debug_message(char *a, ...) ATTRIBUTE((format (printf, 1,2)));
#ifdef WARN
void warn(int w_level,char *a, ...) ATTRIBUTE((format (printf, 2,3)));
#endif
