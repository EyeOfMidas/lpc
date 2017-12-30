inherit "/std/room";


void create()
{
  set_short("a small yard");
  set_long("You're in a small yard surrounded by houses. To the east lies "+
	   "the Saloon, and to the west the tailor has his parlor.");
  add_exit("south","/room/vill_road1");
  add_exit("east","/room/pub2");
  add_exit("west","/room/tailor");
}

