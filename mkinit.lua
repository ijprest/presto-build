-- error if undefined global variable is used!
-- setmetatable(_G, { __index = function(self,key) error("undefined global variable '"..key.."'",2) end } )

-- helper to print debugging messages
make.debug_msg = function(s) end
-- make.debug_msg = function(s) print(unpack(s)) end

-- Handle case-insensitive environment
setmetatable(make.env, {
	__index = function(self, key) return rawget(self, string.upper(key)); end,
	__newindex = function(self, key, value) return rawset(self, string.upper(key), value); end,
})

--[[-------------------------------------------------------------------------
	Name: 	make.util.target_list
	Action:	a list of targets (e.g., dependencies)
-------------------------------------------------------------------------]]--
make.util = {}
make.util.target_list = {}
make.util.target_list_mt = { __index = make.util.target_list }
setmetatable(make.util.target_list, make.util.target_list_mt)
make.util.target_list.new = function(self, t)
	return setmetatable(t or {}, make.util.target_list_mt)
end
make.util.target_list_mt.__tostring = function(self)
	local txt = ""
	for dep_name in pairs(self) do
		if #txt > 0 then txt = txt .. " "; end
		txt = txt .. make.path.quote(dep_name)
	end
	return txt
end

--[[-------------------------------------------------------------------------
	Name: 	make.util.target_list.filter, make.util.target_list.filter_out
	Action:	filter a dependency list by the specified extension
-------------------------------------------------------------------------]]--
function make.util.target_list:filter(ext)
	local filtered = make.util.target_list:new{}
	for dep_name,v in pairs(self) do
		if make.path.get_ext(dep_name) == ext then filtered[dep_name] = v; end
	end
	return filtered
end
function make.util.target_list:filter_out(ext)
	local filtered = make.util.target_list:new{}
	for dep_name,v in pairs(self) do
		if make.path.get_ext(dep_name) ~= ext then filtered[dep_name] = v; end
	end
	return filtered
end



-- the "__target" table is the backing-store for "target"
local __target = {}
local __is_target = {}
__target[__is_target] = true
__target.mt = { __index = __target }

make.status = { none = 0, updated = 1, running = 2, error = 3 }

--[[-------------------------------------------------------------------------
	Name: 	__target:new
	Action:	Constructor
-------------------------------------------------------------------------]]--
function __target:new(t)
	t = t or {}
	t.deps = make.util.target_list:new{}
	if self == target then
		-- Base class is a special case
		setmetatable(t, __target.mt)
	else
		-- this forms the basis of all our inheritance
		self.__index = self
		setmetatable(t, self)
	end
	if t[1] then target[t[1]] = t; t[1] = nil; end
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
			if not(v.name) then error("Dependency '"..k.."' is unnamed.",2) end
			make.debug_msg{"adding dep",v.name,"to",self.name}
			if __target[v.name] then __target[v.name]:check_for_cycle(self.name,v.name,1) end
			self.deps[v.name] = true

		-- don't understand this target type
		else
			error("Dependency "..k.." is not a valid dependency target.",2)
		end
	end
end

--[[-------------------------------------------------------------------------
	Name: 	__target:check_for_cycle()
	Action:	Traverse the dependency hierarchy and ensure there are no cycles
-------------------------------------------------------------------------]]--
function __target:check_for_cycle(t, d, i)
	if self.name == t then error("Cyclical dependency on target '"..t.."' . '"..d.."' . '"..t.."'.",i+2) end
	for k,v in pairs(self.deps) do target[k]:check_for_cycle(t,d,i+1) end
end

