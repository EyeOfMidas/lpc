inherit "/std/thing";
inherit "/std/living";

object debug;
private mixed *states=({0});
mapping envmatch=([]);
mapping matchbase=([]);
string *output_queue=({});

void queue_texts(string *texts)
{
  output_queue+=texts;
  set_heart_beat(1);
}

void heart_beat()
{
  set_heart_beat(0);
  /* don't forget to add ::heart_beat() */
  if(sizeof(output_queue))
  {
    tell_room(environment(),output_queue[0],({this_object()}));
    /* we don't want to que to much, do we? */
    output_queue=output_queue[1..25];
    if(sizeof(output_queue))
      set_heart_beat(1);
  }
}


/* replaces all occurances of repls[n] (n=even) with repls[n+1] in string */
string replace(string qstr, string *repls)
{
  int e;
  if(!qstr || qstr=="") return qstr;
  for(e=0;e<sizeof(repls);e+=2)
	qstr=implode(explode(qstr+repls[e],repls[e]),repls[e+1]);
  return qstr;
}

varargs string treat_string(
		    string str,
		    string a1,
		    string a2,
		    string a3,
		    string a4,
		    string a5,
		    string a6)
{
  if(!a1) a1="";
  if(!a2) a2="";
  if(!a3) a3="";
  if(!a4) a4="";
  if(!a5) a5="";
  if(!a6) a6="";
  return replace(str,({
	"#S1",a1,"#S2",a1,"#S3",a1,"#S4",a1,"#S5",a1,"#S6",a1,
	"#C1",capitalize(a1),"#C2",capitalize(a2),"#C3",capitalize(a3),
	"#C4",capitalize(a4),"#C5",capitalize(a5),"#C6",capitalize(a6),
  }));
}


void set_state(mixed state) { states[0]=state; }
void clear_state(mixed state) { states[0]=({0}); }
void push_state(mixed state) { states=({state})+states; }
mixed query_state() { return states[0]; }

void pop_state()
{
  states=states[1..1000000];
  if(!sizeof(states)) states=({0});
}

void do_grejs(function f,
	      mixed foo,
	      string a,
	      string b,
	      string c,
	      string d,
	      string e,
	      string f)
{
  f(foo,a,b,c,d,e,f);
}

int matchmaker2(string str,mixed *c)
{
  int e;
  string a1,a2,a3,a4,a5,a6;
  mixed tmp,t;

  if(!c) return 0;
  foreach(c,t)
  {
    if(random(100) >= t[0]) continue;
    if((!t[1] && t[2]==str) ||
       t[1]==sscanf(str,t[2],a1,a2,a3,a4,a5,a6))
    {
      tmp=t;
      break;
    }
  }
  if(!tmp) return 0;
  if(debug) tell_object(debug,query_name()+" match:"+tmp[2]+"\n");
  foreach(tmp[3],t)
  {
    if(random(100)<t[0])
    {
      if(!t[1])
      {
        t[2](t[3],a1,a2,a3,a4,a5,a6);
      }else{
        call_out(do_grejs,t[1],t[2],t[3],a1,a2,a3,a4,a5,a6);
      }
    }
  }
  return 1;
}

int matchmaker(string s,mapping m)
{
  if(!stringp(s)) return;
  return matchmaker2(s,m[query_state()]) || matchmaker2(s,m["*"]);
}

void do_submatch(mapping data,
		 string a,
		 string b,
		 string c,
		 string d,
		 string e,
		 string f)
{
  matchmaker(a+"\t"+b+"\t"+c+"\t"+d+"\t"+e+"\t"+f,data[query_state()]);
}

void do_answer_echo(string s,
		    string a1,
		    string a2,
		    string a3,
		    string a4,
		    string a5,
		    string a6)
{
  enable_commands();
  tell_room(environment(),treat_string(s+"\n",a1,a2,a3,a4,a5,a6));
}

void do_answer_say(string s,
		   string a1,
		   string a2,
		   string a3,
		   string a4,
		   string a5,
		   string a6)
{
  string t;
  enable_commands();
  s=treat_string(s+"\n",a1,a2,a3,a4,a5,a6);
  t=query_name()+" says: ";
  queue_texts(explode(sprintf("%*s%-=*s",strlen(t),t,77-strlen(t),s),"\n"));
}

void do_answer_command(string s,
		  string a1,
		  string a2,
		  string a3,
		  string a4,
		  string a5,
		  string a6)
{
  string q;
  q=treat_string(s,a1,a2,a3,a4,a5,a6);
  if(debug)
    tell_object(debug,query_name()+" doing: "+q+"\n");
  command(q);
}

/*** configuration ****/

/* remember, this is just a pointer */
static mapping _tmp;
private static mixed *current_match;

void new_talk_match() { _tmp=matchbase=([]); }
void continue_talk_match() { _tmp=matchbase; }

void new_env_match() { _tmp=envmatch=([]); }
void continue_env_match() { _tmp=envmatch; }

static void stoppa_in_match(mixed *match,mixed stat)
{
  mixed e;
  if(pointerp(stat))
  {
    foreach(stat,e)
    {
      if(!_tmp[e]) _tmp[e]=({});
      _tmp[e]+=({current_match});
    }
  }else{
    if(!_tmp[stat]) _tmp[stat]=({});
    _tmp[stat]+=({current_match});
  }
}

void add_match(int chance,string str,mixed stat)
{
  current_match=({ chance,sizeof(explode(str,"%")),str, ({}) });
  stoppa_in_match(current_match,stat);
}

void copy_match(int chance,string str,mixed stat)
{
  current_match=({ chance,sizeof(explode(str,"%")),str, current_match[3] });
  stoppa_in_match(current_match,stat);
}

void add_call(int chanse,function func,int delay,mixed arg)
{
  current_match[3]+=({({ chanse,delay,func,arg })});
}

void push_matches(int chanse,mixed state)
{
  mapping tmp;
  tmp=([]);
  add_call(chanse,do_submatch,state,tmp);
  push_state(_tmp);
  _tmp=tmp;
}

void pop_matches() 
{
  _tmp=query_state();
  pop_state();
}

void add_echo(int chance,string str,int delay)
{ add_call(chance,do_answer_echo,delay,str); }

void add_emote(int chance,string str,int delay)
{ add_echo(chance,query_name()+" "+str,delay); }

void add_say(int chance,string str,int delay)
{ add_call(chance,do_answer_say,delay,str,this_object()); }

void add_command(int chance,string com,int delay)
{ add_call(chance,do_answer_command,delay,com); }

void add_set_state(int chance,string stat,int delay)
{ add_call(chance,set_state,delay,stat); }

void add_push_state(int chance,string stat,int delay)
{ add_call(chance,push_state,delay,stat); }

void add_pop_state(int chance,int delay)
{ add_call(chance,pop_state,delay,0); }

void add_clear_state(int chance,int delay)
{ add_call(chance,clear_state,delay,0); }


/* lfuns */
void catch_tell(string msg)
{
  if(!stringp(msg)) return;
  if(debug)
    tell_object(debug,query_name()+" got: "+msg+"from:"+
		file_name(previous_object())+"\n");
  msg=lower_case(msg);
  matchmaker(msg,matchbase);
}

object get() { return 0; }

void move_player(string how, mixed where)
{
  ::move_player(how,where);
  matchmaker(hash_name(environment()),envmatch);
}


void set_name(string n)
{
  full_name=capitalize(name=n);
  set_living_name(n);
}

int id(string id)
{
  return living::id(id) || thing::id(id);
}

