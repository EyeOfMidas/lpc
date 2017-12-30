/*
 * socket_efun.c -- socket efuns for MudOS.
 *    5-92 : Dwayne Fontenot (Jacques@TMI) : original coding.
 *   10-92 : Dave Richards (Cynosure) : less original coding.
 *    4-93 : Fredrik Hubinette (hubbe@lysator.liu.se) : adapted for functionpointers
 */
#include "global.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_SOCKETVAR_H
#include <sys/socketvar.h>
#endif
#ifdef _AIX
#include <sys/select.h>
#endif /* _AIX */
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include "comm.h"
#include "interpret.h"
#include "object.h"
#include "exec.h"
#include "debug.h"
#include "socket_efuns.h"
#include "socket_err.h"
#include "main.h"
#include "stralloc.h"
#include "dynamic_buffer.h"
#include "simulate.h"

extern int errno, d_flag;
extern int save_svalue_depth;
extern char *error_strings[];

struct lpc_socket lpc_socks[MAX_EFUN_SOCKS];
static int socket_name_to_sin PROT((char *, struct sockaddr_in *));

/*
 * Initialize the LPC efun socket array
 */
void init_sockets()
{
  int i;

  debug(8192,("init_sockets: initializing %d socket descriptor(s)\n",
	      MAX_EFUN_SOCKS));

  for (i = 0; i < MAX_EFUN_SOCKS; i++)
  {
    lpc_socks[i].fd = -1;
    lpc_socks[i].flags = 0;
    lpc_socks[i].mode = S_STREAM;
    lpc_socks[i].state = CLOSED;
    MEMSET((char *)&lpc_socks[i].l_addr, 0, sizeof (lpc_socks[i].l_addr));
    MEMSET((char *)&lpc_socks[i].r_addr, 0, sizeof (lpc_socks[i].r_addr));
    lpc_socks[i].owner_ob = NULL;
    lpc_socks[i].release_ob = NULL;

    SET_TO_ZERO(lpc_socks[i].read_callback);
    SET_TO_ZERO(lpc_socks[i].write_callback);
    SET_TO_ZERO(lpc_socks[i].close_callback);

    lpc_socks[i].r_buf = NULL;
    lpc_socks[i].r_off = 0;
    lpc_socks[i].r_len = 0;

    lpc_socks[i].w_buf = NULL;
    lpc_socks[i].w_off = 0;
    lpc_socks[i].w_len = 0;
  }
}

void check_svalue(struct svalue *s)
{
  if(s->type & (T_OBJECT | T_FUNCTION))
    if(s->u.ob->flags & O_DESTRUCTED)
      free_svalue(s);
}

/*
 * Create an LPC efun socket
 */
int socket_create(int mode,
              struct svalue *read_callback,
              struct svalue *close_callback)
{
  int type, i, fd, optval;

  switch (mode & S_MODE_MASK)
  {
  case S_STREAM:
    type = SOCK_STREAM;
    break;

  case S_DATAGRAM:
    type = SOCK_DGRAM;
    break;

  default:
    return EEMODENOTSUPP;
  }

