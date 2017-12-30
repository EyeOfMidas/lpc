#include "dynamic_buffer.h"

void debug_save_svalue_list(struct vector *v);
void save_svalue(struct svalue *);
void save_object_desc(struct object *);
char *restore_svalue(char *,struct svalue *);
char *save_object();
int restore_object(char *);

#define SAVE_DEFAULT 0
#define SAVE_OLD_STYLE 1
#define SAVE_AS_ONE_LINE 2
extern int save_style;
