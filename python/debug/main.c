// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

#include "Python.h"

extern void init_client(void);

int main()
{
	Py_SetProgramName("testMicroserver");
	Py_Initialize();
	init_client();
	PyRun_SimpleString(
"import sys\n"
"sys.path.append('E:/test/microserver/go/src/github.com/ibelie/microserver/python/microserver')\n"
"sys.path.append('C:/Users/joung/Documents/project/microserver/go/src/github.com/ibelie/microserver/python/microserver')\n"
"sys.path.append('/home/joungtao/program/microserver/go/src/github.com/ibelie/microserver/python/microserver')\n"
"import client_test as test\n"
"test.setup()\n"
"try:\n"
"	test.test()\n"
"finally:\n"
"	test.teardown()\n"
);
	Py_Finalize();
	getchar();
    return 0;
}