  for (i = 0; i < MAX_EFUN_SOCKS; i++)
  {
    if (lpc_socks[i].state != CLOSED)
      continue;

    fd = socket(AF_INET, type, 0);
    if (fd == -1)
    {
      perror("socket_create: socket");
      return EESOCKET;
    }

    optval = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&optval,
		   sizeof (optval)) == -1)
    {
      perror("socket_create: setsockopt");
      close(fd);
      return EESETSOCKOPT;
    }

    if (set_socket_nonblocking(fd, 1) == -1)
    {
      perror("socket_create: set_socket_nonblocking");
      close(fd);
      return EENONBLOCK;
    }

    lpc_socks[i].fd = fd;
    lpc_socks[i].flags = S_HEADER;
    lpc_socks[i].mode = mode;
    lpc_socks[i].state = UNBOUND;
    MEMSET((char *)&lpc_socks[i].l_addr, 0, sizeof (lpc_socks[i].l_addr));
    MEMSET((char *)&lpc_socks[i].r_addr, 0, sizeof (lpc_socks[i].r_addr));
    lpc_socks[i].owner_ob = current_object;
    lpc_socks[i].release_ob = NULL;

    if(read_callback==NULL)
    {
      free_svalue(&lpc_socks[i].read_callback);
    }else{
      assign_svalue(&lpc_socks[i].read_callback,read_callback);
    }
    free_svalue(&lpc_socks[i].write_callback);

    if (type != SOCK_DGRAM && close_callback != NULL)
    {
      assign_svalue(&lpc_socks[i].close_callback,close_callback);
    }else{
      free_svalue(&lpc_socks[i].close_callback);
    }

    lpc_socks[i].r_buf = NULL;
    lpc_socks[i].r_off = 0;
    lpc_socks[i].r_len = 0;

    lpc_socks[i].w_buf = NULL;
    lpc_socks[i].w_off = 0;
    lpc_socks[i].w_len = 0;

    current_object->flags |= O_EFUN_SOCKET;

    debug(8192,("socket_create: created socket %d mode %d fd %d\n",
		i, mode, fd));

    return i;
  }

  return EENOSOCKS;
}
int socket_from_stdin()
{
  int i;
  
  for (i = 0; i < MAX_EFUN_SOCKS; i++)
    if (lpc_socks[i].state == CLOSED)
      break;

  if(i==MAX_EFUN_SOCKS)
    return EENOSOCKS;
  
  if (set_socket_nonblocking(0, 1) == -1)
  {
    perror("socket_create: set_socket_nonblocking");
    return EENONBLOCK;
  }

  lpc_socks[i].state = LISTEN;
  current_object->flags |= O_EFUN_SOCKET;
  lpc_socks[i].fd = 0;
  lpc_socks[i].flags = S_HEADER;
  lpc_socks[i].mode = S_STREAM;
  MEMSET((char *)&lpc_socks[i].l_addr, 0, sizeof (lpc_socks[i].l_addr));
  MEMSET((char *)&lpc_socks[i].r_addr, 0, sizeof (lpc_socks[i].r_addr));
  lpc_socks[i].owner_ob = current_object;
  lpc_socks[i].release_ob = NULL;
  
  free_svalue(&lpc_socks[i].read_callback);
  free_svalue(&lpc_socks[i].write_callback);
  free_svalue(&lpc_socks[i].close_callback);
  
  lpc_socks[i].r_buf = NULL;
  lpc_socks[i].r_off = 0;
  lpc_socks[i].r_len = 0;
  
  lpc_socks[i].w_buf = NULL;
  lpc_socks[i].w_off = 0;
  lpc_socks[i].w_len = 0;

  current_object->flags |= O_EFUN_SOCKET;
  
  return i;
}


/*
 * Bind an address to an LPC efun socket
 */
int socket_bind(int fd,int port)
{
  int len;
  struct sockaddr_in sin;

  if (fd < 0 || fd >= MAX_EFUN_SOCKS)
    return EEFDRANGE;
  if (lpc_socks[fd].state == CLOSED)
    return EEBADF;
  if (lpc_socks[fd].owner_ob != current_object)
    return EESECURITY;
  if (lpc_socks[fd].state != UNBOUND)
    return EEISBOUND;

  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons((u_short)port);

  if (bind(lpc_socks[fd].fd, (struct sockaddr *)&sin, sizeof (sin)) == -1)
  {
    switch (errno) {
    case EADDRINUSE:
      return EEADDRINUSE;
    default:
      perror("socket_bind: bind");
      return EEBIND;
    }
  }

  len = sizeof (sin);
  if (getsockname(lpc_socks[fd].fd, (struct sockaddr *)&lpc_socks[fd].l_addr,
		  &len) == -1) {
    perror("socket_bind: getsockname");
    return EEGETSOCKNAME;
  }

  lpc_socks[fd].state = BOUND;

  debug(8192,("socket_bind: bound socket %d to %s.%d\n",
	      fd, inet_ntoa(lpc_socks[fd].l_addr.sin_addr),
	      ntohs(lpc_socks[fd].l_addr.sin_port)));

  return EESUCCESS;
}

/*
 * Listen for connections on an LPC efun socket
 */
