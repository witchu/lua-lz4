local lz4 = require("lz4")
local readfile = require("readfile")

local function test_frame(s)
  local e = lz4.compress(s)
  local d = lz4.decompress(e)
  assert(s == d)
  print(#e..'/'..#d)
end

test_frame(string.rep("0123456789", 100000))
test_frame(readfile("../lua_lz4.c"))
test_frame(readfile("../LICENSE"))

print("ok")
