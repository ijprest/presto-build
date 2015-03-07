-- Test all the helper functions
assert(make.path.get_name(make.path.current_file()) == "tests.lua")
assert(make.path.to_os("c:/path/to") == "c:\\path\\to")
assert(make.path.from_os("c:\\path\\to") == "c:/path/to")
assert(string.lower(make.path.short("c:/program files")) == "c:/progra~1")
assert(string.lower(make.path.long("c:/progra~1")) == "c:/program files")
assert(make.path.full("foo.txt") == make.path.combine(make.dir.cd(),"foo.txt"))
assert(make.path.canonicalize("c:/path/to/../to/../.") == "c:/path")
assert(make.path.add_slash("c:/path/to") == "c:/path/to/")
assert(make.path.add_slash("c:/path/to/") == "c:/path/to/")
assert(make.path.remove_slash("c:/path/to") == "c:/path/to")
assert(make.path.remove_slash("c:/path/to/") == "c:/path/to")
assert(make.path.remove_ext("foo.cpp") == "foo")
assert(make.path.remove_ext("c:/path/to/foo.cpp") == "c:/path/to/foo")
assert(make.path.remove_ext("c:/path/to/foo.2.cpp") == "c:/path/to/foo.2") -- double extension
assert(make.path.change_ext("foo.cpp",".obj") == "foo.obj")
assert(make.path.change_ext("c:/path/to/foo.cpp",".obj") == "c:/path/to/foo.obj")
assert(make.path.change_ext("c:/path/to/foo.2.cpp",".obj") == "c:/path/to/foo.2.obj") -- double extension
assert(make.path.change_ext("c:/path/to/foo",".obj") == "c:/path/to/foo.obj")
assert(make.path.combine("c:/path","to/foo.cpp") == "c:/path/to/foo.cpp")
assert(make.path.combine("c:/path/","to/foo.cpp") == "c:/path/to/foo.cpp")
assert(make.path.combine("c:/path/to","foo.cpp") == "c:/path/to/foo.cpp")
assert(make.path.combine("c:/path","to","foo.cpp") == "c:/path/to/foo.cpp")
assert(make.path.common("c:/path/to/1.cpp","c:/path/to/2.cpp") == "c:/path/to")
assert(make.path.common("c:/path/to/1.cpp","c:/path/two/2.cpp") == "c:/path")
assert(string.lower(make.path.where("notepad.exe")) == "c:/windows/system32/notepad.exe")
assert(make.path.where("notepad.exe",{"c:/windows","c:/windows/system32"}) == "c:/windows/notepad.exe")
assert(make.path.where("notepad.exe","c:/windows;c:/windows/system32") == "c:/windows/notepad.exe")
assert(make.file.exists("c:/windows/notepad.exe"))
assert(not(make.file.exists("a-file-that-should-not-exist")))
assert(make.path.remove_ext("c:/path°/to/foo.cpp") == "c:/path°/to/foo") -- Test UTF-8 round-tripping
-- make.path.combine supports the __tostring metamethod
test = setmetatable({}, {__tostring = function() return "test"; end})
assert(tostring(test) == "test")
assert(make.path.combine("c:/path/to",test) == "c:/path/to/test")


tempfile = make.file.temp()
assert(tempfile and make.file.exists(tempfile) and make.file.size(tempfile) == 0)
make.file.delete(tempfile)
assert(not(make.file.exists(tempfile)))
assert(make.file.time(tempfile) == nil)
make.file.touch(tempfile)
assert(make.file.exists(tempfile) and make.file.size(tempfile) == 0)
timestamp = make.file.time(tempfile)
assert(timestamp and timestamp >= 0)
make.file.touch(tempfile)
timestamp2 = make.file.time(tempfile)
assert(timestamp <= timestamp2) -- we need to sleep; otherwise the timestamps are
																-- always equal, and the test isn't very good
make.file.delete(tempfile)
assert(not(make.file.exists(tempfile)))

--
-- Stuff that hasn't been tested yet:
--

--[[
make.path.glob -- how to test reliably?
make.file.copy
make.file.md5
make.dir.is_dir
make.dir.is_empty
make.dir.temp
make.dir.cd
make.dir.md
make.dir.rd
make.proc.spawn
make.proc.flushio
make.proc.wait
make.proc.exit_code
make.now
make.md5
]]--
