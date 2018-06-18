#-*- coding: utf-8 -*-
# Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
# Use of this source code is governed by The MIT License
# that can be found in the LICENSE file.

import common
from io import BytesIO
from typy import SymbolEncodedLen, EncodeSymbol, DecodeSymbol

try:
	import _client
	import _proto

	class MetaEnum(type):
		def __new__(mcs, clsname, bases, attrs):
			return getattr(_proto, '%s_Declare' % clsname)

	class MetaObject(type):
		def __new__(mcs, clsname, bases, attrs):
			return getattr(_proto, clsname)(attrs)

except ImportError:
	_client = None

	class MetaProto(type):
		def __new__(mcs, clsname, bases, attrs):
			try:
				import proto
				return getattr(proto, clsname)
			except ImportError:
				return super(MetaProto, mcs).__new__(mcs, clsname, bases, attrs)
	MetaEnum = MetaObject = MetaProto


class Enum(object):
	__metaclass__ = MetaEnum


class Object(object):
	__metaclass__ = MetaObject


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

		if _client is None:
			cls = super(MetaComponent, mcs).__new__(mcs, clsname, bases, attrs)
		else:
			cls = _client.Component(clsname, attrs)

		if clsname != 'Component':
			if clsname in mcs.Components and not getattr(mcs.Components[clsname], '____virtual__', False):
				raise TypeError, 'Component name "%s" already exists.' % clsname
			mcs.Components[clsname] = cls

		return cls


class Component(object):
	__metaclass__ = MetaComponent

	def __init__(self, entity):
		self.Entity = entity
		import proto
		typyDelegate = getattr(proto, self.__class__.__name__, None)
		self.____typyDelegate__ = typyDelegate and typyDelegate()

	def CreateEntity(self, i, k, t):
		return self.Entity.client.CreateEntity(i, k, t)

	def __getattr__(self, key):
		return getattr(self.____typyDelegate__, key)

	def Deserialize(self, data):
		self.____typyDelegate__ and self.____typyDelegate__.MergeFromString(data)

	def Awake(self, e):
		if e.isAwake:
			print '[Entity] Already awaked:', e
			return e

		client = self.Entity.client
		if e.ID in client.entities:
			client.entities[e.ID]
		entity = client.CreateEntity(e.ID, e.Key, e.Type)
		client.message(e, client.SymDict['OBSERVE'])
		client.entities[entity.ID] = entity
		return entity

	def Drop(self, e):
		if not e or not e.isAwake:
			print '[Entity] Not awaked:', e
			return

		for name in e.____dropComponents__:
			e.components[name].onDrop()
		for _, component in e.components.iteritems():
			delattr(component, 'Entity')

		e.isAwake = False
		client = self.Entity.client
		client.message(e, client.SymDict['IGNORE'])
		del client.entities[e.ID]
		return Entity(ID = e.ID, Key = e.Key, Type = e.Type)


class MetaEntity(type):
	Entities = {}

	def __new__(mcs, clsname, bases, attrs):
		components = {}
		awakeComps = set()
		dropComps = set()
		msgComps = {}
		for name, attr in attrs.iteritems():
			if isinstance(attr, type) and issubclass(attr, Component):
				components[name] = attr
				if hasattr(attr, 'onAwake'):
					awakeComps.add(name)
				if hasattr(attr, 'onDrop'):
					dropComps.add(name)
				for msg in attr.____messages__:
					if msg not in msgComps:
						msgComps[msg] = set()
					msgComps[msg].add(name)
		attrs['____awakeComponents__'] = tuple(awakeComps)
		attrs['____dropComponents__'] = tuple(dropComps)
		attrs['____msgComponents__'] = {k: tuple(v) for k, v in msgComps.iteritems()}
		attrs['____components__'] = components

		if _client is None:
			cls = super(MetaEntity, mcs).__new__(mcs, clsname, bases, attrs)
		else:
			cls = _client.Entity(clsname, attrs)

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
		self.Type = Type or ''

	def __getattr__(self, key):
		import proto
		for component in self.components:
			name = '%s_%sParam' % (component, key)
			if hasattr(proto, name):
				typyDelegate = getattr(proto, name)
				break
		else:
			raise AttributeError, 'Message "%s" does not exist.' % key
		def Message(self, *args):
			self.client.message(self, self.client.SymDict[key], typyDelegate(*args).SerializeToString())
		setattr(Entity, key, Message)
		return object.__getattribute__(self, key)

	def ByteSize(self):
		size = 1 + SymbolEncodedLen(self.Type)
		if self.ID != self.client.IDType[0]:
			size += self.client.IDType[1](self.ID)
		if self.Key != self.client.IDType[0]:
			size += self.client.IDType[1](self.Key)
		return size

	def Serialize(self):
		t = 0
		if self.ID != self.client.IDType[0]:
			t |= 1
		if self.Key != self.client.IDType[0]:
			t |= 2
		output = BytesIO()
		output.write(chr(t))
		if self.ID != self.client.IDType[0]:
			self.client.IDType[2](output.write, self.ID)
		if self.Key != self.client.IDType[0]:
			self.client.IDType[2](output.write, self.Key)
		output.write(EncodeSymbol(self.Type))
		return output.getvalue()

	def Deserialize(self, buffer):
		t, offset = buffer[0], 1
		if t & 1:
			self.ID, offset = self.client.IDType[3](buffer, offset)
		else:
			self.ID = self.client.IDType[0]
		if t & 2:
			self.Key, offset = self.client.IDType[3](buffer, offset)
		else:
			self.Key = self.client.IDType[0]
		self.Type = DecodeSymbol(buffer[offset:])


