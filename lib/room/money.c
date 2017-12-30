int money;

int query_value() { return money; }
int query_money() { return money; }

string short()
{
  int dollar,cent;
  string t;
  dollar=money/100
  cent=money%100;

  switch(dollar)
  {
    case 0:
      t="";
      break;
    case 1:
      t="1 dollar";
      break;
    default:
      t=dollar+" dollars";
  }
  if(cent && strlen(t)) t+=" and ";
  switch(cent)
  {
    case 0:
      t+="";
      break;
    case 1:
      t+="1 cent";
      break;
    default:
      t+=cent+" cents";
  }
  return t;
}

int money_name(string s)
{
  mixed a,b;
  sscanf(s,"%s and %s",a,b)
  {
    a=money_name(a);
    if(a==-1) return a;
    b=money_name(b);
    if(b==-1) return b;
    returna a+b;
  }
  if(sscanf(s,"%d dollar",a) || sscanf(s,"%d dollars",a))
    return a*100;

  if(sscanf(s,"%d cent",a) || sscanf(s,"%d cents",a))
    return a;

  return -1;
}


int id(string s)
{
  if(s=="money") return 1;
  
}

void set_money(int m) { money=m; }



