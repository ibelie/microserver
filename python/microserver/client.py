#-*- coding: utf-8 -*-
# Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
# Use of this source code is governed by The MIT License
# that can be found in the LICENSE file.

import socket
import thread
import common
import classes
import traceback


class TcpClient(classes.BaseClient):
	BUFFER_SIZE = 4096

	def __init__(self, ip, port):
		super(TcpClient, self).__init__()
		import proto
		self.IDType = common.IDTypes[proto.IDType]
		self.ip = ip
		self.port = port
		self.connect()
		thread.start_new_thread(self.recv, ())

	def connect(self):
		self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 5)
		self.socket.connect((self.ip, self.port))

	def disconnect(self):
		self.socket.close()
		self.socket = None

	def recv(self):
		buffer = None
		while 1:
			try:
				if buffer is None:
					buffer = self.socket.recv(self.BUFFER_SIZE)
				else:
					buffer += self.socket.recv(self.BUFFER_SIZE)
				data, buffer = common.unpack(buffer)
				if data is None:
					continue

				self.handler(data)

			except socket.error as e:
				print '[microserver] Socket error, reconnect:', e
				self.disconnect()
				self.connect()
			except Exception as e:
				print '[microserver] Client receive error:', e
				traceback.print_exc()
				break
		self.socket.close()

	def send(self, data):
		self.socket.sendall(common.pack(data))