int socket_listen(int fd,struct svalue *callback)
{
    if (fd < 0 || fd >= MAX_EFUN_SOCKS)
	return EEFDRANGE;
    if (lpc_socks[fd].state == CLOSED)
	return EEBADF;
    if (lpc_socks[fd].owner_ob != current_object)
	return EESECURITY;
    if ((lpc_socks[fd].mode & S_MODE_MASK) == S_DATAGRAM)
	return EEMODENOTSUPP;
    if (lpc_socks[fd].state == UNBOUND)
	return EENOADDR;
    if (lpc_socks[fd].state != BOUND)
	return EEISCONN;

    if (listen(lpc_socks[fd].fd, 5) == -1)
    {
	perror("socket_listen: listen");
	return EELISTEN;
    }

    lpc_socks[fd].state = LISTEN;
    assign_svalue(&lpc_socks[fd].read_callback,callback);
    current_object->flags |= O_EFUN_SOCKET;
    debug(8192,("socket_listen: listen on socket %d\n", fd));
    return EESUCCESS;
}

/*
 * Accept a connection on an LPC efun socket
 */
int socket_accept(int fd,
		  struct svalue *read_callback,
                  struct svalue * write_callback)
{
    int len, accept_fd, i;
    struct sockaddr_in sin;

    if (fd < 0 || fd >= MAX_EFUN_SOCKS)
	return EEFDRANGE;
    if (lpc_socks[fd].state == CLOSED)
	return EEBADF;
    if (lpc_socks[fd].owner_ob != current_object)
	return EESECURITY;
    if ((lpc_socks[fd].mode & S_MODE_MASK) == S_DATAGRAM)
	return EEMODENOTSUPP;
    if (lpc_socks[fd].state != LISTEN)
	return EENOTLISTN;

    lpc_socks[fd].flags &= ~S_WACCEPT;

    len = sizeof (sin);
    accept_fd = accept(lpc_socks[fd].fd, (struct sockaddr *)&sin, (int *)&len);
    if (accept_fd == -1) {
	perror("socket_accept: accept");
	switch (errno) {
	case EWOULDBLOCK:
	    return EEWOULDBLOCK;
	case EINTR:
	    return EEINTR;
	default:
	    perror("socket_accept: accept");
	    return EEACCEPT;
	}
    }

    for (i = 0; i < MAX_EFUN_SOCKS; i++)
    {
	if (lpc_socks[i].state != CLOSED)
	    continue;

	lpc_socks[i].fd = accept_fd;
	lpc_socks[i].flags = S_HEADER;
	lpc_socks[i].mode = lpc_socks[fd].mode;
	lpc_socks[i].state = DATA_XFER;
	lpc_socks[i].l_addr = lpc_socks[fd].l_addr;
	lpc_socks[i].r_addr = sin;
	lpc_socks[i].owner_ob = NULL;
	lpc_socks[i].release_ob = NULL;

	free_svalue(&lpc_socks[i].read_callback);
	free_svalue(&lpc_socks[i].write_callback);
	free_svalue(&lpc_socks[i].close_callback);

	lpc_socks[i].r_buf = NULL;
	lpc_socks[i].r_off = 0;
	lpc_socks[i].r_len = 0;

	lpc_socks[i].w_buf = NULL;
	lpc_socks[i].w_off = 0;
	lpc_socks[i].w_len = 0;

	lpc_socks[i].owner_ob = current_object;
        assign_svalue(&lpc_socks[i].read_callback,read_callback);
        assign_svalue(&lpc_socks[i].write_callback,write_callback);
        assign_svalue(&lpc_socks[i].close_callback,&lpc_socks[fd].close_callback);

	current_object->flags |= O_EFUN_SOCKET;

	debug(8192,("socket_accept: accept on socket %d\n", fd));
	debug(8192,("socket_accept: new socket %d on fd %d\n", i, accept_fd));

	return i;
    }

    close(accept_fd);

    return EENOSOCKS;
}

/*
 * Connect an LPC efun socket
 */
int socket_connect(int fd,
                   char *name,
                   struct svalue *read_callback,
                   struct svalue *write_callback)
{
    if (fd < 0 || fd >= MAX_EFUN_SOCKS) return EEFDRANGE;
    if (lpc_socks[fd].state == CLOSED) return EEBADF;
    if (lpc_socks[fd].owner_ob != current_object) return EESECURITY;
    if ((lpc_socks[fd].mode & S_MODE_MASK) == S_DATAGRAM)
       return EEMODENOTSUPP;

