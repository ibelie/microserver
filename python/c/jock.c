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
	s->sock_fd = fd;
	s->sock_family = family;
	s->sock_type = type;
	s->sock_proto = proto;
	s->sock_timeout = defaulttimeout;

	s->errorhandler = &set_error;

	if (defaulttimeout >= 0.0)
		internal_setblocking(s, 0);
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

	fd = socket(family, type, proto);

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

/* Function to perform the setting of socket blocking mode
   internally. block = (1 | 0). */
static int
internal_setblocking(PySocketSockObject *s, int block)
{
#ifdef MS_WINDOWS
	block = !block;
	ioctlsocket(s->sock_fd, FIONBIO, (u_long*)&block);
#else /* MS_WINDOWS */
	int delay_flag;
	delay_flag = fcntl(s->sock_fd, F_GETFL, 0);
	if (block)
		delay_flag &= (~O_NONBLOCK);
	else
		delay_flag |= O_NONBLOCK;
	fcntl(s->sock_fd, F_SETFL, delay_flag);
#endif /* MS_WINDOWS */

	/* Since these don't return anything */
	return 1;
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

	res = internal_connect(s, SAS2SA(&addrbuf), addrlen, &timeout);

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
		(void) SOCKETCLOSE(fd);
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
