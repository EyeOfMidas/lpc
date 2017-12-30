/* Notes from he author:
 *
 * Once upon a time I write a rather nice soul.
 * Unfortunately the code looked like something the cat
 * dragged in. The format for adding new feelings to it
 * was on the verge to ludicrous. It also had limitations
 * and lacked a few important features....
 *
 *
 * T H E   S O U L   I I
 *
 * It's back! It's bigger, better, slower and maybe even
 * understandable. Just when you thought it was safe to
 * write 'smile' again the man behind such unimaginable
 * things as The Shell, The Soul and LPC4 strikes again.
 *
 * (have patience, it is not finished yet.)
 *
 *     /Profezzorn@Nannymud (email: hubbe@lysator.liu.se)
 */

/* configuration: */

#define DEBUG 0
#ifdef LPC4
inherit "/std/object";

#define HAVE_SPRINTF
#define WIZARD_P(X) 1
#define PERFECT_EXPLODE
#define REPLACE(X,Y,Z) replace(X,Y,Z)

#else
#ifdef __NANNYMUD__

#define HAVE_SPRINTF
#define WIZARD_P(X) ((X)->query_level()>19)
#define REPLACE(X,Y,Z) implode(my_explode(X,Y),Z)
/* #define PERFECT_EXPLODE */

#else /* __NANNYMUD__ */

/* default stupid configuration */
/* #define HAVE_SPRINTF */
#define WIZARD_P(X) 0
#define REPLACE(X,Y,Z) implode(my_explode(X,Y),Z)
/* #define PERFECT_EXPLODE */

#endif /* __NANNYMUD__ */
#endif /* LPC4 */

/* useful defines */

#include "soul.h"

/* if we use the exact same 'spaces' string evewhere we save some memory
 * it also look nicer.
 */
#define SPACES "                                                            "

#define MIN(X,Y) ((X)<(Y)?(X):(Y))

#ifdef LPC4
#define MAP(X,Y,Z) map_array((X),Y,(Z))
#define FILTER(X,Y,Z) filter_array((X),Y,(Z))
#else
#define FILTER(X,Y,Z) map_array((X),"Y",this_object(),(Z))
#endif

#ifdef LPC4
#pragma all_inline
#endif

#ifdef PERFECT_EXPLODE
#define my_explode(X,Y) explode(X,Y)
#else /* perfect_explode */

string *my_explode(string a,string b)
{
  string *c;
  if(a==b) return ({"",""}); 
  if(strlen(b) && a[0..strlen(b)-1]==b)
    return ({ "" })+ my_explode(a[strlen(b)..strlen(a)-1],b);
  if(c=explode(a+b,b)) return c;
  return ({});
}

#endif /* perfect_explode */

#ifdef HAVE_SPRINTF

/* This linebreak has usually a limit about 10000 bytes */
string fast_linebreak(string text,string pre,int width)
{
  return sprintf("%s%-=*s",pre,width,text);
}

#else /* have_sprintf */

/* This doesn't have to be able to linebreak very large strings */
string fast_linebreak(string text,string pre,int width)
{
  string *parts;
  int part;

  parts=my_explode(text,"\n");
  for(part=0;part<sizeof(parts);part++)
  {
    string *words,out;
    int word,len;

    words=my_explode(parts[part]," ");
    out=pre;
    pre=SPACES[1..strlen(pre)];

    for(len=0;len<width && word<sizeof(words);len+=strlen(words[word++]))
      out+=words[word];

    parts[part]=out;
  }

  return implode(parts,"\n");
}

#endif /* have_sprintf */

/* This function is almost only used for error messages */
string show_value(mixed v)
{
  if(stringp(v))
  {
    v=REPLACE(v,"\\","\\\\");
    v=REPLACE(v,"\"","\\\"");
    v=REPLACE(v,"\b","\\b");
    v=REPLACE(v,"\n","\\n");
    return "\""+v+"\"";
  }else if(intp(v)){
    return v+"";
  }else if(objectp(v)){
    return file_name(v);
  }else if(pointerp(v)){
    return "({"+implode(MAP(v,show_value,0),",")+"})";
  }else if(mappingp(v)){
    mixed *a,*b;
    int e;
    a=m_indices(v);
    b=m_values(v);
    for(e=0;e<sizeof(a);e++)
      a[e]=show_value(a[e])+":"+show_value(b[e]);
    return "(["+implode(a,",")+"])";
  }else{
    return "UNKNOWN";
  }
}


/* This routing is simply a must.
 * Implodes an array ({"foo","bar","gazonk"}) to
 * "foo, bar and gazonk"
 * The second (optional) argument can be used to replace 'and' with
 * something else.(probably or)
 */
varargs string implode_nicely(string *words,string conjunction)
{
  int s;

  if(!conjunction) conjunction="and";
  switch(s=sizeof(words))
  {
  default:
    return implode(words[0..s-2],", ")+" "+conjunction+" "+words[s-1];

  case 2:
    return words[0]+" "+conjunction+" "+words[1];

  case 1:
    return words[0];
  
  case 0:
    return "";
  }
}

/* 
 * Bullshit or not, you be the judge!
 * We simply want to make sure that strings are shared sometimes to save
 * memory (if you don't know what shared strings are - don't meddle)
 */
string share_string(string s)
{
#ifdef LPC4
  return s;
#else
  return m_indices(([s:0]))[0];
#endif
}


/* inform this_player(), and possible log that an error has occured */
void inform_error(string excuse,string cause)
{
  write(excuse);

  if(!cause) return;

  if(WIZARD_P(this_player())) write(cause);
#if 0
  log_file("SOUL",cause);
#endif
}

/* Check if two arrays are equal.
 * Note that this function can't be compiled with type-testing in 3.1.2
 * because the compiler doesn't think the operator '&' handles arrays.
 */

#ifdef LPC4
int
#endif
    eq(mixed *a,mixed *b)
{
  return sizeof(a)==sizeof(b) && sizeof(a & b)==sizeof(a);
}

#ifdef LPC4
/* haven't implemented light in lpc4 mudlib yet */
int is_dark() { return 0; }
#else
int is_dark() { return set_light(0)<=0; }
#endif

#ifdef LPC4
#define SUB_ARRAYS(X,Y) ((X)-(Y))
#else
sub_arrays(x,y) { return x-y; }
#define SUB_ARRAYS(X,Y) ((mixed *)sub_arrays((X),(Y)))
#endif

#ifdef LPC4
string big_linebreak(string text,string pre,int width)
{
  return fast_linebreak(text,pre,width);
}
#else /* LPC4 */

/* Standard sprintf has an internal buffer of 10000 bytes. To work around
 * this bug we call fast_sprintf a few times..... This also helps if you
 * don't have sprintf and is linebreaking something that has more words
 * than can be fitted into you arrays.
 */
string big_linebreak(string s,string pre,int width)
{
  int e;
  string done,a,b;

  done="";
  while(strlen(s))
  {
    if(strlen(s)<5000)
    {
      e=5000;
    }else{
      e=5000;
      while(e>=0 && s[e]!=' ') e--;
      if(e==-1) return done+"   "+s+"\n";
    }
    done+=fast_linebreak(s[0..e-1],pre,width);
    pre=SPACES[1..strlen(pre)];
    s=s[e+1..strlen(s)-1];
    for(e=strlen(done)-1;e>=0 && done[e]!='\n';e--);

    a=0;
    b="";
    sscanf(s,"%s %s",a,b);
    while(a && strlen(a)+strlen(done)-e<=width+2)
    {
      done+=" "+a;
      s=b;
      a=0;
      b="";
      sscanf(s,"%s %s",a,b);
    }
    done+="\n";
  }
  return done;
}
#endif /* LPC4 */

#ifdef LPC4
void more(string str) { write(str); }
void more_flush() {}
#else
string morestring="";
void more(string str) { morestring+=str; }

varargs void more_flush(string str)
{
  int e;
  string a;
  int rows;

  if (this_player()->query_rows() > 0)
      rows = (int)this_player()->query_rows() - 2;
  else
      rows = 22;

  if(str=="q")
  {
    morestring="";
    return;
  }
  for(e=0;e<rows;e++)
  {
    if(sscanf(morestring,"%s\n%s",a,morestring))
    {
      write(a+"\n");
    }else{
      write(morestring);
      e=4711;
      morestring="";
    }
  }
  if(strlen(morestring))
  {
    write("*Press return for more or q to end.->");
    input_to("more_flush");
  }
}
#endif

