/* mmm, now I start to see the advantage with shadows... */

inherit "/std/object";
inherit "/inherit/gender";
inherit "/inherit/property";
inherit "/inherit/msg";
inherit "/inherit/money";

int spell_points,max_spell_points;
int hit_points,max_hit_points;

string name,full_name;

int life_count;
int skin_armour;
string short_desc;
string long_desc;

string msgin="\b1PRON arrives.\n";
string msgout="\b1PRON leaves \bWHERE.\n";
string mmsgin="\b1PRON arrives in a puff of smoke.\n";
string mmsgout="\b1PRON dissappears in a puff of smoke.\n";

static list armours;
static int armour_class=-1;
static object *wielded=({});
static object *followers=({});
static object attacker,alt_attacker;
static int attacker_life_count,alt_attacker_life_count;
static int local_weight=0;
mapping magic_protection=([]);

int Dex,Str,Int,Con;

string short() { return short_desc; }
void set_short(string s)  { short_desc=s; }

string query_long() { return long_desc; }
void set_long(string s)  { long_desc=s; }

int query_get() { return 0; }

void say(string s)
{
  tell_room(environment(),s,({this_object()}));
}

int add_weight(int w)
{
  if(local_weight+w>Str+10) return 0;
  local_weight+=w;
  return 1;
}

void attack();
int check_attacker();

#define ARMOUR_TYPES (<"armour","ring","helmet","glove","boot","pants","cloak">)

void create()
{
  enable_commands();
}

string query_name() { return full_name; }

string query_real_name() { return name; }

int id(string s) { return full_name && s==lower_case(full_name); }

void query_life_count() { return life_count; }

list query_worn() { return armours; }

object query_attacker() { return attacker; }

object query_alt_attacker() { return alt_attacker; }

int query_attacker_life_count() { return attacker_life_count; }

int query_alt_attacker_life_count() { return alt_attacker_life_count; }

int query_hit_points() { return hit_points; }

int query_spell_points() { return spell_points; }

int reduce_hit_points(int pts) { return hit_points-pts; }

int reduce_spell_points(int pts) { return spell_points-pts; }

int query_living() { return 1; } /* yes, this is a living */

int calculate_armour_class()
{
  object o;

  armours-=(<0>);
  armour_class=0;
  foreach(indices(armours),o)
    armour_class+=o->query_class();    

  armour_class/=3; /* ? */
  return armour_class;
}

void die()  /* redefine this when you inherit */
{
  object o,oo;
  msg("\b1PRON die$.\n");
  life_count++;
  o=clone_object("/obj/corpse");
  foreach(all_inventory(),oo)
  {
    if(!oo->query_drop(1)) continue;
    oo->move(o);
  }
  o->set_name(query_real_name());
  o->move(environment());  
}

int is_attacking(object ob)
{
  if(ob==attacker && ob->query_life_count()==attacker_life_count) return 1;
  if(ob==alt_attacker && ob->query_life_count()==alt_attacker_life_count) return 1;
  return 0;
}

void attack_object(object o)
{
  set_heart_beat(1);
  if(is_attacking(o)) return;
  alt_attacker=o;
  alt_attacker_life_count=o->query_life_count();
  if(!o->is_attacking(this_object())) attack();
}

int hit_player(int damage)
{
  if(armours[0] || armour_class<0)
    calculate_armour_class();

  if(this_player()!=this_object())
    attack_object(this_player());

  damage-=random((Dex+armour_class*2)/3);
  if(damage<=0) return 0;

  hit_points-=damage;
  if(hit_points<0) die();
  return damage;
}

/* let's do one attack for every wielded weapon... */
void physical_attack()
{
  object o;
  if(!check_attacker()) return;

  wielded-=({0});

  foreach(wielded,o)
  {
    int force,dam;
    if(o->do_attack(attacker))
      continue;

    force=random((Dex+o->query_weapon_class()*2)/3);
    dam=attacker->hit_player(force);
    if(!o->message(force,dam))
    {
      switch(dam)
      {
        case 0:
          switch(force)
          {
            case 0..5:
              msg("\b1PRON missed \b2OBJ.\n",this_object(),attacker);
              break;

            default:
              msg("\b2PRON dodge$ \b1POSS attack.\n",this_object(),attacker);
          }
          break;

        case 1..2:
          msg("\b1PRON tickle$ \b2OBJ in the stomach.\n",this_object(),attacker);
          break;

        case 3..5:
          msg("\b1PRON graze$ \b2OBJ.\n",this_object(),attacker);
          break;

        case 6..10:
          msg("\b1PRON hit$ \b2OBJ.\n",this_object(),attacker);
          break;

        case 11..15:
          msg("\b1PRON hit$ \b2OBJ hard.\n",this_object(),attacker);
          break;

        case 16..20:
          msg("\b1PRON hit$ \b2OBJ very hard.\n",this_object(),attacker);
          break;

        case 21..25:
          msg("\b1PRON hit$ \b2OBJ with a bonecrushing sound.\n",this_object(),attacker);
          break;

        case 26..40:
          msg("\b1PRON massacer$ \b2OBJ to small fragments.\n",this_object(),attacker);
          break;

        default:
          msg("\b1PRON nuke$ \b2OBJ to vapours.\n",this_object(),attacker);
          break;
      }
    }
  }
}

