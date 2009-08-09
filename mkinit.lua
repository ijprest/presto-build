-- error if undefined global variable is used!
-- setmetatable(_G, { __index = function(self,key) error("undefined global variable '"..key.."'",2) end } )

-- helper to print debugging messages
make.debug_msg = function(s) end
-- make.debug_msg = function(s) print(unpack(s)) end

-- the "__target" table is the backing-store for "target"
local __target = {}
local __is_target = {}
__target[__is_target] = true
__target.mt = {
	__index = function(self,key)
		return __target[key]
	end
}


--[[SDOC---------------------------------------------------------------------
	Name: 	__target:new
	Action:	Constructor
---------------------------------------------------------------------EDOC]]--
function __target:new(t)
	t = t or {}
	t.deps = {}
	if self == target then
		-- Base class is a special case
		make.debug_msg{"instantiating base class..."}
		setmetatable(t, __target.mt)
	else
		-- this forms the basis of all our inheritance
		make.debug_msg{"instantiating derived class..."}
		self.__index = self
		setmetatable(t, self)
	end
	return t
end


--[[SDOC---------------------------------------------------------------------
	Name: 	__target:depends_on
	Action:	Add dependencies to the target
---------------------------------------------------------------------EDOC]]--
function __target:depends_on(deps_list)
	for k,v in ipairs(deps_list or {}) do
		-- string is assumed to be the name of a dependency
		if type(v) == "string" then
			self:depends_on{target[v]}

		-- a target directly used as a dependency
		elseif type(v) == "table" and v[__is_target] then
			if not(v.name) then error("dependency "..k.." is unnamed",2) end
			make.debug_msg{"adding dep",v.name,"to",self.name}
			if __target[v.name] then __target[v.name]:check_for_cycle(self.name,v.name,1) end
			self.deps[v.name] = true

		-- don't understand this target type
		else
			error("dependency "..k.." is not a valid dependency target",2)
		end
	end
end

--[[SDOC---------------------------------------------------------------------
	Name: 	__target:check_for_cycle()
	Action:	Traverse the dependency hierarchy and ensure there are no cycles
---------------------------------------------------------------------EDOC]]--
function __target:check_for_cycle(t, d, i)
	if self.name == t then error("cyclical dependency on target '"..t.."' . '"..d.."' . '"..t.."'",i+2) end
	for k,v in pairs(self.deps) do target[k]:check_for_cycle(t,d,i+1) end
end


-- the target table; holds all buildable targets, and acts as a prototype
-- for derived target types
target = {}
setmetatable(target, {
	--[[SDOC---------------------------------------------------------------------
		Name: 		target:__index
		Action:		Read access to "target" table
	---------------------------------------------------------------------EDOC]]--
	__index = function(self, key)
		-- enforce backslash policy
		if string.match(key,"\\") then error("target name should not contain backslashes",2) end
		-- if the target doesn't exist yet, return the key; this allows
		-- undefined targets to be named as dependencies
		return __target[key] or target:new{name=key}
	end,

	--[[SDOC---------------------------------------------------------------------
		Name: 		target:__newindex
		Action:		Write access to "target" table
	---------------------------------------------------------------------EDOC]]--
	__newindex = function(self, key, value)
		make.debug_msg{"Setting", key, value}
		-- enforce backslash policy
		if string.match(key,"\\") then error("target name should not contain backslashes",2) end
		-- prevent redefining protected members (e.g., "target.new")
		if __target[key] ~= nil then error("cannot modify protected value",2) end
		-- ensure that everything added to the table is actually a "target" object
		if type(value) ~= "table" or not(value[__is_target]) then error("rvalue is not a target",2) end
		-- first target specified is the "default goal" target
		if __target.__default == nil then __target.__default = value end
		value.name = key
		__target[key] = value
	end,
})

function __target:timestamp()
	if make.file.exists(self.name) then return make.file.time(self.name); else return 0; end
end
function __target:exists()
	return make.file.exists(self.name)
end

local status = { none = 0, updated = 1, error = 2 }

function __target:bring_up_to_date()
	if self.updated then return status.updated; end -- already done!
	if not(self.deps_newer) then self.deps_newer = {} end

	-- we must build if we don't exist yet
	local must_build = not(self:exists())

	-- loop over all dependencies
	for dep_name in pairs(self.deps) do
		-- update the dependency
		local dep = target[dep_name]
		local dep_status = dep:bring_up_to_date()
		if dep_status == status.updated then
			-- dependency was updated; we must build
			must_build = true; self.deps_newer[dep_name] = true
		elseif dep_status == status.none then
			-- dependency wasn't updated, but we might still need
			-- to build if it's newer
			local self_timestamp = self:timestamp(dep)
			local dep_timestamp = dep:timestamp()
			if self_timestamp <= dep_timestamp then
				must_build = true; self.deps_newer[dep_name] = true
			end
		elseif dep_status == status.error then
			-- ummm...
			error("error updating target '".. dep.name .."'")
		else
			error("unknown status value '".. dep_status .."'")
		end
	end

	-- do we need to build?
	if must_build then
		if self.command then
			-- run the update command
			self.command_status = self:command()
			self.updated = true
			return status.updated
		else
			-- don't know how to build; error
			error("no rule to make target '".. self.name .."'")
			return status.error
		end
	end

	-- no update was necessary
	return status.none
end


































