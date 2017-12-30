/* DO NOT INCLUDE OR INHERIT, USE #include "ctype.h" !!!!!!! */

#include "ctype.h"
#define TELCMDS
#define TELOPTS
#include "arpa/telnet.h"

string *get_telcmds()
{
  if(telcmds) return telcmds;
  return ({
	"SE", "NOP", "DMARK", "BRK", "IP", "AO", "AYT", "EC",
	"EL", "GA", "SB", "WILL", "WONT", "DO", "DONT", "IAC",
      });
}

string *get_telopts()
{
  if(telopts) return telopts;
  return ({
	"BINARY", "ECHO", "RCP", "SUPPRESS GO AHEAD", "NAME",
	"STATUS", "TIMING MARK", "RCTE", "NAOL", "NAOP",
	"NAOCRD", "NAOHTS", "NAOHTD", "NAOFFD", "NAOVTS",
	"NAOVTD", "NAOLFD", "EXTEND ASCII", "LOGOUT", "BYTE MACRO",
	"DATA ENTRY TERMINAL", "SUPDUP", "SUPDUP OUTPUT",
	"SEND LOCATION", "TERMINAL TYPE", "END OF RECORD",
});

}


#define AN  __CF_ISALNUM
#define AL  __CF_ISALPHA
#define CT  __CF_ISCNTRL
#define NU  __CF_ISDIGIT
#define GR  __CF_ISGRAPH
#define LO  __CF_ISLOWER
#define SP  __CF_ISSPACE
#define UP  __CF_ISUPPER

#define PU  __CF_ISPUNCT
#define XD  __CF_ISXDIGIT
#define PR  __CF_ISPRINT

int *__get_CAry()
{
  if(__CAry) return __CAry;
  return ({
	CT,
	CT,
	CT,
	CT,
	CT,
	CT,
	CT,
	CT,
	CT,
	CT|SP,
	CT|SP,
	CT|SP,
	CT|SP,
	CT|SP,
	CT,
	CT,
	CT,
	CT,
	CT,
	CT,
	CT,
	CT,
	CT,
	CT,
	CT,
	CT,
	CT,
	CT,
	CT,
	CT,
	CT,
	CT,
	SP|PR,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|NU|AN|PR|XD,
	GR|NU|AN|PR|XD,
	GR|NU|AN|PR|XD,
	GR|NU|AN|PR|XD,
	GR|NU|AN|PR|XD,
	GR|NU|AN|PR|XD,
	GR|NU|AN|PR|XD,
	GR|NU|AN|PR|XD,
	GR|NU|AN|PR|XD,
	GR|NU|AN|PR|XD,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|AN|AL|UP|PR|XD,
	GR|AN|AL|UP|PR|XD,
	GR|AN|AL|UP|PR|XD,
	GR|AN|AL|UP|PR|XD,
	GR|AN|AL|UP|PR|XD,
	GR|AN|AL|UP|PR|XD,
	GR|AN|AL|UP|PR,
	GR|AN|AL|UP|PR,
	GR|AN|AL|UP|PR,
	GR|AN|AL|UP|PR,
	GR|AN|AL|UP|PR,
	GR|AN|AL|UP|PR,
	GR|AN|AL|UP|PR,
	GR|AN|AL|UP|PR,
	GR|AN|AL|UP|PR,
	GR|AN|AL|UP|PR,
	GR|AN|AL|UP|PR,
	GR|AN|AL|UP|PR,
	GR|AN|AL|UP|PR,
	GR|AN|AL|UP|PR,
	GR|AN|AL|UP|PR,
	GR|AN|AL|UP|PR,
	GR|AN|AL|UP|PR,
	GR|AN|AL|UP|PR,
	GR|AN|AL|UP|PR,
	GR|AN|AL|UP|PR,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|AN|AL|LO|PR|XD,
	GR|AN|AL|LO|PR|XD,
	GR|AN|AL|LO|PR|XD,
	GR|AN|AL|LO|PR|XD,
	GR|AN|AL|LO|PR|XD,
	GR|AN|AL|LO|PR|XD,
	GR|AN|AL|LO|PR,
	GR|AN|AL|LO|PR,
	GR|AN|AL|LO|PR,
	GR|AN|AL|LO|PR,
	GR|AN|AL|LO|PR,
	GR|AN|AL|LO|PR,
	GR|AN|AL|LO|PR,
	GR|AN|AL|LO|PR,
	GR|AN|AL|LO|PR,
	GR|AN|AL|LO|PR,
	GR|AN|AL|LO|PR,
	GR|AN|AL|LO|PR,
	GR|AN|AL|LO|PR,
	GR|AN|AL|LO|PR,
	GR|AN|AL|LO|PR,
	GR|AN|AL|LO|PR,
	GR|AN|AL|LO|PR,
	GR|AN|AL|LO|PR,
	GR|AN|AL|LO|PR,
	GR|AN|AL|LO|PR,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	GR|PR|PU,
	CT     
  });
}

