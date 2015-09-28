#include <stdlib.h>
#include <memory.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "lz4/lz4frame.h"

#include "lz4/lz4.h"
#include "lz4/lz4hc.h"


#define LZ4_DICTSIZE      65536
#define DEF_BUFSIZE       65536
#define MIN_BUFFSIZE      1024

#if LUA_VERSION_NUM < 502
#define luaL_newlib(L, function_table) do { \
  lua_newtable(L);                          \
  luaL_register(L, NULL, function_table);   \
  } while (0)
#endif

#if LUA_VERSION_NUM >= 502
#define LUABUFF_NEW(lua_buff, c_buff, max_size) \
  luaL_Buffer lua_buff;                         \
  char *c_buff = luaL_buffinitsize(L, &lua_buff, max_size);
#define LUABUFF_FREE(c_buff)
#define LUABUFF_PUSH(lua_buff, c_buff, size)    \
  luaL_pushresultsize(&lua_buff, size);
#else
#define LUABUFF_NEW(lua_buff, c_buff, max_size) \
  char *c_buff = malloc(max_size);              \
  if (c_buff == NULL) return luaL_error(L, "out of memory");
#define LUABUFF_FREE(c_buff)                    \
  free(c_buff);
#define LUABUFF_PUSH(lua_buff, c_buff, size)    \
  lua_pushlstring(L, c_buff, size);             \
  free(c_buff);
#endif

#define RING_POLICY_APPEND    0
#define RING_POLICY_RESET     1
#define RING_POLICY_EXTERNAL  2

static int _ring_policy(int buffer_size, int buffer_position, int data_size)
{
  if (data_size > buffer_size || data_size > LZ4_DICTSIZE)
    return RING_POLICY_EXTERNAL;
  if (buffer_position + data_size <= buffer_size)
    return RING_POLICY_APPEND;
  if (data_size + LZ4_DICTSIZE > buffer_position)
    return RING_POLICY_EXTERNAL;
  return RING_POLICY_RESET;
}

static int _lua_table_optinteger(lua_State *L, int table_index, const char *field_name, int value)
{
  int type;
  lua_getfield(L, table_index, field_name);
  type = lua_type(L, -1);
  if (type != LUA_TNIL)
  {
    if (type != LUA_TNUMBER) luaL_error(L, "field '%s' must be a number", field_name);
    value = lua_tointeger(L, -1);
  }
  lua_pop(L, 1);
  return value;
}

static int _lua_table_optboolean(lua_State *L, int table_index, const char *field_name, int value)
{
  int type;
  lua_getfield(L, table_index, field_name);
  type = lua_type(L, -1);
  if (type != LUA_TNIL)
  {
    if (type != LUA_TBOOLEAN) luaL_error(L, "field '%s' must be a boolean", field_name);
    value = lua_toboolean(L, -1);
  }
  lua_pop(L, 1);
  return value;
}

/*****************************************************************************
 * Frame
 ****************************************************************************/

static int lz4_compress(lua_State *L)
{
  size_t in_len;
  const char *in = luaL_checklstring(L, 1, &in_len);
  size_t bound, r;

  LZ4F_preferences_t stack_settings;
  LZ4F_preferences_t *settings = NULL;

  if (lua_type(L, 2) == LUA_TTABLE)
  {
    memset(&stack_settings, 0, sizeof(stack_settings));
    settings = &stack_settings;
    settings->compressionLevel = _lua_table_optinteger(L, 2, "compression_level", 0);
    settings->autoFlush = _lua_table_optboolean(L, 2, "auto_flush", 0);
    settings->frameInfo.blockSizeID = _lua_table_optinteger(L, 2, "block_size", 0);
    settings->frameInfo.blockMode = _lua_table_optboolean(L, 2, "block_independent", 0) ? LZ4F_blockIndependent : LZ4F_blockLinked;
    settings->frameInfo.contentChecksumFlag = _lua_table_optboolean(L, 2, "content_checksum", 0) ? LZ4F_contentChecksumEnabled : LZ4F_noContentChecksum;
  }

  bound = LZ4F_compressFrameBound(in_len, settings);

  {
    LUABUFF_NEW(b, out, bound)
    r = LZ4F_compressFrame(out, bound, in, in_len, settings);
    if (LZ4F_isError(r))
    {
      LUABUFF_FREE(out)
      return luaL_error(L, "compression failed: %s", LZ4F_getErrorName(r));
    }
    LUABUFF_PUSH(b, out, r)
  }

  return 1;
}

