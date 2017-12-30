inherit "/inherit/property";

list items=([]);
string name,long_desc;

int id(string s) { return !!items[s]; }
string query_name() { return name; }

string query_long(string item)
{
  mixed ret;
  if(item)
  {
    ret=items[item];
    if(stringp(ret)) return ret;
  }
  return long_desc;
}

mapping query_items() { return items; }

void add_item(mixed name,mixed desc)
{
  if(listp(name)) name=indices(name);
  if(pointerp(name))
    return map_array(name,add_item,desc);
  items[name]=desc;
}

void add_name(string n){ add_item(n,1); }

void set_name(string n)
{
  name=n;
  add_name(n);
}


void set_long(string s) { long_desc=s; }
void query_long_desc() { return long_desc; }

void long(string item)
{
  string s;
  s=query_long(item);
  if(s) write(sprintf("%-=78s\n",s));
}

string short_desc;
string short() { return short_desc; }
void set_short(string s) { short_desc=s; }
int query_get() { return !query_property("no_get"); }
int query_drop() { return !query_property("no_drop"); }

