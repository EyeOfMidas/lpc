struct vector *allocate_mapping PROT((struct vector*, struct vector*));
void free_mapping PROT((struct vector*));
struct vector *filter_mapping PROT((struct vector*, char*, struct object*,
				    struct svalue*));
struct vector *map_mapping PROT((struct vector*, char*, struct object*,
				 struct svalue*));

void mutilate_mapping(struct vector *,int,struct vector *,int);
void map_set_default(struct vector *v,struct svalue *s);
struct vector *remove_mapping2(struct vector *, struct svalue *);
void remove_mapping(struct vector *, int);
struct vector *subtract_mapping(struct vector *,struct vector *);
struct vector *intersect_mapping(struct vector *,struct vector *);
struct object *new_f_object(long);
struct svalue *map_get_default(struct vector *);