class BaseClient(object):
	def sendData(self):
		raise NotImplementedError

	def onConnect(self):
		self.entities = {}
		self.Symbols = None
		self.SymDict = None

	def handler(self, buffer):
		import proto
		ID, offset = self.IDType[3](buffer, 0)
		if self.Symbols is None:
			version = buffer[offset:offset+16]
			if version != proto.Version:
				print '[Connection] Client version error:', version, proto.version
				return
			offset += 16
			self.Symbols = proto.Symbols
			self.SymDict = {s: i for i, s in enumerate(proto.Symbols)}
			k, offset = self.IDType[3](buffer, offset)
			t, offset = common.readVarint(buffer, offset)
			entity = self.CreateEntity(ID, k, self.Symbols[t])
			self.entities[ID] = entity
		elif ID not in self.entities:
			print '[Connection] Cannot find entity:', ID
			return
		else:
			entity = self.entities[ID]

		while offset < len(buffer):
			method, offset = common.readVarint(buffer, offset)
			name = self.Symbols[method]
			buf, offset = common.readBytes(buffer, offset)
			if name in entity.components:
				entity.components[name].Deserialize(buf)
			elif not entity.isAwake:
				print '[Connection] Entity is not awake:', id, name, entity
				continue
			elif name == 'NOTIFY':
				c, off = common.readVarint(buf, 0)
				p, off = common.readVarint(buf, off)
				compName = self.Symbols[c]
				propName = self.Symbols[p]
				delegate = getattr(proto, '%s_%s' % (compName, propName))()
				delegate.MergeFromString(buf[off:])
				newValue = delegate.Args()[0]
				component = entity.components[compName]
				oldValue = getattr(component, propName)
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
					setattr(component, propName, newValue)
					handler and handler(oldValue, newValue)
			else:
				for component in entity.components:
					delegateName = '%s_%sParam' % (component, name)
					if hasattr(proto, delegateName):
						delegateCls = getattr(proto, delegateName)
						break
				else:
					raise AttributeError, 'Message "%s" does not exist.' % name
				delegate = delegateCls()
				delegate.MergeFromString(buf)
				args = delegate.Args()
				for n in entity.____msgComponents__[name]:
					getattr(entity.components[n], name)(*args)

		if entity and not entity.isAwake:
			entity.isAwake = True
			for name in entity.____awakeComponents__:
				entity.components[name].onAwake()

	def message(self, entity, method, data = None):
		t = self.SymDict[entity.Type] << 2
		if entity.ID != self.IDType[0]:
			t |= 1
		if entity.Key != self.IDType[0]:
			t |= 2
		output = BytesIO()
		common.writeVarint(output.write, t)
		if entity.ID != self.IDType[0]:
			self.IDType[2](output.write, entity.ID)
		if entity.Key != self.IDType[0]:
			self.IDType[2](output.write, entity.Key)
		common.writeVarint(output.write, method)
		if data is not None:
			output.write(data)
		self.sendData(output.getvalue())

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
