local lz4 = require("lz4")
local readfile = require("readfile")

local function decompress(s, e, size)
  local ds = lz4.block_decompress_safe(e, size)
  assert(s == ds)
  local df = lz4.block_decompress_fast(e, size)
  assert(s == df)
end

local function test_block(s)
  local e1 = lz4.block_compress(s)
  decompress(s, e1, #s)
  local e2 = lz4.block_compress_hc(s)
  decompress(s, e2, #s)
  print(#e1.."/"..#e2.."/"..#s)
end

test_block(string.rep("0123456789", 100000))
test_block(readfile("../lua_lz4.c"))
test_block(readfile("../LICENSE"))

print("ok")
