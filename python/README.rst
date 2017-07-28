
Install:
	pip install microserver

Usage:
	import microserver
	import typy

	# Defination of Entity.
	class Set_Your_Entity_Name(microserver.Entity):
		A_Component_Name = typy.Instance(module.A_Component_Name)
		Another_Component_Name = typy.Instance(module.Another_Component_Name)

		# Create an instance of this Entity
		@classmethod
		def Create(cls):
			self = cls()
			self.A_Component_Name = module.A_Component_Name()
			self.Another_Component_Name = module.Another_Component_Name()
			return self


	# Defination of Component.
	class Set_Your_Component_Name(microserver.Component):
		A_Property_Name = typy.pb.Property_Type
		Another_Synchron_Property = typy.pb.Property_Type

		# Called on the entity of this component is awake
		def onAwake(self):
			...

		# Called on thie property 'A_Property_Name' is changed by server-side
		def A_Property_NameHandler(self):
			...

		# A RPC method
		@microserver.Message
		def A_RPC_Name(self, Param_Name_1 = typy.Param_Type, Param_Name_2 = typy.Param_Type):
			...

			# Awake an entity
			awakeEntity = self.Awake(entity)

			# Call a server-side method
			this.Entity.A_Server_Method(Some_Params)
