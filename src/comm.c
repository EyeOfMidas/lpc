/*
 *  comm.c -- communications functions and more.
 *            Dwayne Fontenot (Jacques@TMI)
 */
#include "global.h"
/* #include <varargs.h> */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#define TELOPTS
#include <arpa/telnet.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_SOCKETVAR_H
#include <sys/socketvar.h>
#endif

#include "interpret.h"
#include "comm.h"
#include "socket_efuns.h"
#include "object.h"
#include "sent.h"
#include "debug.h"
#include "comm1.h"
#include "main.h"
#include "stralloc.h"
#include "simulate.h"
#include "exec.h"

/*
 * external function prototypes.
 */
extern char *xalloc(), *string_copy(), *unshared_str_copy();
extern int parse_command();
extern void call_heart_beat();

int total_users = 0;

/*
 * local function prototypes.
 */
RETSIGTYPE sigpipe_handler();
RETSIGTYPE sigalrm_handler();
void hname_handler();
void stdin_handler();

char *query_host_name();


/*
 * external variables.
 */
extern int port_number;
extern int errno;
extern int d_flag;
extern int current_time;
extern struct object *command_giver;
extern struct lpc_socket lpc_socks[];
extern int heart_beat_flag;
extern char *default_fail_message;
int stdin_closed=0;
int stdinisatty=0;
int stdin_is_sock=0;

/*
 * public local variables.
 */
fd_set readmask, writemask;

/*
 * SIGALRM handler.
 */
RETSIGTYPE sigalrm_handler()
{
  heart_beat_flag = 1;
  debug(512,("sigalrm_handler: SIGALRM\n"));
}

INLINE void make_selectmasks()
{
  int i;

  /*
   * generate readmask and writemask for select() call.
   */
  FD_ZERO(&readmask);
  FD_ZERO(&writemask);
  /*
   * set new user accept fd in readmask.
   */
  if(!stdin_closed)
    FD_SET(0,&readmask);

  /*
   * set fd's for efun sockets.
   */
  for(i=0;i<MAX_EFUN_SOCKS;i++)
  {
    if(lpc_socks[i].state != CLOSED)
    {
      if (!(lpc_socks[i].flags & (S_WACCEPT|S_CLOSING)))
	FD_SET(lpc_socks[i].fd,&readmask);
      if (lpc_socks[i].flags & S_BLOCKED)
	FD_SET(lpc_socks[i].fd,&writemask);
    }
  }
}

/*
 * Process I/O.
 */
INLINE void process_io()
{
  int i;

  debug(256,("@"));
  /*
   * check for data pending on efun socket connections.
   */
  for(i=0;i<MAX_EFUN_SOCKS;i++)
  {
    if(lpc_socks[i].state != CLOSED)
      if(FD_ISSET(lpc_socks[i].fd,&readmask))
	socket_read_select_handler(i);

    if(lpc_socks[i].state != CLOSED)
      if(FD_ISSET(lpc_socks[i].fd,&writemask))
	socket_write_select_handler(i);

  }
  handle_line_sockets();

  if(!stdin_closed && FD_ISSET(0,&readmask))
  {
    stdin_handler();
  }
}

#define STDIN_BUF_SIZE 1000
void init_stdin_handler()
{
  stdinisatty=isatty(0);
}

void stdin_handler()
{
  char buf[STDIN_BUF_SIZE];
  int num_bytes;

  if(stdinisatty)
  {
    if(set_socket_nonblocking(0, 1) == -1){
      perror("stdin_handler: set_socket_nonblocking 1");
      return;
    }
  }
  num_bytes = read(0,buf,STDIN_BUF_SIZE);
  if(stdinisatty)
  {
    if(set_socket_nonblocking(0, 0) == -1)
    {
      perror("stdin_handler: set_socket_nonblocking 0");
      return;
    }
  }
  switch(num_bytes)
  {
  case -1:
    switch(errno){
    case EWOULDBLOCK:
      debug(512,("Stdin: Operation would block.\n"));
      break;
    default:
      perror("Stdin read error");
      return;
    }
    break;
  case 0:
    close(0);
    stdin_closed=1;
    break;

  default:
    buf[num_bytes] = '\0';
    push_new_shared_string(buf);
    APPLY_MASTER_OB((void),"stdin",1);
  }
}

#ifndef INET_NTOA_OK
/*
 * Note: if the address string is "a.b.c.d" the address number is
 *       a * 256^3 + b * 256^2 + c * 256 + d
 */
char *inet_ntoa(struct in_addr ad)
{
  u_long s_ad;
  int a, b, c, d;
  static char addr[20]; /* 16 + 1 should be enough */

  s_ad = ad.s_addr;
  d = s_ad % 256;
  s_ad /= 256;
  c = s_ad % 256;
  s_ad /= 256;
  b = s_ad % 256;
  a = s_ad / 256;
  sprintf(addr, "%d.%d.%d.%d", a, b, c, d);
  return(addr);
}
#endif /* INET_NTOA_OK */



