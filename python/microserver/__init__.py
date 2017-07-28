# -*- coding: utf-8 -*-
# Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
# Use of this source code is governed by The MIT License
# that can be found in the LICENSE file.

__version__ = '0.0.1'

try:
	from _client import TcpClient
	IMPLEMENTATION_TYPE = 'c'
except ImportError:
	from client import TcpClient
	IMPLEMENTATION_TYPE = 'python'
