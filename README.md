# LZ4 binding for Lua

[LZ4] is a very fast compression and decompression algorithm. This Lua binding is in conformance with the LZ4 [block] and [frame] specifications and also support [streaming].

[![Build Status](https://travis-ci.org/witchu/lua-lz4.svg)](https://travis-ci.org/witchu/lua-lz4)
[![Build status](https://ci.appveyor.com/api/projects/status/1spury3s6lj9creg?svg=true)](https://ci.appveyor.com/project/witchu/lua-lz4)

## Example usage

Simple frame compression/decompression
```lua
local lz4 = require("lz4")
local s = "LZ4 is a very fast compression and decompression algorithm."
assert(lz4.decompress(lz4.compress(s)) == s)
```

## Build/Install Instructions

With luarocks:
```
luarocks install lua-lz4
```

With make:
```
export LUA_INCDIR=/path/to/lua_header
export LUA_LIBDIR=/path/to/liblua
make
```

## Documentations

### Frame
Easy to use and compressed data contain meta data such as checksum, decompress size, etc. The compressed/decompressed data in frame format can be exchange with other programs.

#### lz4.compress(input[, options])
Compress `input` and return compressed data.
* `input`: input string to be compressed.
* `options`: optional table that can be contains
  * `compression_level`: integer between 0 to 16
  * `auto_flush`: boolean
  * `block_size`: maximum block size can be `lz4.block_64KB`, `lz4.block_256KB.`, `lz4.block_1MB`, `lz4.block_4MB`
  * `block_independent`: boolean
  * `content_checksum`: boolean

#### lz4.decompress(input)
Decompress `input` and return decompressed data.
* `input`: input string to be decompressed.

### Block
Basic compression/decompression in plain block format. Require `decompress_length` to decompress data.

Example:
```lua
local lz4 = require("lz4")
local s = "LZ4 is a very fast compression and decompression algorithm."
assert(lz4.block_decompress_safe(lz4.block_compress(s), #s) == s)
```

#### lz4.block_compress(input[, accelerate])
Compress `input` and return compressed data.
* `input`: input string to be compressed.
* `accelerate`: optional integer

#### lz4.block_compress_hc(input[, compression_level])
Compress `input` in high compression mode and return compressed data.
* `input`: input string to be compressed.
* `compression_level`: optional integer

#### lz4.block_decompress_safe(input, decompress_length)
Decompress `input` and return decompressed data. This function is protected against buffer overflow exploits, including malicious data packets.
* `input`: input string to be decompressed.
* `decompress_length`: length of decompressed data (integer)

#### lz4.block_decompress_fast(input, decompress_length)
Decompress `input` and return decompressed data. It does not provide any protection against intentionally modified data stream (malicious input). Use this function in trusted environment only (data to decode comes from a trusted source).
* `input`: input string to be decompressed.
* `decompress_length`: length of decompressed data (integer)

### Stream
Use streaming to compress/decompress multiple blocks. The compressing blocks can be use content in previous blocks therefore more compression ratio. Decoding buffer should be either to get good performance.
* Exactly same size as encoding buffer, with same update rule (block boundaries at same positions)
      In which case, the decoding & encoding ring buffer can have any size, including very small ones ( < 64 KB).
* Larger than encoding buffer, by a minimum of maxBlockSize more bytes.
      maxBlockSize is implementation dependent. It's the maximum size you intend to compress into a single block.
      In which case, encoding and decoding buffers do not need to be synchronized,
      and encoding ring buffer can have any size, including small ones ( < 64 KB).
* _At least_ 64 KB + 8 bytes + maxBlockSize.
      In which case, encoding and decoding buffers do not need to be synchronized,
      and encoding ring buffer can have any size, including larger than decoding buffer.

Example:
```lua
local lz4 = require("lz4")
local s1 = "LZ4 is a very fast compression and decompression algorithm."
local s2 = "lua-lz4 - LZ4 binding for Lua"
local com = lz4.new_compression_stream()
local dec = lz4.new_decompression_stream()
assert(dec:decompress_safe(com:compress(s1), #s1) == s1)
assert(dec:decompress_safe(com:compress(s2), #s2) == s2)
```

#### lz4.new_compression_stream([ring_buffer_size[, accelerate]])
New a `lz4.compression_stream` object.
* `ring_buffer_size`: integer
* `accelerate`: integer

#### `lz4.compression_stream` methods
* `reset([dictionary])` forget internal dictionary or reset to new dictionary
* `compress(input)`

#### lz4.new_compression_stream_hc([ring_buffer_size[, compression_level]])
New a `lz4.compression_stream_hc` object.
* `ring_buffer_size`: integer
* `compression_level`: integer

#### `lz4.compression_stream_hc` methods
* `reset([dictionary])` forget internal dictionary or reset to new dictionary
* `compress(input)`

#### lz4.new_decompression_stream([ring_buffer_size])
New a `lz4.decompression_stream` object.
* `ring_buffer_size`: integer

#### `lz4.decompression_stream` methods
* `reset([dictionary])` forget internal dictionary or reset to new dictionary
* `decompress_safe(input, decompress_length)`
* `decompress_fast(input, decompress_length)`



[LZ4]: https://github.com/Cyan4973/lz4
[block]: https://github.com/Cyan4973/lz4/blob/master/lz4_Block_format.md
[frame]: https://github.com/Cyan4973/lz4/blob/master/lz4_Frame_format.md
[streaming]: https://github.com/Cyan4973/lz4/blob/master/examples/streaming_api_basics.md
