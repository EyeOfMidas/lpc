
mapping properties=([]);

mixed get_property(string property)
{
  return properties[property];
}

mixed query_property(string property)
{
  mixed res;
  res=get_property(property);
  if(functionp(res))
    res=res(property);

  return res;
}

void set_property(string property,mixed value)
{
  if(value)
  {
    properties[property]=value;
  }else{
    properties=m_delete(properties,property);
  }
}

void add_property(string property)
{
  set_property(property,1);
}
