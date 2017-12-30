#define SAVEFILE "/save/players"

inherit "/std/living";
inherit "/inherit/interactive";
inherit "/inherit/find";

string password;

void save(int nocount)
{
  if(!nocount)
  {
    object o;
    int value;
    string *s;
    foreach(all_inventory(),o) value+=o->query_value();
    set_property("lost_value",value);
    s=map_array(all_inventory(),"query_auto_load");
    if(!sizeof(s)) s=0;
    set_property("auto_load",s);
  }
  if(!db_set(SAVEFILE,query_real_name(),save_object()))
  {
    write("Saving failed.\n");
  }
}

void set_name(string s)
{
  string tmp;
  name=s;
  set_short(capitalize(s+" the novice"));
  full_name=capitalize(name);
  full_name=replace(full_name," ","_");
  tmp=db_get(SAVEFILE,s);
  if(tmp) restore_object(tmp);
  if(!query_long())
    set_long(query_pronoun()+" looks like a tough person.\n");
  move_object("/room/church");
  clone_object("/obj/soul")->move(this_object());
}

void out(string s)
{
  write(sprintf("%-=78s\n",s));
}

int take(object o,int mode)
{
  int w;
  
  if(environment(o)==this_object())
  {
    if(!mode)
    {
      write("You already have "+o->short()+"\n");
    }
    return 1;
  }

  if(!o->query_get())
  {
    write("You can't take "+o->short()+".\n");
    return 1;
  }

  if(!add_weight(w=o->query_weight()))
  {
    write("You cannot carry more.\n");
    return 0;
  }
  write("You take "+o->short()+".\n");
  environment(o)->add_weight(-w);
  o->move(this_object());
  tell_room(environment(),
      query_name()+" takes "+o->short()+".\n",({this_object()}));
  return 1;
}

int drop(object o,int mode)
{
  if(environment()==environment(o) && mode) return 1;
  if(environment(o)!=this_object())
  {
    write("You don't have "+o->short()+".\n");
    return 0;
  }
  if(!unwield(o,1))
    return 0;
  if(!o->query_drop(0)) return 0;
  add_weight(-o->query_weight());
  o->move(environment());
  write("You drop "+o->short()+".\n");
  tell_room(environment(),
      query_name()+" drops "+o->short()+".\n",({this_object()}));
  return 1;
}

int give(object o,object to,int mode)
{
  int w;
  if(environment(o)!=this_object())
  {
    write("You don't have "+o->short()+".\n");
    return 0;
  }
  if(!to->is_living())
  {
    write(capitalize(to->short()+" can't take it.\n"));
    return 0;
  }
  if(environment(to)!=environment())
  {
    write(to->query_name()+" isn't here.\n");
    return 0;
  }
  if(!o->query_drop(0)) return 0;
  w=o->query_weight();
  if(!o->add_weight(o))
  {
    write(to->query_name()+" can't carry more.\n");
    return 0;
  }
  add_weight(-o->query_weight());
  o->move(to);
  to->given(o);
  write("You give "+o->short()+" to "+to->query_name()+".\n");
  to->catch_tell(query_name()+" gives "+o->short()+" to you.\n");
  tell_room(environment(),
      query_name()+" gives "+o->short()+" to "+to->query_name()+".\n",
      ({this_object(),to}));
  return 1;
}

int put(object o,object in,int mode)
{
  int w;
  if(environment(o)!=this_object())
  {
    write("You don't have "+o->short()+".\n");
    return 0;
  }
  if(!in->query_prevent_insert())
  {
    write("You can't put "+o->short()+" in "+in->short()+".\n");
    return 0;
  }
  if(environment(in)!=environment() && environment(in)!=this_object())
  {
    write(in->short()+" isn't here.\n");
    return 0;
  }
  if(!o->query_drop(0)) return 0;
  w=o->query_weight();
  if(!o->add_weight(o))
  {
    write("You can't put more in "+in->query_name()+".\n");
    return 0;
  }
  add_weight(-o->query_weight());
  o->move(in);
  write("You put "+o->short()+" in "+in->query_short()+".\n");
  tell_room(environment(),
      query_name()+" puts "+o->short()+" in "+in->query_name()+".\n",
      ({this_object()}));
  return 1;
}

