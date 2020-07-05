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
void api_client_center_coords(lua_State *L, int x, int y);
const char *api_client_state(lua_State *L);
const char *api_client_tileset_name(lua_State *L);
lua_Object api_client_option_get(lua_State *L, const char *name,
                                 bool is_server_opt);
const char* api_client_option_next(lua_State *L, const char *name,
                                   bool is_server_opt);
/* focus manipulation */
void api_client_unit_focus_add(lua_State *L, Unit *punit);
void api_client_unit_focus_remove(lua_State *L, Unit *punit);
bool api_client_unit_is_in_focus(lua_State *L, Unit *punit);
int api_client_num_units_in_focus(lua_State *L);

/* Unit methods */
bool api_client_unit_occupied(lua_State *L, Unit *punit);
void api_client_unit_give_orders(lua_State *L, Unit *punit, lua_Object seq,
                                 bool vigilant, bool rep);
void api_client_unit_request_activity_targeted(lua_State *L, Unit *punit,
                                               const char *activity_name,
                                               const char *target);
void api_client_unit_do_action(lua_State *L, Unit *punit,
                               const Action *paction, lua_Object target,
                               lua_Object value);
void api_client_unit_do_action_name(lua_State *L, Unit *punit,
                                    const char *name, lua_Object target,
                                    lua_Object value);
void api_client_unit_do_action_id(lua_State *L, Unit *punit,
                                  int act, lua_Object target,
                                  lua_Object value);
void api_client_unit_airlift(lua_State *L, Unit *punit, City *pcity);
void api_client_unit_load(lua_State *L, Unit *pcargo, Unit *ptransport);
void api_client_unit_unload(lua_State *L, Unit *pcargo, Unit *ptransport);
bool api_client_unit_move(lua_State *L, Unit *punit, Tile *ptile);
void api_client_unit_upgrade(lua_State *L, Unit *punit);
void api_client_unit_build_city(lua_State *L, Unit *punit, const char *name);

Unit_List_Link *api_client_private_focus_head(lua_State *L);

/* City methods */
bool api_client_city_occupied(lua_State *L, City *pcity);
void api_client_city_change_production(lua_State *L, City *pcity,
                                       lua_Object prod);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FC__API_CLIENT_BASE_H */

