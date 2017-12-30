INLINE int is_equal(const struct svalue *,const struct svalue *);
INLINE int is_eq(const struct svalue *,const struct svalue *);
INLINE int is_lt(const struct svalue *a,const struct svalue *b);
INLINE int is_gt(const struct svalue *a,const struct svalue *b);

#ifdef ADD_CACHE_SIZE
struct add_cache_entry
{
  char *a,*b,*sum;
  int hits;
};

extern struct add_cache_entry add_cache[ADD_CACHE_SIZE];
void clean_add_cache(void);
void free_add_cache(void);
#endif