void look(string s) {} /* stubs routine */

void alert_followers(object from,string how,object dest)
{
  object o;
  followers-=({0});
  followers&=all_inventory(from);
  map_array(followers,"do_follow",how,dest);
}

int move_player(string how,mixed dest)
{
  object e;
  string in,out;

  e=environment();
  if(!dest) sscanf(how,"%s#%s",how,dest);
  dest=(object)dest;

  e=environment();
  if(e->player_leave(how,dest)) return 0;

  if(how=="X")
  {
    out=mmsgout;
    in=mmsgin;
  }else{
    out=replace(msgout,"\bWHERE",how);
    in=msgin;
  }

  move_object(dest);
  tell_room(e,replace(out,"\b1PRON",query_name()));
  tell_room(environment(),replace(msgin,"\b1PRON",query_name()),({this_player()}));
  if(how!="X") alert_followers(e,how,dest);
  map_array(all_inventory(environment())-({this_object()}),"living_arrives",this_object());
}

void living_arrives(object who)
{
  if(who==alt_attacker)
  {
    swap(attacker,alt_attacker);
    swap(attacker_life_count,alt_attacker_life_count);
  }
  if(who==attacker)
    attack();
}

int do_follow(string how,object dest)
{
  return move_player(how,dest);
}

int check_attacker()
{
  if(!attacker)
  {
    attacker=alt_attacker;
    attacker_life_count=alt_attacker_life_count;
    alt_attacker=0;
  }
  if(!attacker) return;
  if(environment(attacker)!=environment())
  {
    if(alt_attacker && environment(alt_attacker)==environment())
    {
      swap(attacker,alt_attacker);
      swap(attacker_life_count,alt_attacker_life_count);
    }else{
      return;
    }
  }
  if(attacker->query_life_count!=attacker_life_count)
  {
    attacker=0;
    return check_attacker(); /* tailrecurse */
  }
  return 1;
}

int spell_cost;
mixed spell_mess,spell_dam;
string magic_type;

void magical_attack()
{
  if(!check_attacker()) return;
  if(spell_points>=spell_cost)
  {
    int dam;
    spell_points-=spell_cost;
    if(intp(spell_dam))
    {
      dam=attacker->magic_hit(spell_dam,magic_type);
      if(functionp(spell_mess))
      {
        spell_mess(dam);
      }else{
        msg(spell_mess,this_object(),attacker);
        switch(dam)
        {
          case -10000..1:
            msg("\b1PRON are unaffected.\n|\b1PRON is unaffected.\n",attacker);
            break;
  
          case 2..8:
            msg("\b1PRON are slightly shook.\n|\b1PRON looks slightly shaken.\n",attacker);
            break;
  
          case 9..20:
            msg("\b1PRON are damaged.\n|\b1PRON looks damaged.\n",attacker);
            break;
  
          case 21..40:
            msg("\b1PRON are seriously hit.\n|\b1PRON is seriously hit.\n",attacker);
            break;
  
          default:
            msg("\b1PRON are fundamentally destroyed.\n|\b1PRON is fundamentally destroyed.\n",attacker);
            break;
        }
      }
    }else{
      spell_dam(attacker); /* do it yourself */
    }
  }
  spell_mess=spell_dam=spell_cost=0;
}

int magical_hit(int power,string type)
{
  power-=random(magic_protection[type]);
  if(power<=0) return 0;
  hit_points-=power;
  if(hit_points<0) die();
  return power;
}

void set_spell(int cost,mixed dam,mixed mess,string type)
{
  spell_cost=cost;
  spell_dam=dam;
  spell_mess=mess;
  magic_type=type;
}

void attack()
{
  set_heart_beat(1);
  magical_attack();
  physical_attack();
}

void heart_beat()
{
  attack();
  set_heart_beat(check_attacker());
}

/* money stuff */
int money;
void add_money(int m) { money+=m; }
int query_money() { return money; }
int check_money(int m)
{
  if(money>=m)
  {
    money-=m;
    return 1;
  }
  return 0;
}

int silent_unwield(object o,int mode)
{
  if(o->do_unwield(this_object()))
    return 0;

  wielded-=({o});
  return 1;
}

int unwield(object o,int mode)
{
  if(-1==member_array(o,wielded))
  {
    if(mode) return 1;
    tell_object(this_object(),"You're not wielding "+o->short()+".\n");
    return 0;
  }

  if(!silent_unwield(o,mode))
    return 0;

  tell_object(this_object(),"You unwield "+o->short()+".\n");
  tell_room(environment(),query_name()+" wields "+o->short()+".\n",({this_object()}));
  return 1;
}

int silent_wield(object o,int mode)
{
  if(o->do_wield(this_object()))
    return 0;
  wielded+=({o});
  return 1;
}

int wield(object o,int mode)
{
  if(-1!=member_array(o,wielded))
  {
    if(mode) return 1;
    tell_object(this_object(),"You're already wielding "+o->short()+".\n");
    return 0;
  }

  while(sizeof(wielded))
    if(!unwield(wielded[0],0))
      return 0;

  if(!silent_wield(o,mode))
    return 0;

  tell_object(this_object(),"You wield "+o->short()+".\n");
  tell_room(environment(),query_name()+" wields "+o->short()+".\n",({this_object()}));
  return 1;
}
