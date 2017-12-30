#include "efuns.h"
#include "array.h"
#include "comm.h"
#include "socket_err.h"
#include "socket_efuns.h"
#include "simulate.h"
#include "stralloc.h"

extern struct lpc_socket lpc_socks[MAX_EFUN_SOCKS];

/* define this to make socket_write slower */
/* #define PARANOID_SECURITY */

/* obs: this might be useful in the lpc code: */
/* #define NULL (lambda() {}) */

static int i;
#ifdef F_SOCKET_CREATE
void f_socket_create(int num_arg,struct svalue *argp)
{
  int fd;
  struct svalue *ret;
  struct vector *info;

  if(num_arg==3) check_argument(2,T_FUNCTION,F_SOCKET_CREATE);

  info = allocate_array(4);
  info->ref--;
  info->item[0].type = T_NUMBER;
  info->item[0].u.number = -1;
  assign_socket_owner(info->item + 1, current_object);

  info->item[3].type = T_NUMBER;
  info->item[3].u.number = -1;

  push_object(current_object);
  push_new_shared_string("create");
  push_vector(info);

  APPLY_MASTER_OB(ret=,"valid_socket", 3);
  if (!IS_ZERO(ret))
  {
    switch(num_arg)
    {
    default: /* make gcc happy */
    case 3:
      fd = socket_create(argp[0].u.number, argp+1, argp+2);
      break;

    case 2:
      fd = socket_create(argp[0].u.number, NULL, argp+1);
      break;

    case 1:
      fd = socket_create(argp[0].u.number, NULL, NULL);
      break;
    }
  } else {
    fd=EESECURITY;
  }
  pop_n_elems(num_arg);
  push_number(fd);	
}
#endif

#ifdef F_SOCKET_BIND
void f_socket_bind(int num_arg,struct svalue *argp)
{
  int fd, port;
  struct svalue *ret;
  struct vector *info;
  char addr[ADDR_BUF_SIZE];

  fd = argp[0].u.number;
  get_socket_address(fd, addr, &port);

  info = allocate_array(4);
  info->ref--;
  info->item[0].type = T_NUMBER;
  info->item[0].u.number = fd;
  assign_socket_owner(&info->item[1], get_socket_owner(fd));

  SET_STR(info->item,make_shared_string(addr));

  info->item[3].type = T_NUMBER;
  info->item[3].u.number = port;

  push_object(current_object);
  push_new_shared_string("bind");
  push_vector(info);

  APPLY_MASTER_OB(ret=,"valid_socket", 3);
  if (!IS_ZERO(ret))
  {
    i = socket_bind(fd, sp->u.number);
  } else {
    i=EESECURITY;
  }
  pop_n_elems(2);		/* pop both args off stack    */
  push_number(i);		/* push return int onto stack */
}
#endif
  
#ifdef F_SOCKET_LISTEN
void f_socket_listen(int num_arg,struct svalue *argp)
{
  int fd, port;
  struct svalue *ret;
  struct vector *info;
  char addr[ADDR_BUF_SIZE];
  int err;

  fd = argp[0].u.number;
  err=get_socket_address(fd, addr, &port);
  if(err<0)
  {
    pop_n_elems(2);
    push_number(err);
    return;
  }

  info = allocate_array(4);
  info->ref--;
  info->item[0].type = T_NUMBER;
  info->item[0].u.number = fd;
  assign_socket_owner(&info->item[1], get_socket_owner(fd));

  SET_STR(info->item,make_shared_string(addr));

  info->item[3].type = T_NUMBER;
  info->item[3].u.number = port;

  push_object(current_object);
  push_new_shared_string("listen");
  push_vector(info);

  APPLY_MASTER_OB(ret=,"valid_socket",3);
  if (!IS_ZERO(ret)) {
    i = socket_listen(fd, argp+1);
  } else {
    i=EESECURITY;
  }
  pop_n_elems(2);		/* pop both args off stack    */
  push_number(i);		/* push return int onto stack */
}
#endif

