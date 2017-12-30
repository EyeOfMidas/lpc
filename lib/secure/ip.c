/* LPC4 has no automagic communication code, this file opens up a socket
 * and listens for connections from outside. When someone connects,
 * /secure/logon.c is cloned and given the input. Note that the socket
 * owernership never leaves this objects, thus, all communication is done
 * through this object.
 */

#include <socket.h>
#include <arpa/telnet.h>

#define PORT 1905

void low_tell_object(int fd,string s);

int main_socket;
object *players=allocate(256);
string *cache=allocate(256);
int *write_disallow=allocate(256);
int num_players;

object *users()
{
  return players-({0});
}

static void low_fflush(int fd)
{
  string foo;
  if(foo=cache[fd])
  {
    cache[fd]=0;
    low_tell_object(fd,foo);
  }
  if(!players[fd] && !cache[fd])
    socket_close(fd);
}

void fflush(int fd)
{
  if(previous_object()==players[fd])
    low_fflush(fd);
}

static void player_close(int fd,string s)
{
  write("Shutting down. ("+fd+")\n");
  players[fd]->go_ld(fd);
  players[fd]=0;
  num_players--;
}

int do_exec(int fd,object to)
{
  int foo;
  if(previous_object() && previous_object()==players[fd])
  {
    players[fd]=to;
    foo=(int)to->take_a_socket(fd,this_object());
    if(!foo)
    {
      players[fd]=previous_object();
      return 0;
    }
  }
  return 1;
}

static void low_tell_object(int fd,string s)
{
  int e;
  if(write_disallow[fd])
  {
    cache[fd]=s;
  }else{
    if((e=socket_write(fd,s))==EECALLBACK)
    {
      write_disallow[fd]=1;
      cache[fd]="";
    }
  }
}

void tell_object(int fd,string s)
{
  if(previous_object()!=players[fd])
  {
    write("I think someone is trying to tell me something...\n");
    write(fd+" "+s+"\n");
    return;
  }
  if(stringp(cache[fd]))
  {
    cache[fd]+=s;
    return;
  }
  low_tell_object(fd,s);
}

static void player_write_callback(int fd)
{
  write_disallow[fd]=0;
  low_fflush(fd);
}

void disconnect(int fd)
{
  if(players[fd]==previous_object())
  {
    players[fd]=0;
    low_fflush(fd);
    num_players--;
  }
}

static void player_write(int fd,string s)
{
  if(s[0]==255)
  {
    write("Telnet junk ignored\n");
    return;
  }
  sscanf(s,"%s",s);
  low_fflush(fd);
  cache[fd]="";
  players[fd]->catch_input(s);
  low_fflush(fd);
}

void player_read(int fd)
{
  write("Stop that! ("+fd+")\n");
}

void close(int fd)
{
  write("Closing down.\n");
}

static void connect(int s)
{
  int err;
  object foo;
  if(s!=main_socket)
  {
    write("Foo?");
  }
  err=socket_accept(s,player_write,player_write_callback);
  foo=clone_object("/secure/logon");
  num_players++;
  players[err]=foo;
  foo->take_a_socket(err,this_object());
  low_fflush(err);
}

void create()
{
  int err;
  main_socket=socket_create(STREAM | BUFFERED_INPUT | BUFFERED_OUTPUT,
          player_read,close);
  if(main_socket<0)
  {
    write("Couldn't create socket.\n");
    shutdown();
  }
/*  write("Got socket ("+main_socket+")\n"); */
  err=socket_bind(main_socket,PORT);
  if(EESUCCESS!=err)
  {
    write("Couldn't bind socket.\n");
    write(socket_error(err)+"\n");
    shutdown();
  }
  write("Bound socket on port "+PORT+"\n");
  err=socket_listen(main_socket,connect);
  if(EESUCCESS!=err)
  {
    write("Couldn't listen to socket.\n");
    write(socket_error(err)+"\n");
    shutdown();
  }
}
