inherit "/std/room";

void create()
{
  set_short("");
  set_long("You're in New Larstown, to the north lies a small yard, "+
	"and an alley leads south.");
  add_exit("east","/room/vill_road2");
  add_exit("west","/room/vill_track");
  add_exit("south","/room/narr_alley");
  add_exit("north","/room/yard");
}
