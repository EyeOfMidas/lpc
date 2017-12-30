/*
 * comm.h -- definitions and prototypes for comm.c
 *
 */

#ifndef _COMM_H_
#define _COMM_H_

#ifdef _AIX
#include <sys/select.h>
#endif /* _AIX */

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifndef ARPA_INET_H
#include <arpa/inet.h>
#define ARPA_INET_H
#endif

#define MAX_TEXT                   2048
#define MAX_SOCKET_PACKET_SIZE     1024
#define DESIRED_SOCKET_PACKET_SIZE 800
#define MESSAGE_BUF_SIZE           MESSAGE_BUFFER_SIZE /* from options.h */
#define OUT_BUF_SIZE               2048
#define DFAULT_PROTO               0   /* use the appropriate protocol */
#define I_NOECHO                   0x1 /* input_to flag */
#define I_NOESC                    0x2 /* input_to flag */

enum msgtypes {NAMEBYIP, IPBYNAME};

void query_addr_name(struct in_addr addr);
char *query_ip_name(struct in_addr addr);
INLINE void process_io();
#endif /* _COMM_H_ */