void look(string s)
{
  string tmp;
  object o;
  
  out(environment()->query_long());
  foreach(all_inventory(environment()),o)
  {
    if(o==this_object()) continue;
    tmp=o->short();
    if(tmp) write(" "+capitalize(tmp)+".\n");
  }
}

mapping secure_cmds=([]);
mapping cmds=([]);

static string verb;
string notify_fail;

void set_notify_fail(string n) { notify_fail=n; }
void query_notify_fail() { return notify_fail; }
string query_verb() { return verb; }

void catch_input(string str)
{
  mixed tmp,tmp2;
  string arg;
  object o;
  int e;

  set_this_player(this_object());

  if(str[0]=='\'') str="say "+str[1..1000000];

  if(!sscanf(str,"%s %s",verb,arg))
    verb=str;
  notify_fail="What?\n";

  switch(verb)
  {
  case "wield":
    o=find_one_visible_object(arg,({environment(),this_object()}));
    if(take(o,1))
      wield(o,0);
    break;

  case "unwield":
    o=find_one_visible_object(arg,({environment(),this_object()}));
    if(take(o,1))
      unwield(o,0);
    break;

  case "shutdown":
    shutdown();
    return;

  case "quit":
    save();
    msg("\b1PRON left the game.\n",this_object());
    selfdestruct();
    return;

  case "i":
  case "inventory":
    notify_fail="You're not carrying anything.\n";
    foreach(all_inventory(this_object()),o)
    {
      tmp=o->short();
      if(tmp)
      {
        notify_fail="";
        write(" "+capitalize(tmp)+".\n");
      }
    }
    write(notify_fail);
    break;

  case "l":
  case "look":
    if(!arg)
    {
      look();
      break;
    }
    if(!sscanf(arg,"at %s",arg))
    {
      write("Look AT something, will you?\n");
      break;
    }
    /* fall through */

  case "x":
  case "exa":
  case "examine":
    if(!arg)
    {
      write("Examing what?\n");
      break;
    }
    o=find_one_object(arg,({environment(),this_object()}));
    if(!o && environment()->id(arg)) o=environment();
    if(!o)
    {
      write("There is no "+arg+" here.\n");
      break;
    }
    notify_fail=o->query_long();
    if(notify_fail)
      out(notify_fail);
    else
      write("You can't see it.\n");
    break;

  case "take":
  case "get":
    if(!arg)
    {
      write("Take what?\n");
      break;
    }
    tmp=get_visible_objects(arg,({environment(),this_object()}))-({this_object()});
    if(!sizeof(tmp))
    {
      write("There is no "+arg+" here.\n");
      break;
    }
    map_array(tmp,take,0);
    break;

  case "drop":
    if(!arg)
    {
      write("Drop what?\n");
      break;
    }
    tmp=get_visible_objects(arg,({this_object()}));
    if(!sizeof(tmp))
    {
      write("There is no "+arg+" here.\n");
      break;
    }
    foreach(tmp,o) drop(o,0);
    break;

  case "put":
    if(!arg)
    {
      write("Put what?\n");
      break;
    }
    if(!sscanf(arg,"%s in %s",arg,tmp))
    {
      write("Put what in what?\n");
      break;
    }
    tmp2=get_visible_objects(arg,({environment(),this_object()}))-({this_object()});
    if(!sizeof(tmp2))
    {
      write("There is no "+arg+" here.\n");
      break;
    }
    o=find_one_object(arg,({environment(),this_object()}));
    if(!o)
    {
      write("There is no "+arg+" here.\n");
      break;
    }
    foreach(tmp2,tmp) if(take(tmp,1)) put(tmp,o,0);
    break;

  case "give":
    if(!arg)
    {
      write("Give what?\n");
      break;
    }
    if(!sscanf(arg,"%s to %s",arg,tmp))
    {
      write("Give what to whom?\n");
      break;
    }
    tmp2=get_visible_objects(arg,({environment(),this_object()}))-({this_object()});
    if(!sizeof(tmp2))
    {
      write("There is no "+arg+" here.\n");
      break;
    }
    o=present(tmp,environment());
    if(!o || !living(o))
    {
      write(capitalize(tmp)+" is not here.\n");
      break;
    }
    foreach(tmp2,tmp) if(take(tmp,1)) give(tmp,o,0);
    break;

  case "say":
    if(!arg)
    {
      write("Say what?\n");
      break;
    }
    write("You say: "+arg+"\n");
    tell_room(environment(),query_name()+" says: "+arg+"\n",({this_object()}));
    break;
    
  case "who":
    tmp2=0;
    foreach(users(),o)
    {
      if(tmp=o->short())
      {
        write(tmp+".\n");
        tmp2++;
      }
    }
    write(tmp2+" player"+(tmp2==1?"":"s")+".\n");
    break;

  case "help":
    if(!arg) arg="help";
    tmp=read_bytes("/doc/help/"+replace(arg," ","_"));
    if(!tmp)
      if(o=find_one_object(arg,({environment(),this_object()})))
        tmp=o->help(arg);
    if(!tmp) tmp=environment()->help(arg);

    if(tmp)
    {
      if(tmp[-1]!='\n') tmp+="\n";
      write(tmp);
    }else{
      write("No such help available.\n");
    }
    break;

  case "bug":
  case "idea":
  case "typo":
  case "praise":
    /* theese needs to be improved */
    if(!arg)
    {
      write("You must state what you like to "+verb+".\n");
    }else{
      tmp=hash_name(environment());
      if(!tmp) tmp=file_name(environment());
      log_file("REPORTS",
        sprintf("%s by %s %s in %s:\n %s\n",
          verb,query_real_name(),ctime(time()),tmp,arg));
      write("Thank you.\n");
    }
    break;

  case "score":
    write("You have "+cents_to_string(query_money())+".\n");
    break;

  case "save":
    save();
    write("Ok.\n");
    break;

  case "finger":
  case "tell":
  case "converse":
  case "shout":
    write("Not yet implemented.\n");
    break;

  default:
    /* the secure commands needs to be moved before the
     * switch if a 'filter' is added before the switch()
     */
    if(tmp=secure_cmds[verb])
    {
      if(file_name(previous_object())=="/secure/ip")
      {
        if(search_array(tmp,0,arg)!=-1)
          break;
      }else{
        write("Security alert!!!\n");
        return;
      }
    }

    if(tmp=cmds[verb])
      if(search_array(tmp,0,arg)!=-1)
        break;

    if(!command(str))
       write(notify_fail);
  case "":
  }
  write("> ");
}


