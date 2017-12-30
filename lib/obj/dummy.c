#pragma strict_types
#pragma save_types
inherit "/std/thing";


void create(mixed a){
  add_name("thing","things");
  add_name("guttaperkaboll","guttaperkabollar");
  add_name("snurgel","snurgels");
  add_name("gnurf","gnurfs");
  set_short(({"a thing","something","a guttaperkaboll","a snurgel","a gnurf"})[random(4)]);
  set_long (({"It is a thing","It is something","It is a guttaperkaboll",
	      "It is a snurgel","It is a gnurf"})[random(4)]);
  set_add_prop("weight",random(4)-2);
  ::create(a);
}
