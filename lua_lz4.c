#include <stdlib.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "lz4/lz4.h"
#include "lz4/lz4hc.h"

static void luaL_fieldinteger(lua_State *L, int table_index, const char *field_name, int *value)
{
  lua_getfield(L, table_index, field_name);
  int type = lua_type(L, -1);
  if (type != LUA_TNIL)
  {
    if (type != LUA_TNUMBER) luaL_error(L, "field '%s' must be a number", field_name);
    *value = lua_tointeger(L, -1);
  }
  lua_pop(L, 1);
}

/*****************************************************************************
 * Block
 ****************************************************************************/

static int lz4_block_compress(lua_State *L)
{
  size_t in_len;
  const char *in = luaL_checklstring(L, 1, &in_len);
  int accelerate = luaL_optinteger(L, 2, 0);

  if (in_len > LZ4_MAX_INPUT_SIZE)
    return luaL_error(L, "input longer than %d", LZ4_MAX_INPUT_SIZE);

  int bound = LZ4_compressBound(in_len);
#if LUA_VERSION_NUM >= 502
  luaL_Buffer b;
  char *out = luaL_buffinitsize(L, &b, bound);
  int r = LZ4_compress_fast(in, out, in_len, bound, accelerate);
  if (r == 0) return luaL_error(L, "compression failed");
  luaL_pushresultsize(&b, r);
#else
  void *out = malloc(bound);
  if (out == NULL) return luaL_error(L, "out of memory");
  int r = LZ4_compress_fast(in, out, in_len, bound, accelerate);
  if (r == 0)
  {
    free(out);
    return luaL_error(L, "compression failed");
  }
  lua_pushlstring(L, out, r);
  free(out);
#endif

  return 1;
}

static int lz4_block_compress_hc(lua_State *L)
{
  size_t in_len;
  const char *in = luaL_checklstring(L, 1, &in_len);
  int level = luaL_optinteger(L, 2, 0);

  if (in_len > LZ4_MAX_INPUT_SIZE)
    return luaL_error(L, "input longer than %d", LZ4_MAX_INPUT_SIZE);

  int bound = LZ4_compressBound(in_len);
#if LUA_VERSION_NUM >= 502
  luaL_Buffer b;
  char *out = luaL_buffinitsize(L, &b, bound);
  int r = LZ4_compress_HC(in, out, in_len, bound, level);
  if (r == 0) return luaL_error(L, "compression failed");
  luaL_pushresultsize(&b, r);
#else
  void *out = malloc(bound);
  if (out == NULL) return luaL_error(L, "out of memory");
  int r = LZ4_compress_HC(in, out, in_len, bound, level);
  if (r == 0)
  {
    free(out);
    return luaL_error(L, "compression failed");
  }
  lua_pushlstring(L, out, r);
  free(out);
#endif

  return 1;
}

static int lz4_block_decompress_safe(lua_State *L)
{
  size_t in_len;
  const char *in = luaL_checklstring(L, 1, &in_len);
  int out_len = luaL_checkinteger(L, 2);

#if LUA_VERSION_NUM >= 502
  luaL_Buffer b;
  char *out = luaL_buffinitsize(L, &b, out_len);
  int r = LZ4_decompress_safe(in, out, in_len, out_len);
  if (r < 0) return luaL_error(L, "corrupt input or need more output space");
  luaL_pushresultsize(&b, r);
#else
  void *out = malloc(out_len);
  int r = LZ4_decompress_safe(in, out, in_len, out_len);
  if (r < 0)
  {
    free(out);
    return luaL_error(L, "corrupt input or need more output space");
  }
  lua_pushlstring(L, out, r);
  free(out);
#endif

  return 1;
}

static int lz4_block_decompress_fast(lua_State *L)
{
  size_t in_len;
  const char *in = luaL_checklstring(L, 1, &in_len);
  int out_len = luaL_checkinteger(L, 2);

#if LUA_VERSION_NUM >= 502
  luaL_Buffer b;
  char *out = luaL_buffinitsize(L, &b, out_len);
  int r = LZ4_decompress_fast(in, out, out_len);
  if (r < 0) return luaL_error(L, "corrupt input or incorrect output length");
  luaL_pushresultsize(&b, r);
#else
  void *out = malloc(out_len);
  int r = LZ4_decompress_fast(in, out, out_len);
  if (r < 0)
  {
    free(out);
    return luaL_error(L, "corrupt input or incorrect output length");
  }
  lua_pushlstring(L, out, out_len);
  free(out);
#endif

  return 1;
}

static int lz4_block_decompress_safe_partial(lua_State *L)
{
  size_t in_len;
  const char *in = luaL_checklstring(L, 1, &in_len);
  int target_len = luaL_checkinteger(L, 2);
  int out_len = luaL_checkinteger(L, 3);

#if LUA_VERSION_NUM >= 502
  luaL_Buffer b;
  char *out = luaL_buffinitsize(L, &b, out_len);
  int r = LZ4_decompress_safe_partial(in, out, in_len, target_len, out_len);
  if (r < 0) return luaL_error(L, "corrupt input or need more output space");
  if (target_len < r) r = target_len;
  luaL_pushresultsize(&b, r);
#else
  void *out = malloc(out_len);
  int r = LZ4_decompress_safe_partial(in, out, in_len, target_len, out_len);
  if (r < 0)
  {
    free(out);
    return luaL_error(L, "corrupt input or need more output space");
  }
  if (target_len < r) r = target_len;
  lua_pushlstring(L, out, r);
  free(out);
#endif

  return 1;
}

/*****************************************************************************
 * Export
 ****************************************************************************/

static const luaL_Reg export_functions[] = {
  /* Block */
  { "block_compress",                 lz4_block_compress },
  { "block_compress_hc",              lz4_block_compress_hc },
  { "block_decompress_safe",          lz4_block_decompress_safe },
  { "block_decompress_fast",          lz4_block_decompress_fast },
  { "block_decompress_safe_partial",  lz4_block_decompress_safe_partial },
  { NULL,                             NULL },
};

LUALIB_API int luaopen_lz4(lua_State *L)
{
#if LUA_VERSION_NUM >= 502
  luaL_newlib(L, export_functions);
#else
  lua_newtable(L);
  luaL_register(L, NULL, export_functions);
#endif

  int table_index = lua_gettop(L);

  lua_pushfstring(L, "%d.%d.%d", LZ4_VERSION_MAJOR, LZ4_VERSION_MINOR, LZ4_VERSION_RELEASE);
  lua_setfield(L, table_index, "version");

  return 1;
}