/* -Exaggerating, me? */
string number_to_string(int x)
{
  switch(x)
  {
    case 0: return "zero";
    case 1: return "one";
    case 2: return "two";
    case 3: return "three";
    case 4: return "four";
    case 5: return "five";
    case 6: return "six";
    case 7: return "seven";
    case 8: return "eight";
    case 9: return "nine";
    case 10: return "ten";
    case 11: return "eleven";
    case 13: return "thirteen";
    case 14: return "fourteen";
    case 15: return "fifteen";
    case 16: return "sixteen";
    case 17: return "seventeen";
    case 18: return "eightteen";
    case 19: return "nineteen";

    case 20: return "twenty";
    case 30: return "thirty";
    case 40: return "fourty";
    case 50: return "fifty";
    case 60: return "sixty";
    case 70: return "seventy";
    case 80: return "eighty";
    case 90: return "ninety";

    case 21..29: case 31..39: case 41..49: case 51..59:
    case 61..69: case 71..79: case 81..89: case 91..99:
      return number_to_string((x/10)*10)+number_to_string(x%10);

    case 100: case 200: case 300: case 400: case 500:
    case 600: case 700: case 800: case 900:
      return number_to_string(x/100)+" hundred";

    case 101..199: case 201..299: case 301..399: case 401..499:
    case 501..599: case 601..699: case 701..799: case 801..899:
    case 901..999:
      return number_to_string((x/100)*100)+" and "+number_to_string(x%100);

    case 1000..999999:
      if(x%1000)
      {
	return number_to_string(x/1000)+" thousand "+number_to_string(x%1000);
      }else{
	return number_to_string(x/1000)+" thousand";
      }

    case 1000000: return "one million";
    case 1000001..1999999:
      return "one million "+number_to_string(x%1000000);

    case 2000000..999999999:
      if(x%1000000)
      {
	return number_to_string(x/1000000)+" millions "+number_to_string(x%1000000);
      }else{
	return number_to_string(x/1000000)+" millions";
      }

    case -0x7fffffff..-1: return "minus "+number_to_string(-x);

    default:
      return "many";
  }
}

string nr_times(int x)
{
  switch(x)
  {
  case 0: return " no times";
  case 1: return "";
  case 2: return " twice";
  case 3: return " threefold";
  default:
    return " "+number_to_string(x)+" times";
  }
}
#ifdef LPC4
/* simply sorts an array of strings in alphabetical order */
string *sort_words(string *s)
{
  return sort_array(s);
}
#else /* LPC4 */

int letterorder(string a,string b)
{
#ifdef MUDOS
  if(a>b) return 1;
  if(a<b) return -1;
#else
  return a>b;
#endif
}

/* simply sorts an array of strings in alphabetical order */
string *sort_words(string *s)
{
  return sort_array(s,"letterorder",this_object());
}
#endif /* LPC4 */

/* To use shared arrays we need to get them from somewhere global.
 * we use the masterobject. This routine returns it.
 */

#ifdef LPC4
object soul_master() { return (object)file_name(); }
#else
string soul_master()
{
  string f;
  f=file_name(this_object());
  sscanf(f,"%s#",f);
  return f;
}
#endif /* LPC4 */

