#!lpc

function sig;

void main(int argv,string *argc,string *env)
{
  string s,s2;
  object o;
  
  for(s2="";s=gets();s2+=s);
  s=make_object(s2);
  o=(object)s;
  sig=o->signal;
  return o->main(argv,argc,env);
}

int signal(int foo) { return sig(foo); }