static int lz4_decompress(lua_State *L)
{
  size_t in_len;
  const char *in = luaL_checklstring(L, 1, &in_len);
  const char *p = in;
  size_t p_len = in_len;

  LZ4F_decompressionContext_t ctx = NULL;
  LZ4F_frameInfo_t info;
  LZ4F_errorCode_t code;

  code = LZ4F_createDecompressionContext(&ctx, LZ4F_VERSION);
  if (LZ4F_isError(code)) goto decompression_failed;

  {
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    while (1)
    {
#if LUA_VERSION_NUM >= 502
      size_t out_len = 65536;
      char *out = luaL_prepbuffsize(&b, out_len);
#else
      size_t out_len = LUAL_BUFFERSIZE;
      char *out = luaL_prepbuffer(&b);
#endif
      size_t advance = p_len;
      code = LZ4F_decompress(ctx, out, &out_len, p, &advance, NULL);
      if (LZ4F_isError(code)) goto decompression_failed;
      if (out_len == 0) break;
      p += advance;
      p_len -= advance;
      luaL_addsize(&b, out_len);
    }
    luaL_pushresult(&b);
  }

  LZ4F_freeDecompressionContext(ctx);

  return 1;

decompression_failed:
  if (ctx != NULL) LZ4F_freeDecompressionContext(ctx);
  return luaL_error(L, "decompression failed: %s", LZ4F_getErrorName(code));
}

/*****************************************************************************
 * Block
 ****************************************************************************/

static int lz4_block_compress(lua_State *L)
{
  size_t in_len;
  const char *in = luaL_checklstring(L, 1, &in_len);
  int accelerate = luaL_optinteger(L, 2, 0);
  int bound, r;

  if (in_len > LZ4_MAX_INPUT_SIZE)
    return luaL_error(L, "input longer than %d", LZ4_MAX_INPUT_SIZE);

  bound = LZ4_compressBound(in_len);

  {
    LUABUFF_NEW(b, out, bound)
    r = LZ4_compress_fast(in, out, in_len, bound, accelerate);
    if (r == 0)
    {
      LUABUFF_FREE(out)
      return luaL_error(L, "compression failed");
    }
    LUABUFF_PUSH(b, out, r)
  }

  return 1;
}

static int lz4_block_compress_hc(lua_State *L)
{
  size_t in_len;
  const char *in = luaL_checklstring(L, 1, &in_len);
  int level = luaL_optinteger(L, 2, 0);
  int bound, r;

  if (in_len > LZ4_MAX_INPUT_SIZE)
    return luaL_error(L, "input longer than %d", LZ4_MAX_INPUT_SIZE);

  bound = LZ4_compressBound(in_len);

  {
    LUABUFF_NEW(b, out, bound)
    r = LZ4_compress_HC(in, out, in_len, bound, level);
    if (r == 0)
    {
      LUABUFF_FREE(out)
      return luaL_error(L, "compression failed");
    }
    LUABUFF_PUSH(b, out, r)
  }

  return 1;
}

static int lz4_block_decompress_safe(lua_State *L)
{
  size_t in_len;
  const char *in = luaL_checklstring(L, 1, &in_len);
  int out_len = luaL_checkinteger(L, 2);
  int r;

  LUABUFF_NEW(b, out, out_len)
  r = LZ4_decompress_safe(in, out, in_len, out_len);
  if (r < 0)
  {
    LUABUFF_FREE(out)
    return luaL_error(L, "corrupt input or need more output space");
  }
  LUABUFF_PUSH(b, out, r)

  return 1;
}

