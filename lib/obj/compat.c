/* simul efun */

int num;

object clone_object(string s)
{
  (s+"#"+num)->a;
  return find_object(o);
}

string file_name(object o)
{
  return hash_name(o);
}

object _destruct(object o)
{
  o->__selfdestruct();
  return o;
}

object destruct(object o)
{
  if(environment(o))
  {
    environment(o)->add_weight(-o->query_weight());
    while(first_inventory(o))
      first_inventory(o)->__move(environment(o));
  }
  return _destruct(o);
}

string query_verb()
{
  return (query_verb()/"%")[0];
}

varargs mixed call_other(object o,string fun,
			 mixed a,
			 mixed b,
			 mixed c,
			 mixed d,
			 mixed e,
			 mixed f,
			 mixed g,
			 mixed h,
			 mixed i,
			 mixed j,
			 mixed k,
			 mixed l,
			 mixed m)
{
  return get_function(o,fun)(a,b,c,d,e,f,g,h,i,j,k,l,m);
}

int save_object(string file)
{
  file+=".o";
  rm(file);
  write_file(previous_object()->__dump_data());
  return 1;
}


/* inherit */
#pragma save_types
static int restore_object(string file)
{
  file+=".o";
  restore_object(read_file(file));
}

void __selfdestruct() { destruct(); }
void __move(object o) { move_object(o); }
string __dump_data() { return save_object(); }

varargs string add_action(string fun,string v,int flag)
{
  if(flag)
  {
    add_action(v+"%s",get_function(this_object(),fun));
  }else{
    add_action(v+" %s",get_function(this_object(),fun));
  }
  return fun;
}

/* include */
#pragma unpragma_strict_types
inherit "/inherit/compat";




