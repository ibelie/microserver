// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

#ifndef IBELIE_JOCK_H__
#define IBELIE_JOCK_H__

#include "port.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _IblJock {
	SOCKET_T        sock_fd;     /* Socket file descriptor */
	int             sock_family; /* Address family, e.g., AF_INET */
	int             sock_type;   /* Socket type, e.g., SOCK_STREAM */
	int             sock_proto;  /* Protocol type, usually 0 */
	struct sockaddr sock_addr;
} *IblJock;

typedef void   (*IblJock_HandleConnect) (IblJock);
typedef void   (*IblJock_HandleClose)   (IblJock);
typedef size_t (*IblJock_HandleRead)    (IblJock);

IblAPI(IblJock) IblJock_New     (int, int, int, char*, int);
IblAPI(bool)    IblJock_Connect (IblJock);
IblAPI(bool)    IblJock_Close   (IblJock);
IblAPI(bool)    IblJock_Write   (IblJock, byte*, size_t);

#ifdef __cplusplus
}
#endif

#endif /* IBELIE_JOCK_H__ */
