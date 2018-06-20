// Copyright 2017-2018 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

#ifdef _MSC_VER
#	define HAVE_ROUND
#endif
#define PY_SSIZE_T_CLEAN

#define IblPrint_Info PySys_WriteStdout
#define IblPrint_Warn PySys_WriteStdout
#define IblPrint_Err  PySys_WriteStderr

#include "Python.h"
#include "jock.h"

#ifndef PyVarObject_HEAD_INIT
#	define PyVarObject_HEAD_INIT(type, size) PyObject_HEAD_INIT(type) size,
#endif

#ifndef Py_TYPE
#	define Py_TYPE(ob) (((PyObject*)(ob))->ob_type)
#endif

#if PY_MAJOR_VERSION >= 3
#	define PyInt_Check PyLong_Check
#	if PY_VERSION_HEX < 0x03030000
#		error "Python 3.0 - 3.2 are not supported."
#	else
#		define PyString_AsString(ob) \
			(PyUnicode_Check(ob)? PyUnicode_AsUTF8(ob): PyBytes_AsString(ob))
#	endif
#endif

static void _FormatTypeError(PyObject* arg, const char* err) {
	PyObject* repr = PyObject_Repr(arg);
	if (!repr) { return; }
	PyErr_Format(PyExc_TypeError, "%s, but %.100s has type %.100s", err,
		PyString_AsString(repr), Py_TYPE(arg)->tp_name);
	Py_DECREF(repr);
}

static PyObject* _CheckBytes(PyObject* bytes, const char* name) {
	if (!bytes || bytes == Py_None) {
		PyErr_Format(PyExc_TypeError, "the argument respect a string of %s, not None.", name);
		return NULL;
	} else if (PyUnicode_Check(bytes)) {
		return PyUnicode_AsEncodedObject(bytes, NULL, NULL);
	} else if (PyBytes_Check(bytes)) {
		Py_INCREF(bytes);
		return bytes;
	} else {
		_FormatTypeError(bytes, "the argument respect a string");
		return NULL;
	}
}

/*====================================================================*/

static void IblJock_HandleConnect(IblJock jock, IblSock sock, IblSockAddr addr) {

}

static void IblJock_HandleClose(IblJock jock, IblSock sock, IblSockAddr addr) {
	IblJock_Reconnect(jock, addr);
}

static size_t Tcp_HandleRead(IblJock jock, IblSock sock, IblSockAddr addr, byte* data, size_t length) {
	IblPrint_Info("\nTcp_HandleRead: %zu [", length);
	for (register size_t i = 0; i < length; i++) {
		IblPrint_Info("%d, ", data[i]);
	}
	IblPrint_Info("]\n");
}

static struct _IblJock TcpJock = {
	AF_INET,               /* sock_family    */
	SOCK_STREAM,           /* sock_type      */
	0,                     /* sock_proto     */
	NULL,                  /* sock_map       */
	IblJock_HandleConnect, /* handle_connect */
	IblJock_HandleClose,   /* handle_close   */
	Tcp_HandleRead,        /* handle_read    */
};

static PyObject* Update(PyObject* m, PyObject* arg) {
	if (!PyInt_Check(arg) && !PyLong_Check(arg) && !PyFloat_Check(arg)) {
		_FormatTypeError(arg, "the timeout argument respect a number");
		return NULL;
	}
	register double timeout = PyFloat_AsDouble(arg);
	IblJock_Update(&TcpJock, timeout);
	Py_RETURN_NONE;
}

/*====================================================================*/

typedef struct {
	PyObject_HEAD
	Kw_Storage storage;
} TcpClientObject;

extern PyTypeObject TcpClient_Type;

static void TcpClient_Dealloc(register StorageObject* self) {
	Kw_Free(&self->storage);
	PyObject_Del(self);
}

static PyObject* TcpClient_New(PyTypeObject* cls, PyObject* args, PyObject* kwargs) {
	StorageObject* self;
	PyObject *file = NULL;
	static char *kwlist[] = {"filename", 0};

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "S", kwlist, &file)) {
		return NULL;
	} else if (!(file = _CheckBytes(file, "filename"))) {
		return NULL;
	} else if (!(self = PyObject_New(StorageObject, &TcpClient_Type))) {
		Py_DECREF(file);
		return NULL;
	} else if (!(self->storage = Kw_New(PyBytes_AS_STRING(file)))) {
		PyErr_Format(PyExc_RuntimeError, "KiwiLite open storage (%.100s) error.", PyBytes_AS_STRING(file));
		Py_DECREF(file);
		return NULL;
	}
	Py_DECREF(file);
	return (PyObject*)self;
}

