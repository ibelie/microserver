#-*- coding: utf-8 -*-
# Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
# Use of this source code is governed by The MIT License
# that can be found in the LICENSE file.

import common
from io import BytesIO


def Property(func):
	return func


def Message(func):
	return func


class MetaEntity(type):
	Entities = {}

	@classmethod
	def Create(self, t):
		return None


class Entity(object):
	__metaclass__ = MetaEntity

	def __init__(self, ID = common.IDType[0], Key = common.IDType[0], Type = None):
		self.isAwake = False
		self.ID = ID
		self.Key = Key
		if Type is None:
			self.Type = 0
		else:
			self.Type = common.Symbols[Type]

	def ByteSize(self):
		size = common.sizeVarint(self.Type << 2)
		if self.ID != common.IDType[0]:
			size += common.IDType[1](self.ID)
		if self.Key != common.IDType[0]:
			size += common.IDType[1](self.Key)
		return size

	def SerializeUnsealed(self, write):
		t = self.Type << 2
		if self.ID != common.IDType[0]:
			t |= 1
		if self.Key != common.IDType[0]:
			t |= 2
		common.writeVarint(write, t)
		if self.ID != common.IDType[0]:
			common.IDType[2](write, self.ID)
		if self.Key != common.IDType[0]:
			common.IDType[2](write, self.Key)

	def Serialize(self):
		output = BytesIO()
		self.SerializeUnsealed(output.write)
		return output.getvalue()

	def Deserialize(self, buffer):
		t, offset = common.readVarint(buffer, 0)
		if t & 1:
			self.ID, offset = common.IDType[3](buffer, offset)
		else:
			self.ID = common.IDType[0]
		if t & 2:
			self.Key, offset = common.IDType[3](buffer, offset)
		else:
			self.Key = common.IDType[0]
		self.Type = t >> 2


class MetaComponent(type):
	Components = {}


class Component(object):
	__metaclass__ = MetaComponent

	def __init__(self, entity):
		self.Entity = entity

	def Awake(self, e):
		if e.isAwake:
			print '[Entity] Already awaked:', e
			return e

		client = self.Entity.client
		if e.ID in client.entities:
			client.entities[e.ID]
		entity = MetaEntity.Create(e.Type)
		entity.ID = e.ID
		entity.Key = e.Key
		entity.Type = e.Type
		entity.client = client
		client.message(e, common.Symbols['OBSERVE'])
		client.entities[entity.ID] = entity
		return entity

	def Drop(self, e):
		if not e or not e.isAwake:
			print '[Entity] Not awaked:', e
			return

		for name in e.DropComp:
			e.components[name].onDrop()
		for _, component in e.components.iteritems():
			delattr(component, 'Entity')

		e.isAwake = False
		client = self.Entity.connection
		client.message(e, common.Symbols['IGNORE'])
		del client.entities[e.ID]
		return Entity(ID = e.ID, Key = e.Key, Type = e.Type)


class BaseClient(object):
	def __init__(self):
		self.entities = {}

	def handler(self, buffer):
		ID, offset = common.IDType[3](buffer, 0)
		if common.Symbols is None:
			offset = common.readSymbols(buffer, offset)
			t, offset = common.readVarint(buffer, offset)
			entity = MetaEntity.Create(t)
			entity.client = self
			entity.ID = ID
			entity.Key = common.IDType[0]
			entity.Type = t
			self.entities[ID] = entity
		elif ID not in self.entities:
			print '[Connection] Cannot find entity:', ID
			return
		else:
			entity = self.entities[ID]

		while offset < len(buffer):
			method, offset = common.readVarint(buffer, offset)
			name = common.Dictionary[method]
			buf, offset = common.readBytes(buffer, offset)
			if name in entity.components:
				entity.components[name].MergeFromString(buf)
			elif not entity.isAwake:
				print '[Connection] Entity is not awake:', id, name, entity
				continue
			elif name == 'NOTIFY':
				c, off = common.readVarint(buf, 0)
				p, off = common.readVarint(buf, off)
				compName = common.Dictionary[c]
				propName = common.Dictionary[p]
				newValue = common[compName]['Deserialize' + propName](buffer.Bytes())[0]
				component = entity.components[compName]
				oldValue = component[propName]
				handler = getattr(component, propName + 'Handler', None)
				if hasattr(newValue, 'iteritems'):
					for k, n in newValue.iteritems():
						o = oldValue.get(k)
						oldValue[k] = n
						handler and handler(k, o, n)
				elif hasattr(newValue, 'extend'):
					length = len(oldValue)
					oldValue.extend(newValue)
					handler and handler(oldValue[:length], newValue)
				else:
					component[propName] = newValue
					handler and handler(oldValue, newValue)
			else:
				args = entity['Deserialize' + name](buf)
				for n in entity.MessageComp[name]:
					getattr(entity.components[n], name)(*args)

		if entity and not entity.isAwake:
			entity.isAwake = True
			for name in entity.AwakeComp:
				entity.components[name].onAwake()

	def message(self, entity, method, data = None):
		output = BytesIO()
		entity.SerializeUnsealed(output.write)
		common.writeVarint(output.write, method)
		if data is not None:
			output.write(data)
		self.send(output.getvalue())