static int lz4_block_decompress_fast(lua_State *L)
{
  size_t in_len;
  const char *in = luaL_checklstring(L, 1, &in_len);
  int out_len = luaL_checkinteger(L, 2);

  {
    int r;
    LUABUFF_NEW(b, out, out_len)
    r = LZ4_decompress_fast(in, out, out_len);
    if (r < 0)
    {
      LUABUFF_FREE(out)
      return luaL_error(L, "corrupt input or incorrect output length");
    }
    LUABUFF_PUSH(b, out, out_len)
  }

  return 1;
}

static int lz4_block_decompress_safe_partial(lua_State *L)
{
  size_t in_len;
  const char *in = luaL_checklstring(L, 1, &in_len);
  int target_len = luaL_checkinteger(L, 2);
  int out_len = luaL_checkinteger(L, 3);
  int r;

  LUABUFF_NEW(b, out, out_len)
  r = LZ4_decompress_safe_partial(in, out, in_len, target_len, out_len);
  if (r < 0)
  {
    LUABUFF_FREE(out)
    return luaL_error(L, "corrupt input or need more output space");
  }
  if (target_len < r) r = target_len;
  LUABUFF_PUSH(b, out, r)

  return 1;
}

/*****************************************************************************
 * Compression Stream
 ****************************************************************************/

typedef struct
{
  LZ4_stream_t handle;
  int accelerate;
  int buffer_size;
  int buffer_position;
  char *buffer;
} lz4_compress_stream_t;

static lz4_compress_stream_t *_checkcompressionstream(lua_State *L, int index)
{
  return (lz4_compress_stream_t *)luaL_checkudata(L, index, "lz4.compression_stream");
}

static int lz4_cs_reset(lua_State *L)
{
  lz4_compress_stream_t *cs = _checkcompressionstream(L, 1);
  size_t in_len = 0;
  const char *in = luaL_optlstring(L, 2, NULL, &in_len);

  if (in != NULL && in_len > 0)
  {
    int limit_len = LZ4_DICTSIZE;
    if (limit_len > cs->buffer_size) limit_len = cs->buffer_size;
    if (in_len > limit_len)
    {
      in = in + in_len - limit_len;
      in_len = limit_len;
    }
    memcpy(cs->buffer, in, in_len);
    cs->buffer_position = LZ4_loadDict(&cs->handle, cs->buffer, in_len);
  }
  else
  {
    LZ4_resetStream(&cs->handle);
    cs->buffer_position = 0;
  }

  lua_pushinteger(L, cs->buffer_position);

  return 1;
}

static int lz4_cs_compress(lua_State *L)
{
  lz4_compress_stream_t *cs = _checkcompressionstream(L, 1);
  size_t in_len;
  const char *in = luaL_checklstring(L, 2, &in_len);
  size_t bound = LZ4_compressBound(in_len);
  int policy = _ring_policy(cs->buffer_size, cs->buffer_position, in_len);
  int r;

  LUABUFF_NEW(b, out, bound)

  if (policy == RING_POLICY_APPEND || policy == RING_POLICY_RESET)
  {
    char *ring;
    if (policy == RING_POLICY_APPEND)
    {
      ring = cs->buffer + cs->buffer_position;
      cs->buffer_position += in_len;
    }
    else
    { // RING_POLICY_RESET
      ring = cs->buffer;
      cs->buffer_position = in_len;
    }
    memcpy(ring, in, in_len);
    r = LZ4_compress_fast_continue(&cs->handle, ring, out, in_len, bound, cs->accelerate);
    if (r == 0)
    {
      LUABUFF_FREE(out)
      return luaL_error(L, "compression failed");
    }
  }
  else
  { // RING_POLICY_EXTERNAL
    r = LZ4_compress_fast_continue(&cs->handle, in, out, in_len, bound, cs->accelerate);
    if (r == 0)
    {
      LUABUFF_FREE(out)
      return luaL_error(L, "compression failed");
    }
    cs->buffer_position = LZ4_saveDict(&cs->handle, cs->buffer, cs->buffer_size);
  }

  LUABUFF_PUSH(b, out, r)

  return 1;
}

