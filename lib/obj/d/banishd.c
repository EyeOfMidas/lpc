int query_banished(string s)
{
  return (file_size("/save/banish")>0) && !!db_get("/save/banish",s);
}


int banish(string s)
{
  s=lower_case(s);
  if(db_get("/save/players",s)) return 0;
  db_set("/save/banish",s,"");
}