    switch (lpc_socks[fd].state)
    {
      case CLOSED:
      case UNBOUND:
      case BOUND:
        break;

      case LISTEN:    return EEISLISTEN;
      case DATA_XFER: return EEISCONN;
    }

    if (!socket_name_to_sin(name, &lpc_socks[fd].r_addr))
	return EEBADADDR;

    assign_svalue(&lpc_socks[fd].read_callback, read_callback);
    assign_svalue(&lpc_socks[fd].write_callback, write_callback);

    current_object->flags |= O_EFUN_SOCKET;

    if (connect(lpc_socks[fd].fd, (struct sockaddr *)&lpc_socks[fd].r_addr,
	sizeof (struct sockaddr_in)) == -1)
    {
	switch (errno) {
	case EINTR:
	    return EEINTR;
	case EADDRINUSE:
	    return EEADDRINUSE;
	case EALREADY:
	    return EEALREADY;
	case ECONNREFUSED:
	    return EECONNREFUSED;
	case EINPROGRESS:
	    break;
	default:
	    perror("socket_connect: connect");
	    return EECONNECT;
	}
    }

    lpc_socks[fd].state = DATA_XFER;
    lpc_socks[fd].flags |= S_BLOCKED;

    return EESUCCESS;
}

/*
 * Write a message on an LPC efun socket
 */
int socket_write(int fd,struct svalue *message,char *name)
{
  int len, off;
  char *buf;
  struct sockaddr_in sin;

  if (fd < 0 || fd >= MAX_EFUN_SOCKS) return EEFDRANGE;
  if (lpc_socks[fd].state == CLOSED) return EEBADF;
  if (lpc_socks[fd].owner_ob != current_object) return EESECURITY;

  switch (lpc_socks[fd].mode & S_MODE_MASK)
  {
  case S_STREAM:
    if (lpc_socks[fd].state != DATA_XFER) return EENOTCONN;
    if (name != NULL) return EEBADADDR;
    if(lpc_socks[fd].flags & S_BLOCKED)
    {
      if(lpc_socks[fd].mode & S_BUFFERED_OUTPUT)
      {
	len = my_strlen(message) + lpc_socks[fd].w_len;
	buf=(char *)malloc(len+1);
	if (buf == NULL) fatal("Out of memory");
	if(lpc_socks[fd].w_buf)
	  MEMCPY(buf,lpc_socks[fd].w_buf+lpc_socks[fd].w_off,lpc_socks[fd].w_len);
	MEMCPY(buf+lpc_socks[fd].w_len,strptr(message),my_strlen(message));
	buf[len]=0;
	if(lpc_socks[fd].w_buf)
	  free(lpc_socks[fd].w_buf);

	lpc_socks[fd].w_buf = buf;
	lpc_socks[fd].w_off = 0;
	lpc_socks[fd].w_len = len;
	return 0;
      }else{
	return EEALREADY;
      }
    }

    len = my_strlen(message);
    buf = (char *)xalloc(len + 1);
    if (buf == NULL) fatal("Out of memory");
    MEMCPY(buf, strptr(message),my_strlen(message));
    break;

  case S_DATAGRAM:
    if(name == NULL) return EENOADDR;
    if(!socket_name_to_sin(name, &sin)) return EEBADADDR;

    if (sendto(lpc_socks[fd].fd, strptr(message),
	       my_strlen(message) + 1, 0,
	       (struct sockaddr *)&sin, sizeof (sin)) == -1)
    {
      perror("socket_write: sendto");
      return EESENDTO;
    }
    return EESUCCESS;


  default:
    return EEMODENOTSUPP;
  }

  off = send(lpc_socks[fd].fd, buf,len, 0);
  if (off == -1)
  {
    switch (errno)
    {

    case EWOULDBLOCK:
      break;

    default:
      free(buf);
      perror("socket_write: send");
      return EESEND;
    }
  }

  if (off < len)
  {
    lpc_socks[fd].flags |= S_BLOCKED;
    lpc_socks[fd].w_buf = buf;
    lpc_socks[fd].w_off = off;
    lpc_socks[fd].w_len = len - off;
    return EECALLBACK;
  }

  free(buf);

  return EESUCCESS;
}

