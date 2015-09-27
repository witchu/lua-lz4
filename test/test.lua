local lz4 = require("lz4")

local s = string.rep("0123456789", 100000) -- 1M
local e = lz4.compress(s)
local d = lz4.decompress_safe(e, #s)
assert(s == d)
print("ok")
