#pragma save_types

mapping reps=([]);

void set_reputation(string person,string rep,int value)
{
  reps[person][rep]=value;
}

int query_reputation(string person,string rep)
{
  return reps[person][rep];
}

int add_reputation(string person,string rep,int value)
{
  return reps[person][rep]+=value;
}

void hear_info(string person,string rep,int value)
{
  int c;
  c=reps[previous_object()->query_real_name()]["credibility"];
  c=reps[person][rep]*(100-c)+value*credibility;
  reps[person][rep]=c/100;
}

void exchange_reputation(object with,string person,string rep)
{
  with->hear_info(person,rep,reps[person][rep]);
}

void talk_about(object with,string person)
{
  string o;
  if(!reps[person]) return;
  foreach(m_indices(reps[person]),o)
    exchange_reputation(with,person,o);
}

void smalltalk_with(object o)
{
  int e;
  if(!m_sizeof(reps)) return;
  talk_about(o,m_indices(reps)[random(m_sizeof(reps))]);
}