mapping verbs=(mapping)soul_master()->get_verbs();
mapping get_verbs()
{
  if(verbs) return verbs;
  return
    ([
      "ack":"\b1PRON ack\b$ \bADV \bAT",
      "admire":"\b1PRON admire\b$ \b2OBJ \bADV",
      "agree":({"simple",(["prep":" with"])}),
      "apologize":({"simple",(["prep":" to"])}),
      "applaud":({"simple",(["prep":""])}),
      "ask":({"\b1PRON \bADV ask\b$ \bAT: \bMSG?",(["prep":""])}),
      "ayt":"\b1PRON wave\b$ \b1POSS hand in front of \b2POSS face, \bIS \nSUBJ \bADV there?",
      "babble":({"default",([C_ADV:"incoherently",C_MSGS:"'something"]),({"\b1PRON babble\b$ \bTEXT \bADV \bAT",(["prep":" to"])})}),
      "beep":({"default",([C_ADV:"triumphantly",C_BODY:"on the nose"]),({"who_p","\b1PRON \bADV beep\b$ \b1OBJself \bWHERE","\b1PRON \bADV beep\b$ \b2OBJ \bWHERE"})}),
      "beg":({"who_p","\b1PRON beg\b$ \bADV","\b1PRON beg\b$ \b2OBJ for mercy \bADV"}),
      "bite":({"who_p","\b1PRON \bADV bite\b$ \b1OBJ lip","\b1PRON bite\b$ \b2OBJ \bADV"}),
      "blink":({"who_p","\b1PRON blink\b$ \bADV","\b1PRON blink\b$ \bADV at \b2OBJ"}),
      "blush":({"irregular","\b1PRON blush \bADV","\b1PRON blushes \bADV"}),
      "boggle":"\b1PRON boggle\b$ \bADV at the concept",
      "bonk":({"default",([C_BODY:"on the head"]),({"\b1PRON bonk\b$ \b2OBJ \bADV \bWHERE",(["flags":FLAG_TOUCH,])})}),
      "bounce":"\b1PRON bounce\b$ \bADV",
      "bow":({"simple",(["prep":" to"])}),
      "burp":({"default",([C_ADV:"rudely"]),"simple"}),
      "cackle":({"default",([C_ADV:"gleefully"]),"simple"}),
      "capitulate":({"default",([C_ADV:"unconditionally"]),({"simple",(["prep":" to"])})}),
      "caress":({"default",([C_BODY:"on the cheek"]),({"\b1PRON caress\b$ \b2OBJ \bADV \bWHERE",(["flags":FLAG_TOUCH,])})}),
      "cheer":({"default",([C_ADV:"enthusiastically"]),"\b1PRON cheer\b$ \bADV"}),
      "chuckle":({"default",([C_ADV:"politely"]),"simple"}),
      "clap":"\b1PRON clap\b$ \bADV",
      "clear":"\b1PRON clear\b$ \b1POSS throat \bADV",
      "clue":"\b1PRON need\b$ a clue \bADV",
      "comfort":"\b1PRON comfort\b$ \b2OBJ \bADV",
      "complain":({"simple",(["prep":" about"])}),
      "congratulate":"\b1PRON congratulate\b$ \b2OBJ \bADV",
      "cough":({"default",([C_ADV:"noisily"]),"\b1PRON cough\b$ \bADV"}),
      "cringe":({"default",([C_ADV:"in terror"]),"\b1PRON cringe\b$ \bADV"}),
      "criticize":({"who_p","\b1PRON criticize\b$ \bMSG \bADV","\b1PRON criticize\b$ \b2OBJ \bADV"}),
      "cry":({"irregular","\b1PRON cry \bADV","\b1PRON cries \bADV"}),
      "cuddle":"\b1PRON cuddle\b$ \b2OBJ \bADV",
      "curse":({"who_p","\b1PRON curse\b$ \bMSG \bADV","\b1PRON curse\b$ \b2OBJ \bADV"}),
      "curtsey":({"simple",(["prep":" before"])}),
      "dance":({"simple",(["prep":" with"])}),
      "dandle":"\b1PRON dandle\b$ \b2OBJ \bADV",
      "die":({"irregular","\b1PRON fall \bADV down and play dead","\b1PRON falls \bADV to the ground, dead"}),
      "disagree":({"simple",(["prep":" with"])}),
      "drool":({"simple",(["prep":" on"])}),
      "duck":({"who_p","\b1PRON duck\b$ \bADV out of the way","\b1PRON duck\b$ \bADV out of \b2POSS way"}),
      "embrace":({"default",([C_BODY:"in \nYOUR arms"]),({"\b1PRON embrace\b$ \b2OBJ \bADV \bWHERE",(["flags":FLAG_TOUCH,])})}),
      "exclaim":({"\b1PRON \bADV exclaim\b$ \bAT: \bMSG!",(["prep":""])}),
      "excuse":({"who_p","\b1PRON \bADV excuse\b$ \b1OBJself","\b1PRON \bADV excuse\b$ \b1OBJself to \b2OBJ"}),
      "faint":"\b1PRON faint\b$ \bADV",
      "fart":"simple",
      "fear":({"who_p","\b1PRON shiver\b$ \bADV with fear","\b1PRON fear\b$ \b2OBJ \bADV"}),
      "finger":"\b1PRON give\b$ \b2OBJ the finger",
      "flex":({"\b1PRON flex\b$ \b1POSS muscles \bADV",(["prep":""])}),
      "flip":"\b1PRON flip\b$ \bADV head over heels",
      "fondle":"\b1PRON fondle\b$ \b2OBJ \bADV",
      "french":"\b1PRON give\b$ \b2OBJ a REAL kiss, it seems to last forever",
      "frown":"\b1PRON frown\b$ \bADV",
      "fume":"\b1PRON fume\b$ \bADV",
      "gasp":({"default",([C_ADV:"in astonishment"]),"\b1PRON gasp\b$ \bADV"}),
      "giggle":({"default",([C_ADV:"merrily"]),"simple"}),
      "glare":({"default",([C_ADV:"stonily"]),"simple"}),
      "gobble":({"who_p","\b1PRON gobble\b$ HOW","\b1PRON gobble\b$ all over \b2OBJ"}),
      "grimace":"\b1PRON \bADV make\b$ an awful face \bAT",
      "grin":({"default",([C_ADV:"evilly"]),"simple"}),
      "gripe":"\b1PRON gripe\b$ to \b2OBJ \bADV",
      "groan":"simple",
      "grope":"\b1PRON grope\b$ \b2OBJ \bADV",
      "grovel":({"simple",(["prep":" before"])}),
      "growl":"simple",
      "grumble":"\b1PRON grumble\b$ \bADV",
      "grunt":"simple",
      "guffaw":"\b1PRON guffaw\b$ \bADV \bAT",
      "gurgle":"\b1PRON gurgle\b$ \bADV",
      "hate":"\b1PRON hate\b$ \b2OBJ \bADV",
      "hiccup":"\b1PRON hiccup\b$ \bADV",
      "hide":"\b1PRON hide\b$ \bADV behind \b2OBJ",
      "hit":({"default",([C_BODY:"in the face"]),({"\b1PRON hit\b$ \b2OBJ \bADV \bWHERE",(["flags":FLAG_TOUCH | FLAG_OFFENSE])})}),
      "hmm":"\b1PRON hmm\b$ \bADV \bAT",
      "hold":({"default",([C_BODY:"in \nYOUR arms"]),({"\b1PRON hold\b$ \b2OBJ \bADV \bWHERE",(["flags":FLAG_TOUCH,])})}),
      "howl":({"default",([C_ADV:"in pain"]),"simple"}),
      "huff":"\b1PRON huff\b$ \bADV",
      "hug":"\b1PRON hug\b$ \b2OBJ \bADV",
      "ignore":"\b1PRON ignore\b$ \b2OBJ \bADV",
      "jump":({"default",([C_ADV:"up and down in aggravation"]),"\b1PRON jump\b$ \bADV"}),
      "kick":({"default",([C_ADV:"hard"]),({"\b1PRON kick\b$ \b2OBJ \bADV \bWHERE",(["flags":FLAG_TOUCH,])})}),
      "kiss":({"irregular","\b1PRON kiss \b2OBJ \bADV","\b1PRON kisses \b2OBJ \bADV"}),
      "knee":({"default",([C_BODY:"where it hurts"]),({"\b1PRON knee\b$ \b2OBJ \bADV \bWHERE",(["flags":FLAG_TOUCH,])})}),
      "kneel":({"\b1PRON \bADV fall\b$ on \b1POSS knees \bAT",(["prep":" in front of"])}),
      "lag":({"default",([C_ADV:"helplessly"]),"\b1PRON lag\b$ \bADV"}),
      "laugh":"simple",
      "leer":"simple",
      "lick":"\b1PRON lick\b$ \b2OBJ \bADV",
      "lie":({"who_p","\b1PRON lie\b$ \bTEXT \bADV","\b1PRON lie\b$ to \b2OBJ \bADV"}),
      "like":"\b1PRON like\b$ \b2OBJ \bADV",
      "listen":({"simple",(["prep":" to"])}),
      "love":"\b1PRON love\b$ \b2OBJ \bADV",
      "mercy":"\b1PRON beg\b$ \b2OBJ for mercy",
      "moan":"simple",
      "mock":"\b1PRON mock\b$ \b2OBJ \bADV",
      "mumble":({"\b1PRON mumble\b$ \bTEXT \bADV \bAT",(["prep":" to"])}),
      "mutter":({"who_p","\b1PRON mutter\b$ \bTEXT \bADV","\b1PRON mutter\b$ to \b2OBJ \bADV"}),
      "nibble":"\b1PRON nibble\b$ \bADV on \b2POSS ear",
      "nod":({"default",([C_ADV:"solemnly"]),"simple"}),
      "nudge":({"\b1PRON nudge\b$ \b2OBJ \bADV \bWHERE",(["flags":FLAG_TOUCH,])}),
      "pace":({"default",([C_ADV:"impatiently"]),"\b1PRON start\b$ pacing \bADV"}),
      "pale":"\b1PRON turn\b$ white as ashes \bADV",
      "panic":"\b1PRON panic\b$ \bADV",
      "pant":({"default",([C_ADV:"heavily"]),"\b1PRON pant\b$ \bADV \bAT"}),
      "pat":({"default",([C_BODY:"on the head"]),({"\b1PRON pat\b$ \b2OBJ \bADV \bWHERE",(["flags":FLAG_TOUCH,])})}),
      "peer":"\b1PRON peer\b$ at \b2OBJ \bADV",
      "pinch":({"irregular","\b1PRON pinch \b2OBJ \bADV","\b1PRON pinches \b2OBJ \bADV"}),
      "point":"simple",
      "poke":({"default",([C_BODY:"in the ribs"]),({"\b1PRON poke\b$ \b2OBJ \bADV \bWHERE",(["flags":FLAG_TOUCH,])})}),
      "ponder":({"default",([C_ADV:"over some problem"]),"\b1PRON ponder\b$ \bADV"}),
      "pounce":({"default",([C_ADV:"playfully"]),({"\b1PRON pounce\b$ \b2OBJ \bADV \bWHERE",(["flags":FLAG_TOUCH,])})}),
      "pout":"\b1PRON pout\b$ \bADV",
      "puff":"\b1PRON puff\b$ \bADV",
      "puke":({"simple",(["prep":" on"])}),
      "punch":({"default",([C_BODY:"in the eye"]),({"irregular","\b1PRON punch \b2OBJ \bADV \bWHERE","\b1PRON punches \b2OBJ \bADV \bWHERE"})}),
      "purr":"simple",
      "puzzle":"\b1PRON look\b$ \bADV puzzled \bAT",
      "raise":"\b1PRON \bADV raise\b$ an eyebrow \bAT",
      "recoil":({"default",([C_ADV:"with fear"]),({"simple",(["prep":" from"])})}),
      "relax":({"irregular","\b1PRON relax \bADV","\b1PRON relaxes \bADV"}),
      "remember":({"\b1PRON remember\b$ \bAT \bADV",(["prep":""])}),
      "roll":({"default",([C_ADV:"to the ceiling"]),"\b1PRON roll\b$ \b1POSS eyes \bADV"}),
      "rotate":"\b1PRON rotate\b$ \bADV",
      "ruffle":"\b1PRON ruffle\b$ \b2POSS hair \bADV",
      "salute":"\b1PRON salute\b$ \b2OBJ \bADV",
      "say":({"default",([C_MSGS:"'nothing"]),({"\b1PRON \bADV say\b$ \bTEXT \bAT",(["prep":" to"])})}),
      "scowl":({"default",([C_ADV:"darkly"]),"simple"}),
      "scratch":({"default",([C_ADV:"on the head"]),({"who_p",({"irregular","\b1PRON scratch \b1OBJself \bADV","\b1PRON scratches \b1OBJself \bADV"}),({"irregular","\b1PRON scratch \b2OBJ \bADV","\b1PRON scratches \b2OBJ \bADV"})})}),
      "scream":({"default",([C_ADV:"loudly"]),"\b1PRON scream\b$ \bTEXT \bADV \bAT"}),
      "shake":({"who_p","\b1PRON shake\b$ \b1POSS head \bADV","\b1PRON shake\b$ hands with \b2OBJ \bADV"}),
      "shiver":({"default",([C_ADV:"from the cold"]),"\b1PRON shiver\b$ \bADV"}),
      "shrug":"\b1PRON shrug\b$ \bADV",
      "sigh":"\b1PRON sigh\b$ \bADV",
      "sing":({"\b1PRON \bADV sing\b$ \bMSG \bAT",(["prep":" to"])}),
      "slap":({"default",([C_BODY:"in the face"]),({"\b1PRON slap\b$ \b2OBJ \bADV \bWHERE",(["flags":FLAG_TOUCH,])})}),
      "smile":({"default",([C_ADV:"happily"]),"simple"}),
      "smirk":"\b1PRON smirk\b$ \bADV",
      "snap":"\b1PRON snap\b$ \b1POSS fingers \bAT",
      "snarl":"simple",
      "sneeze":({"default",([C_ADV:"loudly"]),"simple"}),
      "snicker":"\b1PRON snicker\b$ \bADV",
      "sniff":"\b1PRON sniff\b$ \bADV",
      "snigger":({"default",([C_ADV:"jeeringly"]),"simple"}),
      "snore":"\b1PRON snore\b$ \bADV",
      "snort":"\b1PRON snort\b$ \bADV \bAT",
      "snuggle":"\b1PRON snuggle\b$ \b2OBJ \bADV",
      "sob":"\b1PRON sob\b$ \bADV",
      "spank":({"default",([C_BODY:"on the butt"]),({"\b1PRON spank\b$ \b2OBJ \bADV \bWHERE",(["flags":FLAG_TOUCH,])})}),
      "spit":({"simple",(["prep":" on"])}),
      "squeeze":({"default",([C_ADV:"fondly"]),"\b1PRON squeeze\b$ \b2OBJ \bADV"}),
      "squint":"\b1PRON squint\b$ \bADV",
      "stare":"simple",
      "stomp":({"who_p","\b1PRON stomp\b$ \b1POSS foot \bADV","\b1PRON stomp\b$ on \b2POSS foot \bADV"}),
      "strangle":"\b1PRON strangle\b$ \b2OBJ \bADV",
      "stroke":({"default",([C_BODY:"on the cheek"]),({"\b1PRON stroke\b$ \b2OBJ \bADV \bWHERE",(["flags":FLAG_TOUCH,])})}),
      "strut":({"default",([C_ADV:"proudly"]),"\b1PRON strut\b$ \bADV"}),
      "stumble":"\b1PRON stumble\b$ \bADV",
      "stupid":"\b1PRON look\b$ \bADV stupid",
      "sulk":({"default",([C_ADV:"in the corner"]),"\b1PRON sulk\b$ \bADV"}),
      "surprise":"\b1PRON surprise\b$ \b2OBJ \bADV",
      "surrender":({"simple",(["prep":" to"])}),
      "swear":({"\b1PRON swear\b$ \bMSG \bAT \bADV",(["prep":" before"])}),
      "sweat":"\b1PRON sweat\b$ \bADV",
      "tackle":({"\b1PRON tackle\b$ \b2OBJ \bADV",(["prep":""])}),
      "tap":({"default",([C_ADV:"impatiently",C_BODY:"on the shoulder"]),({"who_p","\b1PRON tap\b$ \b1POSS foot \bADV","\b1PRON tap\b$ \b2OBJ \bWHERE"})}),
      "taunt":"\b1PRON taunt\b$ \b2OBJ \bADV",
      "tease":"\b1PRON tease\b$ \b2OBJ \bADV",
      "thank":"\b1PRON thank\b$ \b2OBJ \bADV",
      "think":({"default",([C_ADV:"carefully"]),"\b1PRON think\b$ \bADV"}),
      "thumb":"\b1PRON \bADV suck\b$ \b1POSS thumb",
      "tickle":"\b1PRON tickle\b$ \b2OBJ \bADV",
      "tongue":"\b1PRON stick\b$ \b1POSS tongue out \bAT",
      "touch":({"irregular","\b1PRON touch \b2OBJ \bADV","\b1PRON touches \b2OBJ \bADV"}),
      "tremble":"\b1PRON tremble\b$ \bADV",
      "twiddle":"\b1PRON twiddle\b$ \b1POSS thumbs \bADV",
      "twitch":({"irregular","\b1PRON twitch \bADV","\b1PRON twitches \bADV"}),
      "utter":({"\b1PRON \bADV utter\b$ \bTEXT \bAT",(["prep":" to"])}),
      "watch":({"default",([C_ADV:"carefully"]),({"who_p",({"irregular","\b1PRON watch the surroundings \bADV","\b1PRON watches the surroundings \bADV"}),({"irregular","\b1PRON watch \b2OBJ \bADV","\b1PRON watches \b2OBJ \bADV"})})}),
      "wave":({"default",([C_ADV:"happily"]),"simple"}),
      "whine":"\b1PRON whine\b$ \bADV",
      "whistle":({"default",([C_ADV:"appreciatively"]),"simple"}),
      "wiggle":({"\b1PRON wiggle\b$ \b1POSS bottom \bAT \bADV",(["prep":""])}),
      "wink":({"default",([C_ADV:"suggestivly"]),"simple"}),
      "wobble":({"\b1PRON wobble\b$ \bAT \bADV",(["prep":""])}),
      "worship":"\b1PRON worship\b$ \b2OBJ \bADV",
      "wrinkle":({"\b1PRON wrinkle\b$ \b1POSS nose \bAT \bADV",(["prep":""])}),
      "yawn":"\b1PRON yawn\b$ \bADV",
      "yell":({"default",([C_ADV:"in a high pitched voice"]),"\b1PRON yell\b$ \bTEXT \bADV \bAT"}),
      "yodel":({"\b1PRON yodel\b$ a merry tune \bADV",(["prep":""])}),
      ]);
}