static int lz4_cs_tostring(lua_State *L)
{
  lz4_compress_stream_t *p = _checkcompressionstream(L, 1);
  lua_pushfstring(L, "lz4.compression_stream (%p)", p);
  return 1;
}

static int lz4_cs_gc(lua_State *L)
{
  lz4_compress_stream_t *p = _checkcompressionstream(L, 1);
  free(p->buffer);
  return 0;
}

static const luaL_Reg compress_stream_functions[] = {
  { "reset",    lz4_cs_reset },
  { "compress", lz4_cs_compress },
  { NULL,       NULL },
};

static int lz4_new_compression_stream(lua_State *L)
{
  int buffer_size = luaL_optinteger(L, 1, DEF_BUFSIZE);
  int accelerate = luaL_optinteger(L, 2, 1);
  lz4_compress_stream_t *p;

  if (buffer_size < MIN_BUFFSIZE) buffer_size = MIN_BUFFSIZE;

  p = lua_newuserdata(L, sizeof(lz4_compress_stream_t));
  LZ4_resetStream(&p->handle);
  p->accelerate = accelerate;
  p->buffer_size = buffer_size;
  p->buffer_position = 0;
  p->buffer = malloc(buffer_size);
  if (p->buffer == NULL) luaL_error(L, "out of memory");

  if (luaL_newmetatable(L, "lz4.compression_stream"))
  {
    // new method table
    luaL_newlib(L, compress_stream_functions);
    // metatable.__index = method table
    lua_setfield(L, -2, "__index");

    // metatable.__tostring
    lua_pushcfunction(L, lz4_cs_tostring);
    lua_setfield(L, -2, "__tostring");

    // metatable.__gc
    lua_pushcfunction(L, lz4_cs_gc);
    lua_setfield(L, -2, "__gc");
  }
  lua_setmetatable(L, -2);

  return 1;
}

/*****************************************************************************
 * Compression Stream HC
 ****************************************************************************/

typedef struct
{
  LZ4_streamHC_t handle;
  int level;
  int buffer_size;
  int buffer_position;
  char *buffer;
} lz4_compress_stream_hc_t;

static lz4_compress_stream_hc_t *_checkcompressionstream_hc(lua_State *L, int index)
{
  return (lz4_compress_stream_hc_t *)luaL_checkudata(L, index, "lz4.compression_stream_hc");
}

static int lz4_cs_hc_reset(lua_State *L)
{
  lz4_compress_stream_hc_t *cs = _checkcompressionstream_hc(L, 1);
  size_t in_len = 0;
  const char *in = luaL_optlstring(L, 2, NULL, &in_len);

  if (in != NULL && in_len > 0)
  {
    int limit_len = LZ4_DICTSIZE;
    if (limit_len > cs->buffer_size) limit_len = cs->buffer_size;
    if (in_len > limit_len)
    {
      in = in + in_len - limit_len;
      in_len = limit_len;
    }
    memcpy(cs->buffer, in, in_len);
    cs->buffer_position = LZ4_loadDictHC(&cs->handle, cs->buffer, in_len);
  }
  else
  {
    LZ4_resetStreamHC(&cs->handle, cs->level);
    cs->buffer_position = 0;
  }

  lua_pushinteger(L, cs->buffer_position);

  return 1;
}

