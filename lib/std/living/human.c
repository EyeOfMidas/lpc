inherit "/std/living/genders";
inherit "/std/living/living";

void create(string str)
{
 genders::create(str);
 living::create(str);
}

/* Maybe I should let this order determine the importance of the organ... :) */
#define PART_HEAD 0
#define PART_BODY 1
#define PART_RARM 2
#define PART_LARM 3
#define PART_RHAND 4
#define PART_LHAND 5
#define PART_RLEG 6
#define PART_LLEG 7
#define NO_PARTS 8

#define B_TYPE 0
#define B_HP 1
#define B_STR 2		/* int == strength in head */
#define B_DEX 3
#define NO_B 4

#define B_TYPE_ORIGINAL 0
#define B_TYPE_VEGETABLE 1
#define B_TYPE_ROBOT 2
#define B_TYPE_REAL 3

mixed *bodyparts=allocate(NO_PARTS);

static int rnd(int x) { return (x*2+random(x))/3; }
static int hp(int x) { return x*8+42; }

void set_level(int x)
{
  int e;
  for(e=0;e<NO_PARTS;e++)
    bodyparts[e]=({B_TYPE_ORIGINAL,hp(x),rnd(x),rnd(x)});
}

int get_part_no(string str)
{
  switch(str)
  {
    case "head": return PART_HEAD;
    case "body": return PART_BODY;
    case "left arm": return PART_LARM;
    case "right arm": return PART_RARM;
    case "left hand": return PART_LHAND;
    case "right hand": return PART_RHAND;
    case "left leg": return PART_LLEG;
    case "right leg": return PART_RLEG;
    default: return -1;
  }
}

mixed query_bodypart(int part)
{ return bodyparts[part]; }

void set_bodypart(int e,mixed part)
{ bodyparts[e]=part; }

int query_body_stat(int part,int stat)
{
  mixed a;
  a=bodyparts[part];
  if(pointerp(a)) return a[stat];
  switch(stat)
  {
    case B_TYPE: return (int)a->query_b_type();
    case B_HP: return (int)a->query_b_hp();
    case B_STR: return (int)a->query_b_str();
    case B_DEX: return (int)a->query_b_dex();
  }
}

string total_body_stat()
{
  /* Here there should be a nice function that returns a nice
   * overview of your damages etc.
   */     
}

int is_humanoid() { return 1; }