mapping adverbs=(mapping)soul_master()->get_adverbs();

/* this function can't take typing in most muds */
mapping get_adverbs()
{
  string *q,f;
  if(adverbs) return adverbs;
  f=file_name(this_object());
  sscanf(f,"%s#",f);
  if(f[0]!='/') f="/"+f;
  f=read_file(f+"_adverbs");
  if(f)
    q=my_explode(f,"\n")-({""});
  else
    q=({});

  return mkmapping(q,q);
}

mapping how=([]);
mapping xadverbs=([]);
mapping xverbs=([]);
mapping bodydata=([]);
mixed *failed_action;
mixed last_action;

string verb_string=(string)soul_master()->get_verb_string();
string get_verb_string()
{
  if(verb_string) return verb_string;
  verb_string=implode_nicely(sort_words(m_indices(verbs)));
  verb_string=share_string(big_linebreak(verb_string,"  ",77));
  return verb_string;
}

string xverb_string;
string get_xverb_string()
{
  if(xverb_string) return xverb_string;
  xverb_string=implode_nicely(sort_words(m_indices(xverbs)));
  xverb_string=share_string(big_linebreak(xverb_string,"  ",77));
  return xverb_string;
}

string adverb_string=(string)soul_master()->get_adverb_string();
string get_adverb_string()
{
  if(adverb_string) return adverb_string;
  adverb_string=implode_nicely(sort_words(m_indices(adverbs)));
  adverb_string=share_string(big_linebreak(adverb_string,"  ",77));
  return adverb_string;
}

string xadverb_string;
string get_xadverb_string()
{
  if(xadverb_string) return xadverb_string;
  xadverb_string=implode_nicely(sort_words(m_indices(xadverbs)));
  xadverb_string=share_string(big_linebreak(xadverb_string,"  ",77));
  return xadverb_string;
}

int current_flag;
int see_names;
object *people;

/* set_flag:
 * Sets current_flag, checks if names should show or not.
 * Used from parse()
 */

void set_flag(int flag)
{
  current_flag=flag;
  see_names=!is_dark() || (flag & FLAG_SOUND);
}

/* obj(), pron() and poss() are used by fix_msg */

string obj(object who,object active)
{
  if(who==active)
  {
    if(who==this_player())
    {
      return "yourself";
    }else{
      return "you";
    }
  }else{
    if(see_names)
    {
      if(who==this_player())
      {
        return (string)who->query_objective()+"self";
      }else{
        return (string)who->query_name();
      }
    }else{
      return "Someone";
    }
  }
}

string pron(object who,object active)
{
  if(who==active)
  {
    if(who==this_player())
    {
      return "you";
    }else{
      return "you";
    }
  }else{
    if(see_names)
    {
      return (string)who->query_name();
    }else{
      return "Someone";
    }
  }
}

string poss(object who,object active)
{
  string foo;
  if(who==active)
  {
    if(who==this_player())
    {
      return "your own";
    }else{
      return "your";
    }
  }else{
    if(see_names)
    {
      if(who==this_player())
      {
        return (string)who->query_possessive()+" own";
      }else{
        foo=(string)who->query_name();
        if(foo[-1]=='s') return foo+"'";
        return foo+"'s";
      }
    }else{
      return "Someone's";
    }
  }
}


/* string fix_msg(string s,mixed *persons,object active)
 *
 * This function does the actuall replacing of tokes agains real names.
 * Input:
 *  s:       The string to do the replacing on.
 *  persons: Contains the persons around ordered as follows:
 *           ({
 *             ({ 1st person(s) }),
 *             ({ 2nd person(s) }),
 *             ({ 3rd person(s) }),
 *           })

 *           No limitations are imposed, you can have any amount of groups
 *           of people, and every group can contain any amount of people.
 *           Having one person in several groups will not make much sense though.
 *  active:  The person who will hear this message. Can be zero to indicate
 *           any person outside the groups.
 */

string fix_msg(string s,mixed *persons,object active)
{
  string *t,*t2,tmp;
  int e,d,p;
#if DEBUG>3
  write("fix_msg '"+s+"' "+show_value(persons)+".\n");
#endif
  t=my_explode(s,"\b");
  for(e=1;e<sizeof(t);e++)
  {
#if DEBUG>3
    write("fixing part "+e+" '"+t[e]+"'.\n");
#endif
    if(sscanf(t[e],"%dPRON%s",p,tmp)==2 ||
        (sscanf(t[e],"%dpron%s",p,tmp)==2 && -1!=member_array(active,persons[p-1])))
    {
      p--;
      t[e]=implode_nicely(MAP(persons[p],pron,active))+tmp;

    }else if(sscanf(t[e],"%dPOSS%s",p,tmp)==2 ||
        (sscanf(t[e],"%dposs%s",p,tmp)==2 && -1!=member_array(active,persons[p-1]))){
      p--;
      t[e]=implode_nicely(map_array(persons[p],"poss",this_object(),active))+tmp;

    }else if(sscanf(t[e],"%dOBJ%s",p,tmp)==2 ||
        (sscanf(t[e],"%dobj%s",p,tmp)==2 && -1!=member_array(active,persons[p-1]))){
      p--;
      t[e]=implode_nicely(MAP(persons[p],obj,active))+tmp;

    }else if(sscanf(t[e],"%dpron%s",p,tmp)==2){
      p--;
      if(sizeof(persons[p])==1)
      {
        t[e]=persons[p][0]->query_pronoun()+tmp;
      }else{
        t[e]="they"+tmp;
      }
    }else if(sscanf(t[e],"%dposs%s",p,tmp)==2){
      p--;
      if(sizeof(persons[p])==1)
      {
	t[e]=persons[p][0]->query_possessive()+tmp;
      }else{
        t[e]="their"+tmp;
      }
    }else if(sscanf(t[e],"%dobj%s",p,tmp)==2){
      p--;
      if(sizeof(persons[p])==1)
      {
	if(persons[p][0]==this_player())
	  t[e]=this_player()->query_objective()+"self"+tmp;
	else
	  t[e]=persons[p][0]->query_objective()+tmp;
      }else{
        t[e]="them"+tmp;
      }
    }else if(sscanf(t[e],"\b%dIS%s",p,tmp)){
      p--;
      if(member_array(active,persons[p])>=0 && sizeof(persons[p])==1)
	t[e]="is"+tmp;
      else
	t[e]="are"+tmp;
    }else{
      t[e]="\b"+t[e];
    }
  }
#if DEBUG>3
  write("fix_msg returned '"+implode(t,"")+"'.\n");
#endif
  return implode(t,"");
}

