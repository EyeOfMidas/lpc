string *define_include_dirs(string bindir,string libdir)
{
  string foo;
  if(foo=getenv("LPC_INCLUDE_PATH"))
    return map_array(foo/":",lambda(string s)
		   {
		     if(-1==strstr("%s",s))
		     {
		       if(s[-1]!='/') return s+"/%s";
		       return s+"%s";
		     }else{
		       return s;
		     }
		   });
  return ({libdir+"/include/%s",});
}

#if 0
string get_simul_efun(string bindir,string libdir)
{
  string s;
  if(s=getenv("LPC_SIMUL_EFUN")) return s;
  return 0;
}
#endif

int query_player_level(string what) { return 1; }
int valid_exec(string name,object from, object to) { return 1; }
int valid_add_simul_efun() { return 1; }
int valid_override() { return 1; }
inline string get_root_uid() { return "Root"; }

int query_valid_destruct(object o) { return 1; }
mixed creator_file(string file) { return get_root_uid(); }
int valid_seteuid(object o,string euid) { return 1; }
int valid_socket(object ob,string action,mixed *info) { return 1; }

function in,sig;

void batch(string flag,string *argv,string *env)
{
  int e;
  object script;

  if(!sizeof(argv))
  {
    write("Usage: lpc -C[stay] <script>\n");
    exit(2);
  }

  script=(object)(argv[0]);
  sig=script->signal;

  e=script->main(sizeof(argv),argv,env);
  
  if(flag!="stay")
    exit(intp(e)&&e);

  if(script) in=script->stdin;
}

void stdin(string foo) { in(foo); }
int signal(int foo) { return sig(foo); }
