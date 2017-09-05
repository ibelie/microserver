// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

#include "jock.h"

#ifdef MS_WINDOWS
#	define SOCKETCLOSE closesocket
#else
#	define SOCKETCLOSE close
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
	struct timeval tv, *tvp;
	fd_set ifdset, ofdset, efdset;
	if (!jock->sock_map) {
		IblPrint_Err("[Jock] Update with uninitialized socket map.\n");
		return;
	} else if (timeout > (double)LONG_MAX) {
		IblPrint_Err("[Jock] Update timeout(%.2f) period too long.\n", timeout);
		return;
	}

	FD_ZERO(&ifdset);
	FD_ZERO(&ofdset);
	FD_ZERO(&efdset);
	if (timeout <= 0) {
		tvp = (struct timeval *)0;
	} else {
		tv.tv_sec = (long)timeout;
		tv.tv_usec = (long)((timeout - (double)(tv.tv_sec)) * 1E6);
		tvp = &tv;
	}

	register IblSock max = 0;
	register IblMap_Item iter;
	for (iter = IblMap_Begin(jock->sock_map); iter; iter = IblMap_Next(jock->sock_map, iter)) {
		register JockMap item = (JockMap)iter;
		FD_SET(item->key, &ifdset);
		FD_SET(item->key, &efdset);
		if (IblBuffer_Length(&(item->wbuf)) > 0) {
			FD_SET(item->key, &ofdset);
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

	register IblMap_Item iter;
	for (iter = IblMap_Begin(jock->sock_map); iter; iter = IblMap_Next(jock->sock_map, iter)) {
		register JockMap item = (JockMap)iter;
		if (FD_ISSET(item->key, &ifdset)) {
			//TODO: Read
		}
		if (FD_ISSET(item->key, &ofdset)) {
			//TODO: Write
		}
		if (FD_ISSET(item->key, &efdset)) {
			//TODO: Close
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
		jock->handle_connect(sock, &(item->addr));
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

static PyObject *
sock_recv(PySocketSockObject *s, PyObject *args)
{
	int recvlen, flags = 0;
	ssize_t outlen;
	PyObject *buf;

	if (!PyArg_ParseTuple(args, "i|i:recv", &recvlen, &flags))
		return NULL;

	if (recvlen < 0) {
		PyErr_SetString(PyExc_ValueError,
						"negative buffersize in recv");
		return NULL;
	}

	/* Allocate a new string. */
	buf = PyString_FromStringAndSize((char *) 0, recvlen);
	if (buf == NULL)
		return NULL;

	/* Call the guts */
	outlen = sock_recv_guts(s, PyString_AS_STRING(buf), recvlen, flags);
	if (outlen < 0) {
		/* An error occurred, release the string and return an
		   error. */
		Py_DECREF(buf);
		return NULL;
	}
	if (outlen != recvlen) {
		/* We did not read as many bytes as we anticipated, resize the
		   string if possible and be successful. */
		if (_PyString_Resize(&buf, outlen) < 0)
			/* Oopsy, not so successful after all. */
			return NULL;
	}

	return buf;
}

PyDoc_STRVAR(recv_doc,
"recv(buffersize[, flags]) -> data\n\
\n\
Receive up to buffersize bytes from the socket.  For the optional flags\n\
argument, see the Unix manual.  When no data is available, block until\n\
at least one byte is available or until the remote end is closed.  When\n\
the remote end is closed and all data is read, return the empty string.");

static PyObject *
sock_sendall(PySocketSockObject *s, PyObject *args)
{
	char *buf;
	int len, n = -1, flags = 0, timeout, saved_errno;
	Py_buffer pbuf;

	if (!PyArg_ParseTuple(args, "s*|i:sendall", &pbuf, &flags))
		return NULL;
	buf = pbuf.buf;
	len = pbuf.len;

	if (!IS_SELECTABLE(s)) {
		PyBuffer_Release(&pbuf);
		return select_error();
	}

	do {
		BEGIN_SELECT_LOOP(s)
		timeout = internal_select_ex(s, 1, interval);
		n = -1;
		if (!timeout) {
			n = send(s->sock_fd, buf, len, flags);
		}
		if (timeout == 1) {
			PyBuffer_Release(&pbuf);
			PyErr_SetString(socket_timeout, "timed out");
			return NULL;
		}
		END_SELECT_LOOP(s)
		/* PyErr_CheckSignals() might change errno */
		saved_errno = errno;
		/* We must run our signal handlers before looping again.
		   send() can return a successful partial write when it is
		   interrupted, so we can't restrict ourselves to EINTR. */
		if (PyErr_CheckSignals()) {
			PyBuffer_Release(&pbuf);
			return NULL;
		}
		if (n < 0) {
			/* If interrupted, try again */
			if (saved_errno == EINTR)
				continue;
			else
				break;
		}
		buf += n;
		len -= n;
	} while (len > 0);
	PyBuffer_Release(&pbuf);

	if (n < 0)
		return s->errorhandler();

	Py_INCREF(Py_None);
	return Py_None;
}

PyDoc_STRVAR(sendall_doc,
"sendall(data[, flags])\n\
\n\
Send a data string to the socket.  For the optional flags\n\
argument, see the Unix manual.  This calls send() repeatedly\n\
until all data is sent.  If an error occurs, it's impossible\n\
to tell how much data has been sent.");

static PyObject *
select_select(PyObject *self, PyObject *args)
{
#ifdef SELECT_USES_HEAP
	pylist *rfd2obj, *wfd2obj, *efd2obj;
#else  /* !SELECT_USES_HEAP */
	/* XXX: All this should probably be implemented as follows:
	 * - find the highest descriptor we're interested in
	 * - add one
	 * - that's the size
	 * See: Stevens, APitUE, $12.5.1
	 */
	pylist rfd2obj[FD_SETSIZE + 1];
	pylist wfd2obj[FD_SETSIZE + 1];
	pylist efd2obj[FD_SETSIZE + 1];
#endif /* SELECT_USES_HEAP */
	PyObject *ifdlist, *ofdlist, *efdlist;
	PyObject *ret = NULL;
	PyObject *tout = Py_None;
	fd_set ifdset, ofdset, efdset;
	double timeout;
	struct timeval tv, *tvp;
	long seconds;
	int imax, omax, emax, max;
	int n;

	/* convert arguments */
	if (!PyArg_UnpackTuple(args, "select", 3, 4,
						  &ifdlist, &ofdlist, &efdlist, &tout))
		return NULL;

	if (tout == Py_None)
		tvp = (struct timeval *)0;
	else if (!PyNumber_Check(tout)) {
		PyErr_SetString(PyExc_TypeError,
						"timeout must be a float or None");
		return NULL;
	}
	else {
		timeout = PyFloat_AsDouble(tout);
		if (timeout == -1 && PyErr_Occurred())
			return NULL;
		if (timeout > (double)LONG_MAX) {
			PyErr_SetString(PyExc_OverflowError,
							"timeout period too long");
			return NULL;
		}
		seconds = (long)timeout;
		timeout = timeout - (double)seconds;
		tv.tv_sec = seconds;
		tv.tv_usec = (long)(timeout * 1E6);
		tvp = &tv;
	}


#ifdef SELECT_USES_HEAP
	/* Allocate memory for the lists */
	rfd2obj = PyMem_NEW(pylist, FD_SETSIZE + 1);
	wfd2obj = PyMem_NEW(pylist, FD_SETSIZE + 1);
	efd2obj = PyMem_NEW(pylist, FD_SETSIZE + 1);
	if (rfd2obj == NULL || wfd2obj == NULL || efd2obj == NULL) {
		if (rfd2obj) PyMem_DEL(rfd2obj);
		if (wfd2obj) PyMem_DEL(wfd2obj);
		if (efd2obj) PyMem_DEL(efd2obj);
		return PyErr_NoMemory();
	}
#endif /* SELECT_USES_HEAP */
	/* Convert sequences to fd_sets, and get maximum fd number
	 * propagates the Python exception set in seq2set()
	 */
	rfd2obj[0].sentinel = -1;
	wfd2obj[0].sentinel = -1;
	efd2obj[0].sentinel = -1;
	if ((imax=seq2set(ifdlist, &ifdset, rfd2obj)) < 0)
		goto finally;
	if ((omax=seq2set(ofdlist, &ofdset, wfd2obj)) < 0)
		goto finally;
	if ((emax=seq2set(efdlist, &efdset, efd2obj)) < 0)
		goto finally;
	max = imax;
	if (omax > max) max = omax;
	if (emax > max) max = emax;

	n = select(max, &ifdset, &ofdset, &efdset, tvp);

#ifdef MS_WINDOWS
	if (n == SOCKET_ERROR) {
		PyErr_SetExcFromWindowsErr(SelectError, WSAGetLastError());
	}
#else
	if (n < 0) {
		PyErr_SetFromErrno(SelectError);
	}
#endif
	else {
		/* any of these three calls can raise an exception.  it's more
		   convenient to test for this after all three calls... but
		   is that acceptable?
		*/
		ifdlist = set2list(&ifdset, rfd2obj);
		ofdlist = set2list(&ofdset, wfd2obj);
		efdlist = set2list(&efdset, efd2obj);
		if (PyErr_Occurred())
			ret = NULL;
		else
			ret = PyTuple_Pack(3, ifdlist, ofdlist, efdlist);

		Py_DECREF(ifdlist);
		Py_DECREF(ofdlist);
		Py_DECREF(efdlist);
	}

  finally:
	reap_obj(rfd2obj);
	reap_obj(wfd2obj);
	reap_obj(efd2obj);
#ifdef SELECT_USES_HEAP
	PyMem_DEL(rfd2obj);
	PyMem_DEL(wfd2obj);
	PyMem_DEL(efd2obj);
#endif /* SELECT_USES_HEAP */
	return ret;
}

#ifdef __cplusplus
}
#endif
