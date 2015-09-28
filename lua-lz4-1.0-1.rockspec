package = "lua-lz4"
  version = "1.0-1"
  source = {
    url = "git://github.com/witchu/lua-lz4"
  }
  description = {
    summary = "LZ4 fast compression algorithm binding for Lua.",
    detailed = [[
This Lua binding is in conformance with the LZ4 block and frame specifications and also support streaming.
    ]],
    homepage = "https://github.com/witchu/lua-lz4",
    license = "Apache License 2.0"
  }
  dependencies = {
    "lua >= 5.1, < 5.4"
  }
  build = {
    type = "make",
    build_variables = {
      LUA_CFLAGS="$(CFLAGS)",
      LIBFLAG="$(LIBFLAG)",
      LUA_LIBDIR="$(LUA_LIBDIR)",
      LUA_BINDIR="$(LUA_BINDIR)",
      LUA_INCDIR="$(LUA_INCDIR)",
      LUA="$(LUA)",
    },
    install_variables = {
      INST_PREFIX="$(PREFIX)",
      INST_BINDIR="$(BINDIR)",
      INST_LIBDIR="$(LIBDIR)",
      INST_LUADIR="$(LUADIR)",
      INST_CONFDIR="$(CONFDIR)",
    },

    platforms = {
      windows = {
        type = "builtin",
        modules = {
          lz4 = {
            sources = {
              "lua_lz4.c",
              "lz4/lz4.c",
              "lz4/lz4hc.c",
              "lz4/lz4frame.c",
              "lz4/xxhash.c",
            },
            defines = { "LUA_BUILD_AS_DLL", "LUA_LIB", "WIN32_LEAN_AND_MEAN" },
          },
        }
      },
    },
  }
