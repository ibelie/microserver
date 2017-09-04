// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

#ifndef IBELIE_JOCK_H__
#define IBELIE_JOCK_H__

#include "port.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void   (*IblJock_HandleConnect) (SOCKET_T, struct sockaddr *);
typedef void   (*IblJock_HandleClose)   (SOCKET_T, struct sockaddr *);
typedef size_t (*IblJock_HandleRead)    (SOCKET_T, struct sockaddr *);

typedef struct _IblJock {
	int sock_family; /* Address family, e.g., AF_INET */
	int sock_type;   /* Socket type, e.g., SOCK_STREAM */
	int sock_proto;  /* Protocol type, usually 0 */
	IblJock_HandleConnect handle_connect;
	IblJock_HandleClose   handle_close;
	IblJock_HandleRead    handle_read;
} *IblJock;

IblAPI(SOCKET_T) IblJock_Connect   (IblJock, char*, int);
IblAPI(SOCKET_T) IblJock_Reconnect (IblJock, struct sockaddr *);
IblAPI(bool)     IblJock_Close     (SOCKET_T);
IblAPI(bool)     IblJock_Write     (SOCKET_T, byte*, size_t);
IblAPI(void)     IblJock_Update    (double);

#ifdef __cplusplus
}
#endif

#endif /* IBELIE_JOCK_H__ */
