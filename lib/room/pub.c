inherit "/std/room";
inherit "/inherit/msg";

void create()
{
  set_short("the saloon");
  set_long("You're in the Saloon, you can order drinks here. "+
	"Menu: Tequila $1 , Hair of dog c20, beer c50, Whisky "+
	"on the rocks $2. ");
  
}

int buy(string what)
{
  int cost;
  int msg;
  switch(lower_case(what))
  {
    case "tequila":
      cost=100;
      
      break;
  }
}


void init()
{
  ::init();
  add_action("buy %s",buy);
}