void parse_one_line(int fd)
{
  int e;
  char *s;

  if((lpc_socks[fd].mode & S_MODE_MASK) != S_STREAM) return;
  if(!(lpc_socks[fd].mode & S_BUFFERED_INPUT)) return;
  if(lpc_socks[fd].r_buf == NULL)
  {
    lpc_socks[fd].flags&=~S_MORE_LINES;
    return;
  }

  if(lpc_socks[fd].r_len>0 && lpc_socks[fd].r_buf)
  {
    s=lpc_socks[fd].r_buf+lpc_socks[fd].r_off;
    for(e=0;e<lpc_socks[fd].r_len;e++)
    {
      if(s[e]=='\n')
      {
	e++;
	break;
      }
    }
    lpc_socks[fd].r_off+=e;
    lpc_socks[fd].r_len-=e;
    push_number(fd);
    push_shared_string(make_shared_binary_string(s,e));
    lpc_socks[fd].flags |= S_MORE_LINES;
    debug(8192,("read_socket_handler: apply read callback\n"));
    check_svalue(&lpc_socks[fd].read_callback);
    apply_lambda(&lpc_socks[fd].read_callback, 2,1);
  }
  if(lpc_socks[fd].state==CLOSED || (lpc_socks[fd].flags & S_CLOSING))
    return;

  if(lpc_socks[fd].r_len<1)
  {
    lpc_socks[fd].flags &= ~S_MORE_LINES;
    if(lpc_socks[fd].r_buf)
      free(lpc_socks[fd].r_buf);
    lpc_socks[fd].r_buf=NULL;
  }
}

void handle_line_sockets()
{
  int i,s;
  do{
    s=0;
    for(i=0;i<MAX_EFUN_SOCKS;i++)
    {
      if((lpc_socks[i].mode & S_MODE_MASK) != S_STREAM) continue;
      if(!(lpc_socks[i].mode & S_BUFFERED_INPUT)) continue;
      if(lpc_socks[i].flags & S_MORE_LINES)
      {
        parse_one_line(i);
        s=1;
      }
    }
  }while(s);
}

/*
 * Handle LPC efun socket read select events
 */
