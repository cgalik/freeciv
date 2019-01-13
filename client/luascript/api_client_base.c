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
#include "connection.h"
#include "featured_text.h"
#include "unit.h"

/* common/scriptcore */
#include "luascript.h"

/* client */
#include "chatline_common.h"
#include "client_main.h"
#include "goto.h"

#include "api_client_base.h"

/*****************************************************************************
  Load unit in transport.
*****************************************************************************/
void api_client_unit_load(lua_State *L, Unit *pcargo, Unit *ptransport)
{
  LUASCRIPT_CHECK_STATE(L);
  LUASCRIPT_CHECK_ARG_NIL(L, pcargo, 2, Unit);
  LUASCRIPT_CHECK_ARG_NIL(L, ptransport, 3, Unit);

  dsend_packet_unit_load(&client.conn, pcargo->id, ptransport->id);
}

/*****************************************************************************
  Move unit.
*****************************************************************************/
void api_client_unit_move(lua_State *L, Unit *punit, Tile *ptile)
{
  LUASCRIPT_CHECK_STATE(L);
  LUASCRIPT_CHECK_ARG_NIL(L, punit, 2, Unit);
  LUASCRIPT_CHECK_ARG_NIL(L, ptile, 3, Tile);

  send_goto_tile(punit, ptile);
}

/*****************************************************************************
  Upgrade unit.
*****************************************************************************/
void api_client_unit_upgrade(lua_State *L, Unit *punit)
{
  LUASCRIPT_CHECK_STATE(L);
  LUASCRIPT_CHECK_ARG_NIL(L, punit, 2, Unit);

  dsend_packet_unit_upgrade(&client.conn, punit->id);
}

/*****************************************************************************
  Print a message in the chat window.
*****************************************************************************/
void api_client_chat_base(lua_State *L, const char *msg)
{
  LUASCRIPT_CHECK_STATE(L);
  LUASCRIPT_CHECK_ARG_NIL(L, msg, 2, string);

  output_window_printf(ftc_chat_luaconsole, "%s", msg);
}
