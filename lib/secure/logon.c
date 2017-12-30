/* This is were the real 'mudlib' starts. This file is cloned by ip.c
 * and prompts for name and password. It then clones the approperiate
 * player.c and pass the input to it. Currently, this code is a bit messy,
 * but it seems to work anyway.
 */

#define SAVEFILE "/save/players"

inherit "/std/object";
#include <ctype.h>
#include <arpa/telnet.h>

static int my_socket=-1;
static object my_connection;

static int state,nstate,ystate;
static string ymessage,nmessage;

/* Player data */
string name;
string password,mailadress;

int isalphastring(string a)
{
  int e;
  for(e=0;e<strlen(a);e++) if(!isalpha(a[e])) return 0;
  return 1;
}

static void tell_object(string foo)
{
  my_connection->tell_object(my_socket,foo);
}

static void enter_game(object plobj)
{ 
  tell_object(read_bytes("/etc/motd"));
  seteuid(lower_case(name));
  plobj->do_disconnect();
  plobj->set_name(name);
  if(!my_connection->do_exec(my_socket,plobj))
  {
    tell_object("TELNET PANIC: 0x7fffcfa8 0x0001d764 0xaaaaaaaa 0x00000000\n");
    tell_object("core dumped.\n");
    my_connection->disconnect(my_socket);
  }
  plobj->catch_input("look");
  destruct();
}

static void selfdestruct()
{
  if(my_connection)
    my_connection->disconnect(my_socket);
  destruct();
}

/* Write a finite state machine just for the login sequenece... *sigh*/

static void catch_input(string s)
{
  int e;
  string a,b,*c;
  object p;
  remove_call_out(selfdestruct);
  call_out(selfdestruct,120);

/*  write("*");
  for(e=0;e<strlen(s);e++) write(s[e]+":"+e+" ");
  write("\n");
*/

  switch(state)
  {
    case 0:  /* Get the name */
      s=lower_case(s);
      if(strlen(s)<3)
      {
        tell_object("Too short name.\nGive another name:");
        return ;
      }

      if(strlen(s)>15)
      {
        tell_object("Too long name.\nGive another name:");
        return;
      }

      if(!isalphastring(s))
      {
        tell_object("Illigal character in name.\nTry again:");
        return;
      }

      if(lower_case(s)=="guest")
      {
        enter_game(clone_object("obj/guest"));
        break;
      }

      if(lower_case(s)=="info")
      {
        tell_object(read_bytes("/etc/info"));
        tell_object("Enter your name: ");
        break;
      }

      name=lower_case(s);

      if("/obj/d/banishd"->query_banished(name))
      {
        tell_object("That name is occupied.\nTry again:");
        return;
      }
      s=db_get(SAVEFILE,name);
      if(stringp(s))
      {
        restore_object(s);
        state=1;
        tell_object(LOCAL_ECHO_OFF);
        tell_object("Enter password: ");
        break;
      }

      tell_object(read_bytes("/etc/new_character"));
      tell_object("Are you sure you spelled the name right? :");
      state=1000;

      ystate=10;
      nstate=9;

      break;

    case 1:        /* Get the password */
      tell_object(LOCAL_ECHO_ON);
      tell_object("\n");
      if(crypt(s,password)!=password)
      {
        tell_object("Wrong password!\n");
        selfdestruct();
        return;
      }
      if(find_player(name))
      {
        tell_object("You are already playing, throw the other copy out? :");
        state=1000;
        ystate=2;
        nstate=3;
        break;
      }

    case 2:
      enter_game(clone_object("/obj/player"));
      break;

    case 3:
      selfdestruct();
      break;

    case 9:
      tell_object("Ok, try enter name again: ");
      state=0;
      break;
      
    case 10:        /* Get the new password */
      tell_object(LOCAL_ECHO_OFF); 
      tell_object("Ok, Enter new password: ");
      state=11;
      break;

    case 11:        /* Get the new password (again) */
      password=s;
      tell_object(LOCAL_ECHO_OFF);
      tell_object("\nOk, Enter new password (again): ");
      state=111;
      break;

    case 111:
      if(s!=password)
      {
        tell_object(LOCAL_ECHO_OFF); 
        tell_object("\nYou didn't write same password, try again.\nEnter password: ");
        state=11;
        break;
      }
      password=crypt(password,0);
      tell_object(LOCAL_ECHO_ON+"\nEnter mailadress or 'none': ");
      state=13;
      break;

    case 13:
      if(s!="none" && (!sscanf(s,"%s@%s",a,b) || !strlen(a) || !strlen(b)))
      {    
        tell_object("You must enter a mailadress or 'none'\nTry again:");
        break;
      }
      mailadress=s;
      if(1!=db_set(SAVEFILE,name,save_object()))
      {
	tell_object("Failed to create save entry.\n");
	destruct(this_object());
	return;
      }
      enter_game(clone_object("/obj/player"));
      break;

    case 1000:
    {
      if(strlen(s)==0 || (s[0]!='y' && s[0]!='n'))
      {
        tell_object("Answer yes or no please.\n:");
        return;
      }
      if(s[0]=='y') state=ystate;
      else state=nstate;
      catch_input("");
      break;
    }
  }
}


static int take_a_socket(int s,object o)
{
  mixed *tmp;
  my_connection=o;
  my_socket=s;
  state=0;
  tmp="/secure/access_check"->access_check(socket_address(s));
  tell_object(tmp[1]);
  if(!tmp[0])
  {
    selfdestruct();
    return;
  }
  tell_object(read_bytes("/etc/welcome"));
  tell_object("What is your name: ");
  call_out(selfdestruct,120);
  return 1;
}
