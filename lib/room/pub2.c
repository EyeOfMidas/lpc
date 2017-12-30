inherit "/std/room";
inherit "/inherit/msg";

void create()
{
  set_short("The saloon");
  set_long("You're in the local Saloon, people come here to refresh themselves "+
    "after a good days work. You can order drinks here. A sign on the wall reads:\n"+
    "\n"+
    "     Drinks:\n"+
    "       Beer                 75c\n"+
    "       Gin and tonic        $1.25\n"+
    "       Whisky on the rocks  $3\n"+
    "       Tequila              $5\n"+
    "       Panther Juice        $15\n");

  add_exit("west","/room/yard");
}

int buy(string s)
{
  object drink;
  int cost,strength,heal;
  string short,long;

  switch(s)
  {
    case "beer":
      cost=75;
      short="a beer";
      long="It's just a glass of beer.";
      strength=2;
      heal=0;
      break;

    case "gin":
    case "gin and tonic":
      cost=125;
      short="a glass of gin and tonic";
      long="It's a womens drink.";
      strength=4;
      heal=2;
      break;

    case "whiskey":
    case "whiskey on the rocks":
      cost=300;
      short="whiskey on the rocks";
      long="The brown liquid is just waiting to make you brain mushy.";
      strength=8;
      heal=6;
      break;

    case "tequila":
      cost=500;
      short="a tequila";
      long="A glass full of clear liquid.";
      strength=16;
      heal=15;
      break;

    case "juice":
    case "panther juice":
      cost=1500;
      short="a glass of panther juice";
      long="The red-brown liquid seems to be moving gently in the glass.";
      strength=32;
      heal=32;
      break;

    default:
      write("You can't buy that here.\n");
      return 1;
  }

  if(this_player()->query_money()<cost)
  {
    write("You ain't got enough money.\n");
    return 1;
  }
  if(!this_player()->add_weight(1))
  {
    write("You can't carry more weakling.\n");
    return 1;
  }
  this_player()->add_money(-cost);
  drink=clone_object("/obj/drink");
  drink->set_short(short);
  drink->set_long(long);
  drink->set_strength(strength);
  drink->set_heal(heal);
  drink->move(this_player());

  msg("\b1PRON buys "+short+".\n",this_player());
  return 1;
}

void init()
{
  ::init();
  add_action("buy %s",buy);
  add_action("order %s",buy);
}

int prevent_exit()
{
  object o;
  foreach(deep_inventory(this_player()),o)
    if(o->id("pub_drink")) return 1;
  return 0;
}
