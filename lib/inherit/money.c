string cents_to_string(int cents)
{
  string *tmp;
  if(!cents) return "no money";
  
  if(cents/100)
    tmp=({cents/100 +" dollar"+(cents/100==1?"":"s")});
  else
    tmp=({});

  if(cents % 100)
    tmp+=({cents%100+" cent"+(cents%100==1?"":"s") });

  return implode_nicely(tmp);
}

int string_to_cents(string desc)
{
  int tmp,tmp2;
  desc=desc-","-"and";
  while(sscanf(desc," %s",desc));
  if(desc=="no money" || desc=="") return 0;
  if(sscanf(desc+" ","%d dollar %s",tmp,desc) ||
     sscanf(desc+" ","%d dollars %s",tmp,desc))
  {
    tmp2=string_to_cents(desc);
    if(tmp2==-1) return -1;
    return tmp*100+tmp2;
  }

  if(sscanf(desc+" ","%d cent %s",tmp,desc) ||
     sscanf(desc+" ","%d cents %s",tmp,desc))
  {
    tmp2=string_to_cents(desc);
    if(tmp2==-1) return -1;
    return tmp+tmp2;
  }
  return -1;
}