void socket_read_select_handler(int fd)
{
  int cc=-1; /* make gcc happy */
  int addrlen;
  char buf[BUF_SIZE], addr[ADDR_BUF_SIZE];
  struct sockaddr_in sin;

  debug(8192,("read_socket_handler: fd %d state %d\n",
	      fd, lpc_socks[fd].state));

  switch (lpc_socks[fd].state)
  {
  case CLOSED:
  case UNBOUND:
    return;

  case BOUND:
    switch (lpc_socks[fd].mode & S_MODE_MASK)
    {
    case S_STREAM:
      break;

    case S_DATAGRAM:
      debug(8192,("read_socket_handler: DATA_XFER DATAGRAM\n"));
      addrlen = sizeof (sin);
      cc = recvfrom(lpc_socks[fd].fd, buf, sizeof (buf), 0,
		    (struct sockaddr *)&sin, &addrlen);
      if (cc <= 0)
	break;
      debug(8192,("read_socket_handler: read %d bytes\n", cc));
      sprintf(addr, "%s %d", inet_ntoa(sin.sin_addr),
	      ntohs(sin.sin_port));
      push_number(fd);
      push_shared_string(make_shared_binary_string(buf, cc));
      push_new_shared_string(addr);
      debug(8192,("read_socket_handler: apply\n"));
      check_svalue(&lpc_socks[fd].read_callback);
      apply_lambda(&lpc_socks[fd].read_callback, 3,1);
      return;
    }
    break;

  case LISTEN:
    debug(8192,("read_socket_handler: apply read callback\n"));
    lpc_socks[fd].flags |= S_WACCEPT;
    push_number(fd);
    check_svalue(&lpc_socks[fd].read_callback);
    apply_lambda(&lpc_socks[fd].read_callback, 1,1);
    return;

  case DATA_XFER:
    switch (lpc_socks[fd].mode & S_MODE_MASK)
    {

    case S_DATAGRAM:
      break;
            
    case S_STREAM:
      if(lpc_socks[fd].mode & S_BUFFERED_INPUT)
      { 
	char *s;
	int e;
    
	debug(8192,("read_socket_handler: DATA_XFER STREAM (buffered)\n"));
	cc = recv(lpc_socks[fd].fd, buf, sizeof (buf), 0);
	if (cc <= 0) break;
	debug(8192,("read_socket_handler: read %d bytes\n", cc));
    
	if(lpc_socks[fd].r_buf != NULL)
	{
	  e=lpc_socks[fd].r_len+cc;
	  s=(char *)malloc(e+1);
	  strncpy(s,lpc_socks[fd].r_buf+lpc_socks[fd].r_off,
		  lpc_socks[fd].r_len);
	  strncpy(s+lpc_socks[fd].r_len,buf,cc);
	  free(lpc_socks[fd].r_buf);
	  lpc_socks[fd].r_buf=s;
	  lpc_socks[fd].r_len=e;
	  lpc_socks[fd].r_off=0;
	}else{
	  s=(char *)malloc(cc);
	  strncpy(s,buf,cc);
	  lpc_socks[fd].r_buf=s;
	  lpc_socks[fd].r_len=cc;
	  lpc_socks[fd].r_off=0;
	}
	lpc_socks[fd].flags |=S_MORE_LINES;
	return;
      }
      debug(8192,("read_socket_handler: DATA_XFER STREAM\n"));
      cc = recv(lpc_socks[fd].fd, buf, sizeof (buf), 0);
      if (cc <= 0)
	break;
      debug(8192,("read_socket_handler: read %d bytes\n", cc));
      push_number(fd);
      push_shared_string(make_shared_binary_string(buf, cc));
      debug(8192,("read_socket_handler: apply read callback\n"));
      check_svalue(&lpc_socks[fd].read_callback);
      apply_lambda(&lpc_socks[fd].read_callback, 2,1);
      return;
    }
    break;
  }
  if(cc == -1)
  {
    switch(errno)
    {
    case EINTR:
    case EWOULDBLOCK:
      return;
    }
  }
  debug(8192,("read_socket_handler: apply close callback\n"));
  push_number(fd);
  check_svalue(&lpc_socks[fd].close_callback);
  safe_apply_lambda(&lpc_socks[fd].close_callback, 1);

  socket_close(fd,1);
}

/*
 * Handle LPC efun socket write select events
 */
void socket_write_select_handler(int fd)
{
  int cc;

  debug(8192,("write_socket_handler: fd %d state %d\n",
	      fd, lpc_socks[fd].state));

  if ((lpc_socks[fd].flags & S_BLOCKED) == 0)
    return;

  if (lpc_socks[fd].w_buf != NULL)
  {
    cc = send(lpc_socks[fd].fd, lpc_socks[fd].w_buf + lpc_socks[fd].w_off,
	      lpc_socks[fd].w_len, 0);
    if (cc == -1)
    {
#if 1
      switch(errno)
      {
      case EINTR:
      case EWOULDBLOCK:
	return;
      }
      debug(8192,("read_socket_handler: apply close callback\n"));
      push_number(fd);
      check_svalue(&lpc_socks[fd].close_callback);
      safe_apply_lambda(&lpc_socks[fd].close_callback, 1);

      socket_close(fd,2);
#endif
      return;
    }
    lpc_socks[fd].w_off += cc;
    lpc_socks[fd].w_len -= cc;
    if (lpc_socks[fd].w_len != 0)
      return;
    free(lpc_socks[fd].w_buf);
    lpc_socks[fd].w_buf = NULL;
    lpc_socks[fd].w_off = 0;
  }

  lpc_socks[fd].flags &= ~S_BLOCKED;

  if(lpc_socks[fd].flags & S_CLOSING)
  {
    socket_close(fd,2);
    return;
  }

  debug(8192,("write_socket_handler: apply write_callback\n"));

  push_number(fd);
  check_svalue(&lpc_socks[fd].write_callback);
  apply_lambda(&lpc_socks[fd].write_callback, 1,1);
}

/*
 * Close an LPC efun socket
 */
