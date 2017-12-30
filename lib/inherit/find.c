#pragma save_types
int is_adjective() { return 0; }

object *get_objects(string s,object *in)
{
  int all,num,e,d,c,plural;
  mixed *adj;
  object *ret,o;
  string a,b;

  s=(s/" "-({""}))*" ";
  if(s=="") return ({});
  if(!in) in=({this_player(),environment(this_player())});

  if(sscanf(s,"%s in %s",a,b) || sscanf(s,"%s from %s",a,b))
  {
    in=get_objects(b,in);
    s=a;
  }

  for(a=0;e<sizeof(in);e++) if(o=present(s,in[e])) return ({o});

  sscanf(s,"the %s",s);
  s+="    ";
  if(sscanf(s,"everything %s",s) || sscanf(s,"all %s",s))
  {
    plural=1;
    all=1000;
  }

  if(sscanf(s,"%s %s",a,s))
  {
    switch(a)
    {
    case "first": num=0; break;
    case "second": num=1; break;
    case "third": num=2; break;
    case "fourth": num=3; break;
    case "fifth": num=4; break;
    case "sixth": num=5; break;
    case "seventh": num=6; break;
    case "eighth": num=7; break;
    case "ninth": num=8; break;
    case "tenth": num=9; break;
    case "eleventh": num=10; break;
    case "twelfth": num=11; break;
    default: s=a+" "+s;
    }
  }

  if(sscanf(s,"%s %s",a,s))
  {
    switch(a)
    {
    case "one": all=1; break;
    case "two": all=2; break;
    case "three": all=3; break;
    case "four": all=4; break;
    case "five": all=5; break;
    case "six": all=6; break;
    case "seven": all=7; break;
    case "eight": all=8; break;
    case "nine": all=9; break;
    case "ten": all=10; break;
    case "eleven": all=11; break;
    case "twelve": all=12; break;
    default: s=a+" "+s;
    }
  }

  adj=({});

#define F_AND 0
#define F_OR 1

  while(sscanf(s,"%s %s",a,s))
  {
    if(is_adjective(a))
    {
      if(sscanf(s,"and %s",s))
      {
        adj+=({a,F_AND});
      }else if(sscanf(s,"or %s",s)){
        adj+=({a,F_OR});
      }else{
        adj+=({a,F_AND});
      }
    }else{
      s=a+" "+s;
      break; /* maybe? */
    }
  }


  ret=({});
  if(sscanf(s,"%s and %s",s,a)) ret+=get_objects(a);

  while(s[-1]==' ') s=s[0..strlen(s)-2];
  if((sscanf(s,"%d%s",e,a) && a=="") || sscanf(s,"%d coins",e))
  {
    object o;
    if(o=present("money",in[0])) /* change this */
    {
      if(o->query_value()==e) return ({o});
      if(o->query_value()<e) return ({});
      o->add_value(-e);
      o=clone_object("/obj/money");
      o->set_value(e);
      o->move(in[0]);
      return ({o});
    }
  }

  s=((s/" ")-({""}))*" ";
  if(num==1 && s[-1]>='0' && s[-1]<='9')
  {
    /* handle things like 'bottle 2' */

    for(e=1;s[-e]!=' ' && s[-1];e++);
    sscanf(s[e+1..strlen(s)],"%d",num);
    s=s[0..e-1];
  }

  for(e=0;e<sizeof(in);e++)
  {
    object *tmp;
    tmp=all_inventory(in[e]);
    for(d=0;d<sizeof(tmp);d++)
    {
      int t;
      mapping adjectives;
      mixed *tmp2;

      if((s!="" && s!="things") || !all)
      {
	if(plural)
	  tmp2=tmp[d]->query_plural_names();
	else
	  tmp2=tmp[d]->query_names();

	if(!tmp2 || -1==member_array(s,tmp2))
	  if(!tmp[d]->id(s) && !(s[-1]=='s' && tmp[d]->id(s[0..strlen(s)-2])))
	    continue;

      }
      t=1;

      if(sizeof(adj))
      {
        tmp2=tmp[d]->query_adjectives();

        if(tmp2)
          adjectives=mkmapping(tmp2,tmp2);
        else
          adjectives=([]);

        for(c=0;c<sizeof(adj) && t;c+=2)
          t=(adjectives[adj[c]] || t) ^ (adj[c+1]==F_OR);
      }

      if(t)
      {
        if(num) { num--; continue; }
	ret+=tmp[d..d];
	if(--all<0) return ret[d..d];
      }
    }
  }
  return ret;
}

object find_one_object(string s,object *in)
{
  object *tmp;
  tmp=get_objects(s,in);
  if(!tmp || !sizeof(tmp)) return 0;
  return tmp[0];
}

object *get_visible_objects(string s,object *in)
{
  return filter_array(get_objects(s,in),"short");
}

object find_one_visible_object(string s,object *in)
{
  int e;
  object *tmp;
  tmp=get_objects(s,in);
  e=search_array(tmp,"short");
  if(e==-1) return 0;
  return tmp[e];
}
