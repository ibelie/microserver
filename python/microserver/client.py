#-*- coding: utf-8 -*-
# Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
# Use of this source code is governed by The MIT License
# that can be found in the LICENSE file.

class TcpClient(object):
	def __init__(self, ip, port):
		self.ip = ip
		self.port = port
