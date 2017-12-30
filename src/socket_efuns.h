/*
 *  socket_efuns.h -- definitions and prototypes for socket_efuns.c
 *                    5-92 : Dwayne Fontenot : original coding.
 *                   10-92 : Dave Richards : less original coding.
 */

#ifndef _SOCKET_EFUNS_H_
#define _SOCKET_EFUNS_H_

#include "interpret.h"
#include "comm.h"

/* basic modes */
#define S_STREAM 0
#define S_DATAGRAM 1

/* stream minor modes */
#define S_BUFFERED_OUTPUT 16
#define S_BUFFERED_INPUT 32

#define S_MODE_MASK 15

enum socket_state { CLOSED, UNBOUND, BOUND, LISTEN, DATA_XFER };

#define ADDR_BUF_SIZE 200
#define	BUF_SIZE	2048	/* max reliable packet size	   */

struct lpc_socket
{
    int			fd;
    short		flags;
    short               mode;
    enum socket_state	state;
    struct sockaddr_in	l_addr;
    struct sockaddr_in	r_addr;
    struct object *	owner_ob;
    struct object *	release_ob;

    struct svalue       read_callback;
    struct svalue       write_callback;
    struct svalue       close_callback;

    char *		r_buf;
    int			r_off;
    long		r_len;

    char *		w_buf;
    int			w_off;
    int			w_len;
};

#define	S_RELEASE	0x01
#define	S_BLOCKED	0x02
#define	S_HEADER	0x04
#define	S_WACCEPT	0x08
#define	S_MORE_LINES	0x10
#define S_CLOSING       0x20

int socket_create PROT((int socket_mode, struct svalue *, struct svalue *));
int socket_close(int,int);
void close_referencing_sockets(struct object *ob);
void socket_read_select_handler(int fd);
void socket_write_select_handler(int fd);
void handle_line_sockets(void);
INLINE int set_socket_nonblocking(int fd,int which);
INLINE int set_socket_owner(int fd,int which);
int get_socket_address(int fd,char *addr,int *port);
int socket_bind(int fd,int port);
struct object *get_socket_owner(int fd);
int socket_listen(int fd,struct svalue *callback);
int socket_accept(int fd,
		  struct svalue *read_callback,
                  struct svalue * write_callback);
int socket_connect(int fd,
                   char *name,
                   struct svalue *read_callback,
                   struct svalue *write_callback);
int socket_write(int fd,struct svalue *message,char *name);
int socket_release(int fd,struct svalue *callback);
int socket_acquire(int fd,
	struct svalue *read_callback,
	struct svalue *write_callback,
	struct svalue *close_callback);
char *dump_socket_status(void);
int socket_from_stdin(void);
void assign_socket_owner(struct svalue *sv,struct object *ob);
#endif /* _SOCKET_EFUNS_H_ */
