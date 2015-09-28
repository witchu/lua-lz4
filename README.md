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




[LZ4]: https://github.com/Cyan4973/lz4
[block]: https://github.com/Cyan4973/lz4/blob/master/lz4_Block_format.md
[frame]: https://github.com/Cyan4973/lz4/blob/master/lz4_Frame_format.md
[streaming]: https://github.com/Cyan4973/lz4/blob/master/examples/streaming_api_basics.md
