#-*- coding: utf-8 -*-
# Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
# Use of this source code is governed by The MIT License
# that can be found in the LICENSE file.

import common
from io import BytesIO


def Property(func):
	func.____isPropertyHandler__ = True
	return func


def Message(func):
	func.____isMessage__ = True
	return func


def ServerPackage(package):
	def _ServerPackage(component):
		component.____package__ = package
		return component
	return _ServerPackage


class MetaComponent(type):
	Components = {}

	def __new__(mcs, clsname, bases, attrs):
		messages = {}
		for name, attr in attrs.iteritems():
			if getattr(attr, '____isMessage__', False):
				messages[name] = attr
		attrs['____messages__'] = messages

		properties = {}
		for name, attr in attrs.iteritems():
			if getattr(attr, '____isPropertyHandler__', False):
				properties[name] = attr
		for name in properties:
			attrs[name + 'Handler'] = attrs.pop(name)
		attrs['____properties__'] = properties

		cls = super(MetaComponent, mcs).__new__(mcs, clsname, bases, attrs)

		if clsname != 'Component':
			if clsname in mcs.Components and not getattr(mcs.Components[clsname], '____virtual__', False):
				raise TypeError, 'Component name "%s" already exists.' % clsname
			mcs.Components[clsname] = cls

		return cls


class Component(object):
	__metaclass__ = MetaComponent

	def __init__(self, entity):
		self.Entity = entity
		typyCls = getattr(entity.client.proto, self.__class__.__name__, None)
		self.____typyInst__ = typyCls and typyCls()

	def Deserialize(self, data):
		self.____typyInst__ and self.____typyInst__.MergeFromString(data)

	def Awake(self, e):
		if e.isAwake:
			print '[Entity] Already awaked:', e
			return e

		client = self.Entity.client
		if e.ID in client.entities:
			client.entities[e.ID]
		entity = client.CreateEntity(e.ID, e.Key, client.Dictionary[e.Type])
		client.message(e, client.Symbols['OBSERVE'])
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
		client = self.Entity.client
		client.message(e, client.Symbols['IGNORE'])
		del client.entities[e.ID]
		return Entity(ID = e.ID, Key = e.Key, Type = e.Type)


class MetaEntity(type):
	Entities = {}

	def __new__(mcs, clsname, bases, attrs):
		components = {}
		for name, attr in attrs.iteritems():
			if isinstance(attr, type) and issubclass(attr, Component):
				components[name] = attr
		attrs['____components__'] = components

		cls = super(MetaEntity, mcs).__new__(mcs, clsname, bases, attrs)

		if clsname != 'Entity':
			if clsname in mcs.Entities and not getattr(mcs.Entities[clsname], '____virtual__', False):
				raise TypeError, 'Entity name "%s" already exists.' % clsname
			mcs.Entities[clsname] = cls

		return cls


class Entity(object):
	__metaclass__ = MetaEntity

	def __init__(self, client, ID = None, Key = None, Type = None):
		self.client = client
		self.isAwake = False
		self.ID = ID or client.IDType[0]
		self.Key = Key or client.IDType[0]
		if Type is None:
			self.Type = 0
		else:
			self.Type = client.Symbols[Type]

	def ByteSize(self):
		size = common.sizeVarint(self.Type << 2)
		if self.ID != self.client.IDType[0]:
			size += self.client.IDType[1](self.ID)
		if self.Key != self.client.IDType[0]:
			size += self.client.IDType[1](self.Key)
		return size

	def SerializeUnsealed(self, write):
		t = self.Type << 2
		if self.ID != self.client.IDType[0]:
			t |= 1
		if self.Key != self.client.IDType[0]:
			t |= 2
		common.writeVarint(write, t)
		if self.ID != self.client.IDType[0]:
			self.client.IDType[2](write, self.ID)
		if self.Key != self.client.IDType[0]:
			self.client.IDType[2](write, self.Key)

	def Serialize(self):
		output = BytesIO()
		self.SerializeUnsealed(output.write)
		return output.getvalue()

	def Deserialize(self, buffer):
		t, offset = common.readVarint(buffer, 0)
		if t & 1:
			self.ID, offset = self.client.IDType[3](buffer, offset)
		else:
			self.ID = self.client.IDType[0]
		if t & 2:
			self.Key, offset = self.client.IDType[3](buffer, offset)
		else:
			self.Key = self.client.IDType[0]
		self.Type = t >> 2


class BaseClient(object):
	def __init__(self):
		self.entities = {}
		self.Symbols = None
		self.Dictionary = None

	def handler(self, buffer):
		ID, offset = self.IDType[3](buffer, 0)
		if self.Symbols is None:
			offset, self.Symbols, self.Dictionary = common.readSymbols(buffer, offset)
			k, offset = self.IDType[3](buffer, offset)
			t, offset = common.readVarint(buffer, offset)
			entity = self.CreateEntity(ID, k, self.Dictionary[t])
			self.entities[ID] = entity
		elif ID not in self.entities:
			print '[Connection] Cannot find entity:', ID
			return
		else:
			entity = self.entities[ID]

		while offset < len(buffer):
			method, offset = common.readVarint(buffer, offset)
			name = self.Dictionary[method]
			buf, offset = common.readBytes(buffer, offset)
			if name in entity.components:
				entity.components[name].Deserialize(buf)
			elif not entity.isAwake:
				print '[Connection] Entity is not awake:', id, name, entity
				continue
			elif name == 'NOTIFY':
				c, off = common.readVarint(buf, 0)
				p, off = common.readVarint(buf, off)
				compName = self.Dictionary[c]
				propName = self.Dictionary[p]
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

	def CreateEntity(self, i, k, t):
		eType = MetaEntity.Entities[t]
		entity = eType(self, i, k, t)
		components = {}
		for cName, cType in eType.____components__.iteritems():
			component = cType(entity)
			components[cName] = component
			setattr(entity, cName, component)
		entity.components = components
		hasattr(entity, 'onCreate') and entity.onCreate()
		return entity