--[[-------------------------------------------------------------------------
	Name: 	__target:bring_up_to_date()
	Action:	Brings a target (and all its dependencies) up to date
-------------------------------------------------------------------------]]--
function __target:bring_up_to_date()
	if self.status == make.status.updated or -- already done!
		 self.status == make.status.running or -- still running!
		 self.status == make.status.error then -- failed!
		return self.status
	end
	if not(self.deps_newer) then self.deps_newer = make.util.target_list:new{}; end

	-- we must build if we don't exist yet
	local must_build = make.flags.always_make or not(self:exists())
	local must_wait = false

	-- loop over all dependencies
	for dep_name in pairs(self.deps) do
		-- update the dependency
		local dep = target[dep_name]
		local ok, dep_status = pcall(dep.bring_up_to_date,dep)
		if not ok then dep_status = make.status.error; end

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
			print("presto: *** Error updating target '".. dep.name .."':")
			if dep.errmsg then print("presto: *** "..dep.errmsg) end
			if not make.flags.keep_going then
				self.status = make.status.error
				return self.status
			end
			-- prune the dep so we don't keep trying (and printing errors)
			self.deps[dep_name] = nil
			self.dep_status = make.status.error
		elseif dep_status == make.status.running then
			-- dependency is being built
			must_wait = true
			-- if job slots are full then break
			if make.jobs.count >= make.jobs.slots then break; end
		else
			error("Unknown make.status value '".. dep_status .."'.")
		end
	end

	-- are any of our children currently building?
	if must_wait then
		return make.status.running
	end

	-- if any deps failed, then we can't continue (even with -k)
	if self.dep_status == make.status.error then
		self.status = make.status.error
		return self.status
	end

	-- do we need to build?
	if must_build then
		if make.flags.question and self.command ~= phony_target.command then
			-- not a true error; code just indicates that the target status
			self.status = make.status.error
			self.errmsg = "Must remake target '" .. self.name .. "'"
			error(self.errmsg,0)
		end

		if self.command then
			-- run the update command
			return make.jobs.start(self)
		elseif not(make.flags.always_make) or not(self:exists()) then
			-- don't know how to build; error
			self.status = make.status.error
			self.errmsg = "No rule to make target '".. self.name .."'"
			error(self.errmsg,0)
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
		if __target[key] ~= nil then error("cannot modify protected value (or existing target) '"..tostring(key).."'",2) end
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
make.jobs = {	pos = 0, slots = 1, count = 0, running = {} }
make.jobs.dispatch = function(self)
	while true do
		local shouldwait = true
		local waiting = {}
		local job_count = make.jobs.count
		-- iterate over all the jobs
		for jobid,job in pairs(make.jobs.running) do
			-- restart the thread and let it do some work
			make.jobs.current = job
			local ok, proc = coroutine.resume(job.co)
			make.jobs.current = nil
			if not ok or coroutine.status(job.co) == "dead" then
				-- job finished; remove from list
				make.jobs.running[jobid] = nil
				for target_name in pairs(job.targets) do
					target[target_name].status = make.status.updated
					if not ok then
						target[target_name].status = make.status.error
						target[target_name].errmsg = proc
					end
				end
				make.jobs.count = make.jobs.count - 1
				if not ok then
					local errmsg = ""
					for target_name in pairs(job.targets) do errmsg = errmsg .. " '" .. target_name .. "'"; end
					print("presto: *** Error updating target"..errmsg..".")
					if proc then print("presto: *** "..proc); end
					make.exit()
				end
			elseif proc ~= nil then
				-- job still running, but waiting on an external process
				job.proc = proc
				table.insert(waiting,proc)
			else
				-- job has real work to do
				shouldwait = false
			end
		end
		if job_count ~= make.jobs.count and make.jobs.count < make.jobs.slots then break; end -- open slots
		if job_count == 0 then break; end -- no more running jobs

		-- wait for some change in job status
		make.proc.wait(waiting)
	end
end

