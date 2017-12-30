inherit "std/object";
#include <map.h>

/*
 * Krav:
 *
 *
 * Editering: lpc? c? basic? amos?
 * Formulas:
 * Centers: SlumC, MarketC, MoneyC, FactoryC
 * factors:   moneyflow
 * areatypes: centrum, habit, factory, officearea
 * roomtypes: park, roads[size], house[storages]
 *
 * Data:
 * streets ([gata:(<koord,...>),...])
 * rooms(a) ([Xkoord:([Ykoord:Z,...]),...])
 * Z ([Zkoord:room,...])  | room  
 *
 * rooms(b) ([koord:room,...])
 *
 * room 
 *
 *
 */

/**** Roombase *********************************************/

mapping rooms=([]);

void create(string str)
{
  restore_object(read_file("/room/map"));
}

mixed *get_data(int x,int y,int z)
{
  mixed q;
  if(mappingp(q=rooms[x])) q=q[y];
  if(!mappingp(q))
  {
    if(z) return 0;
    return q;
  }
  return q[z][0..20];
}

void register(mixed *o,int x,int y,int z)
{
  mixed q,p;
  if(!(q=rooms[x])) q=rooms[x]=([]);
  p=q[y];
  if(!p) p=([]);
  if(!mappingp(p)) p=([0:p]);
  p[z]=o;
/*  p=m_cleanup(p); */
  if(m_sizeof(p)==1 && !m_indices(p)[0]) p=m_values(p)[0];
  q[y]=p;
}

/**** Zipbase ********************************************/

mapping Zip=
  ([
    R_Field:({(<"">),R_Field,"the contryside",0}),
    R_Air:({(<"">),R_Air,"open air",0}),
    R_Ground:({(<"">),R_Ground,"under ground",0}),
    ]);

mixed *get_zip_data(int zipcode) { return Zip[zipcode]; }

list get_zip_members(int *zipcode)
{
  string s;
  list c;
  mixed *q;
  if(intp(zipcode)) return Zip[zipcode][ZIP_MEMBERS];
  c=(<>);
  foreach(zipcode,s)
  {
    q=Zip[s];
    if(q) c&=q[ZIP_MEMBERS];
  }
  return c;
}

void remove_zipcode(int zipcode)
{
  if(l_sizeof(Zip[zipcode][ZIP_MEMBERS])) return;
  Zip=m_delete(Zip,zipcode);
}

mixed *add_zip(int zipcode)
{
  mixed *q;
  if(q=Zip[zipcode]) return q;
  else return Zip[zipcode]=({(<>),0,0,0});
}

void add_koord_to_zip(int zip,mixed x,int y,int z)
{
  string koord;
  if(stringp(x)) koord=x;
  else koord=pack(x,y,z);
  add_zip(zip)[ZIP_MEMBERS]|=(<koord>);
}

void remove_koord_from_zip(int zip,mixed x,int y,int z)
{
  string koord;
  mixed *zz;
  if(stringp(x)) koord=x;
  else koord=pack(x,y,z);
  zz=add_zip(zip);
  zz[ZIP_MEMBERS]=l_delete(zz[ZIP_MEMBERS],koord);
  remove_zipcode(zip);
}

int find_free_zip()
{
  int e;
  e=1;
  while(Zip[e]) e++;
  return e;
}

/**** Mockbase ****/

mixed get_koord_data(mixed x,int y,int z)
{
  mixed data,data2,zip2;
  int level;

  if(stringp(x)) unpack(x,x,y,z);
  data=get_data(x,y,z);
  if(data) return data;
  data=allocate(ROOM_);
  if(z==0)
  {
    data[ROOM_ZIPCODE]=R_Field;
    return data;
  }
  data2=get_koord_data(x,y,0);
  zip2=Zip[data2[ROOM_ZIPCODE]];

  if(zip2[ZIP_TYPE]!=R_Water)
  {
    level=data2[ROOM_LEVEL];
  }else{
    level=0;
  }

  level+=z;
  data[ROOM_LEVEL]=z;
  if(zip2[ZIP_TYPE]==R_House && zip2[ZIP_DATA])
  {
    if((level>0 && level<zip2[ZIP_DATA][0]) ||
       (level<0 && -level<=zip2[ZIP_DATA][1]))
    {
      data[ROOM_ZIPCODE]=data2[ROOM_ZIPCODE];
      return data;
    }
  }
  if(z>0)
  {
    data[ROOM_ZIPCODE]=R_Air;
  }else{
    if(zip2[ZIP_TYPE]==R_Water && data2[ROOM_LEVEL]<=level)
    {
      data[ROOM_ZIPCODE]=data2[ROOM_ZIPCODE];
    }else{
      data[ROOM_ZIPCODE]=R_Ground;
    }
  }
  return data;
}
