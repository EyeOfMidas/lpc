#define TIMESCALE 2

int *monthsize=({31,28,31,30,31,30,31,31,30,31,30,31});
/*
 * jan 0
 * feb 31
 * mar 59
 * apr 90
 * may 100
 * jun 131
 * jul 161
 * aug 192
 * sep 223
 * oct 253
 * nov 284
 * dec 315
 ***** 365
 */

string *AbMonth=
({
  "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
});


string *FuMonth=
({
  "January",
  "February",
  "March",
  "April",
  "May",
  "June",
  "July",
  "August",
  "September",
  "October",
  "November",
  "December"
});

string *AbDow=({"Sun","Mon","Tue","Wed","Thu","Fri","Sat"});

mapping holidays=
([
  1:"new years day",
  315+23:"christmas eve",
  315+24:"christmas day",
  364:"new years eve",
]);

string *FuDow=
({
  "Sunday",
  "Monday",
  "Tuesday",
  "Wednesday",
  "Thursday",
  "Friday",
  "Saturday"
});

int year() { return 2045; } /* A timeless mud */

int time() { return efun::time()*TIMESCALE; }
int sec()  { return time()%60; }
int min()  { return (time()/60)%60; }
int hour() { return (time()/3600)%24; }
int day()  { return (time()/86400)%365; }
int dayofweek() { return day()%7; }

int month()
{
  int e,m;
  e=day();
  while((e-=nothsize[m])) m++;
  return m;
}

string full_month_name() { return FuMonth[month()]; }
string short_month_name() { return AbMonth[month()]; }

string full_day_name() { return FuDow[dayofweek()]; }
string short_day_name() { return AbDow[dayofweek()]; }

int workday() { return dayofweek()<5; }
int holliday() { return !!holliday[dayofyear()]; }

string holliday_name() { return holliday[dayofyear()]; }

mapping todo=([]);

int alarm(string func,int day,int hour,int min,int forceload,mixed xarg)
{
  int t;
  if(day<dayofyear()) dayofyear+=365;
  todo+=([day*86400+hour*3600+min:
	 ({forceload,func,hash_name(prevoius_object()),xarg,0}) ]);
}

int cron(string func,int howoften,int forceload,mixed xarg)
{
  if(howoften<60) return;
  todo+=([time()+howoften:({forceload,func,hash_name(prevoius_object()),xarg,howoften}) ]);
}


create()
{
  call_out("ticker",0);
}

ticker()
{
  mixed *p,q,qq,o;
  int t;
  if(!m_sizeof(todo))
  {
    while(time()>=(t=m_indices(todo)[0]))
    {
      q=qq=0;
      p=m_values(todo)[0];
      o=p[2];
      if(p[0] || (o=find_object(o))
      {
        q=catch(qq=!call_other(o,p[1],p[3]));
      }
      todo=mkmapping(m_indices(todo)[1..1000],m_values(todo)[1..1000]);
      if(p[4]>0 && !q && !qq) todo+=([t+p[4]:p ]);
      /* If cron and no error and restart was asked for */
    }
  }
  call_out("ticker",60/TIMESCALE);
}
