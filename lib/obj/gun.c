inherit "/std/thing";
inherit "/inherit/msg";

int bullets;
int chanse=-1;
string type="gun";
object wielder;

int quality;
int query_quality() { return quality; }
void set_quality(int q) { quality=q; }

int damage;
int query_damage() { return damage; }
void set_damage(int d) { damage=d; }

string query_gun_type()
{
  return "gun";
}

int query_chanse()
{
  if(chanse<0)
  {
    if(!wielder) return 0;
    chanse=query_quality()*wielder->qury_skill(query_gun_type())/100;
    if(!chanse)
      chanse=query_quality()*wielder->qury_skill("firearms")/200;
  }else{
    return chanse;
  }
}

void fire(object who)
{
  if(this_player()->check_busy()) return;

  if(bullets)
  {
    bullets--;
    if(query_chanse()>=random(100))
    {
      if(query_chanse()>=random(100))
      {
        msg("\b1OBJ fire$ and hit \b2OBJ between the eyes.\n",this_player(),who);
        who->hit_player(0x7fffffff);
      }else{
        msg("\b1OBJ fire$ and wound$ \b2OBJ.\n",this_player(),who);
        who->hit_player(random(query_damage()));
      }
    }else{
      msg("\b1OBJ fire and miss.\n",this_player());
    }
  }else{
    msg("\b1POSS gun goes 'click'.\n",this_player());
  }
}

void do_attack(object who)
{
  fire(who);
  return 1;
}

int shoot(string s)
{
  object o;
  if(wielder!=this_player())
  {
    write("You're not wielding the "+query_name()+".\n");
    return 1;
  }
  notify_fail("Shoot what?\n");
  o=present(s,environment(this_player()));
  if(!o) return 0;
  fire(o);
  return 1;
}

int do_fire(string s)
{
  string a,b;
  if(!sscanf(s,"%s at %s",a,b))
  {
    notify_fail("Fire what at what?\n");
    return 0;
  }
  if(!id(a))
  {
    notify_fail("Fire what?\n");
    return 0;
  }
  return shoot(b);
}

/* put gun in holster
 * draw gun
 * reload gun
 * fire gun at <foo>
 */

void create()
{
  set_damage(1);
  set_quality(1);
  set_name("gun");
  set_short("a gun");
  set_long("It's not a very good gun.\n");
  set_property("value",300);
}

int do_wield(object who)
{
  wielder=who;
}

int do_unwield()
{
  wielder=0;
}

int reload(string s)
{
  /* */
}

void init()
{
  ::init();
  add_action("fire %s",do_fire);
  add_action("shoot %s",shoot);
  add_action("reload %s",reload);
}
