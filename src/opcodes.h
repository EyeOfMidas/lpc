extern void assign(struct lvalue *l,struct svalue *s);
extern void assign_and_free(struct lvalue *l,struct svalue *s);
extern void lvalue_to_svalue(struct svalue *s,struct lvalue *l);
extern void lvalue_to_svalue_no_free(struct svalue *s,struct lvalue *l);
extern struct lvalue *push_indexed_lvalue(struct svalue *s,struct svalue *i);
extern int *lvalue_to_intp(struct lvalue *l,char *fel);
