-- error if undefined global variable is used!
-- setmetatable(_G, { __index = function(self,key) error("undefined global variable '"..key.."'",2) end } )

-- helper to print debugging messages
make.debug_msg = function(s) end
-- make.debug_msg = function(s) print(unpack(s)) end

-- the "__target" table is the backing-store for "target"
local __target = {}
local __is_target = {}
__target[__is_target] = true
__target.mt = { __index = __target }

make.status = { none = 0, updated = 1, error = 2 }

--[[-------------------------------------------------------------------------
	Name: 	__target:new
	Action:	Constructor
-------------------------------------------------------------------------]]--
function __target:new(t)
	t = t or {}
	t.deps = {}
	if self == target then
		-- Base class is a special case
		setmetatable(t, __target.mt)
	else
		-- this forms the basis of all our inheritance
		self.__index = self
		setmetatable(t, self)
	end
	return t
end


--[[-------------------------------------------------------------------------
	Name: 	__target:depends_on
	Action:	Add dependencies to the target
-------------------------------------------------------------------------]]--
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

--[[-------------------------------------------------------------------------
	Name: 	__target:check_for_cycle()
	Action:	Traverse the dependency hierarchy and ensure there are no cycles
-------------------------------------------------------------------------]]--
function __target:check_for_cycle(t, d, i)
	if self.name == t then error("cyclical dependency on target '"..t.."' . '"..d.."' . '"..t.."'",i+2) end
	for k,v in pairs(self.deps) do target[k]:check_for_cycle(t,d,i+1) end
end

--[[-------------------------------------------------------------------------
	Name: 	__target:bring_up_to_date()
	Action:	Brings a target (and all its dependencies) up to date
-------------------------------------------------------------------------]]--
function __target:bring_up_to_date()
	if self.updated then return make.status.updated; end -- already done!
	if not(self.deps_newer) then self.deps_newer = {} end

	-- we must build if we don't exist yet
	local must_build = not(self:exists())

	-- loop over all dependencies
	for dep_name in pairs(self.deps) do
		-- update the dependency
		local dep = target[dep_name]
		local dep_status = dep:bring_up_to_date()
		if dep_status == make.status.updated then
			-- dependency was updated; we must build
			must_build = true; self.deps_newer[dep_name] = true
		elseif dep_status == make.status.none then
			-- dependency wasn't updated, but we might still need
			-- to build if it's newer
			local self_timestamp = self:timestamp(dep)
			local dep_timestamp = dep:timestamp()
			if self_timestamp < dep_timestamp then
				must_build = true; self.deps_newer[dep_name] = true
			end
		elseif dep_status == make.status.error then
			-- ummm...
			error("error updating target '".. dep.name .."'")
		else
			error("unknown make.status value '".. dep_status .."'")
		end
	end

	-- do we need to build?
	if must_build then
		if self.command then
			-- run the update command
			make.jobs:start(self.command, self)
			make.jobs:dispatch()
			self.updated = true
			return make.status.updated
		else
			-- don't know how to build; error
			error("no rule to make target '".. self.name .."'")
			return make.status.error
		end
	end

	-- no update was necessary
	return make.status.none
end

--[[-------------------------------------------------------------------------
	Name: 	__target:timestamp()
					__target:exists()
	Action:	Default targets use on-disk timestamps
-------------------------------------------------------------------------]]--
function __target:timestamp()
	if make.file.exists(self.name) then return make.file.time(self.name); else return 0; end
end
function __target:exists()
	return make.file.exists(self.name)
end


-- the target table; holds all buildable targets, and acts as a prototype
-- for derived target types
target = {}
setmetatable(target, {
	--[[-------------------------------------------------------------------------
		Name: 		target:__index
		Action:		Read access to "target" table
	-------------------------------------------------------------------------]]--
	__index = function(self, key)
		-- enforce backslash policy
		if string.match(key,"\\") then error("target name should not contain backslashes",2) end
		-- if the target doesn't exist yet, return the key; this allows
		-- undefined targets to be named as dependencies
		return __target[key] or target:new{name=key}
	end,

	--[[-------------------------------------------------------------------------
		Name: 		target:__newindex
		Action:		Write access to "target" table
	-------------------------------------------------------------------------]]--
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


--[[-------------------------------------------------------------------------
	Name: 	make.jobs.dispatch
	Action:	Cycle through and resume all running jobs
-------------------------------------------------------------------------]]--
make.jobs = {	pos = 0, running = {} }
make.jobs.dispatch = function(self)
	while true do
		--print("make.jobs.dispatch: dispatching jobs")
		local shouldwait = true
		local waiting = {}
		local job_count = 0
		-- iterate over all the jobs
		for jobid,job in pairs(self.running) do
			job_count = job_count + 1
			-- restart the thread and let it do some work
			make.jobs.current = job
			local ok, proc = coroutine.resume(job.co)
			make.jobs.current = nil
			if not ok or coroutine.status(job.co) == "dead" then
				-- job finished; remove from list
				self.running[jobid] = nil
			elseif proc ~= nil then
				-- job still running, but waiting on an external process
				job.proc = proc
				table.insert(waiting,proc)
			else
				-- job has real work to do
				shouldwait = false
			end
		end
		--print("make.jobs.dispatch: dispatched "..tostring(job_count).." jobs")
		if job_count == 0 then break; end -- no more running jobs
	end
end

--[[-------------------------------------------------------------------------
	Name: 	make.jobs.start
	Action:	Start a job coroutine
-------------------------------------------------------------------------]]--
make.jobs.start = function(self, fn, target)
	-- increment the job number
	self.pos = self.pos + 1
	-- create & start the coroutine
	local co = coroutine.create(fn)
	self.current	= { id = self.pos, co = co }
	local status, proc = coroutine.resume(co, target)
	-- insert the new coroutine into the list of running jobs
	if status and coroutine.status(co) ~= "dead" then
		self.current.proc = proc
		self.running[self.pos] = self.current
	end
	self.current = nil
end

--[[-------------------------------------------------------------------------
	Name: 	make.run
	Action:	Run an external program (within a job coroutine!)
-------------------------------------------------------------------------]]--
make.run = function(command, printfn)
	-- spawn a new process
	local proc = make.proc.spawn(command)
	proc.print = printfn or print
	-- pipe all output until the process exits
	while proc.exit_code == nil do
		coroutine.yield(proc)
		make.proc.flushio(proc)
	end
end


-- redefine "print" for the function; our new version will
-- prepend every line with the job number
local oldprint = print
print = function(p1,...)
	if make.jobs.current then
		oldprint(make.jobs.current.id .. ": " .. tostring(p1), ...)
	else
		oldprint(p1,...)
	end
end



--
-- Phony targets don't correspond to actual files on disk
--
phony_target = target:new{}
-- "empty" command to run by default, so presto won't raise
-- an error if the command isn't defined
phony_target.command = function() end
-- always considered to not exist (even if there happened
-- to be a file with the same name); as a result, phony
-- targets will always try to be updated
phony_target.exists = function() return false; end
phony_target.timestamp = function(self) return 0; end





