static PyObject* TcpClient_Get(StorageObject* self, PyObject* k) {
	struct _bytes key, value;
	if (!(k = _CheckBytes(k, "key"))) {
		return NULL;
	}
	key.data = (byte*)PyBytes_AS_STRING(k);
	key.length = PyString_GET_SIZE(k);
	if (!Kw_Get(self->storage, &key, &value)) {
		PyErr_Format(PyExc_KeyError, "KiwiLite get value of key (%.100s).", PyBytes_AS_STRING(k));
		Py_DECREF(k);
		return NULL;
	}
	Py_DECREF(k);
	register PyObject* v = PyBytes_FromStringAndSize((char*)value.data, value.length);
	free(value.data);
	return v;
}

static PyMethodDef TcpClient_Methods[] = {
	{ "Get", (PyCFunction)TcpClient_Get, METH_O,
		"Get value bytes of key." },
	{ NULL, NULL}
};

PyTypeObject TcpClient_Type = {
	PyVarObject_HEAD_INIT(0, 0)
	"_kiwilite.Storage",                      /* tp_name           */
	sizeof(StorageObject),                    /* tp_basicsize      */
	0,                                        /* tp_itemsize       */
	(destructor)TcpClient_Dealloc,              /* tp_dealloc        */
	0,                                        /* tp_print          */
	0,                                        /* tp_getattr        */
	0,                                        /* tp_setattr        */
	0,                                        /* tp_compare        */
	0,                                        /* tp_repr           */
	0,                                        /* tp_as_number      */
	0,                                        /* tp_as_sequence    */
	0,                                        /* tp_as_mapping     */
	PyObject_HashNotImplemented,              /* tp_hash           */
	0,                                        /* tp_call           */
	0,                                        /* tp_str            */
	0,                                        /* tp_getattro       */
	0,                                        /* tp_setattro       */
	0,                                        /* tp_as_buffer      */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags          */
	"Storage for kiwilite",                   /* tp_doc            */
	0,                                        /* tp_traverse       */
	0,                                        /* tp_clear          */
	0,                                        /* tp_richcompare    */
	0,                                        /* tp_weaklistoffset */
	0,                                        /* tp_iter           */
	0,                                        /* tp_iternext       */
	TcpClient_Methods,                          /* tp_methods        */
	0,                                        /* tp_members        */
	0,                                        /* tp_getset         */
	0,                                        /* tp_base           */
	0,                                        /* tp_dict           */
	0,                                        /* tp_descr_get      */
	0,                                        /* tp_descr_set      */
	0,                                        /* tp_dictoffset     */
	0,                                        /* tp_init           */
	0,                                        /* tp_alloc          */
	TcpClient_New,                              /* tp_new            */
};

static const char module_docstring[] =
"A c-approach for microserver python client.";

static PyMethodDef ModuleMethods[] = {
	{"Update", (PyCFunction)Update, METH_O,
		"Select sockets with timeout."},
	{ NULL, NULL}
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef _module = {
	PyModuleDef_HEAD_INIT,
	"_client",
	module_docstring,
	-1,
	ModuleMethods, /* m_methods */
	NULL,
	NULL,
	NULL,
	NULL
};
#define INITFUNC PyInit__client
#define INITFUNC_ERRORVAL NULL
#else /* Python 2 */
#define INITFUNC init_client
#define INITFUNC_ERRORVAL
#endif

#ifdef __cplusplus
extern "C" {
#endif

PyMODINIT_FUNC INITFUNC(void) {
	PyObject* m;
#if PY_MAJOR_VERSION >= 3
	m = PyModule_Create(&_module);
#else
	m = Py_InitModule3("_client", ModuleMethods, module_docstring);
#endif
	if (!m) {
		return INITFUNC_ERRORVAL;
	}

	TcpClient_Type.ob_type = &PyType_Type;
	if (PyType_Ready(&TcpClient_Type) < 0) {
		Py_DECREF(m);
		return INITFUNC_ERRORVAL;
	}
	PyModule_AddObject(m, "TcpClient", (PyObject*)&TcpClient_Type);

#if PY_MAJOR_VERSION >= 3
	return m;
#endif
}

#ifdef __cplusplus
}
#endif
