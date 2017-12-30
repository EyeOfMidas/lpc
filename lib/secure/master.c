/* note that there is no documentation on how to write a masterobject,
 * I try to use comments in this object instead.
 */

#pragma strict_types
#include "../include/signal.h"
/*
 * This function should return 1 if the program is allowed to enter the mud.
 * If it returns 2 then other objects will be automaticly approved if they
 * inherit that program.
 */

int query_approve(string name)
{
  switch(name)
  {
  case "std/object":
  case "std/room": return 2;
  }

 if(name[0..7]=="inherit/" || name[0..3]=="obj/" ||
	name[0..3]=="std/" || name[0..6]=="secure/") return 1;
 return 0;
}

/*
 * if an argument on the command line to the driver starts with -f
 * whatever is after it will be sent with a call to this function.
 * To test a new function xx in object yy, do
 * parse "-fcall yy xx arg" "-fshutdown"
 */
static void flag(string str)
{
  string file, arg,func;
  int num_arg;
  int *foo;
  int e;

  write("Master: Doing Flag '"+str+"'\n");
  if (str == "shutdown") {
    write("Cpu usage in master "+object_cpu(this_object())+"\n");
    shutdown();
    return;
  }
  if(str=="for-test")
  {
    for(write("e=0;\n"),e=0;write("e<10;\n"),e<10;write("e++;\n"),e++)
      write("e="+e+"\n");
    shutdown();
  }

  if (sscanf(str, "echo %s", arg) == 1) {
    write(arg + "\n");
    return;
  }
  if (sscanf(str, "call %s %s %s", file, func, arg))
  {
    object o;
    mixed d;
    o=clone_object(file);
    d=(mixed)call_function(get_function(o,func), arg);
    write("Calling "+func+" in "+file+" with "+arg+"\n");
    write("Got "+sprintf("%O",d)+" back.\n");
    return;
  }
  write("master: Unknown flag " + str + "\n");
}

/*
 * Write an error message into a log file. The error occured in the object
 * 'file', giving the error message 'message'. This function is never
 * called in script mode
 */
void log_error(string file, string message)
{
  string name;

  name = "log";
  sscanf(file,"/w/%s/",name);
  write_file("/log/"+name, message);
}

/*
 * When an object is destructed, this function is called with every
 * item in that room. We get the chance to save players !
 * If nothing is done, the object is destructed.
 */
void destruct_environment_of(object ob)
{
  return;
    ob->catch_tell("Everything you see is disolved. Luckily, you are transported somewhere...\n");
    ob->move_player("is transfered#room/void");
}

/*
 * Define where the '#include' statement is supposed to search for files.
 * "." will automatically be searched first, followed in order as given
 * below. The path should contain a '%s', which will be replaced by the file
 * searched for.
 */
string *define_include_dirs()
{
  return ({"/include/%s",});
}

/* The master is given the uid and euid that this function returns. */
string get_root_uid() { return "Root"; }

/* May previous_object destruct o? If so, this function should return 1 */
int query_valid_destruct(object o)
{
  if(geteuid(previous_object())==get_root_uid())
    return 1;
  return 0;
}

/* When an object is cloned, this function is called to give it an uid and
 * euid, if it doesn't return a string, the cloning will be considered illigal.
 * This function might also want to look at this_player() and previous_object()
 * to figure out what uid/euid to give the new object.
 */
mixed creator_file(string file)
{
  string *str;

  if (!file || !stringp(file)) return "NOONE";
  str = explode(file, "/");

  if (sizeof(str)<2) return 0;
  if (str[0] == "secure") return get_root_uid();
  if (str[0] == "global" || str[0] == "std" || str[0] =="room" ||
      str[0] == "obj") return "Backbone";
  if (str[0] == "tmp") return str[1];
  if (str[0] == "include") return 0;
  if (str[0] == "failsafe") return "failsafe";
  if (sizeof(str) > 2 && str[0] == "d")
    return "Dom: "+str[1];
  if (sizeof(str) == 2 && str[1] == "common" && str[0] == "w")
    return "womble-frog";
  if (sizeof(str) < 3 || str[0] != "w" ) return 0;
  return str[1];
}

/* This function is called whenever a write to a file is executed,
 * euid the euid of the object that wants to write, path is the file it wants
 * to write to, and func is a string that describes what it is trying to do.
 * valid_write should return 1 if the object is allowed to write.
 * This function is not called in script mode, it is assumed that the user
 * is allowed to write anywhere in script mode.
 */

int valid_write(string path, string euid, string func)
{
  string *b;
  if(euid==get_root_uid()) return 1;
  b=explode(path,"/");
  while(sizeof(b) && b[0]=="") b=b[1..sizeof(b)-1];
  if(sizeof(b)<2) return 0;
  switch(b[0])
  {
  case "save":
  case "obj":
    if(euid=="Backbone") return 1;

  case "open":
  case "log":
    return 1;

  case "w":
    return b[1]==euid;
  }
}

/* this function is like valid_write, but affects reads */

int valid_read(string path, string euid, string func)
{
  string *b;

  if(valid_write(path,euid,func)) return 1;
  if(euid=="Backbone") return 1;
  b=explode(path,"/");
  while(sizeof(b) && b[0]=="") b=b[1..sizeof(b)-1];
  if(sizeof(b)<2) return 0;
  switch(b[0])
  {
    case "std":
    case "log":
    case "doc":
    case "players":
    case "etc":
    case "open": return 1;
    case "w":
      return b[1]==euid;
  }
}