/*
 * This is a basic structure in this version of the soul. It is the general
 * description form for an output message. All lines of commands to the soul
 * will produce this data before it parsed and outputted. 
 *
 * FEEL=
 * ({
 *   ({
 *     ({ ({1st persons}),({2nd persons}),....  }),
 *     ({ 1st p message, 2nd p message, .... }),
 *     flags,
 *     ({ ob, fun }),
 *   }),
 *
 *   ({
 *     ({ ({1st persons}),({2nd persons}),....  }),
 *     ({ 1st p message, 2nd p message, .... }),
 *     flags,
 *     ({ ob, fun }),
 *   }),
 * 
 *   .....
 * })

 *
 * If you know C, maybe this is clearer:
 *
 * typedef string message;
 * typedef object *group;
 * struct sent
 * {
 *   group *persons;
 *   message *messages;
 *   int flag;    // see FLAGS_ above
 * };
 * struct sent *FEEL;
 *
 * The struct sent may (will probably) be expanded in the future.
 * 
 */

/* parse:
 * A lot of feelings will begin with \b1OBJ, this function will prevent
 * repeating \b1OBJ a lot by merging several sentences beginning with
 * \b1OBJ to one. Example: it would transform "Profezzorn smiles. Profezzorn
 * chuckles. Profezzorn laughs." to "Profezzorn smiles, chuckles and laughs."
 *
 * This function also calls fix_msg on all the strings. ie. the returned
 * string should be ready to send to the player who.
 *
 * The arguments are:
 *   a FEEL structure
 *   the object who, the person who will see this message.
 *   some preparsed mappings produced by the function preparse() below
 */

string parse(mixed *feel,object who,mapping *preparsed)
{
  int e,cnt;
  string *msg,*sent,tmp,lastname;
  object *lastobj;
  mixed *groups;

  msg=({});

#if DEBUG
  write("parse : "+show_value(feel)+" for "+who->query_real_name()+".\n");
#endif
  
  sent=lastobj=0;
  cnt=1;
  for(e=0;e<sizeof(feel);e++)
  {
    if(tmp=preparsed[e][who])
    {
      set_flag(feel[e][2]);

      groups=feel[e][0];

      if(is_dark())
      {
        /* everybody knows if there is sound involved */
        if(!(current_flag & FLAG_SOUND))
        {
          if(-1==member_array(who,groups[0]))
          {
	    /* second only person knows if it is physical */
	    if(!(current_flag & FLAG_TOUCH) || -1==member_array(who,groups[1]))
	      continue;
          }
        }
      }

      if(sscanf(tmp,"\b1PRON %s",tmp))
      {
	tmp=fix_msg(tmp,groups,who);
	if(lastobj && eq(lastobj,groups[0]))
	{
	  if(sent[sizeof(sent)-1]==tmp)
	  {
	    cnt++;
	  }else{
	    sent[sizeof(sent)-1]+=nr_times(cnt);
	    sent+=({tmp});
	    cnt=1;
	  }
	}else{
	  if(sent)
	  {
	    msg+=({lastname+" "+implode_nicely(sent)+nr_times(cnt)+"."});
	    cnt=1;
	  }
	  sent=({tmp});
	  
	  lastobj=groups[0];
	  lastname=capitalize(fix_msg("\b1PRON",groups,who));
	}
      }else{
	if(sent)
	{
	  msg+=({lastname+" "+implode_nicely(sent)+nr_times(cnt)+"."});
	  cnt=1;
	}

	msg+=({capitalize(fix_msg(tmp,groups,who))+"."});
	
	lastobj=0;
      }
    }
  }
  if(sent)
  {
    msg+=({lastname+" "+implode_nicely(sent)+nr_times(cnt)+"."});
    cnt=1;
  }
  return implode(msg," ");
}

/* Preparse:
 * The function parse() needs to know quickly which message the object who
 * is to have. This function makes mappings of the type:
 * ({
 *   ([ person: message, ... ]),
 *   ([ person: message, ... ]),
 * })

 * Where each mapping corresponds to an array in the FEEL structure.
 *
 */

mapping *preparse(mixed *feel)
{
  int e,d,q;
  mapping tmp,*ret,*tmp3;
  mixed *groups;
  object *group;
  string msg;

#if DEBUG>3
  write("Preparse "+show_value(feel)+".\n");
#endif
  ret=allocate(sizeof(feel));
  for(e=0;e<sizeof(feel);e++)
  {
    tmp=([]);
    groups=feel[e][0];
    for(d=sizeof(groups)-1;d>=0;d--)
    {
      msg=feel[e][1][d];
      group=groups[d];
      for(q=sizeof(group)-1;q>=0;q--) tmp[group[q]]=msg;
    }
    tmp[0]=msg; 
    ret[e]=tmp;
  }
  return ret;
}

/* do magic:
 * this function is called from reduce_verb, it is meant to do some generic
 * basic action. data[0] is a string that should form the messages for the
 * FEEL structures after the basic replacements and stuff has been done.
 * data[1] is a mapping that can contain the following information:
 *
 * ([
 *   "prep":" to", // preposition: probably at, to or before
 *   "flags":FLAG_SOUND|FLAG_OFFENSE, // See definitions for FLAG_* above 
 *   "func",({ ob, func, data }),
 // may be extended in the future
 * ])
 * There may be more properties for feelings in the future.
 * do_magic() returns a FEEL struct.
 */

/* small routine for do_magic() */
private string quote(string s) { return "'"+s+"'"; }

mixed *do_magic(mixed *cond)
{
  string st_msg,nd_msg,rd_msg,tmp;
  mapping xdata;
  mixed *data;

#if DEBUG>2
  write("do_magic: "+show_value(cond)+"\n");
#endif

  data=cond[C_DATA];
  st_msg=data[0];

  if(sizeof(data)>1)
    xdata=data[1];
  else
    xdata=([]);
  
  if(sizeof(cond[C_ADV]))
    st_msg=REPLACE(st_msg," \bADV"," "+implode_nicely(cond[C_ADV]));
  else
    st_msg=REPLACE(st_msg," \bADV","");

  if(sizeof(cond[C_ADJ]))
    st_msg=REPLACE(st_msg," \bADJ"," "+implode_nicely(cond[C_ADJ]));
  else
    st_msg=REPLACE(st_msg," \bADJ","");
  
  if(sizeof(cond[C_BODY]))
    st_msg=REPLACE(st_msg," \bWHERE"," "+implode_nicely(cond[C_BODY]));
  else
    st_msg=REPLACE(st_msg," \bWHERE","");

  tmp=xdata["prep"];
  if(!tmp) tmp=" at";
  if(sizeof(cond[C_WHO]))
  {
    st_msg=REPLACE(st_msg," \bAT",tmp+" \b2OBJ");
  }else{
    st_msg=REPLACE(st_msg," \bAT","");
  }

  st_msg=REPLACE(st_msg," \bTEXT",
    implode_nicely(MAP(cond[C_MSGS],quote,0)));

  st_msg=REPLACE(st_msg," \bMSG"," "+implode(cond[C_MSGS]," "));

  if(sscanf(st_msg,"%s\b|%s",st_msg,nd_msg))
  {
    if(sscanf(nd_msg,"%s\b|%s",nd_msg,rd_msg))
    {
      nd_msg=REPLACE(nd_msg,"\b$","s");
      rd_msg=REPLACE(rd_msg,"\b$","s");
    }else{
      rd_msg=nd_msg=REPLACE(nd_msg,"\b$","s");
    }
  }else{
    nd_msg=rd_msg=REPLACE(st_msg,"\b$","s");
  }
  st_msg=REPLACE(st_msg,"\b$","");

  if(sscanf(st_msg,"%s\b2",tmp) && !sizeof(cond[C_WHO]))
  {
    notify_fail("You need to do '"+cond[C_VERB]+"' to someone.\n");
    return 0;
  }

  return
    ({
      ({ 
        ({ ({ this_player() }), cond[C_WHO],
	     SUB_ARRAYS(people,({this_player()})+cond[C_WHO]) }),
        ({ st_msg, nd_msg, rd_msg }),
        xdata["flags"],
        xdata["func"],
      })
    });
  
}


/* reduce verb should return an array on FEEL form */
/* Data is originally from the mapping describing verbs and
 * cond is a CONDITION structure (see above)
 */

