// Copyright 2017 - 2018 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

#include "jock.h"

#ifdef MS_WINDOWS
#	define SOCKETCLOSE closesocket
#else
#	define SOCKETCLOSE close
#endif

#ifndef FD_SETSIZE
#	define FD_SETSIZE 512
#endif

#ifndef BUFFER_SIZE
#	define BUFFER_SIZE 4096
#endif

#ifdef __cplusplus
extern "C" {
#endif

IblMap_KEY_NUMERIC(JockMap, IblSock,
	IblJock   jock;
	IblBuffer rbuf;
	IblBuffer wbuf;
	struct sockaddr addr;
);

static void _PrintError(void) {
#ifdef MS_WINDOWS
	char *s_buf = NULL; /* Free via LocalFree */
	DWORD err = (DWORD)WSAGetLastError();
	if (err || errno < 0 || errno >= _sys_nerr) {
		if (!err) { err = (DWORD)errno; }
		register int len = FormatMessage(
			/* Error API error */
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,  /* no message source */
			err,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* Default language */
			(LPTSTR) &s_buf,
			0,     /* size not used */
			NULL); /* no args */
		if (len == 0) {
			/* Only seen this in out of mem situations */
			IblPrint_Err("[Jock] Windows Error 0x%X.\n", err);
			s_buf = NULL;
		} else {
			/* remove trailing cr/lf and dots */
			while (len > 0 && (s_buf[len-1] <= ' ' || s_buf[len-1] == '.')) {
				s_buf[--len] = '\0';
			}
			IblPrint_Err("[Jock] %s\n", s_buf);
		}
		LocalFree(s_buf);
		return;
	} else if (errno > 0) {
		IblPrint_Err("[Jock] %s\n", _sys_errlist[errno]);
		return;
	}
#endif

	if (errno == 0) {
		IblPrint_Err("[Jock] Error 0.\n");
	} else {
		IblPrint_Err("[Jock] %s\n", strerror(errno));
	}
}

void IblJock_Initiate(IblJock jock) {
	if (!jock->sock_map) {
		jock->sock_map = JockMap_New();
		if (!jock->sock_map) {
			IblPrint_Err("[Jock] Initiate socket map failed.\n");
		}
	}
}

void IblJock_Release(IblJock jock) {
	if (jock->sock_map) {
		register IblMap_Item iter;
		for (iter = IblMap_Begin(jock->sock_map); iter; iter = IblMap_Next(jock->sock_map, iter)) {
			register JockMap item = (JockMap)iter;
			(void)SOCKETCLOSE(item->key);
			IblBuffer_Free(&(item->rbuf));
			IblBuffer_Free(&(item->wbuf));
		}
		IblMap_Free(jock->sock_map);
	}
}

void IblJock_Update(IblJock jock, double timeout) {
	struct timeval tv, *tvp = (struct timeval *)NULL;
	fd_set ifdset, ofdset, efdset;
	size_t its_len = 0;
	JockMap its[FD_SETSIZE];
	char buf[BUFFER_SIZE];
	if (!jock->sock_map) {
		IblPrint_Err("[Jock] Update with uninitialized socket map.\n");
		return;
	} else if (IblMap_Size(jock->sock_map) <= 0) {
		return;
	} else if (timeout > (double)LONG_MAX) {
		IblPrint_Err("[Jock] Update timeout(%.2f) period too long.\n", timeout);
		return;
	}

	FD_ZERO(&ifdset);
	FD_ZERO(&ofdset);
	FD_ZERO(&efdset);
	if (timeout > 0) {
		tv.tv_sec = (long)timeout;
		tv.tv_usec = (long)((timeout - (double)(tv.tv_sec)) * 1E6);
		tvp = &tv;
	}

	register IblSock max = 0;
	register IblMap_Item iter;
	if (IblMap_Size(jock->sock_map) > FD_SETSIZE) {
		IblPrint_Err("[Jock] Update socket map size(%d) more than FD_SETSIZE(" #FD_SETSIZE ").\n", IblMap_Size(jock->sock_map));
	}
	for (iter = IblMap_Begin(jock->sock_map); iter; iter = IblMap_Next(jock->sock_map, iter), fds_len++) {
		register JockMap item = (JockMap)iter;
		FD_SET(item->key, &ifdset);
		FD_SET(item->key, &efdset);
		if (IblBuffer_Length(&(item->wbuf)) > 0) {
			FD_SET(item->key, &ofdset);
		}
		if (fds_len <= FD_SETSIZE) {
			its[fds_len] = item;
		}
#ifndef _MSC_VER
		if (item->key > max) {
			max = item->key;
		}
#endif
	}

#ifdef MS_WINDOWS
	if (select(max, &ifdset, &ofdset, &efdset, tvp) == SOCKET_ERROR) {
		_PrintError();
		return;
	}
#else
	if (select(max, &ifdset, &ofdset, &efdset, tvp) < 0) {
		_PrintError();
		return;
	}
#endif

	for (register size_t i = 0; i < fds_len; i++) {
		register JockMap item = its[i];
		if (FD_ISSET(item->key, &ifdset)) {
			register ssize_t len = recv(item->key, buf, BUFFER_SIZE, 0);
			if (len < 0) {
				_PrintError();
			} else if (!jock->handle_read) {
				IblPrint_Err("[Jock] Update recv no handler.\n");
			} else if (!IblBuffer_Write(&(item->rbuf), buf, len)) {
				IblPrint_Err("[Jock] Update recv write buffer error.\n");
			} else {
				IblBuffer_Read(&(item->rbuf), jock->handle_read(jock, item->key, &(item->addr),
					IblBuffer_Bytes(&(item->rbuf)), IblBuffer_Length(&(item->rbuf))));
			}
		}
		if (FD_ISSET(item->key, &ofdset)) {
			register int n = send(item->key, IblBuffer_Bytes(&(item->wbuf)),
				IblBuffer_Length(&(item->wbuf)), 0);
			if (n < 0) {
				_PrintError();
			} else {
				IblBuffer_Read(&(item->wbuf), n);
			}
		}
		if (FD_ISSET(item->key, &efdset)) {
			if (jock->handle_close) {
				jock->handle_close(jock, item->key, &(item->addr));
			}
			(void)SOCKETCLOSE(item->key);
			IblBuffer_Free(&(item->rbuf));
			IblBuffer_Free(&(item->wbuf));
			IblMap_Del(jock->sock_map, &(item->key));
		}
	}
}

bool IblJock_Close(IblJock jock, IblSock sock) {
	(void)SOCKETCLOSE(sock);
	if (!jock->sock_map) {
		IblPrint_Err("[Jock] Close with uninitialized socket map.\n");
		return false;
	}
	register JockMap item = (JockMap)IblMap_Get(jock->sock_map, &sock);
	if (!item) {
		IblPrint_Err("[Jock] Close socket not in map.\n");
		return false;
	}
	IblBuffer_Free(&(item->rbuf));
	IblBuffer_Free(&(item->wbuf));
	return IblMap_Del(jock->sock_map, &sock);
}

bool IblJock_Write(IblJock jock, IblSock sock, byte* data, size_t length) {
	if (!jock->sock_map) {
		IblPrint_Err("[Jock] Write with uninitialized socket map.\n");
		return false;
	}
	register JockMap item = (JockMap)IblMap_Get(jock->sock_map, &sock);
	if (!item) {
		IblPrint_Err("[Jock] Write socket not in map.\n");
		return false;
	}
	return IblBuffer_Write(&(item->wbuf), data, length);
}

IblSock IblJock_Reconnect(IblJock jock, IblSockAddr addr) {
	IblSock sock;
	u_long nonblock = 1;
	if (!jock->sock_map) {
		IblPrint_Err("[Jock] Connect with uninitialized socket map.\n");
		return -1;
	}

	if ((sock = socket(jock->sock_family, jock->sock_type, jock->sock_proto)) < 0) {
		_PrintError();
		return -1;
	}
#ifdef MS_WINDOWS
	ioctlsocket(sock, FIONBIO, &nonblock);
#else
	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
#endif

	if (connect(sock, addr, sizeof(struct sockaddr_in)) < 0) {
		(void)SOCKETCLOSE(sock);
		_PrintError();
		return -1;
	}

	register JockMap item = (JockMap)IblMap_Set(jock->sock_map, &sock);
	if (item) {
		item->jock = jock;
		IblBuffer_Init(&(item->rbuf));
		IblBuffer_Init(&(item->wbuf));
		memcpy((char*)(&(item->addr)), addr, sizeof(struct sockaddr_in));
	} else {
		(void)SOCKETCLOSE(sock);
		IblPrint_Err("[Jock] Set socket map error.\n");
		return -1;
	}

	if (jock->handle_connect) {
		jock->handle_connect(jock, sock, &(item->addr));
	}

	return sock;

}

IblSock IblJock_Connect(IblJock jock, char* host, int port) {
	char ch;
	int d1, d2, d3, d4;
	struct sockaddr_in addr = {0};
	if (jock->sock_family != AF_INET) {
		IblPrint_Err("[Jock] Connect socket family must be AF_INET(%d).\n", AF_INET);
		return -1;
	} else if (port < 0 || port > 0xffff) {
		IblPrint_Err("[Jock] Connect port(%d) must be 0-65535.\n", port);
		return -1;
	} else if (sscanf(host, "%d.%d.%d.%d%c", &d1, &d2, &d3, &d4, &ch) != 4 ||
		0 > d1 || d1 > 255 || 0 > d2 || d2 > 255 || 0 > d3 || d3 > 255 || 0 > d4 || d4 > 255) {
		IblPrint_Err("[Jock] Connect bad host(%s).\n", host);
		return -1;
	} else {
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(((long) d1 << 24) | ((long) d2 << 16) | ((long) d3 << 8) | ((long) d4 << 0));
		addr.sin_port = htons((short)port);
#ifdef HAVE_SOCKADDR_SA_LEN
		addr.sin_len = sizeof(addr);
#endif
		return IblJock_Reconnect(jock, (IblSockAddr)&addr);
	}
}

#ifdef __cplusplus
}
#endif
