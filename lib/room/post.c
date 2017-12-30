inherit "/std/room";
#include "mail.h"


void create()
{
  set_short("the post office");
  set_long("You're in the local post office.\n"+
	"Commands are: read, mail and from.\n" );
  add_property("silence_please");
}


void send_mail(string to,string subject,string body)
{
  object o,oo;
  string from;
  mixed *mail,*tmp;
  

  
  mail=({this_player()->query_real_name(),to,time(),subject,body});

  to=lower_case(to);
  to=replace(to,","," ");
  to=replace(to,"&"," ");

  foreach((to/" ")-({" "}),to)
  {
    if(!stringp(db_get("/save/players",to))
    {
      write("No player called "+to+".\n");
    }else{
      write("Sending mail to "+to+".\n");
      if(o=find_player(to))
      {
        o->catch_tell("You have new mail.\n");
        if((oo=present("mail_reader",o)) && file_name(oo)=="obj/mail_reader")
        {
          oo->put_mail(mail);
          continue;
        }        
      }
      if(stringp(tmp=db_get(SAVEFILE,to)))
      {
        tmp=decode_value(tmp);
      }else{
        tmp=({0});
      }
      tmp[0]++; /* you have unread mail now */
      tmp+=({mail});
      db_set(SAVEFILE,to,code_value(tmp));
    }
  }
}

int check_for_new_mail(string name)
{
  mixed tmp;
  if(stringp(tmp=db_get(SAVEFILE,name)))
  {
    tmp=decode_value(tmp);
    if(sizeof(tmp)>1) if(tmp[0]) return tmp[0];
  }
}

void init()
{
  ::init();
  move_object(clone_object("/obj/mail_reader"),this_player());
}

void exit()
{
  object o;
  /* I'm not root for nothing you know... */
  if(o=present("mail_reader",this_player()) efun::destruct(o);
}