#ifdef F_SOCKET_ACCEPT
void f_socket_accept(int num_arg,struct svalue *argp)
{
  int fd, port;
  struct svalue *ret;
  struct vector *info;
  char addr[ADDR_BUF_SIZE];
  int err;  

  check_argument(2,T_FUNCTION,F_SOCKET_ACCEPT);

  fd = argp[0].u.number;
  err=get_socket_address(fd, addr, &port);
  if(err<0)
  {
    pop_n_elems(3);
    push_number(err);
    return;
  }

  info = allocate_array(4);
  info->ref--;
  info->item[0].type = T_NUMBER;
  info->item[0].u.number = fd;
  assign_socket_owner(&info->item[1], get_socket_owner(fd));

  SET_STR(info->item,make_shared_string(addr));

  info->item[3].type = T_NUMBER;
  info->item[3].u.number = port;

  push_object(current_object);
  push_new_shared_string("accept");
  push_vector(info);

  APPLY_MASTER_OB(ret=,"valid_socket", 3);
  if (!IS_ZERO(ret))
  {
    i = socket_accept(fd, argp+1,argp+2);
  }else{
    i=EESECURITY;
  }
  pop_n_elems(3);		/* pop both args off stack    */
  push_number(i);		/* push return int onto stack */
}
#endif

#ifdef F_SOCKET_CONNECT
void f_socket_connect(int num_arg,struct svalue *argp)
{
  int fd, port;
  struct svalue *ret;
  struct vector *info;
  char addr[ADDR_BUF_SIZE];
  int err;

  check_argument(2,T_FUNCTION,F_SOCKET_CONNECT);
  check_argument(3,T_FUNCTION,F_SOCKET_CONNECT);

  fd = argp[0].u.number;
  err=get_socket_address(fd, addr, &port);
  if(err<0)
  {
    pop_n_elems(3);
    push_number(err);
    return;
  }

  info = allocate_array(4);
  info->ref--;
  info->item[0].type = T_NUMBER;
  info->item[0].u.number = fd;
  assign_socket_owner(&info->item[1], get_socket_owner(fd));

  SET_STR(info->item,make_shared_string(addr));

  info->item[3].type = T_NUMBER;
  info->item[3].u.number = port;

  push_object(current_object);
  push_new_shared_string("connect");
  push_vector(info);

  APPLY_MASTER_OB(ret=,"valid_socket", 3);
  if (!IS_ZERO(ret))
  {
    i = socket_connect(fd, strptr(argp+1), argp+2,argp+3);
  } else {
    i=EESECURITY;
  }
  pop_n_elems(4);		/* pop all args off stack     */
  push_number(i);		/* push return int onto stack */
}
#endif

#ifdef F_SOCKET_WRITE
void f_socket_write(int num_arg,struct svalue *argp)
{
  int fd, port;
  char addr[ADDR_BUF_SIZE];
  int err;

  if(num_arg==3)
  {
    check_argument(2,T_STRING,F_SOCKET_WRITE);
  }

  fd = argp[0].u.number;
  err=get_socket_address(fd, addr, &port);
  if(err<0)
  {
    pop_n_elems(3);
    push_number(err);
    return;
  }

#ifdef PARANOID_SECURITY
  get_socket_address(fd, addr, &port);

  info = allocate_array(4);
  info->ref--;
  info->item[0].type = T_NUMBER;
  info->item[0].u.number = fd;
  assign_socket_owner(&info->item[1], get_socket_owner(fd));

  SET_STR(info->item,make_shared_string(addr));

  info->item[3].type = T_NUMBER;
  info->item[3].u.number = port;

  push_object(current_object);
  push_new_shared_string("write");
  push_vector(info);

  APPLY_MASTER_OB(ret=,"valid_socket", 3);
  if (!IS_ZERO(ret))
  {
    i = socket_write(fd, argp+1,
		     (num_arg == 3) ? strptr(argp+2) : (char *)NULL);
  } else {
    i=EESECURITY;
  }
#else
  i = socket_write(fd, argp+1,
		   (num_arg == 3) ? strptr(argp+2) : (char *)NULL);

#endif
  pop_n_elems(num_arg);
  push_number(i);		/* push return int onto stack */
}
#endif