static int lz4_cs_hc_compress(lua_State *L)
{
  lz4_compress_stream_hc_t *cs = _checkcompressionstream_hc(L, 1);
  size_t in_len;
  const char *in = luaL_checklstring(L, 2, &in_len);
  size_t bound = LZ4_compressBound(in_len);
  int policy = _ring_policy(cs->buffer_size, cs->buffer_position, in_len);
  int r;

  LUABUFF_NEW(b, out, bound)

  if (policy == RING_POLICY_APPEND || policy == RING_POLICY_RESET)
  {
    char *ring;
    if (policy == RING_POLICY_APPEND)
    {
      ring = cs->buffer + cs->buffer_position;
      cs->buffer_position += in_len;
    }
    else
    { // RING_POLICY_RESET
      ring = cs->buffer;
      cs->buffer_position = in_len;
    }
    memcpy(ring, in, in_len);
    r = LZ4_compress_HC_continue(&cs->handle, ring, out, in_len, bound);
    if (r == 0)
    {
      LUABUFF_FREE(out)
      return luaL_error(L, "compression failed");
    }
  }
  else
  { // RING_POLICY_EXTERNAL
    r = LZ4_compress_HC_continue(&cs->handle, in, out, in_len, bound);
    if (r == 0)
    {
      LUABUFF_FREE(out)
      return luaL_error(L, "compression failed");
    }
    cs->buffer_position = LZ4_saveDictHC(&cs->handle, cs->buffer, cs->buffer_size);
  }

  LUABUFF_PUSH(b, out, r)

  return 1;
}

static int lz4_cs_hc_tostring(lua_State *L)
{
  lz4_compress_stream_hc_t *p = _checkcompressionstream_hc(L, 1);
  lua_pushfstring(L, "lz4.compression_stream_hc (%p)", p);
  return 1;
}

static int lz4_cs_hc_gc(lua_State *L)
{
  lz4_compress_stream_hc_t *p = _checkcompressionstream_hc(L, 1);
  free(p->buffer);
  return 0;
}

static const luaL_Reg compress_stream_hc_functions[] = {
  { "reset",    lz4_cs_hc_reset },
  { "compress", lz4_cs_hc_compress },
  { NULL,       NULL },
};

static int lz4_new_compression_stream_hc(lua_State *L)
{
  int buffer_size = luaL_optinteger(L, 1, DEF_BUFSIZE);
  int level = luaL_optinteger(L, 2, 0);
  lz4_compress_stream_hc_t *p;

  if (buffer_size < MIN_BUFFSIZE) buffer_size = MIN_BUFFSIZE;

  p = lua_newuserdata(L, sizeof(lz4_compress_stream_hc_t));
  LZ4_resetStreamHC(&p->handle, level);
  p->level = level;
  p->buffer_size = buffer_size;
  p->buffer_position = 0;
  p->buffer = malloc(buffer_size);
  if (p->buffer == NULL) luaL_error(L, "out of memory");

  if (luaL_newmetatable(L, "lz4.compression_stream_hc"))
  {
    // new method table
    luaL_newlib(L, compress_stream_hc_functions);
    // metatable.__index = method table
    lua_setfield(L, -2, "__index");

    // metatable.__tostring
    lua_pushcfunction(L, lz4_cs_hc_tostring);
    lua_setfield(L, -2, "__tostring");

    // metatable.__gc
    lua_pushcfunction(L, lz4_cs_hc_gc);
    lua_setfield(L, -2, "__gc");
  }
  lua_setmetatable(L, -2);

  return 1;
}

/*****************************************************************************
 * Decompression Stream
 ****************************************************************************/

typedef struct
{
  LZ4_streamDecode_t handle;
  int buffer_size;
  int buffer_position;
  char *buffer;
} lz4_decompress_stream_t;

static lz4_decompress_stream_t *_checkdecompressionstream(lua_State *L, int index)
{
  return (lz4_decompress_stream_t *)luaL_checkudata(L, index, "lz4.decompression_stream");
}

