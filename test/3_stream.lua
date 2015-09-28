local lz4 = require("lz4")
local readfile = require("readfile")

local data = {
  string.rep("0123456789", 100000),
  "Hello, World!!",
  readfile("../LICENSE"),
  readfile("../lua_lz4.c"),
  "p = lua_newuserdata(L, sizeof(lz4_compress_stream_hc_t));",
  "static lz4_compress_stream_t *_checkcompressionstream(lua_State *L, int index)",
  "lz4_compress_stream_t *cs = _checkcompressionstream(L, 1);",
  "Hello, World!!",
  "lz4_compress_stream_hc_t *cs = _checkcompressionstream_hc(L, 1);",
}

local compressor = {
  lz4.new_compression_stream(),
  lz4.new_compression_stream_hc(),
}

local decompressor = {
  lz4.new_decompression_stream(),
  lz4.new_decompression_stream(),
  lz4.new_decompression_stream(),
  lz4.new_decompression_stream(),
}

local function decompress(e1, e2, s)
  local size = #s
  local d1 = decompressor[1]:decompress_safe(e1, size)
  assert(d1 == s)
  local d2 = decompressor[2]:decompress_fast(e1, size)
  assert(d2 == s)
  local d3 = decompressor[3]:decompress_safe(e2, size)
  assert(d3 == s)
  local d4 = decompressor[4]:decompress_fast(e2, size)
  assert(d4 == s)
end

for _, s in ipairs(data) do
  local e1 = compressor[1]:compress(s)
  local e2 = compressor[2]:compress(s)
  local b1 = lz4.block_compress(s)
  local b2 = lz4.block_compress_hc(s)
  decompress(e1, e2, s)
  print(#e1.."/"..#e2.."/"..#b1.."/"..#b2.."/"..#s)
end

print("ok")
