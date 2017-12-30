/* This file contains functions that will appear as efuns when compiling
 * other objects. Note that this is not done automatically as in the old
 * days, instead the old behaviour is simulated by master.c with the use
 * of add_simul_efun.
 */

varargs string implode_nicely(string *dum,string sep)
{
  int s;
  if(!sep) sep="and";
  s=sizeof(dum);
  switch(s)
  {
  default:
    return implode(dum[0..s-2],", ")+" "+sep+" "+dum[s-1];

  case 2:
    return dum[0]+" "+sep+" "+dum[1];

  case 1:
    return dum[0];
  
  case 0:
    return "";
  }
}


static mapping living_names=([]);
static mapping livings=([]);

void set_living_name(string n,object o)
{
  if(living_names[n])
  {
    living_names[n]=({o})+living_names[n];
  }else{
    living_names[n]=({o});
  }
  livings[o]=n;
}

mixed find_living(string n,int all)
{
  object *tmp;
  if(tmp=living_names[n])
  {
    tmp-=({0}); /* remove destructed objects */
    if(!sizeof(tmp))
      m_delete(living_names,n);
  }else{
    tmp=({});
  }
  if(all) return tmp;
  if(sizeof(tmp)) return tmp[0];
  return 0;
}

string living(object o) { return livings[o]; }
int query_number_of_livings() { return m_sizeof(livings); }

object *users()
{
  return "/secure/ip"->users();
}

mapping find_player_cache=([]);
object find_player(string name)
{
  object ret;
  string tmp;
  if(ret=find_player_cache[name]) return ret;

  foreach(users(),ret)
  {
    tmp=ret->query_real_name();
    find_player_cache[tmp]=ret;
    if(tmp==name) return ret;
  }
  return 0;
}

void tell_object(object o,string msg) { o->catch_tell(msg); }
void notify_fail(string n) { this_player()->set_notify_fail(n); }

int abs(int x) { return x<0?-x:x; }
int atoi(string a) { return (int)a; }
mixed max(mixed x,mixed y) { return x>y?x:y; }
mixed min(mixed x,mixed y) { return x<y?x:y; }

int transfer(object o,object to)
{
  object from;
  int w;
  if(!o->query_get(to)) return 1;

  from=environment(o);
  if(!to->query_room())
  {
    if(!to->can_put_and_get(o)) return 2;
    if(!from->can_put_and_get(o)) return 3;
    if(!o->query_drop(0)) return 4;

    if(w=o->query_weight())
    {
      if(!to->add_weight(w)) return 5;
      from->add_weight(-w);
    }
  }else{
    if(w=o->query_weight())
      from->add_weight(-w);
  }
  if(living(from))
    if(!from->silent_unwield(o,1))
      return 7;
  o->move(to);

  if(environment(o)!=to) return 6;
  return 0;
}

/* this _used_ to be an efun:
NAME
	pluralize - attempt to pluralize a noun

SYNTAX
	string pluralize(string noun);

DESCRIPTION
	Try to return the plural form of a given noun. Doesn't always
	work, but makes an acceptable 'default' for when no plural form
	is given.

NOTA BENE
	Might not work very well with most languages except english.
*/

string pluralize(string p)
{
  while(sscanf(p,"%s %s",p,p));
  p=lower_case(p);
  switch(p)
  {
    case "moose":
    case "fish":
    case "deer":
    case "sheep": return p;
    case "child": return "children";
    case "tooth": return "teeth";
    case "ox":
    case "vax": return p+"en";
    case "bus": return "busses";
    case "chef": return "chefs";
    case "foot": return "feet";
  }
  
  if(p[-1]=='x' || p[-1]=='s' || (p[-1]=='h' && (p[-2]=='c' || p[-2]=='s')))
    return p+"es";

  if(sscanf(p,"%sff",p) || sscanf(p,"%sfe",p)) return p+"ves";
/*  if(sscanf(p,"%sef",p)) return p+"efs"; */
  if(sscanf(p,"%sf",p)) return p+"ves";
  if(sscanf(p,"%sy",p)) return p+"ies";
  if(sscanf(p,"%sus",p)) return p+"i";
  if(sscanf(p,"%sman",p)) return p+"men";
  if(sscanf(p,"%sis",p)) return p+"es";
  if(sscanf(p,"%so",p)) return p+"s";
  return p+"s";
}


string tail(string file)
{
  string f;
  int s;
  f=master()->valid_read(file,geteuid(previous_object()),"tail");
  if(!f) return;
  if(stringp(f)) file=f;
  s=file_size(file);
  s-=54*20;
  if(s<0) s=0;
  f=read_bytes(file,s);
  sscanf(f,"%*s\n%s",f);
  return f;
}

string cat(string file)
{
  string f;
  f=master()->valid_read(file,geteuid(previous_object()),"cat");
  if(!f) return;
  if(stringp(f)) file=f;
  return read_file(file,1,20);
}

void log_file(string file,string entry)
{
  if(file[0]!='/') file="/log/"+file;
  string f;
  f=master()->valid_read(file,geteuid(previous_object()),"log_file");
  if(!f) return;
  if(stringp(f)) file=f;
  write_file(file,entry);
  if(file_size(file)>50000) /* MAX_LOG_SIZE */
    rename(file,file+".old");
}

object present(string s,object o)
{
  int num;
  string s2;
  if(s[-1]>='0' && s[-1]<='9')
  {
    num=strlen(s)-2;
    while(s[num]>='0' && s[num]<='9') num--;
    if(s[num]==' ')
    {
      s2=s[0..num-1];
      num=(int)s[num+1..strlen(s)-1];
      foreach(all_inventory(o),o)
	if(o->id(s) && !--num)
	  return o;
      return 0;
    }
  }
  foreach(all_inventory(o),o)
    if(o->id(s))
      return o;
  return 0;
}

varargs void tell_room(object o,string s,object *avoid)
{
  if(avoid)
    map_array(all_inventory(o)-avoid,"catch_tell",s);
  else
    map_array(all_inventory(o),"catch_tell",s);
}
