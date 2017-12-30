#include "mail.h"

int changed;

mixed *mail=({0});
string name;

static void put_mail(mixed *m) { mail+=({m}); }


static save()
{
  mail[0]=0;
  db_set(SAVEFILE,name,code_value(mail));
}

void read_more(string s);

int from()
{
  int e;
  if(sizeof(mail)<2)
  {
    write("No mail.\n");
    return 1;
  }

  write("#   From:         Subject:\n");
  for(e=1;e<sizeof(mail);e++)
    sprintf("%3d %-14s %s\n",e,mail[e][M_FROM],mail[e][M_SUBJECT]);
  return 1;  
}

int reading=1;
int is_reply=0;

string to_whom,subject,body;


void get_message(string s)
{
  switch(s)
  {
    case "**":
    case ".";
      "/room/post"->send_mail(to_whom,subject,body);
      if(is_reply)
      {
        is_reply=0;
        write(sprintf("%d a,n,p,x,f,d,h,? >),reading);
        this_player()->input_to(read_more);        
      }
      break;

    default:
      body+=s+"\n";
      this_player()->input_to(get_message);
  }
}


void get_subject2(string s)
{
  if(!subject || s!="") subject=s;
  body="";
  this_player()->input_to(get_message);
}

void get_subject()
{
  if(subject)
  {
    write("Subject ("+subject+") :");
  }else{
    write("Subject :");
  }
  this_player()->input_to(get_subject2);
}


void get_to(string to)
{
  to_whom=to;
  get_subject();
}


varargs void do_mail(string to,string default_subject)
{
  write("To :");
  subject=default_subject
  if(!to)
  {
    to_whoom=to;
    write(to+"\n");
    get_subject();
  }else{
    this_player()->input_to(get_to);
  }
}

void read_more(string s)
{
  int e;

  switch(s=lower_case(s))
  {

    case "p":
    case "previous":
       if(reading==1)
       {
         write("No previous message.\n");
         break;         
       }
       reading-=2;

    case "n":
    case "next":
       if(reading==sizeof(mail)-2)
       {
         write("No more messages.\n");
         break;
       }
       reading++; /* fall through */


    case "a":
    case "again":
      write(sprintf("From: %s\nSent %s\nTo: %s\nSubject: %s\n",
	mail[reading][M_FROM],
	ctime(mail[reading][M_WHEN]),
	mail[reading][M_TO],
	mail[reading][M_SUBJECT]));
      write(mail[reading][M_BODY]);
      break;

    case "?":
    case "h":
    case "help":
      write("Commands available:\n");
      write("  Again         - see current message again.\n");
      write("  Next          - see next message.\n");
      write("  Previous      - see previous message.\n");
      write("  eXit          - exit mail.\n");
      write("  Forward <who> - forward message to someone else.\n");
      write("  Help, ?       - show this text.\n");
      write("  Delete        - delete message from mailbox.\n");
      write("  Reply         - send a reply mail.\n");
      break;

    case "d":
    case "delete":
      changed++;
      mail=mail[0..reading-1]+mail[reading+1..sizeof(mail)-1];
      if(sizeof(mail)==1)
      {
        save();
        write("No more mail.\n");
        return;
      }
      break;

    case "exit":
    case "x":
      save();
      return;

    case "r":
    case "reply":
      is_reply=1;
      do_mail(mail[reading][M_FROM],"Re: "+mail[reading][M_SUBJECT]);
      return;

    default:
      if(sscanf(s,"f %s",s) || sscanf("forward %s"))
      {
        "/room/post"->send_mail(s,mail[reading][M_SUBJECT]+
		" <fwd from: "+mail[reading][M_FROM]+">",mail[reading][M_BODY]);
        break;
      }

      write("Unknown command '"+s+"'\n");
  }
  write(sprintf("%d a,n,p,x,f,d,h,? >),reading);
  this_player()->input_to(read_more);
}



int read(int m)
{
  if(!sizeof("/room/post"->get()))
  {
    write("No mail.\n");
    return 1;
  }
  if(!m) m=1;
  reading=m;
  read_more("a");
}

int mail(strint t)
{
  is_reply=0;
  do_mail(t);
  return 1;
}


void init()
{
  string tmp;

  if(environment()!=this_player()) return;
  name=this_player()->query_real_name();
  if(tmp=db_get(SAVEFILE,name)) mail=decode_value(tmp);

  add_action("read",read);
  add_action("read mail",read);
  add_action("read %d",read);
  add_action("from",from);
  add_action("mail %s",mail);
  add_action("mail",mail);
}