#ifdef F_SOCKET_CLOSE
void f_socket_close(int num_arg,struct svalue *argp)
{
  int fd, port;
  struct svalue *ret;
  struct vector *info;
  char addr[ADDR_BUF_SIZE];
  int err;

  fd = argp[0].u.number;
  err=get_socket_address(fd, addr, &port);
  if(err<0)
  {
    pop_n_elems(1);
    push_number(err);
    return;
  }

  info = allocate_array(4);
  info->ref--;
  info->item[0].type = T_NUMBER;
  info->item[0].u.number = fd;
  assign_socket_owner(&info->item[1], get_socket_owner(fd));

  SET_STR(info->item,make_shared_string(addr));

  info->item[3].type = T_NUMBER;
  info->item[3].u.number = port;

  push_object(current_object);
  push_new_shared_string("close");
  push_vector(info);

  APPLY_MASTER_OB(ret=,"valid_socket", 3);
  if (!IS_ZERO(ret))
  {
    i = socket_close(fd,0);
  } else { 
    i=EESECURITY;
  }
  pop_stack();		/* pop int arg off stack      */
  push_number(i);	/* Security violation attempted */
}
#endif

#ifdef F_SOCKET_RELEASE
void f_socket_release(int num_arg,struct svalue *argp)
{
  int fd, port;
  struct svalue *ret;
  struct vector *info;
  char addr[ADDR_BUF_SIZE];
  int err;

  fd = argp[0].u.number;
  err=get_socket_address(fd, addr, &port);
  if(err<0)
  {
    pop_n_elems(2);
    push_number(err);
    return;
  }

  info = allocate_array(4);
  info->ref--;
  info->item[0].type = T_NUMBER;
  info->item[0].u.number = fd;
  assign_socket_owner(&info->item[1], get_socket_owner(fd));

  SET_STR(info->item,make_shared_string(addr));

  info->item[3].type = T_NUMBER;
  info->item[3].u.number = port;

  push_object(current_object);
  push_new_shared_string("release");
  push_vector(info);

  APPLY_MASTER_OB(ret=,"valid_socket", 3);
  if (!IS_ZERO(ret))
  {
    i = socket_release(argp[0].u.number, argp+1);
  } else {
    i=EESECURITY;
  }
  pop_n_elems(2);		/* pop all args off stack     */
  push_number(i);		/* push return int onto stack */
}
#endif

#ifdef F_SOCKET_ACQUIRE
void f_socket_acquire(int num_arg,struct svalue *argp)
{
  int fd, port;
  struct svalue *ret;
  struct vector *info;
  char addr[ADDR_BUF_SIZE];
  int err;

  check_argument(2,T_FUNCTION,F_SOCKET_ACQUIRE);
  check_argument(3,T_FUNCTION,F_SOCKET_ACQUIRE);

  fd = argp[0].u.number;
  err=get_socket_address(fd, addr, &port);
  if(err<0)
  {
    pop_n_elems(3);
    push_number(err);
    return;
  }

  info = allocate_array(4);
  info->ref--;
  info->item[0].type = T_NUMBER;
  info->item[0].u.number = fd;
  assign_socket_owner(&info->item[1], get_socket_owner(fd));

  SET_STR(info->item,make_shared_string(addr));

  info->item[3].type = T_NUMBER;
  info->item[3].u.number = port;

  push_object(current_object);
  push_new_shared_string("acquire");
  push_vector(info);

  APPLY_MASTER_OB(ret=,"valid_socket", 3);
  if (!IS_ZERO(ret))
  {
    i = socket_acquire(fd,argp+1,argp+2,argp+3);
  } else {
    i=EESECURITY;
  }
  pop_n_elems(4);		/* pop both args off stack    */
  push_number(i);		/* push return int onto stack */
}
#endif

#ifdef F_SOCKET_ERROR
void f_socket_error(int num_arg,struct svalue *argp)
{
  char *error;
  extern char *socket_error(int);

  error = socket_error(sp->u.number);
  pop_stack();			/* pop int arg off stack      */
  push_new_shared_string(error); /* push return string onto stack */
}
#endif