/* This function is called whenever an error occurs, It is up to this
 * function to deliver the error message to the wizard who made it.
 * Note that this function is called after the error has been cleared,
 * it is therefor impossible to use previous_objects() to gain further
 * information about the error. The arguments are:
 * error, as string describing the error
 * current, the object the error occured in
 * prog_name, the name of the program it occured in
 * ob_name, the name of the object it occured in, if any
 * initer, previous_objects()[0] when the error occured.
 * linenumber, what line it occured on.
 */
void handle_error(string error,
		   object current,
		   string prog_name,
		   string ob_name,
		   object initer,
		   int linenumber)
{
  if(this_player())
  {
    if(1 /* put a test if the player is wizard here */ )
    {
      this_player()->catch_tell(
		  sprintf("Error %sIn %s (%s) line %d\n",
			  error,
			  ob_name,
			  prog_name,
			  linenumber));
    }else{
      this_player()->catch_tell(
	  "You think you see some gremlins in the corner of your eye.\n");
    }
    this_player()->fflush();
  }
    
}

/* May the object o set it's euid to euid? if so, this function should
 * return 1
 */
int valid_seteuid(object o,string euid)
{
  if(o==this_object() || geteuid(o)==get_root_uid()) return 1;
  return 0;
}


/* Is the object ob allowed to do the action 'action' on a socket?
 * info is an array containing more precise information about what it's
 * trying to do, and on what socket.
 */

int valid_socket(object ob,string action,mixed *info)
{
  if(geteuid(ob)!=get_root_uid()) return 1;
  return 1;
}

/* The driver catches signals, they all wind up here, sig is a bitfield
 * containing all signals ( or rather: 1<<signal_number ) received or:ed
 * together. Currently SIGTERM, SIGINT, SIGUSR1 and SIGUSR2 are catched and
 * if the driver is compiled without DEBUG defined, SIGSEGV, SIGFPE, SIGBUS
 * and SIGQUIT are also catched. If this function returns 0 a 'default' action
 * for that signal is executed. Note that this function can be called when
 * other code is executing therefor special care has to be taken so that
 * it does not interfere with that code.
 */
int signal(int sig)
{
  perror(sprintf("master: Got signal %x.\n",sig));
  return 0;
}


/* This function is called whenever the driver receives something on stdin.
 * It can be used to make it possible to login from stdin for easy debugging.
 */
void stdin(string s) {}

/* this function is called when the memory is low, generally, it should
 * try to finish the mud in a nice manner within a few minutes. It is also
 * the default action for SIGUSR1
 */
void shut() { }

/* is previous_object() allowed to use add_worth() ?
 * if so, this function should return 1
 */
int valid_add_worth() { return 1; }


/* this function is called when the driver is runned in script mode
 * see doc/script_mode for further details (not lib/doc/script_mode)
 */
int batch(string arg,string *argv,string *env)
{
  write("Wrong master for script mode.\n");
  exit(1);
}

/* this function is called when the -I option is used, it will probably only
 * be used when running the driver from /etc/services
 */
void stdin_is_sock(int script_mode) { }

/* When the driver casts a string to an object, and can't find a program to
 * use for cloning, this function is called, it is supposed to return an
 * object which will be given the name 'file' in the hashtable.
 */
object compile_object(string file)
{
  object o;
  string who, where;
  if(sscanf(file,"w/%s/%s",who,where))
  {
    o=call_other("/w/"+who+"/vcompile","compile_object",file);
  }else if(sscanf(file,"%s#%s",who,where)){
    o=clone_object(who);
  }
  if(!o) return o;
  if(o->query_prevent_virtual(file)) destruct(o);
  return o;
}

/* This function is called when an error occurs in the heart_beat of object o,
 * it can be used either to restart it, or to write some messages to that
 * object and whatever objects are around.
 */
void kill_heart_beat(object o) {}

/* This function is called just before the mud is being shut down.
 */
void game_shutting_down() {}

/* all the setup isn't done when create() in master.c is called,
 * (include dires for instance) so we wait 1 second and load the rest
 * here
 */
void load_ip()
{
  function f;
  foreach(get_function_list((object)"/secure/simul_efun"),f)
    add_simul_efun(function_name(f),f);

  (object)"/secure/ip";
}

/* Start everything */
void create()
{
  write("Starting mud "+ctime(time())+".\n");
  call_out(load_ip,1);
}


/* This function should return an array of strings which will be bound
 * when compiling programs. Every entry in this array will use 2 bytes
 * per program, but calling these functions will be quicker because the
 * driver won't have to search for them.
 * Sorting these functions so the one used most is first is a little
 * (probably very little) quicker.
 */

string *get_cached_functions()
{
  return ({"id","short","long","catch_input"});
}

/* This function should return 1 if the object o may replace the
 * efun/simul_efun name with the optional argument f. If f==0 it means
 * that o wants to turn the simulation of that function off.
 */
int valid_add_simul_efun(object o,string name,function f)
{
  if(geteuid(o)==getuid(this_object())) return 1;
}

/* This function isn't used yet, but it will be in a near future.
 * It is meant to regulate the usage of efun::
 */
int valid_override() { return 1; }