mixed *reduce_verb(mixed cond)
{
  mixed tmp,ptr,tmp2;
  mixed data;

#if DEBUG>2
  write("reduce_verb: "+show_value(cond)+"\n");
#endif

  data=cond[C_DATA];
  if(!pointerp(data)) data=cond[C_DATA]=({data});
  

  if(pointerp(data[0]))
  {
    return data;
  }else if(stringp(data[0])){
    switch(data[0])
    {
      /* this adds default to the COND structure
       * usage: ({"default", ([ C_ADV : ({ defaultadverbs }), ... ]), data })
       * reduce_verb is then called with COND modified and data as the new data.
       */
      case "default":
        for(tmp=0;tmp<sizeof(cond);tmp++)
	{
	  if(data[1][tmp])
	  {
	    if(!cond[tmp])
	    {
	      cond[tmp]=data[1][tmp];
	    }else if((pointerp(cond[tmp]) && !sizeof(cond[tmp]))){
	      if(pointerp(data[1][tmp]))
	      {
		cond[tmp]=data[1][tmp];
	      }else{
		cond[tmp]=({data[1][tmp]});
	      }
	    }
	  }
	}
	cond[C_DATA]=data[2];
        return reduce_verb(cond);


	/* report an error.
	 * this might be used to idicate that a feeling NEEDS an adverb or something.
	 * usage: ({"error","<mortal message>","<wizard message>"})
	 * if you just give ({"error"}) the standard error will be given.
	 */
      case "error":
        switch(sizeof(data))
        {
          default:
          case 3:
            inform_error(data[1],data[2]);
            break;

          case 2:
            inform_error(data[1],0);
            break;

          case 1:
        }
        return 0;


	/* Irregular messages.
	 * this is used to give different messages to 1st, 2nd and 3rd persons.
	 * usage: ({"irregular", 1st_person_data, 2nd_person_data, ....})
	 * reduce_verb is called for each data and then compiled into one feeling
	 */
      case "irregular":
	tmp2=({});
	
	for(tmp=1;tmp<sizeof(data);tmp++)
	{
	  cond[C_DATA]=data[tmp];
	  tmp2+=reduce_verb(cond);
	}
	for(tmp=1;tmp<sizeof(tmp[0][0]);tmp++)
	{
	  tmp2[0][0][tmp]=tmp2[ MIN(tmp,sizeof(tmp2)) ][0][tmp];
	  tmp2[0][1][tmp]=tmp2[ MIN(tmp,sizeof(tmp2)) ][1][tmp];
	}
	return tmp2[0..0];

	/* set flags
	 * usage: ({"flags",[ FLAG_SOUND |  FLAG_TOUCH | FLAG_OFFENSE ],data})
	 * flags are added _after_ reduce_verb has been called with the data
	 * see the defines for FLAG_* above
	 */
      case "flags": 
	cond[C_DATA]=data[2];
	tmp=reduce_verb(cond);
	for(tmp2=0;tmp2<sizeof(tmp);tmp2++)
	{
	  while(sizeof(tmp2[tmp])<3)
	    tmp2[tmp]+=({0});
	  tmp2[tmp][2]=data[1];
	}
	return tmp;
	
	/* random
	 * usage: ({"rnd", data1, data2, data3, ... });
	 * reduce_verb will bee called with one of the data*
	 */
      case "rnd":
	tmp=random(sizeof(data)-1);
	break;
	
	/* self_p - did we do it to ourself? 
	 * usage: ({"self_p", data_if_it_was_someone_else, data_if_it_was_ourself})
	 */
      case "self_p":
        tmp=sizeof(cond[C_WHO])==1 && cond[C_WHO][0]==this_player();
        break;

	/* these check the corresponding index in COND and if it is set, use the first
	 * data, else the second
	 * usage: ({"*_p",not_true_data,true_data})
	 */
      case "who_p": tmp=sizeof(cond[C_WHO]); break;
      case "adv_p": tmp=sizeof(cond[C_ADV]); break;
      case "adj_p": tmp=sizeof(cond[C_ADJ]); break;
      case "msg_p": tmp=sizeof(cond[C_MSGS]); break;
      case "body_p":tmp=sizeof(cond[C_BODY]);break;

	/* this is a dumb definition, but it works */
      case "simple":
        data[0]="\b1PRON "+cond[C_VERB]+"\b$ \bADV \bAT";
        /* fall through */

      default:
        if(data[0][0]=='/')
          return reduce_verb((mixed *)
            call_other(data[0],data[1],data[2],cond));
        /* This is a simple-form feeling, we do some magic here */

      return do_magic(cond);
    }
    tmp++;
    if(tmp>=sizeof(data)) tmp=sizeof(data)-1;
    cond[C_DATA]=data[tmp];
    return reduce_verb(cond);

  }else if(objectp(data[0])){

    /*
     * If this function wish to affect cond it should call
     * previous_object()->reduce_verb(), ie. this function.
     */

    /*
     * Also note that this function is NOT allowed to have any side
     * effects, as this feelings might not be done due to errors
     * while parsing, There is another functionality for doing side
     * effect functions. (See the 'kiss' verb)
     */

    cond[C_DATA]=(mixed *)data[0]->reduce_verb(data[1],cond);
    return reduce_verb(cond);
  }else{
    /* Feeling syntax error, someone screwed up */
    inform_error("Sorry, the verb '"+cond[C_VERB]+"' doesn't work.\n",
        "unknown type to reduce_verb ("+show_value(data)+")\n");
    return 0;
  }
}

/* msg:
 * This routine is not used by the soul, it provided because it can be
 * convenient to call from other commands.
 * Message is a message on basically the same form as data[0] in
 * do_magic, (or the base message form in the verbs mapping)
 * first is the first person ie. this_player()
 * second is the second person.
 * (both first and second can be several people)
 */

varargs string msg(string message,mixed first,mixed second)
{
  string st_msg,nd_msg,rd_msg;
  mixed *tmp;
  object o;
  int e;

  if(sscanf(message,"%s\b|%s",st_msg,nd_msg))
  {
    if(sscanf(nd_msg,"%s\b|%s",nd_msg,rd_msg))
    {
      nd_msg=REPLACE(nd_msg,"\b$","s");
      rd_msg=REPLACE(rd_msg,"\b$","s");
    }else{
      rd_msg=nd_msg=REPLACE(nd_msg,"\b$","s");
    }
  }else{
    nd_msg=rd_msg=REPLACE(st_msg=message,"\b$","s");
  }
  st_msg=REPLACE(st_msg,"\b$","");

  if(!first) first=({this_player()});
  
  if(!second) second=({});
  
  if(!pointerp(second)) second=({second});
  
  if(!pointerp(first)) first=({first});
  
  tmp=({first,second});
  

  for(e=0;e<sizeof(first);e++)
    tell_object(o,fix_msg(st_msg,tmp,first[e]));

  for(e=0;e<sizeof(second);e++)
    tell_object(o,fix_msg(nd_msg,tmp,second[e]));

  tell_room(environment(first[0]),fix_msg(rd_msg,tmp,0),first+second);
}

/* This function outputs a FEEL struct to the players involved.
 * Normally called from do_feel()
 */
void output_feeling(mixed *feel)
{
  int e,d,c;
  mapping done;
  mixed *preparsed,*group;
  string tmp;

  done=([]);
  preparsed=preparse(feel);
  for(e=0;e<sizeof(feel);e++)
  {
    for(d=0;d<sizeof(feel[e][0]);d++)
    {
      group=feel[e][0][d];
      for(c=0;c<sizeof(group);c++)
      {
        if(done[group[c]]) continue;
        done[group[c]]=1;
	tmp=parse(feel,group[c],preparsed);
	if(strlen(tmp))
	  tell_object(group[c],tmp+"\n");
      }
    }
  }
}

/*
pron: name   you  he/she/it        they
poss: name's your his/her/its      their
obj:  name   you  him/her/them     them
*/

int help(string s)
{
  string res;
  if(!s) return 0;
  switch(s)
  {
  case "feelings":
    more("General commands available:\n");
    more(get_verb_string());
    if(m_sizeof(xverbs))
    {
      more("Extra commands available:\n");
      more(get_xverb_string());
    }
    more("Persons and adverbs can be shorted to shorted unique prefix.\n");
    more("See also: help adverbs and help feeling list\n");
    more_flush();
    return 1;

  case "adverbs":
    more("Adverbs that can be used together with feeling-commands:\n");
    more(get_adverb_string());
    more_flush();
    return 1;

  case "soul version":
    write("Soul version 2.0, written by hubbe@lysator.liu.se.\n");
    return 1;
  }
}

/* get the name of a player/monster
 * used by the parser
 */
string get_name(object o)
{
  return lower_case((string)o->query_name());
}

/* is it a living object ?
 * used by the parser
 */
int isliving(object o)
{
  return !!(o->query_living() && o->short());
}

/* prefix:
 * is pr a 'shortest unique prefix' for any of the strings in
 * the array dum? If so return that index from dum. If none matched
 * return 0, if several matched, do a notify_fail using errm and
 * a list of all matching strings and then return -1
 */
mixed prefix(string *dum,string pr,string errm)
{
  string *q;

  /* regexp gives nasty messages unless we remove these */
  pr=REPLACE(pr,"(","");
  pr=REPLACE(pr,")","");
  pr=REPLACE(pr,"[","");
  pr=REPLACE(pr,"]","");
  pr=REPLACE(pr,"//","");
  q=regexp(dum,"^"+pr);
  if(sizeof(q)==1) return q[0];
  if(!sizeof(q)) return 0;
  notify_fail(errm+"\n"+
	      big_linebreak(implode_nicely(q,"or"),"",78)+"\n");
  return -1;
}

/* which number:
 * returns a string that describes the order of the number
 * ie 1 -> first, 2-> second etc. etc.
 */
