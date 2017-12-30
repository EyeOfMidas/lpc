inherit "/std/room";

void create()
{
  set_short("Village track");
  set_long("You're on a track going into the village, southwards the track "+
	   "becomes a road and eastwards it ends with a green lawn in front "+
	   "of the church. To the north is a track leading up to the hilles.");
  add_exit("east","/room/vill_road1");
  add_exit("west","/room/vill_green");
  add_exit("north","/room/vill_track2");
}
