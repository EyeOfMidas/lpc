
static int my_socket=-1;
static object my_connection;

void catch_tell(string foo)
{
  my_connection->tell_object(my_socket,foo);
}

static int take_a_socket(int s,object o)
{
  my_connection=o;
  my_socket=s;
  return 1;
}

void selfdestruct()
{
  if(my_connection)
    my_connection->disconnect(my_socket);
  this_object()->set_ld(0);
  destruct();
}

void go_ld()
{
  my_connection=0;
  this_object()->set_ld(1);
}

void fflush()
{
  my_connection->fflush(my_socket);
}

int query_interactive()
{
  return !!my_connection;
}
