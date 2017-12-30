inherit "inherit/property";

mapping dest_dir=([]);
mapping items=([]);
string short_desc,long_desc;


int query_room() { return 1; }

string short()
{
  return short_desc+
  "["+(map_array(indices(dest_dir),lambda(string x) { return x[0..0]; })*",")+"]";
}

int id(string s) { return !!items[s]; }

mapping query_items() { return items; }
void add_item(mixed name,mixed desc)
{
  if(listp(name)) name=indices(name);
  if(pointerp(name))
    return map_array(name,add_item,desc);
  items[name]=desc;
}

void set_short(string s) { short_desc=s; }
void set_long(string s) { long_desc=s; }
void query_long_desc() { return long_desc; }

string query_long(string item)
{
  mixed ret;
  if(item)
  {
    ret=items[item];
    if(stringp(ret)) return ret;
  }
  switch(m_sizeof(dest_dir))
  {
    case 0:
      return long_desc+"\n  There are no obvious exits.\n";

    case 1:
      return long_desc+"\n  The only obvious exit is "+m_indices(dest_dir)[0]+".\n";

    default:
      return long_desc+"\n  Obvous exits are: "+implode_nicely(m_indices(dest_dir))+".\n";
  }
}


void long(string item)
{
  string s;
  s=query_long(item);
  if(s) write(sprintf("%-=78s\n",s));
}

string expand_dir(string d)
{
  switch(d)
  {
  case "n":
  case "north":	return "north";
  case "s":
  case "south":	return "south";
  case "e":
  case "east":	return "east";
  case "w":
  case "west":	return "west";
  case "northwest":
  case "nw":	return "northwest";
  case "northeast":
  case "ne":	return "northeast";
  case "southeast":
  case "se":	return "southeast";
  case "southwest":
  case "sw":	return "southwest";
  case "u":
  case "up":	return "up";
  case "d":
  case "down":	return "down";
  default:
    return d;
  }
}

string short_dir(string d)
{
  switch(d)
  {
  case "n":
  case "north":	return "n";
  case "s":
  case "south":	return "s";
  case "e":
  case "east":	return "e";
  case "w":
  case "west":	return "w";
  case "northwest":
  case "nw":	return "nw";
  case "northeast":
  case "ne":	return "ne";
  case "southeast":
  case "se":	return "se";
  case "southwest":
  case "sw":	return "se";
  case "u":
  case "up":	return "u";
  case "d":
  case "down":	return "d";
  default:
    return d;
  }
}

int do_move()
{
  int dest,dir;
  dest=dest_dir[dir=expand_dir(query_verb())];
  this_player()->move_player(dir,dest);
  this_player()->look();
  return 1;
}

void init()
{
  string tmp;
  foreach(m_indices(dest_dir),tmp)
  {
    add_action(tmp,do_move);
    add_action(short_dir(tmp),do_move);
  }
}

void add_exit(string dir,string where)
{
  if(where[0]!='/')
  {
    int e;
    string file=file_name(this_object());
    e=strlen(file);
    while(file[--e]!='/');
    where=combine_path(where,file[0..e]);
  }
  dest_dir[dir]=where;
}

/* door action */

/*
  ([
    "open_msg":"",
    "close_msg":"",
    "other_close_msg":"",
    "other_open_msg":"",
    "auto_open_msg":"",
    "lock_id":"",
  ])
*/
