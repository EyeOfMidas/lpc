#ifndef ARRAY_H
#define ARRAY_H
#include "interpret.h"
struct vector
{
  int ref;
  int size;
  int malloced_size;
  short flags;
  short types;
#ifdef DEBUG
  int extra_ref;
  struct vector *next;
#endif
  struct svalue item[1];
};


struct vector *resize_array(struct vector *,int);
void order_alist(struct vector *);
struct vector *shrink_array(struct vector *, int);
int check_for_circularity(struct vector *,struct vector *);
void donk_alist_item(struct vector *,int);
void check_vector_for_destruct(struct vector *);
void check_alist_for_destruct(struct vector *);
struct vector *allocate_n_array(struct svalue *,int);
int search_alist(struct svalue *key,struct vector * keylist);
struct vector *insert_alist(
	struct svalue *key,
	struct svalue *key_data,
	struct vector *list);
struct vector *map_array (struct vector *,struct svalue *,struct svalue *,int);
int search_array (struct vector *,struct svalue *,struct svalue *,int);
struct vector *make_unique PROT((struct vector *arr,char *func,
    struct svalue *skipnum));
struct vector *filter (struct vector *,struct svalue *,struct svalue *,
		       int);
struct vector *deep_inventory PROT((struct object *, int));
struct vector *add_array PROT((struct vector *, struct vector *));
struct vector *all_inventory PROT((struct object *));
char *implode_string PROT((struct vector *, char *));
struct vector *slice_array PROT((struct vector *,int,int));
struct vector *allocate_array_no_init PROT((int,int));
struct vector *allocate_array PROT((int));
struct vector *match_regexp PROT((struct vector *v, struct regexp *reg));
struct vector *file_stat(char *file,int raw);
struct vector *explode(struct svalue *str,struct svalue *del);
void implode(struct svalue *to,struct vector *v,struct svalue *del);
int assoc(struct svalue *key,struct vector * list);
INLINE int alist_cmp(struct svalue *p1,struct svalue *p2);

#ifndef DEBUG
void real_free_vector PROT((struct vector *));
#define free_vector(v) { if(!(v)->ref--) real_free_vector(v); }
#else
void free_vector PROT((struct vector *));
#endif

#endif