static int lz4_ds_reset(lua_State *L)
{
  lz4_decompress_stream_t *ds = _checkdecompressionstream(L, 1);
  size_t in_len = 0;
  const char *in = luaL_optlstring(L, 2, NULL, &in_len);

  if (in != NULL && in_len > 0)
  {
    int limit_len = LZ4_DICTSIZE;
    if (limit_len > ds->buffer_size) limit_len = ds->buffer_size;
    if (in_len > limit_len)
    {
      in = in + in_len - limit_len;
      in_len = limit_len;
    }
    memcpy(ds->buffer, in, in_len);
    ds->buffer_position = LZ4_setStreamDecode(&ds->handle, ds->buffer, in_len);
  }
  else
  {
    LZ4_setStreamDecode(&ds->handle, NULL, 0);
    ds->buffer_position = 0;
  }

  lua_pushinteger(L, ds->buffer_position);

  return 1;
}

static void _lz4_ds_save_dict(lz4_decompress_stream_t *ds, const char *dict, int dict_size)
{
  int limit_len = LZ4_DICTSIZE;
  if (limit_len > ds->buffer_size) limit_len = ds->buffer_size;

  if (dict_size > limit_len)
  {
    dict += dict_size - limit_len;
    dict_size = limit_len;
  }

  memmove(ds->buffer, dict, dict_size);
  LZ4_setStreamDecode(&ds->handle, ds->buffer, dict_size);

  ds->buffer_position = dict_size;
}

static int lz4_ds_decompress_safe(lua_State *L)
{
  lz4_decompress_stream_t *ds = _checkdecompressionstream(L, 1);
  size_t in_len;
  const char *in = luaL_checklstring(L, 2, &in_len);
  size_t out_len = luaL_checkinteger(L, 3);
  int policy = _ring_policy(ds->buffer_size, ds->buffer_position, out_len);
  int r;

  if (policy == RING_POLICY_APPEND || policy == RING_POLICY_RESET)
  {
    char *ring;
    size_t new_position;
    if (policy == RING_POLICY_APPEND)
    {
      ring = ds->buffer + ds->buffer_position;
      new_position = ds->buffer_position + out_len;
    }
    else
    { // RING_POLICY_RESET
      ring = ds->buffer;
      new_position = out_len;
    }
    r = LZ4_decompress_safe_continue(&ds->handle, in, ring, in_len, out_len);
    if (r < 0) return luaL_error(L, "corrupt input or need more output space");
    ds->buffer_position = new_position;
    lua_pushlstring(L, ring, r);
  }
  else
  { // RING_POLICY_EXTERNAL
    LUABUFF_NEW(b, out, out_len)
    r = LZ4_decompress_safe_continue(&ds->handle, in, out, in_len, out_len);
    if (r < 0)
    {
      LUABUFF_FREE(out)
      return luaL_error(L, "corrupt input or need more output space");
    }
    _lz4_ds_save_dict(ds, out, r); // memcpy(ds->buffer, out, out_len)
    LUABUFF_PUSH(b, out, r)
  }

  return 1;
}

static int lz4_ds_decompress_fast(lua_State *L)
{
  lz4_decompress_stream_t *ds = _checkdecompressionstream(L, 1);
  size_t in_len;
  const char *in = luaL_checklstring(L, 2, &in_len);
  size_t out_len = luaL_checkinteger(L, 3);
  int policy = _ring_policy(ds->buffer_size, ds->buffer_position, out_len);
  int r;

  if (policy == RING_POLICY_APPEND || policy == RING_POLICY_RESET)
  {
    char *ring;
    size_t new_position;
    if (policy == RING_POLICY_APPEND)
    {
      ring = ds->buffer + ds->buffer_position;
      new_position = ds->buffer_position + out_len;
    }
    else
    { // RING_POLICY_RESET
      ring = ds->buffer;
      new_position = out_len;
    }
    r = LZ4_decompress_fast_continue(&ds->handle, in, ring, out_len);
    if (r < 0) return luaL_error(L, "corrupt input or need more output space");
    ds->buffer_position = new_position;
    lua_pushlstring(L, ring, out_len);
  }
  else
  { // RING_POLICY_EXTERNAL
    LUABUFF_NEW(b, out, out_len)
    r = LZ4_decompress_fast_continue(&ds->handle, in, out, out_len);
    if (r < 0)
    {
      LUABUFF_FREE(out)
      return luaL_error(L, "corrupt input or need more output space");
    }
    _lz4_ds_save_dict(ds, out, out_len); // memcpy(ds->buffer, out, out_len)
    LUABUFF_PUSH(b, out, out_len)
  }

  return 1;
}

