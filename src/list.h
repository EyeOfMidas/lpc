#define NONE 0
#define NOTA 1
#define GETA 2
#define NOTB 3
#define GETB 4
#define BOTH 5
#define EITHER 6

#define OPER_EQ_SKIPA	(NOTA<<0)
#define OPER_EQ_SKIPB	(NOTB<<0)
#define OPER_EQ_TAKEA	(GETA<<0)
#define OPER_EQ_TAKEB	(GETB<<0)
#define OPER_EQ_TAKE	(EITHER<<0)
#define OPER_EQ_SKIP	(NONE<<0)
#define OPER_EQ_BOTH	(BOTH<<0)

#define OPER_A_SKIP	(NOTA<<4)
#define OPER_A_TAKE	(GETA<<4)

#define OPER_B_SKIP	(NOTB<<8)
#define OPER_B_TAKE	(GETB<<8)

#define OPER_XOR	(OPER_EQ_SKIP | OPER_A_TAKE | OPER_B_TAKE)
#define OPER_AND	(OPER_EQ_TAKE | OPER_A_SKIP | OPER_B_SKIP)
#define OPER_OR		(OPER_EQ_TAKE | OPER_A_TAKE | OPER_B_TAKE)
#define OPER_ADD	(OPER_EQ_BOTH | OPER_A_TAKE | OPER_B_TAKE)
#define OPER_SUB	(OPER_EQ_SKIPA | OPER_A_TAKE | OPER_B_SKIP)

struct vector *do_array_surgery(struct vector *,struct vector *,int,int);
struct vector *mklist(struct vector *v);
struct vector *allocate_list(struct vector *v);
struct vector *remove_list(struct vector *, struct svalue *);
void mutilate_mapping(struct vector *a,int ap,struct vector *b,int bp);
