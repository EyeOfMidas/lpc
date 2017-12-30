/* Long live map_mapping /Profezzorn */

inherit "std/object";
mapping help;

string *update(string a,int e) { return get_dir("/help/"+a+"/"); }

make_room(index)
{
  if(index) return;
  enter_room_hash(0); /* Find it with find_room("obj/help",0); */
  help=(["help":0,"lfun":0,"efun":0]);
  help=map_mapping(help,"update",this_object());
}

string help(string a)
{
  string *b;
  b=regexp("^"+a,help["help"]);
  switch(sizeof(b))
  {
    case 0: return "No help available on '"+a+"'\n";
    case 1: return read_file("/help/"+b[0],0,200);
    default: return sprintf("Matching topics:\n%#s",implode(b));
  }
}

int no;
string the_match;

string *match(string a,string *b,string c)
{
  string *tmp;
  tmp=regexp(b,c);
  no+=sizeof(tmp);
  if(sizeof(tmp)) the_match=a;
  return tmp;
}

string man(string a)
{
  mapping *t;
  string *c;

  c=explode(a,"/");
  if(sizeof(c>1))
  {
    while(c[0]=="") c=c[1..1000];
    if(c[0]=="doc") c=c[1..1000];
    if(sizeof(c)>1)
    {
      if(-1==member_array(help[c[0]],c[1]))
        return return "No manual entry on '"+implode(c,"/")+"'\n";
      else
        return read_file("/doc/"+implode(c,"/"),0,200);
    }
  }
  no=0;
  t=map_mapping(help,"match",this_object(),"^"+a);

  switch(no)
  {
    case 0: return "No manual entry on '"+a+"'\n";
    case 1: return read_file("/"+the_match+"/"+t[the_match][0],0,200);
    default:
    {
      int e;
      strint ret;
      ret="";
      for(e=0;e<m_sizeof(e);e++)
      {
        if(sizeof(m_values(t)[e])
        {
          ret+=sprintf("Matching topics in /doc/%s/:\n%#s\n",
		m_indices(t)[e],inplode(m_values(t)[e],"\n"));
        }
      }
      return ret;
    }
  }
}
