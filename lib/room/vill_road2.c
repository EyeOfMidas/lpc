inherit "/std/room";

void create()
{
  set_short("");
  set_long("You're in the middle of New Larstown, to the north lies the "+
	"pawnshop, and to the south lies the sheriff's office. ");
  add_exit("east","/room/");
  add_exit("west","/room/vill_road1");
  add_exit("north","/room/shop");
  add_exit("shop","/room/adv_guild");
}
