inherit "/std/object";
string name;   /* This is the capitalized name */
string msgin="enters.";
string msgout="leaves DIR.";
static object body;
int brief_mode;
int local_weight;

int is_living() { return 1; }
int is_player() { return 0; }
int is_npc() { return 0; }
string query_name() { return name; }
void catch_tell(string s) { }

void emote(string s)
{
  tell_room(environment(),query_name()+" "+s+"\n",({this_object()}));
}

void create(string s)
{
  ::create(s);
  enable_commands();
}

int do_look(string s)
{
  int perc, light;
  object o;
  string sl;
  if(body) perc=(int)body->get_tot_add_prop("perception");
  light=(int)environment()->query_add_prop("light");
  if(s && s!="" && !(o=present(s,environment())) && light>=perc)
  {
    foreach(all_inventory(environment()),o)
    {
      if(sl=(string)o->get_item(s))
      {
	catch_tell(sl+"\n");
	return 1;
      }
    }
  }
  if(!o) o=environment();
  catch_tell( (string)o->long(s,light*perc/100) );
  return 1;
}

void move_player(string dir,mixed where,string leavehow,string enterhow)
{
  if(!leavehow) leavehow=replace(msgout,"DIR",dir);
  if(!enterhow) enterhow=msgin;
  emote(leavehow);
  move_object((object)where);
  emote(enterhow);
  if(is_player())
  {
    if(brief_mode)
      catch_tell((string)environment()->display_short());
    else
      do_look(0);
  }
}

int hit_player(int str,int pene,int orientation)
{
  
}

int add_weight(int t)
{
  if(local_weight+t>(int)body->query_add_prop("str"))
  {
    return 0;
  }else{
    local_weight+=t;
    return t;
  }
}