--[[-------------------------------------------------------------------------
	Name: 	make.jobs.start
	Action:	Start a job coroutine
-------------------------------------------------------------------------]]--
make.jobs.start = function(target)
	-- increment the job number
	make.jobs.pos = make.jobs.pos + 1

	-- create & start the coroutine
	local co = coroutine.create(target.command)
	make.jobs.current = { id = make.jobs.pos, co = co, targets = {} }
	make.jobs.current.targets[target.name] = true
	local ok, proc = coroutine.resume(co, target)
	if not ok then
		-- coroutine threw an error
		target.status = make.status.error
		target.errmsg = proc
	elseif coroutine.status(co) ~= "dead" then
		-- insert the new coroutine into the list of running jobs
		make.jobs.current.proc = proc
		make.jobs.running[make.jobs.pos] = make.jobs.current
		make.jobs.count = make.jobs.count + 1
		target.status = make.status.running -- job is running
	else
		-- job is not running (simple; already finished)
		target.status = make.status.updated
	end
	make.jobs.current = nil
	return target.status
end

--[[-------------------------------------------------------------------------
	Name: 	make.run
	Action:	Run an external program (within a job coroutine!)
-------------------------------------------------------------------------]]--
make.run = function(command, env, printfn)
	-- spawn a new process
	printfn = printfn or print
	if make.flags.noisy then printfn(command); end
	local proc = make.proc.spawn(command, env)
	proc.print = printfn
	-- pipe all output until the process exits
	local exit_code = make.proc.exit_code(proc)
	while exit_code == nil do
		coroutine.yield(proc)
		make.proc.flushio(proc)
		exit_code = make.proc.exit_code(proc)
	end
	-- throw error if command failed
	if exit_code ~= 0 then
		error("[".. command .."] Error "..tostring(exit_code),0)
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


--[[-------------------------------------------------------------------------
	Name: 	make.update.goals
	Action:	the main loop; iterates over all the goals and updates them,
					dispatching running jobs as necessary
-------------------------------------------------------------------------]]--
make.goals = make.util.target_list:new{}
function make.update_goals()
	if not next(make.goals) then
		if __target.__default == nil then error("*** No targets.  Stop.",0); end
		make.goals[target.__default.name] = true
	end

	while true do
		local job_count = make.jobs.count
		local goal_count = 0

		-- loop over all our goals
		for goal_name in pairs(make.goals) do
			goal_count = goal_count + 1

			-- bring the current goal up to date
			local goal = target[goal_name]
			local ok,goal_status = pcall(goal.bring_up_to_date,goal)
			if not ok then
				goal.errmsg = goal_status
				goal_status = make.status.error
			end

			if goal_status ~= make.status.running then
				if goal_status == make.status.error then
					-- goal finished with an error
					if not make.flags.question then
						print("presto: *** Error updating goal '".. goal_name .."'.")
					end
					if goal.errmsg then print("presto: *** " .. goal.errmsg) end
					make.exit()
				elseif goal_status == make.status.none then
					-- goal was already up to date
					print("presto: *** nothing to be done for '".. goal_name .."'.")
				else -- goal_status == make.status.updated
					-- goal updated successfully
					print("presto: *** target '".. goal_name .."' is up to date")
				end

				-- done with this target; remove it from the list
				make.goals[goal_name] = nil
			end

			if make.jobs.count >= make.jobs.slots then break; end
		end

		-- dispatch any running jobs
		if make.jobs.count > 0 and (make.jobs.count == job_count or make.jobs.count >= make.jobs.slots) then
			make.jobs.dispatch()
		end

		-- stop if we've run out of goals
		if goal_count == 0 then break; end
	end

	-- if there were any errors, print a final message and exit
	if make.failure then
		print("presto: *** One or more targets not remade due to errors.")
		error("Stop.",0) -- this should be unhandled; will reach the OS
	end
end

function make.exit(exit_code)
	if not make.flags.keep_going then
		if make.jobs.count > 0 then
			print("presto: *** Waiting for other jobs to finish...")
			while make.jobs.count > 0 do
				pcall(make.jobs.dispatch) -- use pcall to suck up errors
			end
			print("presto: *** One or more targets not remade due to errors.")
		end
		error("Stop.",0) -- this should be unhandled; will reach the OS
	end
	make.failure = true
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


