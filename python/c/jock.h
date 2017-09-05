// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

#ifndef IBELIE_JOCK_H__
#define IBELIE_JOCK_H__

#include "port.h"

#ifdef MS_WINDOWS
#	include <winsock2.h>
#else
#	include <sys/socket.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Abstract the socket file descriptor type */
#ifdef MS_WINDOWS
typedef SOCKET IblSock;
#else
typedef int IblSock;
#endif

typedef struct sockaddr *IblSockAddr

typedef void   (*IblJock_HandleConnect) (IblSock, IblSockAddr);
typedef void   (*IblJock_HandleClose)   (IblSock, IblSockAddr);
typedef size_t (*IblJock_HandleRead)    (IblSock, IblSockAddr);

typedef struct _IblJock {
	int    sock_family; /* Address family, e.g., AF_INET */
	int    sock_type;   /* Socket type, e.g., SOCK_STREAM */
	int    sock_proto;  /* Protocol type, usually 0 */
	IblMap sock_map;
	IblJock_HandleConnect handle_connect;
	IblJock_HandleClose   handle_close;
	IblJock_HandleRead    handle_read;
} *IblJock;

IblAPI(void)    IblJock_Initiate  (IblJock);
IblAPI(void)    IblJock_Release   (IblJock);
IblAPI(void)    IblJock_Update    (IblJock, double);
IblAPI(bool)    IblJock_Close     (IblJock, IblSock);
IblAPI(bool)    IblJock_Write     (IblJock, IblSock, byte*, size_t);
IblAPI(IblSock) IblJock_Reconnect (IblJock, IblSockAddr);
IblAPI(IblSock) IblJock_Connect   (IblJock, char*, int);

#ifdef __cplusplus
}
#endif

#endif /* IBELIE_JOCK_H__ */
