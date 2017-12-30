/*
  ioctl.c: part of the MudOS release -- Truilkan@TMI

  isolates the code which sets the various socket modes since various
  machines seem to need this done in different ways.
*/

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#ifndef TESTING
#include "global.h"
#include "socket_efuns.h"
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

#ifndef INLINE
#define INLINE
#endif

/*
 * set process receiving SIGIO/SIGURG signals to us.
 */

INLINE int set_socket_owner(int fd,int which)
{
  return ioctl(fd, SIOCSPGRP, &which);
}

/*
 * allow receipt of asynchronous I/O signals.
 */

INLINE int set_socket_async(int fd,int which)
{
  return ioctl(fd, FIOASYNC, &which);
}

/*
 * set socket non-blocking
 */

INLINE int set_socket_nonblocking(int fd,int which)
{
#ifdef USE_IOCTL_FIONBIO
  return ioctl(fd, FIONBIO, &which);
#else

#ifdef USE_FCNTL_O_NDELAY
  return fcntl(fd, F_SETFL, which?O_NDELAY:0);
#else

#ifdef USE_FCNTL_O_NONBLOCK
  return fcntl(fd, F_SETFL, which?O_NONBLOCK:0);
#else

#ifdef USE_FCNTL_FNDELAY
  return fcntl(fd, F_SETFL, which?FNDELAY:0);
#else
INSERT YOUR NONBLOCK METHOD HERE
#endif
#endif
#endif
#endif

}

#ifdef TESTING

#include <signal.h>
/* a part of the autoconf thingy */

RETSIGTYPE sigalrm_handler0(int tmp) { exit(0); }
RETSIGTYPE sigalrm_handler1(int tmp) { exit(1); }

main()
{
  char foo[1000];
  if(set_socket_nonblocking(0,1)==-1) exit(1);
  signal(SIGALRM, sigalrm_handler1);
  alarm(1);
  read(0,foo,999);
  if(set_socket_nonblocking(0,0)==-1) exit(1);
  signal(SIGALRM, sigalrm_handler0);
  alarm(1);
  read(0,foo,999);
  exit(1);
}
#endif
