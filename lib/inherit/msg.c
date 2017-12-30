/*
           (self)	(3rd)		(plural)	(plur 3rd)
pronoun:   you		he/she/it	names		they
possesive: your		his/her/its	foo's & bar's	their
objective: you		him/her/it	names		them

pron: name   you  he/she/it	they
poss: name's your his/her/its	their
obj:  name   you  him/her/them	them

\b1PRON hit$ \b2PRON on \b2poss the head with \b1poss sign
*/

string fix_msg(string s,mixed *persons,object active)
{
  string *t,*t2,tmp;
  int e,d,p;
  t=explode(s,"\b");
  for(e=1;e<sizeof(t);e++)
  {
    if(sscanf(t[e],"%dPRON%s",p,tmp)==2 ||
	(sscanf(t[e],"%dpron%s",p,tmp)==2) && -1!=member_array(active,persons[p]))
    {
      p--;
      t[e]=implode_nicely(map_array(persons[p],lambda(object who,object active)
		{
		  if(who==active) return "you";
		  return who->query_name();
		}
		,active))+tmp;
    }else if(sscanf(t[e],"%dPOSS%s",p,tmp)==2 ||
	(sscanf(t[e],"%dposs%s",p,tmp)==2) && -1!=member_array(active,persons[p])){
      p--;
      t[e]=implode_nicely(map_array(persons[p],lambda(object who,object active)
		{
                  string foo;
		  if(who==active) return "your";
		  foo=who->query_name();
                  if(foo[-1]=='s') return foo+"'";
		  return foo+"'s";
		}
		,active))+tmp;
    }else if(sscanf(t[e],"%dOBJ%s",p,tmp)==2 ||
	(sscanf(t[e],"%dobj%s",p,tmp)==2) && -1!=member_array(active,persons[p])){
      p--;
      t[e]=implode_nicely(map_array(persons,lambda(object who,object active)
		{
		  if(who==active) return "you";
		  return who->query_name();
		}
		,active))+tmp;
    }else if(sscanf(t[e],"%dpron%s",p,tmp)==2){
      if(sizeof(persons[p])==1)
      {
        t[e]=persons[p]->query_pronoun()+tmp;
      }else{
        t[e]="they"+tmp;
      }
    }else if(sscanf(t[e],"%dposs%s",p,tmp)==2){
      if(sizeof(persons[p])==1)
      {
        t[e]=persons[p]->query_possessive()+tmp;
      }else{
        t[e]="their"+tmp;
      }
    }else if(sscanf(t[e],"%dobj%s",p,tmp)==2){
      if(sizeof(persons[p])==1)
      {
        t[e]=persons[p]->query_objective()+tmp;
      }else{
        t[e]="them"+tmp;
      }
    }else{
      t[e]="\n"+t[e];
    }
  }
  return implode(t,"");
}


varargs string msg(string message,mixed first,mixed second)
{
  string st_msg,nd_msg,rd_msg;
  mixed *tmp;
  object o;

  if(sscanf(message,"%s|%s",st_msg,nd_msg))
  {
    if(sscanf(nd_msg,"%s|%s",nd_msg,rd_msg))
    {
      nd_msg=replace(nd_msg,"$","s");
      rd_msg=replace(rd_msg,"$","s");
    }else{
      rd_msg=nd_msg=replace(nd_msg,"$","s");
    }
  }else{
    nd_msg=rd_msg=replace(st_msg=message,"$","s");
  }
  st_msg=replace(st_msg,"$","");

  if(!first) first=({this_object()});
  if(!second) second=({});
  if(!pointerp(second)) second=({second});
  if(!pointerp(first)) first=({first});
  tmp=({first,second});

  foreach(first,o) o->catch_tell(capitalize(fix_msg(st_msg,tmp,o)));
  foreach(second,o) o->catch_tell(capitalize(fix_msg(nd_msg,tmp,o)));
  tell_room(environment(first[0]),capitalize(fix_msg(rd_msg,tmp,0)),
	    first+second);
}
