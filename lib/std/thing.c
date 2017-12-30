inherit "std/object";
inherit "inherit/property";
#pragma strict_types
#pragma save_types

string short_desc,lockee;
mapping items,taste,smell,sound,feel,commands;
string *names,*pl_names,*adj;

string short()
{
  return short_desc;
}

int com(string a,string b,string c,string d,string e,string f,string g,string h)
{
  mixed *data;
  data=commands[query_verb()];
  return get_function(data[0],data[1])(a,b,c,d,e,f,g,h);
}

void init()
{
  int e;
  mixed *a,*b;
  ::init();
  if(!commands) return;
  a=m_indices(commands); b=m_values(commands);
  for(;e<sizeof(a);e++) add_action(a[e],com,0);
}

mapping general_set(mixed item,mixed desc,mapping map)
{
  string e;
  if(lockee) return map;
  if(!map) map=([]);
  if(pointerp(item)) 
  {
    foreach(item,e) map[e]=desc;
  }else
    map[item]=desc;
  return map;
}

mapping general_remove(mixed item,mapping map)
{
  int e;
  if(lockee) return map;
  if(!map) return 0;
  if(pointerp(item))
    map=map-mkmapping(item,item);
  else
    map=m_delete(map,items);
  if(m_sizeof(map)) return map;
  return 0;
}

mixed general_get(mixed item,mapping map) { return map?map[item]:0; }

/**** Configuration ****/
void lock() { if(!lockee) lockee=file_name(previous_object()); }
void unlock() { if(file_name(previous_object())==lockee) lockee=0; }

void set_item(mixed item,string desc) { items=general_set(item,desc,items); }
void set_smell(mixed item,string desc) { smell=general_set(item,desc,smell); }
void set_sound(mixed item,string desc) { sound=general_set(item,desc,sound); }
void set_feel(mixed item,string desc) { feel=general_set(item,desc,feel); }
void set_taste(mixed item,string desc) { taste=general_set(item,desc,taste); }

void remove_item(mixed item) { items=general_remove(item,items); }
void remove_smell(mixed item) { smell=general_remove(item,smell); }
void remove_sound(mixed item) { sound=general_remove(item,sound); }
void remove_feel(mixed item) { feel=general_remove(item,feel); }
void remove_taste(mixed item) { taste=general_remove(item,taste); }

void set_short(string s) { if(!lockee) short_desc=s; }
void set_long(string long) { set_item(0,long); }

void set_command(string co,string fun,mixed ob)
{
  commands=general_set(co,({ob,fun}),commands);
}
void remove_command(string co)
{
  commands=general_remove(co,commands);
}

void update_actions()
{
  object o;
  if(environment()) move_object(environment());
  foreach(all_inventory(),o) o->update_actions();
}

/**** get ****/
string get_item(mixed item) { return items?items[item]:0; }
string get_feel(mixed item) { return feel?feel[item]:0; }
string get_smell(mixed item) { return smell?smell[item]:0; }
string get_sound(mixed item) { return sound?sound[item]:0; }
string get_taste(mixed item) { return taste?taste[item]:0; }

/**** query ****/
string query_short() { return short_desc; }
string query_long() { return get_item(0); }
mapping query_item() { return items; }
mapping query_smell() { return smell; }
mapping query_sound() { return sound; }
mapping query_feel() { return feel; }
mapping query_taste() { return taste; }

/***** Some basic stuff *****/
varargs void add_name(string name,string pl)
{
  if(lockee) return;
  if(!names) names=pl_names=({});
  if(!pl) pl=pluralize(name);
  names+=({name});
  pl_names+=({pl});
}

void set_name(string n) { add_name(n); }

void add_adjective(string ad)
{
  if(lockee) return;
  if(!adj) adj=({});
  adj+=({ad});
}

varargs void remove_name(string name,string pl)
{
  if(lockee || !names) return;
  if(!pl) pl=pluralize(name);
  names-=({name});
  pl_names-=({pl});
}

int id(string s)
{
  if(pointerp(names) && member_array(s,names)!=-1) return 1;
}

string *query_name() { return names[0]; }
string *query_names() { return names; }
string *parse_plyral_names() { return pl_names; }
string *parse_adjectives() { return adj; }

/* No light check yet */
string long(string id,int perc)
{
  return query_long();
}

int query_get() { return !query_property("no_get"); }
int query_drop() { return !query_property("no_drop"); }
