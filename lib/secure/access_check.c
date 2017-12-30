
/*
 format:

oklogin:
nr.nr.nr.nr [dd:hh:mm]-[dd:hh:mm]+message

nologin:
nr.nr.nr.nr [dd:hh:mm]-[dd:hh:mm]-message

*/

mixed *rows=({});

void restart()
{
  string file;

  file=read_file("/etc/access_list");
  if(!file) return;
  rows=file/"\n";
  rows=regexp(rows,"^[^#]")-({""});

  rows=map_array(rows,lambda(string row)
  {
    string nr,message;
    int ok;
    mixed *from_time,*to_time;
    from_time=allocate(3);
    to_time=allocate(3);
    if(sscanf(row,"%s [%s:%s:%s]-[%s:%s:%s]%c%s",nr,
			from_time[0],from_time[1],from_time[2],
			to_time[0],to_time[1],to_time[2],
			ok,message)!=9)
      return ({"Error in access_file"});

    nr=replace(nr,"*","[^.]*");
    message=replace(message,"\\n","\n");
    if(message[-1]!='\n') message+="\n";
    return ({nr,from_time,to_time,ok=='+',message});
  });
}

void create() { restart(); }

int time_cmp(mixed *t)
{
  int tmp,tmp2;
  
  if(t[0]!="*")
  {
    tmp=(int)t[0];
    tmp2=time()/(24*60*60)%7;
    if(tmp2<tmp) return -1;
    if(tmp2>tmp) return 1;
  }

  if(t[1]!="*")
  {
    tmp=(int)t[1];
    tmp2=(time()/60/60)%24;
    if(tmp2<tmp) return -1;
    if(tmp2>tmp) return 1;
  }

  if(t[2]!="*")
  {
    tmp=(int)t[2];
    tmp=tmp/100*60+tmp%100;
    tmp2=(time()/60)%60;
    if(tmp2<tmp) return -1;
    if(tmp2>tmp) return 1;
  }
  return 0;
}

/* returns ({ok_to_login, messag }) */

mixed *access_check(string nr)
{
  string *row;
  mixed *tmp;

  tmp=({nr});
  foreach(rows,row)
  {
    if(!sizeof(regexp(tmp,row[0]))) continue;
    if(time_cmp(row[1])>0) continue;
    if(time_cmp(row[2])<0) continue;
    return row[3..4];
  }
  return ({0,"Error in /etc/access_list ?\n"});
}
