#-*- coding: utf-8 -*-
# Copyright 2017-2018 ibelie, Chen Jie, Joungtao. All rights reserved.
# Use of this source code is governed by The MIT License
# that can be found in the LICENSE file.

import io
import socket
import common
import classes
import asyncore
import traceback


def Update(timeout = 0.01):
	asyncore.loop(timeout = timeout, use_poll = True, count = 1)


class TcpClient(asyncore.dispatcher, classes.BaseClient):
	BUFFER_SIZE = 4096

	def __init__(self, host, port):
		super(TcpClient, self).__init__()
		import proto
		self.IDType = common.IDTypes[proto.IDType]
		self.write_buffer = io.BytesIO()
		self.read_buffer = ''
		self.host = host
		self.port = port
		self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
		self.connect((self.host, self.port))

	def handle_connect(self):
		self.onConnect()

	def handle_close(self):
		self.close()
		self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
		self.connect((self.host, self.port))

	def handle_read(self):
		self.read_buffer += self.recv(self.BUFFER_SIZE)
		data, self.read_buffer = common.unpack(self.read_buffer)
		if data is not None:
			try:
				self.handler(data)
			except Exception as e:
				print '[microserver] Client receive error:', e
				traceback.print_exc()

	def handle_write(self):
		data = self.write_buffer.getvalue()
		if data:
			self.write_buffer = io.BytesIO(data[self.send(data):])
			self.write_buffer.seek(0, io.SEEK_END)

	def writable(self):
		return self.write_buffer and self.write_buffer.getvalue()

	def sendData(self, data):
		self.write_buffer.write(common.pack(data))
