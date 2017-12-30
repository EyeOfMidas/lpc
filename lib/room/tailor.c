inherit "/std/room";

object burton;

void create()
{
  set_short("The tailors");
  set_long("This is the Joe Burton's shop. He's the local tailor, hatmaker "+
    "and undertaker.");

  add_exit("east","/room/yard");

  if(!burton)
  {
    burton=clone_object("/obj/monster");
    burton->set_name("joe");
    burton->set_alt_name("mr. burton");
    burton->set_alias("joe burton");
    burton->set_short("Joe Burton the tailor");
    burton->set_long("He's the kind of jolly fellow who asks you if you want "+
    "a coffin as well while he's got your measures handy.");
    burton->set_level(9),
    burton->move(this_object());
  }
}


