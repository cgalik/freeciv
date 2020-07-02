/*****************************************************************************
 Freeciv - Copyright (C) 2005 - The Freeciv Project
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*****************************************************************************/

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

/* common */
#include "featured_text.h"

/* common/scriptcore */
#include "luascript.h"

/* client */
#include "chatline_common.h"
#include "client_main.h"
#include "options.h"
#include "tilespec.h"

#include "api_client_base.h"

/* FIXME: don't know how to include mapview_common.h
 *  with each its include/?_g.h */
extern void center_tile_mapcanvas(struct tile* ptile);

/* Move this to some header files [[ */
/* NB! If you return from it, the current index is still in L! */
#define sequence_iterate(__luastate__, __seq__)                            \
  for (int _seqno = 1; LUA_TNIL != lua_geti(__luastate__, __seq__, _seqno);\
       _seqno++)
#define sequence_iterate_end(__luastate__) lua_pop(__luastate__, 1)
#define macro2str_base(__macro__) #__macro__
#define macro2str(__macro__) macro2str_base(__macro__)
/* ]] */

/*****************************************************************************
  Print a message in the chat window.
*****************************************************************************/
void api_client_chat_base(lua_State *L, const char *msg)
{
  LUASCRIPT_CHECK_STATE(L);
  LUASCRIPT_CHECK_ARG_NIL(L, msg, 2, string);

  output_window_printf(ftc_chat_luaconsole, "%s", msg);
}

/**********************************************************************//***
  Current player playing the client (NULL if global observer/unconnected)
***************************************************************************/
Player *api_client_player(lua_State *L)
{
  return client_player();
}

/**********************************************************************//***
  Gets a server option value available for the client
***************************************************************************/
lua_Object api_client_option_get(lua_State *L, const char *name,
                                 bool is_server_opt)
{
  struct option *opt = name
    ? optset_option_by_name(is_server_opt ? server_optset : client_optset,
                            name) : NULL;

  struct ft_color col;
  int t;

  LUASCRIPT_CHECK_STATE(L, 0);

  if (!opt) {
    lua_pushnil(L);
    return lua_gettop(L);
  }

  switch (option_type(opt)) {
  case OT_BOOLEAN:
    lua_pushboolean(L, option_bool_get(opt));
    break;
  case OT_INTEGER:
    lua_pushinteger(L, option_int_get(opt));
    break;
  case OT_STRING:
    tolua_pushstring(L, option_str_get(opt));
    break;
  case OT_ENUM: /* Push a table with integer value and translatable line */
    {
      lua_createtable(L, 0, 2);
      t = lua_gettop(L);
      lua_pushinteger(L, option_enum_get_int(opt));
      lua_setfield(L, t, "int");
      lua_pushstring(L, option_enum_get_str(opt));
      lua_setfield(L, t, "str");
    }
    break;
  case OT_BITWISE: /* FIXME: want a pretty table here */
    lua_pushinteger(L, option_bitwise_get(opt));
    break;
  case OT_FONT:
    tolua_pushstring(L, option_font_get(opt));
    break;
  case OT_COLOR:
    col = option_color_get(opt);
    lua_createtable(L, 0, 2);
    t = lua_gettop(L);
    tolua_pushstring(L, col.foreground);
    lua_setfield(L, t, "foreground");
    tolua_pushstring(L, col.background);
    lua_setfield(L, t, "background");
    break;
  case OT_VIDEO_MODE:
    lua_createtable(L, 2, 0);
    {
      struct video_mode vm = option_video_mode_get(opt);
      t = lua_gettop(L);
      lua_pushstring(L, "width");
      lua_pushinteger(L, vm.width);
      lua_settable(L, t);
      lua_pushstring(L, "height");
      lua_pushinteger(L, vm.height);
      lua_settable(L, t);
    }
    break;
  default: /* should not be here */
    fc_assert(FALSE);
    luaL_error(L, "Unknown option type");
    lua_pushnil(L);
  }
  return lua_gettop(L);
}

/**********************************************************************//***
  Gets next server option name (for iteration)
***************************************************************************/
const char* api_client_option_next(lua_State *L, const char *name,
                                   bool is_server_opt)
{
  struct option *opt;

  LUASCRIPT_CHECK_STATE(L, NULL);

  if (!name) {
    return option_name(optset_option_first(is_server_opt
                                           ? server_optset
                                           : client_optset));
  }
  if ((opt = optset_option_by_name(is_server_opt
                                   ? server_optset
                                   : client_optset,
                                   name))) {
    if ((opt = option_next(opt))) {
      return option_name(opt);
    }
  }
  return NULL;
}

/**********************************************************************//***
  Centers the view at given tile
***************************************************************************/
void api_client_center(lua_State *L, Tile* tile)
{
  LUASCRIPT_CHECK_STATE(L);

  center_tile_mapcanvas(tile);
}

/**********************************************************************//***
  Gets a string describing the current state of the client
***************************************************************************/
const char *api_client_state(lua_State *L)
{
    LUASCRIPT_CHECK_STATE(L, NULL);

  switch (client_state()) {
  case C_S_INITIAL:
    return "Initial";
  case C_S_DISCONNECTED:
    return "Disconnected";
  case C_S_PREPARING:
    return "Preparing";
  case C_S_RUNNING:
    if (client_is_observer()) {
      return "Observing";
    } else {
      return "Playing";
    }
  case C_S_OVER: return "Gameover";
  default:
    /* should not get here */
    fc_assert(FALSE);
    return NULL;
  }
}

/**********************************************************************//***
  Gets current tileset name
***************************************************************************/
const char *api_client_tileset_name(lua_State *L)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  struct tileset *ts = get_tileset();

  if (ts) {
    return tileset_name_get(ts);
  } else {
    return NULL; /* FIXME: is it possible? */
  }
}
