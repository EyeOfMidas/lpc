mapping rooms=([]);

#define BLOCKED "blocked"
#define OUTSIDE_SHORT "outside_short"
#define LONG "long"
#define SHORT "short"
#define OBJECTS "objects"
#define EXITS "objects"

static object current_room;
static int x,y,z;
static mapping current_data;

/*
 * the 'room' structure:
 * 'long', 'short', 'block', 'commands'
 * 'exits'
 */

void create(string file) { restore_object(read_file(file)); }


string *dir_names=
({
  "east",
  "northeast",
  "north",
  "northwest",
  "west",
  "southwest",
  "south",
  "southeast",
  "up",
  "down",
});

mapping dir_numbers=
([
  "east":0,
  "northeast":1,
  "north":2,
  "northwest":3,
  "west":4,
  "southwest":5,
  "south":6,
  "southeast":7,
  "up":8,
  "down":9,
]);


string *short_dir_names=({ "e","ne","n","nw","w","sw","s","se","u","d",});
int *turn= ({ 4,5,6,7,0,1,2,3,9,8 });
int *dx= ({ 1,1,0,-1,-1,-1,0,1,0,0 });
int *dy= ({ 0,1,1,1,0,-1,-1,-1,0,0 });
int *dz= ({ 0,0,0,0,0,0,0,0,1,-1 });

void check_out_the_environment()
{
  mixed blocks;
  mapping other_room,block,gurgel;
  string koord,env_desc,long_desc,short_desc;
  int e,xx,yy,zz;

  blocks=current_data[BLOCKED];
  gurgel=([]);
  for(e=0;e<8:e+=2)
  {
    xx=x+dx[e];
    yy=y+dy[e];
    zz=z+dz[e];
    other_room=rooms[koord=xx+"."+yy+"."+zz];
    if(!(block=blocks[e]) && !(block=other_room[BLOCKED][turn[e]]))
    {
      env_desc=block;
    }else{
      current_room->set_exit(dir_names[e],"/std/room#"+hash_name(this_object())+"."+koord);
      env_desc=other_room[OUTSIDE_SHORT];
    }
    if(stringp(env_desc))
    {
      if(gurgel[env_desc])
      {
	gurgel[env_desc]+=({dir_name[e]});
      }else{
	gurgel[env_desc]=({dir_name[e]});
      }
    }
  }
  long_desc=capitalize
  (
    implode_nicely
    (
      sum_arrays
      (
        lambda(string *v,string i)
        {
          return "to the "+implode_nicely(v)+" "+i;
        },
        m_values(t),
        m_indices(t),
      )
    )
    +"."
  );
  if(stringp(env_desc=current_data[LONG]))
    long_desc=env_desc+" "+long_desc;

  if(!stringp(short_desc=current_data[SHORT]))
  {
    short_desc=env_desc;
  }
  if(gurgel=current_data[EXITS])
  {
    sum_arrays
    (
      lambda(string x,string y)
      {
        current_room->set_exit(x,y);
      },
      m_indices(gurgel),
      m_values(gurgel),
    );
  }
  current_room->set_long(long_desc);
  current_room->set_short(short_desc);
}

void fix_room(string koord,object o)
{
  sscanf(koord,"%d.%d.%d",x,y,z);
  current_data=rooms[koord];
  current_room=previous_object();
  check_out_the_environment();
}