/* this is only efficient when adding commands to the playerobject
 * more or less permanently (due to slow remove_cmd)
 */

mixed add_cmd(string verb,function f)
{
  mixed tmp;
  if(tmp=cmds[verb])
  {
    cmds[verb]=({f})+(tmp-({0}));
  }else{
    cmds[verb]=({f});
  }
}

mixed remove_cmd(mixed any)
{
  if(objectp(any))
  {
    if(any!=previous_object()) return;
    cmds=map_mapping(cmds,lambda(function *funs,object o)
      {
        return filter_array(funs,lambda(function f,object any)
          {
            return f && function_object(f)!=any;
          },o);
      },any);
  }else if(functionp(any)){
    cmds=map_mapping(cmds,lambda(function *funs,function f)
      {
        return funs-({0,f});
      },any);
  }else if(stringp(any)){
    if(cmds[any])
    {
      cmds[any]=filter_array(cmds[any],lambda(function f,object any)
        {
          return f && function_object(f)!=any;
        },previous_object());
    }
  }else if(pointerp(any)){
    map_array(any,remove_cmd);
  }else if(listp(any)){
    map_array(indices(any),remove_cmd);
  }else{
    write("Odd type to remove_cmd()\n");
  }
}

void set_ld(int l)
{
  if(l)
  {
    msg("\n1PRON suddenly turns into a stone statue.\n",this_object());
  }
}
