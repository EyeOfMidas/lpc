inherit "/std/room";

void reset()
{
  if(!present("gun",this_object()))
    clone_object("/obj/gun")->move(this_object());
}

void create()
{
  set_short("the town church.\n");
  set_long("You're in the newly built town church in New Larstown. "+
	"Two rows of benches line the walk up to the altar, where "+
	"there is a large wooden cross nailed to the wall. ");
  add_item("benches","Doesn't look to comfortable.\n");
  add_exit("south","/room/vill_green");
  reset();
}