static int lz4_ds_tostring(lua_State *L)
{
  lz4_decompress_stream_t *p = _checkdecompressionstream(L, 1);
  lua_pushfstring(L, "lz4.decompression_stream (%p)", p);
  return 1;
}

static int lz4_ds_gc(lua_State *L)
{
  lz4_decompress_stream_t *p = _checkdecompressionstream(L, 1);
  free(p->buffer);
  return 0;
}

static const luaL_Reg decompress_stream_functions[] = {
  { "reset",            lz4_ds_reset },
  { "decompress_safe",  lz4_ds_decompress_safe },
  { "decompress_fast",  lz4_ds_decompress_fast },
  { NULL,               NULL },
};

static int lz4_new_decompression_stream(lua_State *L)
{
  int buffer_size = luaL_optinteger(L, 1, DEF_BUFSIZE);
  lz4_decompress_stream_t *p;

  if (buffer_size < MIN_BUFFSIZE) buffer_size = MIN_BUFFSIZE;

  p = lua_newuserdata(L, sizeof(lz4_decompress_stream_t));
  LZ4_setStreamDecode(&p->handle, NULL, 0);
  p->buffer_size = buffer_size;
  p->buffer_position = 0;
  p->buffer = malloc(buffer_size);
  if (p->buffer == NULL) luaL_error(L, "out of memory");

  if (luaL_newmetatable(L, "lz4.decompression_stream"))
  {
    // new method table
    luaL_newlib(L, decompress_stream_functions);
    // metatable.__index = method table
    lua_setfield(L, -2, "__index");

    // metatable.__tostring
    lua_pushcfunction(L, lz4_ds_tostring);
    lua_setfield(L, -2, "__tostring");

    // metatable.__gc
    lua_pushcfunction(L, lz4_ds_gc);
    lua_setfield(L, -2, "__gc");
  }
  lua_setmetatable(L, -2);

  return 1;
}

/*****************************************************************************
 * Export
 ****************************************************************************/

static const luaL_Reg export_functions[] = {
  /* Frame */
  { "compress",                       lz4_compress },
  { "decompress",                     lz4_decompress },
  /* Block */
  { "block_compress",                 lz4_block_compress },
  { "block_compress_hc",              lz4_block_compress_hc },
  { "block_decompress_safe",          lz4_block_decompress_safe },
  { "block_decompress_fast",          lz4_block_decompress_fast },
  { "block_decompress_safe_partial",  lz4_block_decompress_safe_partial },
  /* Stream */
  { "new_compression_stream",         lz4_new_compression_stream },
  { "new_compression_stream_hc",      lz4_new_compression_stream_hc },
  { "new_decompression_stream",       lz4_new_decompression_stream },
  { NULL,                             NULL },
};

LUALIB_API int luaopen_lz4(lua_State *L)
{
  int table_index;
  luaL_newlib(L, export_functions);

  table_index = lua_gettop(L);

  lua_pushfstring(L, "%d.%d.%d", LZ4_VERSION_MAJOR, LZ4_VERSION_MINOR, LZ4_VERSION_RELEASE);
  lua_setfield(L, table_index, "version");

  lua_pushinteger(L, LZ4F_max64KB);
  lua_setfield(L, table_index, "block_64KB");
  lua_pushinteger(L, LZ4F_max256KB);
  lua_setfield(L, table_index, "block_256KB");
  lua_pushinteger(L, LZ4F_max1MB);
  lua_setfield(L, table_index, "block_1MB");
  lua_pushinteger(L, LZ4F_max4MB);
  lua_setfield(L, table_index, "block_4MB");

  return 1;
}
