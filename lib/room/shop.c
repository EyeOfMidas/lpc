inherit "/std/room";
inherit "/inherit/find";
inherit "/inherit/msg";
inherit "/inherit/money";

void create()
{
  set_short("the pawnshop");
  set_long("You're in the pawnshop, you can buy and sell things here. "+
    "Commands are: list, buy, sell and value.");

  add_exit("south","/room/vill_road2");
}

int buy(string s)
{
  int value;
  object *obs,o;

  obs=get_objects(s,({ (object)"/room/store" }));
  if(!sizeof(obs))
  {
    write("There is no such object in store.\n");
  }else{
    value=0;
    foreach(obs,o) value+=o->query_property("value");
    value*=2;
    if(value>this_player()->query_money())
    {
      write("You can't afford it.\n");
    }else{
      value=0;
      foreach(obs,o)
      {
        if(transfer(o,this_player()))
        {
          value=1;
        }else{
          msg("\b1PRON buy$ "+o->short()+".\n");
          this_player()->add_money(-2*o->query_property("value"));
        }
      }
      if(value) write("You can't carry more.\n");
    }
  }
  return 1;
}

int sell(string s)
{
  string *tmp;
  object *obs;

  obs=get_objects(s,({ this_player() }));
  if(!sizeof(obs))
  {
    write("You have no such object.\n");
  }else{
    tmp=map_array(obs,lambda(object o,object w)
      {
	int v;
        if(v=o->query_property("value"))
        {
	  if(!this_player()->unwield(o,1))
	    return 0;
          if(!transfer(o,w))
          {
            this_player()->add_money(v);
            return o->short();
          }else{
	    write("You can't sell "+o->short()+".\n");
	  }
        }else{
          write(capitalize(o->short()+" is worthless.\n"));
        }
        return 0;    
      },(object)"/room/store")-({0});
    if(sizeof(tmp))
    {
      s=implode_nicely(tmp);
      write("You sell "+s+".\n");
      tell_room(this_object(),this_player()->query_name()+
		" sells "+s+".\n",({this_player()}));
    }
  }
  return 1;
}

int value(string s)
{
  object *obs,o;

  obs=get_objects(s,({ this_player() }));
  if(!sizeof(obs))
  {
    write("You have no such object.\n");
  }else{
    foreach(obs,o)
    {
      int v;
      string s,s2;
  
      if(!(s=o->short())) continue;
      if(v=o->query_property("value"))
      {
        s2=(string)cents_to_string(v);
      }else{
        s2="nothing";
      }
      write(sprintf("%' .'-30s%s\n",s,s2));
    }
  }
  return 1;
}

int _list(string s)
{
  object *obs,o;

  if(!s) s="all";
  obs=get_objects(s,({ (object)"/room/store" }));
  if(!sizeof(obs))
  {
    write("There is no such object in store.\n");
  }else{
    foreach(obs,o)
    {
      int v;
      string s,s2;
  
      if(!(s=o->short())) continue;
      if(v=o->query_property("value"))
      {
        s2=cents_to_string(2*v);
      }else{
        s2="nothing";
      }
      write(sprintf("%' .'-30s%s\n",s,s2));
    }
  }
  return 1;
}

void init()
{
  ::init();
  add_action("list %s",_list);
  add_action("list",_list);
  add_action("buy %s",buy);
  add_action("sell %s",sell);
  add_action("value %s",value);
}
