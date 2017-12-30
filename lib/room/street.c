inherit "std/room";

void create(mixed a)
{
  set_short("filthy corner");
  set_long("You are standing in a dark corner of the city Chiba. A timeless "+
	   "mist lies over the seemingly endless street. You have a feeling "+
	   "that it would be futile to move as this corner is identical to "+
	   "the next one.");
  set_item("corner","It's really quite a filthy corner, maybe you should "+
	   "hang somewhere else.");
  set_item("street","It's really quite a filthy street, maybe you should "+
	   "hang somewhere else.");
  set_item("city","It's really quite a filthy city, maybe you should "+
	   "hang somewhere else.");
  set_exit("north","/room/street");
  set_exit("south","/room/street");
  set_exit("west","/room/street");
  set_exit("east","/room/street");
  clone_object("/obj/dummy")->move(this_object());
  clone_object("/obj/dummy")->move(this_object());
  clone_object("/obj/dummy")->move(this_object());
  ::create(a);
}