string which_number(int x)
{
  switch(x)
  {
  case 0: return "zeroth";
  case 1: return "first";
  case 2: return "second";
  case 3: return "third";
  case 4: return "fourth";
  case 5: return "fifth";
  case 6: return "sixth";
  case 7: return "seventh";
  case 8: return "eighth";
  case 9: return "ninth";
  case 10: return "tenth";
  case 11: return "eleventh";
  case 12: return "twelvth";
  case 13: return "thirteenth";
  case 14: return "fourteenth";
  case 15: return "fifteenth";
  case 16: return "sixteenth";
  case 17: return "seventeenth";
  case 18: return "eightteenth";
  case 19: return "nineteenth";
  default:
    switch(x%100)
    {
    case  1: case 21: case 31: case 41: case 51:
    case 61: case 71: case 81: case 91:
      return x+"st";

    case  2: case 22: case 32: case 42: case 52:
    case 62: case 72: case 82: case 92:
      return x+"nd";

    case  3: case 23: case 33: case 43: case 53:
    case 63: case 73: case 83: case 93:
      return x+"th";

    default: return x+"th"; break;
    }
  }
}

mixed *brokendown_data;

/* This function does all the parsing!
 * it parses the words in the string text, and returns an array of
 * COND structs, one for each sentence.
 */
mixed *webster(string text)
{
  string *words,tmp,*tmp2,message;
  int e,u,except;
  object ob,*oldwho;
  mixed p,*verbdata;
  mapping persons;
  string _how,current_word;
  mixed *cond;
  
  brokendown_data=({});
  oldwho=({});
  
  cond=({0,0,0,({}),({}),({}),({}),({})});

  /* remove double spaces */
  words=my_explode(text," ")-({""});

  /* find all present livings */
  people=FILTER(all_inventory(environment(this_player())),isliving,0);

  for(e=0;e<sizeof(words);e++)
  {
    current_word=words[e];
#if DEBUG
    write("Webster: words["+e+"]=\""+current_word+"\"\n");
#endif
    if(current_word[strlen(current_word)-1]==',')
      current_word=words[e]=current_word[0..strlen(current_word)-2];

    if(how[current_word])
    {
      _how=current_word;
      continue;
    }

    if(current_word[0]=='"')
    {
      message=current_word[1..100000];
      for(e++;message[strlen(message)-1]!='"' && e<sizeof(words);e++)
	message+=" "+words[e];
      if(message[strlen(message)-1]=='"')
      {
	message=message[0..strlen(message)-2];
	e--;
      }
      cond[C_MSGS]+=({message});
      
      continue;
    }

    switch(current_word) 
    {
      /* null words */
    case "and":
    case "&":
    case "":
    case "at":
    case "to":
    case "before":
    case "in":
    case "on":
    case "the":
      break;

      /* ourselves */
    case "me":
    case "myself":
    case "I":
      if(except)
	cond[C_WHO]-=({this_player()});
      
      else
	cond[C_WHO]+=({this_player()});
      
      break;

      /* the people from the last sentence */
    case "them":
    case "him":
    case "her":
    case "it":
      if(current_word=="them")
      {
	if(sizeof(oldwho)<2) return notify_fail("Who?\n"),0;
      }else{
	if(sizeof(oldwho)!=1 ||
	   ((mixed)oldwho[0]->query_objective())!=current_word)
	  return notify_fail("Who?\n"),0;
      }
      if(except)
	cond[C_WHO]-=oldwho;
      else
	cond[C_WHO]+=oldwho;
      break;

      /* everybody in the room */
    case "all":
    case "everybody":
    case "everyone":
      if(except)
      {
	cond[C_WHO]=({});
	
      }else{
	cond[C_WHO]+=SUB_ARRAYS(people,({this_player()}))
	  ;
      }
      break;

      /* negate persons */
    case "except":
    case "but":
      if(!except && !sizeof(cond[C_WHO]))
      {
	notify_fail("That '"+current_word+"' doesn't look grammatically right there.\n");
	return 0;
      }
      except=!except; /* watch those double negations */
      break;

      /* magical adverb */
    case "plainly":
      cond[C_ADV]=({""});
      
      break;

      /* all the reset */
    default:
      if((p=verbs[current_word]) || (p=xverbs[current_word]))
      {
	/* here I must add some magic for when there is a person
	 * with the same name in the room
	 */

	if(cond[1])
	{
	  brokendown_data+=({cond});
	  
	  oldwho=cond[C_WHO];
	  except=0;
	}
	cond=({0,current_word,p,({}),({}),({}),({}),({})});
	
	break;
      }

      /* find their names */
      persons=mkmapping(MAP(people,get_name,0),people);
      if(ob=persons[current_word])
      {
	if(except)
	  cond[C_WHO]-=({ob});
	
	else
	  cond[C_WHO]+=({ob});
	
	break;
      }

      if(adverbs[current_word] || xadverbs[current_word])
      {
	if(_how)
	{
	  cond[C_ADV]+=({_how+" "+current_word});
	  
	  _how=0;
	}else{
	  cond[C_ADV]+=({current_word});
	  
	}
	break;
      }

      if(p=bodydata[current_word])
      {
	cond[C_BODY]+=({p});
	
	break;
      }

      if(p=prefix(m_indices(persons),current_word,"Who do you mean?"))
      {
	if(p==-1)
	{
	  failed_action=
	    ({
	      e?implode(words[0..e-1]," "):"",
	      current_word,
	      implode(words[e+1..sizeof(words)]," ")
	    });
	  
	  return 0;
	}
	words[e]=get_name(persons[p]);
	if(except)
	  cond[C_WHO]-=({persons[p]});
	
	else
	  cond[C_WHO]+=({persons[p]});
	
	break;
      }
      
      u=e;
      tmp=current_word;
      p=prefix(m_indices(adverbs),tmp,"What adverb was that?");
      if(p==-1)
	p=prefix(m_indices(xadverbs),tmp,"What adverb was that?");
      while(p==-1 && u+1<sizeof(words))
      {
	u++;
	tmp+=" "+words[u];
	p=prefix(m_indices(adverbs),tmp,"What adverb was that?");
	if(p==-1)
	  p=prefix(m_indices(xadverbs),tmp,"What adverb was that?");
      }

      if(p)
      {
	if(p==-1)
	{
	  failed_action=
	    ({
	      e?implode(words[0..e-1]," "):"",
	      current_word,
	      implode(words[u+1..sizeof(words)]," ")
	    });
	  
	  return 0;
	}
	tmp2=explode(p," ");
	for(u=0;tmp2 && u<sizeof(tmp2) && e<sizeof(words);u++)
	{
	  if(tmp2[u]==words[e]) { e++; continue; }
	  if(tmp2[u][0..strlen(words[e])-1]==words[e]) e++;
	  break;
	}
	e--;
	if(_how)
	{
	  cond[C_ADV]+=({_how+" "+p});
	  
	  _how=0;
	}else if(p=="plainly"){
	  cond[C_ADV]=({""});
	  
	}else{
	  cond[C_ADV]+=({p});
	  
	}
	break;
      }

      if(e)
	notify_fail("The "+which_number(e+1)+" word in that sentence doesn't make sense to me.\n");
      return 0;
    }
  }
  if(!cond[1])
  {
    notify_fail("No verb?\n");
    return 0;
  }
  last_action=implode(words," ");
  brokendown_data+=({cond});
  
  return brokendown_data;
}

int do_feel(string str)
{
  int e;
  mixed *feel;

#ifndef LPC4
  str=query_verb()+(str?" "+str:"");
#endif

  if(!webster(str)) return 0;

  feel=({});
  
  for(e=0;e<sizeof(brokendown_data);e++)
  {
    mixed *tmp;
    tmp=reduce_verb(brokendown_data[e]);
    if(!tmp) return 0;
    brokendown_data[e][C_DATA]=tmp;
    feel+=tmp;
  }
  output_feeling(feel);
  return 1;
}

/* Takes an array of verbs, only removes extra verbs */
void remove_verb(string *v)
{
  int e;
  for(e=0;e<sizeof(v);e++) xverbs=m_delete(xverbs,v[e]);
  xverb_string=0;
}


#ifdef LPC4
void add_verbs(mapping v)
{
  xverbs=v&xverbs;
  xverb_string=0;
}
#else
add_verbs(mapping v)
{
  remove_verb(m_indices(xverbs) & m_indices(v));
  xverbs+=v;
  xverb_string=0;
}
#endif

mapping query_xverbs() { return xverbs; }


#if 1

/* these functions dumps feelings to file in a parsable format,
 * you may ignore them, they were only used when I ported feelings
 * to the 2.0 format from the old soul
 */
#define DUMPFILE "/open/feelingdump"

string dump_flags(int flag)
{
  string *tmp;
  tmp=({});
  if(flag & FLAG_SOUND) tmp+=({"FLAG_SOUND"});
  if(flag & FLAG_TOUCH) tmp+=({"FLAG_TOUCH"});
  if(flag & FLAG_OFFENSE) tmp+=({"FLAG_OFFENSE"});
  if(!sizeof(tmp)) return "0";
  return implode(tmp,"|");
}