#ifdef F_SOCKET_ADDRESS
void f_socket_address(int num_arg,struct svalue *argp)
{
  char addr[50];
  int port,ret;

  ret=get_socket_address(argp[0].u.number,addr,&port);
  pop_stack();
  if(ret==EESUCCESS)
  {
    push_new_shared_string(addr);
  }else{
    push_number(ret);
  }
}
#endif

#ifdef F_SOCKET_ADDRESS
void f_socket_port(int num_arg,struct svalue *argp)
{
  char addr[50];
  int port,ret;

  ret=get_socket_address(argp[0].u.number,addr,&port);
  pop_stack();
  if(ret==EESUCCESS)
  {
    push_number(port);
  }else{
    push_number(ret);
  }
}
#endif

#ifdef F_DUMP_SOCKET_STATUS
void f_dump_socket_status(int num_arg,struct svalue *argp)
{
  push_shared_string((char *)dump_socket_status());
}
#endif

#ifdef F_SET_SOCKET_READ
void f_set_socket_read(int num_arg,struct svalue *argp)
{
  int fd;
  fd=argp[0].u.number;
  if(fd < 0 || fd >= MAX_EFUN_SOCKS)
  {
    pop_stack();
    pop_stack();
    push_number(EEFDRANGE);
  }else if(current_object!=lpc_socks[fd].owner_ob){
    pop_stack();
    pop_stack();
    push_number(EESECURITY);
  }else{
    assign_svalue(&lpc_socks[fd].read_callback,argp+1);
    pop_stack();
    pop_stack();
    push_number(EESUCCESS);
  }
}
#endif

#ifdef F_SET_SOCKET_WRITE
void f_set_socket_write(int num_arg,struct svalue *argp)
{
  int fd;
  fd=argp[0].u.number;
  if(fd < 0 || fd >= MAX_EFUN_SOCKS)
  {
    pop_stack();
    pop_stack();
    push_number(EEFDRANGE);
  }else if(current_object!=lpc_socks[fd].owner_ob){
    pop_stack();
    pop_stack();
    push_number(EESECURITY);
  }else{
    assign_svalue(&lpc_socks[fd].write_callback,argp+1);
    pop_stack();
    pop_stack();
    push_number(EESUCCESS);
  }
}
#endif


#ifdef F_SET_SOCKET_CLOSE
void f_set_socket_close(int num_arg,struct svalue *argp)
{
  int fd;
  fd=argp[0].u.number;
  if(fd < 0 || fd >= MAX_EFUN_SOCKS)
  {
    pop_stack();
    pop_stack();
    push_number(EEFDRANGE);
  }else if(current_object!=lpc_socks[fd].owner_ob){
    pop_stack();
    pop_stack();
    push_number(EESECURITY);
  }else {
    assign_svalue(&lpc_socks[fd].close_callback,argp+1);
    pop_stack();
    pop_stack();
    push_number(EESUCCESS);
  }
}
#endif

#ifdef F_SET_SOCKET_MODE
void f_set_socket_mode(int num_arg,struct svalue *argp)
{
  int fd,mode;
  fd=argp[0].u.number;
  mode=argp[1].u.number;
  pop_stack();
  pop_stack();

  if(fd < 0 || fd >= MAX_EFUN_SOCKS)
  {
    push_number(EEFDRANGE);
  }else if(current_object!=lpc_socks[fd].owner_ob){
    push_number(EESECURITY);
  }else if((mode & S_MODE_MASK) != (lpc_socks[fd].mode & S_MODE_MASK)){
    push_number(EEMODENOTSUPP);
  }else{
    lpc_socks[fd].mode=mode;
    push_number(EESUCCESS);
  }
}
#endif

#ifdef F_SOCKET_FROM_STDIN
void f_socket_from_stdin(int num_arg,struct svalue *argp)
{
  extern int stdin_is_sock;
  if(stdin_is_sock)
  {
    push_number(socket_from_stdin());
  }else{
    error("Stdin is not a socket.\n");
  }
}
#endif
