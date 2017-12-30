inherit "/std/room";

void create()
{
  set_short("the store");
  set_long("This is where the shop stores all it's items, nobody is "+
	   "supposed to be able to come here.");
}

void reset()
{
  object o;
  foreach(all_inventory(),o) if(!random(4)) o->selfdestruct();
}
