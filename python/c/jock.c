// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

#include "jock.h"

#ifdef __cplusplus
extern "C" {
#endif

static double defaulttimeout = -1.0; /* Default timeout for new sockets */

PyMODINIT_FUNC
init_sockobject(PySocketSockObject *s,
                SOCKET_T fd, int family, int type, int proto)
{
#ifdef RISCOS
    int block = 1;
#endif
    s->sock_fd = fd;
    s->sock_family = family;
    s->sock_type = type;
    s->sock_proto = proto;
    s->sock_timeout = defaulttimeout;

    s->errorhandler = &set_error;

    if (defaulttimeout >= 0.0)
        internal_setblocking(s, 0);

#ifdef RISCOS
    if (taskwindow)
        socketioctl(s->sock_fd, 0x80046679, (u_long*)&block);
#endif
}

/* Create a new, uninitialized socket object. */

static PyObject *
sock_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyObject *new;

	new = type->tp_alloc(type, 0);
	if (new != NULL) {
		((PySocketSockObject *)new)->sock_fd = -1;
		((PySocketSockObject *)new)->sock_timeout = -1.0;
		((PySocketSockObject *)new)->errorhandler = &set_error;
	}
	return new;
}

/* Initialize a new socket object. */

static int
sock_initobj(PyObject *self, PyObject *args, PyObject *kwds)
{
	PySocketSockObject *s = (PySocketSockObject *)self;
	SOCKET_T fd;
	int family = AF_INET, type = SOCK_STREAM, proto = 0;
	static char *keywords[] = {"family", "type", "proto", 0};

	if (!PyArg_ParseTupleAndKeywords(args, kwds,
									 "|iii:socket", keywords,
									 &family, &type, &proto))
		return -1;

	Py_BEGIN_ALLOW_THREADS
	fd = socket(family, type, proto);
	Py_END_ALLOW_THREADS

#ifdef MS_WINDOWS
	if (fd == INVALID_SOCKET)
#else
	if (fd < 0)
#endif
	{
		set_error();
		return -1;
	}
	init_sockobject(s, fd, family, type, proto);

	return 0;

}

static PyObject *
sock_connect(PySocketSockObject *s, PyObject *addro)
{
	sock_addr_t addrbuf;
	int addrlen;
	int res;
	int timeout;

	if (!getsockaddrarg(s, addro, SAS2SA(&addrbuf), &addrlen))
		return NULL;

	Py_BEGIN_ALLOW_THREADS
	res = internal_connect(s, SAS2SA(&addrbuf), addrlen, &timeout);
	Py_END_ALLOW_THREADS

	if (timeout == 1) {
		PyErr_SetString(socket_timeout, "timed out");
		return NULL;
	}
	if (res != 0)
		return s->errorhandler();
	Py_INCREF(Py_None);
	return Py_None;
}

PyDoc_STRVAR(connect_doc,
"connect(address)\n\
\n\
Connect the socket to a remote address.  For IP sockets, the address\n\
is a pair (host, port).");

/* s.close() method.
   Set the file descriptor to -1 so operations tried subsequently
   will surely fail. */

static PyObject *
sock_close(PySocketSockObject *s)
{
	SOCKET_T fd;

	if ((fd = s->sock_fd) != -1) {
		s->sock_fd = -1;
		Py_BEGIN_ALLOW_THREADS
		(void) SOCKETCLOSE(fd);
		Py_END_ALLOW_THREADS
	}
	Py_INCREF(Py_None);
	return Py_None;
}

PyDoc_STRVAR(close_doc,
"close()\n\
\n\
Close the socket.  It cannot be used after this call.");

/* s.setsockopt() method.
   With an integer third argument, sets an integer option.
   With a string third argument, sets an option from a buffer;
   use optional built-in module 'struct' to encode the string. */

static PyObject *
sock_setsockopt(PySocketSockObject *s, PyObject *args)
{
	int level;
	int optname;
	int res;
	char *buf;
	int buflen;
	int flag;

	if (PyArg_ParseTuple(args, "iii:setsockopt",
						 &level, &optname, &flag)) {
		buf = (char *) &flag;
		buflen = sizeof flag;
	}
	else {
		PyErr_Clear();
		if (!PyArg_ParseTuple(args, "iis#:setsockopt",
							  &level, &optname, &buf, &buflen))
			return NULL;
	}
	res = setsockopt(s->sock_fd, level, optname, (void *)buf, buflen);
	if (res < 0)
		return s->errorhandler();
	Py_INCREF(Py_None);
	return Py_None;
}

PyDoc_STRVAR(setsockopt_doc,
"setsockopt(level, option, value)\n\
\n\
Set a socket option.  See the Unix manual for level and option.\n\
The value argument can either be an integer or a string.");

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
		Py_BEGIN_ALLOW_THREADS
		timeout = internal_select_ex(s, 1, interval);
		n = -1;
		if (!timeout) {
#ifdef __VMS
			n = sendsegmented(s->sock_fd, buf, len, flags);
#else
			n = send(s->sock_fd, buf, len, flags);
#endif
		}
		Py_END_ALLOW_THREADS
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

#ifdef __cplusplus
}
#endif