int socket_close(int fd,int force)
{
  if(d_flag>8)
    printf("so_close: %d\n",fd);
  if (fd < 0 || fd >= MAX_EFUN_SOCKS)
    return EEFDRANGE;
  if (lpc_socks[fd].state == CLOSED)
    return EEBADF;
  if (!force && lpc_socks[fd].owner_ob != current_object)
    return EESECURITY;

  if ((lpc_socks[fd].mode & S_MODE_MASK) != S_DATAGRAM)
  {
    if(!(lpc_socks[fd].flags & S_CLOSING))
      shutdown(lpc_socks[fd].fd, 0);
  }
  lpc_socks[fd].flags|=S_CLOSING;

  if((lpc_socks[fd].flags & S_BLOCKED) && force<2)
  {
    lpc_socks[fd].flags|=S_CLOSING;
    return EEALREADY;
  }

  while (close(lpc_socks[fd].fd) == -1 && errno == EINTR); /* empty while */
  lpc_socks[fd].state = CLOSED;

  if (lpc_socks[fd].r_buf != NULL) free(lpc_socks[fd].r_buf);
  lpc_socks[fd].r_buf=NULL;
  if (lpc_socks[fd].w_buf != NULL) free(lpc_socks[fd].w_buf);
  lpc_socks[fd].w_buf=NULL;

  free_svalue(&lpc_socks[fd].read_callback);
  free_svalue(&lpc_socks[fd].write_callback);
  free_svalue(&lpc_socks[fd].close_callback);

  debug(8192,("socket_close: closed fd %d\n", fd));
  return EESUCCESS;
}

/*
 * Release an LPC efun socket to another object
 */
int socket_release(int fd,struct svalue *callback)
{
    if (fd < 0 || fd >= MAX_EFUN_SOCKS)
	return EEFDRANGE;
    if (lpc_socks[fd].state == CLOSED)
	return EEBADF;
    if (lpc_socks[fd].owner_ob != current_object)
	return EESECURITY;
    if (lpc_socks[fd].flags & S_RELEASE)
	return EESOCKRLSD;

    lpc_socks[fd].flags |= S_RELEASE;
    lpc_socks[fd].release_ob = callback->u.ob;

    push_number(fd);
    push_object(callback->u.ob);
    safe_apply_lambda(callback, 2);

    if ((lpc_socks[fd].flags & S_RELEASE) == 0)
	return EESUCCESS;

    lpc_socks[fd].flags &= ~S_RELEASE;
    lpc_socks[fd].release_ob = NULL;

    return EESOCKNOTRLSD;
}

/*
 * Aquire an LPC efun socket from another object
 */
int socket_acquire(int fd,
	struct svalue *read_callback,
	struct svalue *write_callback,
	struct svalue *close_callback)
{
    if (fd < 0 || fd >= MAX_EFUN_SOCKS)
	return EEFDRANGE;
    if (lpc_socks[fd].state == CLOSED)
        return EEBADF;
    if ((lpc_socks[fd].flags & S_RELEASE) == 0)
	return EESOCKNOTRLSD;
    if (lpc_socks[fd].release_ob != current_object)
	return EESECURITY;

    lpc_socks[fd].flags &= ~S_RELEASE;
    lpc_socks[fd].owner_ob = current_object;
    lpc_socks[fd].release_ob = NULL;

    
    assign_svalue(&lpc_socks[fd].read_callback,read_callback);
    assign_svalue(&lpc_socks[fd].write_callback,write_callback);
    assign_svalue(&lpc_socks[fd].close_callback,close_callback);

    return EESUCCESS;
}

/*
 * Return the string representation of a socket error
 */
char *socket_error(int error)
{
    error = -(error + 1);
    if (error < 0 || error >= ERROR_STRINGS)
	return "socket_error: invalid error number";
    return error_strings[error];
}

/*
 * Return the current socket owner
 */
struct object *get_socket_owner(int fd)
{
  if (fd < 0 || fd >= MAX_EFUN_SOCKS)
    return (struct object *)NULL;
  if (lpc_socks[fd].state == CLOSED)
    return (struct object *)NULL;
  return lpc_socks[fd].owner_ob;
}

/*
 * Initialize a T_OBJECT svalue
 */
