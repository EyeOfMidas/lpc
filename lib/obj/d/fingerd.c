#define SAVEFILE "/save/players"

string name;
string short_desc;
int level;
int last_login;
string plan;

static object finger(string s)
{
  object o;
	name=short_desc=plan=0;
	level=last_login=0;

  if(o=find_player(s)) o->save();
	s=db_get(SAVEFILE,s);
	if(!s) return 0;

	/* by doing this we can keep variables secret */
	restore_object(s);
	return o;
}


mapping m_finger(string s)
{
  object o;
  o=finger(s);
  return ([
    "name":name,
    "level":level,
    "short":short_desc,
    "last_login":last_login,
    "plan":plan,
    "logged_on":o,
  ]);
}

string s_finger(string s)
{
	finger(s);
	return save_object();
}
