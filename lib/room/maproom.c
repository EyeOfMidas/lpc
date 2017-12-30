#include "map.h"
inherit "std/room";

int x,y,z;
int room_type;
int zipcode;
mixed zipcodes;
int cache_flow;
int height;
string name;
mixed *extra_data;

void reset()
{
  if(!first_inventory()) destruct();
}

string get_map_dir_nr(int i)
{
  switch(i)
  {
  case 0: return "north";
  case 1: return "south";
  case 2: return "east";
  case 3: return "west";
  case 4: return "up";
  case 5: return "down";
  }
}

int get_map_nr_dir(string i)
{
  switch(i)
  {
  case "north": return 0;
  case "south": return 1;
  case "east": return 2;
  case "west": return 3;
  case "up": return 4;
  case "down": return 5;
  default: return -1;
  }
}

int get_dx(int i)
{
  switch(i)
  {
  case 2: return 1;
  case 3: return -1;
  }
}

int get_dy(int i)
{
  switch(i)
  {
  case 0: return 1;
  case 1: return -1;
  }
}

int get_dz(int i)
{
  switch(i)
  {
  case 4: return 1;
  case 5: return -1;
  }
}

string create(string s)
{
  mixed *data,*zipdata;
  ::create(s);
  unpack(s,x,y,z);
  data=(mixed *)MAPBASE->get_koord_data(x,y,z);
  zipcodes=data[ROOM_ZIPCODE];
  if(intp(zipcodes)) zipcode=zipcodes; else zipcode=zipcodes[0];
  zipcode=data[ROOM_ZIPCODE];
  cache_flow=data[ROOM_CACHE_FLOW];
  height=data[ROOM_LEVEL];
  zipdata=(mixed *)MAPBASE->get_zip_data(zipcode);
  if(zipdata)
  {
    room_type=zipdata[ZIP_TYPE];
    name=zipdata[ZIP_NAME];
    extra_data=zipdata[ZIP_DATA];
  }else{
    room_type=R_Dummy;
    name="The void";
    extra_data=0;
  }
  switch(room_type)
  {
  case R_Water: set_short("In water"); break;
  case R_House: set_short("Inside"); break;
  case R_Forest: set_short("In a forest"); break;
  case R_Street: set_short("On a street"); break;
  case R_Dummy: set_short("Nowhere"); break;
  case R_Air: set_short("Open air"); break;
  case R_Ground: set_short("Down under"); break;
  case R_Field: set_short("Field"); break;
  }
  if(name) set_short(name);
}

string query_exit_desc(string dir)
{
  switch(room_type)
  {
  case R_Water:
    return "you see water";

  case R_House:
    return "there is house";

  case R_Forest:
    return "you see a forest";

  case R_Street:
    return "the street continues";

  case R_Dummy:
    return 0;

  case R_Air:
    return "there is open air";

  case R_Ground:
    return "you see a wall of dirt";

  case R_Field:
    return "there is a field";
  }
}

string get_numbered_room_name(int i)
{
  return file_name()+"#"+pack(x+get_dx(i),y+get_dy(i),z+get_dz(i));
}

string collect_environment_desc(int flag)
{
  mapping t;
  string q,dir;
  int e;
  t=([]);
  for(e=0;e<4;e++)
  {
    dir=get_map_dir_nr(e);
    q=get_numbered_room_name(e);
    if(query_is_exit(dir) && q->query_is_exit(opposite_dir(dir)))
       set_exit(dir,q);

    q=(string)q->query_exit_desc(dir);
    if(q)
    {
      if(t[q])
      {
	t[q]+=({get_map_dir_nr(e)});
      }else{
	t[q]=({get_map_dir_nr(e)});
      }
    }
  }
  if(!m_sizeof(t)) return 0;
  return capitalize
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
}

int query_is_exit(string dir)
{
  switch(room_type)
  {
  case R_Water:
  case R_House:
  case R_Dummy:
  case R_Air:
  case R_Ground:
    return 0;

  case R_Forest:
  case R_Street:
  case R_Field:
    return 1;
  }
}

void build_desc()
{
  string *t;
  t=({});
  switch(room_type)
  {
  case R_Water:
    t+=({"You are standing in deep water."});
    break;

  case R_House:
    if(name)
      t+=({"You are inside a house called "+name+"."});
    else
      t+=({"You are inside."});
    break;

  case R_Forest:
    if(name)
      t+=({"You are in a the forest "+name+"."});
    else
      t+=({"You are in a forest."});
    break;

  case R_Street:
    if(name)
      t+=({"You are on "+name+"."});
    else
      t+=({"You are on a street."});
    break;

  case R_Dummy:
    t+=({"You are in a big void, in fact you really shouldn't be here..."});
    break;

  case R_Air:
    t+=({"You are floating around in open air, strange.."});
    break;
    
  case R_Ground:
    t+=({"You're dead! (or at least buried)"});
    break;

  case R_Field:
    t+=({"You're on a large field."});
    break;
  }
  t+=({collect_environment_desc(0)});
  set_long(implode(t," "));
}

void init()
{
  if(!exits)
  {
    build_desc();
  }
  ::init();
}

int query_room_type() { return room_type; }
int query_x_koor() { return x; }
int query_y_koor() { return y; }
int query_z_koor() { return z; }
int query_zipcode() { return zipcode; }
int query_cache_flow() { return cache_flow; }
int query_height() { return height; }
string query_name() { return name; }
mixed *query_extra_data() { return extra_data; }