void assign_socket_owner(struct svalue *sv,struct object *ob)
{
    if (ob != NULL)
    {
      sv->type = T_OBJECT;
      sv->u.ob = ob;
      add_ref(ob, "assign_socket_owner");
    }else{
      SET_TO_ZERO(*sv);
    }
}

/*
 * Convert a string representation of an address to a sockaddr_in
 */
static int socket_name_to_sin(char *name,struct sockaddr_in *sin)
{
    int port;
    char *cp, addr[ADDR_BUF_SIZE];

    strncpy(addr, name, ADDR_BUF_SIZE);

    cp = STRCHR(addr, ' ');

    if (cp == NULL) return 0;

    *cp = '\0';
    port = atoi(cp + 1);

    sin->sin_family = AF_INET;
    sin->sin_port = htons((u_short)port);
    sin->sin_addr.s_addr = inet_addr(addr);

    return 1;
}

/*
 * Close any sockets owned by ob
 */
void close_referencing_sockets(struct object *ob)
{
  int i;
  struct object *save_current_object;

  save_current_object = current_object;

  current_object = ob;
  for (i = 0; i < MAX_EFUN_SOCKS; i++)
    if (lpc_socks[i].owner_ob == ob && lpc_socks[i].state != CLOSED)
      socket_close(i,1);

  current_object = save_current_object;
}

/*
 * Return the remote address for an LPC efun socket
 */
int get_socket_address(int fd,char *addr,int *port)
{
  if (fd < 0 || fd >= MAX_EFUN_SOCKS)
  {
    addr[0] = '\0';
    *port = 0;
    return EEFDRANGE;
  }
  *port = (int)ntohs(lpc_socks[fd].r_addr.sin_port);
  sprintf(addr, "%s", inet_ntoa(lpc_socks[fd].r_addr.sin_addr));
  return EESUCCESS;
}

/*
 * Return the string representation of a sockaddr_in
 */
static char *inet_address(struct sockaddr_in *sin)
{
  static char addr[50], port[7];

  if (ntohl(sin->sin_addr.s_addr) == INADDR_ANY)
  {
    strcpy(addr, "*");
  }else{
    strncpy(addr, inet_ntoa(sin->sin_addr),sizeof(addr)-7);
  }

  strcat(addr, ".");

  if (ntohs(sin->sin_port) == 0) strcpy(port, "*");
  else sprintf(port, "%d", ntohs(sin->sin_port));

  strcat(addr, port);
  return addr;
}



/*
 * Dump the LPC efun socket array
 */
char *dump_socket_status()
{
  int i;
  char b[100];

  init_buf();

  my_strcat("Fd    State      Mode       Local Address          Remote Address\n");
  my_strcat("--  ---------  --------  ---------------------  ---------------------\n");

  for(i = 0; i < MAX_EFUN_SOCKS; i++)
  {
    sprintf(b,"%2d  ", lpc_socks[i].fd);
    my_strcat(b);

    switch(lpc_socks[i].state){
    case CLOSED:
      my_strcat("CLOSED ");
      break;
    case UNBOUND:
      my_strcat("UNBOUND");
      break;
    case BOUND:
      my_strcat(" BOUND ");
      break;
    case LISTEN:
      my_strcat("LISTEN ");
      break;
    case DATA_XFER:
      my_strcat("DTXFER ");
      break;
    default:
      my_strcat("    ??    ");
      break;
    }
    my_putchar(' ');

    switch(lpc_socks[i].mode & S_MODE_MASK){
    case S_STREAM:
      my_strcat("STREAM");
      break;
    case S_DATAGRAM:
      my_strcat("DGRAM ");
      break;
    default:
      my_strcat("  ??  ");
      break;
    }
    my_putchar(' ');

    if(lpc_socks[i].mode & S_BUFFERED_INPUT)
    {
      my_putchar('I');
    }else{
      my_putchar('.');
    }

    if(lpc_socks[i].mode & S_BUFFERED_OUTPUT)
    {
      my_putchar('O');
    }else{
      my_putchar('.');
    }
    my_putchar(' ');

    sprintf(b,"%-21s  ", inet_address(&lpc_socks[i].l_addr));
    my_strcat(b);
    sprintf(b,"%-21s\n", inet_address(&lpc_socks[i].r_addr));
    my_strcat(b);
  }
  return free_buf();
}
