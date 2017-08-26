#-*- coding: utf-8 -*-
# Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
# Use of this source code is governed by The MIT License
# that can be found in the LICENSE file.

import struct
from io import BytesIO


def sizeVarint(x):
	n = 0
	while 1:
		n += 1
		x >>= 7
		if x == 0:
			break
	return n

def writeVarint(write, data):
	while data >= 0x80:
		write(struct.pack('>B', (data & 0x7F) | 0x80))
		data >>= 7
	write(struct.pack('>B', data & 0x7F))

def readVarint(buffer, offset):
	s = x = 0
	i = 0
	l = len(buffer) - offset
	while i < l:
		b = ord(buffer[offset + i])
		if b & 0x80:
			x |= (b & 0x7f) << s
			s += 7
			i += 1
			continue
		elif i > 9 or (i == 9 and b > 1):
			raise RuntimeError, 'Varint overflow: %s' % repr(buffer[offset:i])
		x |= b << s
		offset += i + 1
		break
	return x, offset


def writeBytes(write, data):
	writeVarint(write, len(data))
	write(data)

def readBytes(buffer, offset):
	length, offset = readVarint(buffer, offset)
	length += offset
	return buffer[offset:length], length


C2BMap = {}
B2CMap = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' + 'abcdefghijklmnopqrstuvwxyz' + '0123456789' + '-_'

for i in xrange(len(B2CMap)):
	C2BMap[B2CMap[i]] = i

def writeBase64(write, data):
	i = 0
	l = len(data)
	while i + 1 < l:
		byte1 = C2BMap[data[i]]
		i += 1
		byte2 = C2BMap[data[i]]
		i += 1
		write(struct.pack('>B', (byte1 << 2) | (byte2 >> 4)))
		if i < l:
			byte3 = C2BMap[data[i]]
			i += 1
			write(struct.pack('>B', ((byte2 << 4) & 0xF0) | (byte3 >> 2)))
			if i < l:
				byte4 = C2BMap[data[i]]
				i += 1
				write(struct.pack('>B', ((byte3 << 6) & 0xC0) | byte4))

def readBase64(buffer, offset, count):
	end = offset + count
	output = BytesIO()

	while offset < end:
		byte1 = ord(buffer[offset])
		offset += 1
		output.write(B2CMap[byte1 >> 2])
		if offset < end:
			byte2 = ord(buffer[offset])
			offset += 1
			output.write(B2CMap[((byte1 & 0x03) << 4) | (byte2 >> 4)])
			if offset < end:
				byte3 = ord(buffer[offset])
				offset += 1
				output.write(B2CMap[((byte2 & 0x0F) << 2) | (byte3 >> 6)])
				output.write(B2CMap[byte3 & 0x3F])
			else:
				output.write(B2CMap[(byte2 & 0x0F) << 2])
		else:
			output.write(B2CMap[(byte1 & 0x03) << 4])

	return output.getvalue(), offset


IDTypes = {
	'RUID': ('AAAAAAAAAAA', lambda v: 8, writeBase64, lambda b, o: readBase64(b, o, 8)),
	'UUID': ('AAAAAAAAAAAAAAAAAAAAAA', lambda v: 16, writeBase64, lambda b, o: readBase64(b, o, 16)),
	'STRID': ('', lambda v: sizeVarint(len(v)) + len(v), writeBytes, readBytes),
}


def readSymbols(buffer, offset):
	Symbols = {}
	Dictionary = {}
	buf, offset = readBytes(buffer, offset)
	off = 0
	while off < len(buf):
		symbol, off = readBytes(buf, off)
		value, off = readVarint(buf, off)
		Symbols[symbol] = value
		Dictionary[value] = symbol
	return offset, Symbols, Dictionary


def pack(data):
	output = BytesIO()
	writeBytes(output.write, data)
	return output.getvalue()


def unpack(buffer):
	l = len(buffer)
	if l <= 0:
		return None, buffer

	length, offset = readVarint(buffer, 0)
	length += offset

	if offset != 0 and length <= l:
		return buffer[offset:length], buffer[length:]

	return None, buffer
