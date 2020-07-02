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

#ifndef FC__API_CLIENT_BASE_H
#define FC__API_CLIENT_BASE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* common/scriptcore */
#include "luascript_types.h"

struct lua_State;

void api_client_chat_base(lua_State *L, const char *msg);

Player *api_client_player(lua_State *L);
void api_client_center(lua_State *L, Tile* tile);
const char *api_client_state(lua_State *L);
const char *api_client_tileset_name(lua_State *L);
lua_Object api_client_option_get(lua_State *L, const char *name,
                                 bool is_server_opt);
const char* api_client_option_next(lua_State *L, const char *name,
                                   bool is_server_opt);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FC__API_CLIENT_BASE_H */

