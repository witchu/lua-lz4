# LZ4 binding for Lua

[LZ4] is a very fast compression and decompression algorithm. This Lua binding is in conformance with the LZ4 [block] and [frame] specifications and also support [streaming].

[![Build Status](https://travis-ci.org/witchu/lua-lz4.svg)](https://travis-ci.org/witchu/lua-lz4)
[![Build status](https://ci.appveyor.com/api/projects/status/1spury3s6lj9creg?svg=true)](https://ci.appveyor.com/project/witchu/lua-lz4)

## Example usage

Simple frame compression/decompression
```lua
local lz4 = require("lz4")
local s = "Hello, World!!"
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

#### lz4.block_compress(input[, accelerate])
#### lz4.block_compress_hc(input[, quality])
#### lz4.block_decompress_safe(input, decompress_length)
#### lz4.block_decompress_fast(input, decompress_length)

### Stream

#### lz4.new_compression_stream([ring_buffer_size[, accelerate]])
#### lz4.new_compression_stream_hc([ring_buffer_size[, qualtily]])
#### lz4.new_decompression_stream(ring_buffer_size)



[LZ4]: https://github.com/Cyan4973/lz4
[block]: https://github.com/Cyan4973/lz4/blob/master/lz4_Block_format.md
[frame]: https://github.com/Cyan4973/lz4/blob/master/lz4_Frame_format.md
[streaming]: https://github.com/Cyan4973/lz4/blob/master/examples/streaming_api_basics.md