string dump_one_xverb(mixed a)
{
  mixed tmp,tmp2,tmp3;;
  if(!pointerp(a)) return show_value(a);
  if(stringp(a[0]))
  {
    switch(a[0])
    {
    case "flags":
      return "({\"flags\","+dump_flags(a[1])+","+dump_one_xverb(a[2])+"})";

    case "default":
      tmp=({});
      if(a[1][C_METAVERB])
	tmp+=({"C_METAVERB:"+show_value(a[1][C_METAVERB])});
      if(a[1][C_VERB])
	tmp+=({"C_VERB:"+show_value(a[1][C_VERB])});
      if(a[1][C_DATA])
	tmp+=({"C_DATA:"+show_value(a[1][C_DATA])});
      if(a[1][C_WHO])
	tmp+=({"C_WHO:"+show_value(a[1][C_WHO])});
      if(a[1][C_ADV])
	tmp+=({"C_ADV:"+show_value(a[1][C_ADV])});
      if(a[1][C_ADJ])
	tmp+=({"C_ADJ:"+show_value(a[1][C_ADJ])});
      if(a[1][C_MSGS])
	tmp+=({"C_MSGS:"+show_value(a[1][C_MSGS])});
      if(a[1][C_BODY])
	tmp+=({"C_BODY:"+show_value(a[1][C_BODY])});

      if(sizeof(tmp))
	return "({\"default\",(["+implode(tmp,",")+"]),"+dump_one_xverb(a[2])+"})";
      else
	return dump_one_xverb(a[2]);
	
    case "irregular":
    case "rnd":
    case "who_p":
    case "self_p":
    case "adv_p":
    case "adj_p":
    case "msg_p":
    case "body_p":
      return "({"+show_value(a[0])+","+
	implode(MAP(a[1..1000],dump_one_xverb,0),",")+"})";
	
    default:
      if(sizeof(a)==2 && mappingp(a[1]) && a[1]["flags"])
      {
	tmp=a[1]+([]);
	tmp2=tmp["flags"];
	tmp=show_value(m_delete(tmp,"flags"));

	return "({"+show_value(a[0])+
	  ",([\"flags\":"+dump_flags(tmp2)+","+(tmp[2..10000])+"})";
      }
    }
  }
  return show_value(a);
}


void do_dump_verbs(mapping v)
{
  int e;
  mixed *a;
  mixed *b;

  a=m_indices(v);
  b=m_values(v);
  if(sizeof(a)>60)
  {
    call_out("do_dump_verbs",2,mkmapping(a[61..10000],b[61..10000]));
    a=a[0..60];
    b=b[0..60];
    for(e=0;e<sizeof(a);e++)
      write_file(DUMPFILE,"  "+show_value(a[e])+":"+dump_one_xverb(b[e])+",\n");
  }else{
    for(e=0;e<sizeof(a);e++)
      write_file(DUMPFILE,"  "+show_value(a[e])+":"+dump_one_xverb(b[e])+",\n");
    write_file(DUMPFILE,"])\n");
    write("Done.\n");
  }
}

void dump_verbs(mapping v)
{
  if(!v) v=xverbs;
  write("Dumping to "+DUMPFILE+".\n");
  rm(DUMPFILE);
  write_file(DUMPFILE,"([\n");
  do_dump_verbs(v);
}

#endif
#define OLD_SIMP 0
#define OLD_DEFA 1
#define OLD_DEUX 2
#define OLD_PERS 3
#define OLD_QUAD 4
#define OLD_PREV 5
#define OLD_SHRT 6
#define OLD_PHYS 7
#define OLD_FULL 8

/* this function converts a v1.x feeling to a v2.0 feeling */
string convert_simple_string(string a)
{
  a=REPLACE(a,"\nAT","\bAT");
  a=REPLACE(a,"\nHOW","\bADV");
  a=REPLACE(a,"\nWHAT","\bMSG");
  a=REPLACE(a,"\nMSG","\bTEXT");
  a=REPLACE(a,"\nWHERE","\bWHERE");
  a=REPLACE(a,"$","\b$");
  a=REPLACE(a,"\nIS","\bIS");

  a=REPLACE(a,"\nWHO","\b2OBJ");
  a=REPLACE(a,"\nPOSS","\b2POSS");
  a=REPLACE(a,"\nTHEIR","\b2poss");
  a=REPLACE(a,"\nMY","\b1OBJ");
  a=REPLACE(a,"\nOBJ","\b2obj");
  a=REPLACE(a,"\nYOUR","\b1POSS");
  a=REPLACE(a,"\bSUBJ","\b2pron");
  return a;
}

mixed convert_feeling(string verb,mixed *verbdata)
{
  string a;
  mixed data;
  mapping xdata;

  xdata=([]);
  switch(verbdata[0])
  {
  case OLD_DEFA:
    /* a="\b1PRON "+verb+"\b$ \bADV \bAT"; */
    a="simple";

  case OLD_PREV:
    if(!a)
      a="\b1PRON "+verb+"\b$"+verbdata[2]+" \b2OBJ \bADV";

  case OLD_PHYS:
    if(!a)
    {
      xdata["flags"]|=FLAG_TOUCH;
      a="\b1PRON "+verb+"\b$"+verbdata[2]+" \b2OBJ \bADV \bWHERE";
    }

  case OLD_SHRT:
    if(!a)
      a="\b1PRON "+verb+"\b$"+verbdata[2]+" \bADV";

  case OLD_SIMP:
    if(!a)
      a=convert_simple_string("\b1PRON"+verbdata[2]);

    if(sizeof(verbdata)>3 && verbdata[3]!=" at")
      xdata["prep"]=verbdata[3];

    if(!m_sizeof(xdata))
      data=a;
    else
      data=({a,xdata});
    
    break;

  case OLD_PERS:
    data=({"who_p",
	"\b1PRON"+convert_simple_string(verbdata[2]),
	"\b1PRON"+convert_simple_string(verbdata[3])
    });
    
    break;

  case OLD_DEUX:
    data=({"irregular",
	"\b1PRON"+convert_simple_string(verbdata[2]),
	"\b1PRON"+convert_simple_string(verbdata[3])
    });
    
    break;

  case OLD_QUAD:
    data=({"who_p",
	     ({"irregular",
		 "\b1PRON"+convert_simple_string(verbdata[2]),
		 "\b1PRON"+convert_simple_string(verbdata[3])
		 }),
	     ({"irregular",
		 "\b1PRON"+convert_simple_string(verbdata[4]),
		 "\b1PRON"+convert_simple_string(verbdata[5])
		 })
	     });
    
    break;

  case OLD_FULL:
    data=({"who_p",
	  ({"flag_p",
	      ({"irregular",
		  "\b1PRON"+convert_simple_string(verbdata[2]),
		  "\b1PRON"+convert_simple_string(verbdata[4]),
	      }),
	      ({"irregular",
		  "\b1PRON"+convert_simple_string(verbdata[5]),
		  "\b1PRON"+convert_simple_string(verbdata[7]),
	      }),
	  }),
	  ({"flag_p",
	      ({"irregular",
		  "\b1PRON"+convert_simple_string(verbdata[8]),
		  "\b1PRON"+convert_simple_string(verbdata[9]),
		  "\b1PRON"+convert_simple_string(verbdata[10]),
	      }),
	      ({"irregular",
		  "\b1PRON"+convert_simple_string(verbdata[11]),
		  "\b1PRON"+convert_simple_string(verbdata[12]),
		  "\b1PRON"+convert_simple_string(verbdata[13]),
	      }),
	  }),
      });
    
  }

  if(verbdata[1] && sizeof(verbdata[1]))
  {
    verbdata=verbdata[1];
    /* convert defaults */

    data=({"default",([]),data});
    if(verbdata[0]) data[1][C_ADV]=verbdata[0];
    if(sizeof(verbdata)>1 && verbdata[1]) data[1][C_MSGS]=verbdata[1];
    if(sizeof(verbdata)>2 && verbdata[2]) data[1][C_BODY]=verbdata[2];
  }
  return data;
}

/* this is the v1.0 compatible add_verb,
 * it converts everything to the new format and then adds it.
 */

void add_verb(mapping v)
{
  int e;
  mixed *a,*b;
  write("Please tell people to convert to the new soul format.\n");
  a=m_indices(v);
  b=m_values(v);
  if(sizeof(a)>60)
  {
    call_out("add_verb",2,mkmapping(a[61..10000],b[61..10000]));
    a=a[0..60];
    b=b[0..60];
  }
  for(e=0;e<sizeof(a);e++)
    b[e]=convert_feeling(a[e],b[e]);

  add_verbs(mkmapping(a,b));
}

/* The mudlib might require this */
#ifndef LPC4
void move(object x) { move_object(this_object(),x); }
#endif
int get() { return 1; }
int drop() { return 1; }
int id(string s) { return s=="soul"; }
int query_prevent_shadow() { return 1; }
int query_allow_shadow() { return 0; }
void long() { write("You can't see it.\n"); }

#ifdef LPC4
void init()
{
  add_action("%s",do_feel,-10000);
}
#else /* LPC4 */
void init()
{
  add_action("do_feel","",1);
  remove_call_out("dig");
  call_out("dig",5+random(20));
}

/* The soul blocks _a_lot_ of actions. Other objects might want a chanse
 * to look at the input before the soul does. Therefor we dig ourself
 * down into the inventory and lie last.
 */
void dig()
{
  if(present("soul 2",environment())) return;
#ifndef NATIVE
  while(this_object() && next_inventory(this_object()))
    move_object(next_inventory(this_object()),environment(this_object()));
#else
  while(this_object() && next_inventory(this_object()))
    next_inventory(this_object())->move(environment(this_object()));
#endif
}
#endif /* LPC4 */

