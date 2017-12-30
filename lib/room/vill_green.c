inherit "/std/room";

void create()
{
  set_short("outside church");
  set_long("You're outside the church somewhat west of New Larstown. "+
	"although only a few trees and some bushes grows it's still "+
	"the greenest place in town, and outside town, there's only "+
	"the desert. ");
  add_exit("north","/room/church");
  add_exit("east","/room/vill_track");
  add_exit("west","/room/hump");
}
