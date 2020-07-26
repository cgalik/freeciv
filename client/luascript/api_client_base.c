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
#include "specialist.h"
#include "tech.h"

/* common/scriptcore */
#include "luascript.h"

/* common/aicore */
#include "aicore/cm.h"

/* client */
#include "chatline_common.h"
#include "citydlg_common.h"
#include "client_main.h"
#include "control.h"
#include "goto.h"
#include "options.h"
#include "tilespec.h"

#include "api_client_base.h"

/* FIXME: don't know how to include mapview_common.h
 *  with each its include/?_g.h */
extern void center_tile_mapcanvas(struct tile* ptile);
/* FIXME: same problem with stuff rom
 * agents/cma_fec.h->agents/cma_core.h->common/aicore/cm.h */
extern bool cma_is_city_under_agent(const struct city *pcity,
                                    struct cm_parameter *parameter);
extern const char *cmafec_get_short_descr(const struct cm_parameter
                                           *const parameter);

enum actmove_res_type {
  AMRT_NO,
  AMRT_AUTO, /* All bigger values filled by add_top() */
  AMRT_AUTO_EACH,
  AMRT_ALL
};

/* Move this to some header files [[ */
/* NB! If you return from it, the current index is still in L! */
#define sequence_iterate(__luastate__, __seq__)                            \
  for (int _seqno = 1; LUA_TNIL != lua_geti(__luastate__, __seq__, _seqno);\
       _seqno++)
#define sequence_iterate_end(__luastate__) lua_pop(__luastate__, 1)
#define macro2str_base(__macro__) #__macro__
#define macro2str(__macro__) macro2str_base(__macro__)
/* ]] */

static bool add_top(struct packet_unit_orders *p, lua_State *L,
                    enum actmove_res_type amrt);
static enum unit_orders pchar2order(const char *order);
static enum direction8 top2dir8(lua_State *L);

/**********************************************************************//***
  unit_activity_by_name() wrapper that keeps ACTIVITY_LAST if wrong name
  (needed to avoid dio_put_uint8() errors)
***************************************************************************/
static inline bool try_set_activity(enum unit_activity *activity,
                                    const char *name)
{
  enum unit_activity act
   = unit_activity_by_name(name, fc_strcasecmp);
  if (unit_activity_is_valid(act)) {
    *activity = act;
    return TRUE;
  } else {
    return FALSE;
  }
}

/**********************************************************************//***
  Helper function, tries to interpret lua top as a direction.
***************************************************************************/
static enum direction8 top2dir8(lua_State *L)
{
  enum direction8 dir;
  tolua_Error err;

  switch (lua_type(L, -1)) {
  case LUA_TNUMBER:
    dir = (enum direction8) lua_tointeger(L, -1);
    break;
  case LUA_TSTRING:
    dir = direction8_by_name(lua_tostring(L, -1), fc_strcasecmp);
    if (!is_valid_dir(dir)) {
      dir = (enum direction8) (lua_tostring(L, -1)[0] - '0');
      if (dir < 0 || dir > direction8_invalid()) {
        dir = direction8_invalid();
      }
    }
    break;
  case LUA_TUSERDATA:
    if (tolua_isusertype(L, -1, "Direction", 0, &err)) {
      dir = *((enum direction8 *) lua_touserdata(L, -1));
    }
    break;
  default:
    dir = direction8_invalid();
  }
  return dir;
}

/**********************************************************************//***
  Helper function modified from savegame.c
  Returns order from a string, cares only first byte except for '[aAhH]'.
***************************************************************************/
static enum unit_orders pchar2order(const char *order)
{
  fc_assert(order);

  switch (*order) {
  case 'm':
  case 'M':
    return ORDER_MOVE;
  case 'w':
  case 'W':
    return ORDER_FULL_MP;
  case 'b':
  case 'B':
    return ORDER_BUILD_CITY;
  case 'a':
  case 'A':
    if(!(fc_strcasecmp(order, "ACTIONMOVE")
         || fc_strcasecmp(order, "ACTION_MOVE"))) {
      return ORDER_ACTION_MOVE;
    } else {
      return ORDER_ACTIVITY;
    }
  case 'd':
  case 'D':
    return ORDER_DISBAND;
  case 'u':
  case 'U':
    return ORDER_BUILD_WONDER;
  case 't':
  case 'T':
  case 'e':
  case 'E':
    return ORDER_TRADE_ROUTE;
  case 'h':
  case 'H':
    if(!(fc_strcasecmp(order, "HELPWONDER")
         || fc_strcasecmp(order, "HELP WONDER"))) {
      return ORDER_BUILD_WONDER;
    } else {
      return ORDER_HOMECITY;
    }
  case 'x':
  case 'X':
    return ORDER_ACTION_MOVE;
  }

  return ORDER_LAST;
}

/**********************************************************************//***
  Helper function, gets another order from top stack value and pops it.
  Returns false iff the value is invalid (performs minimal checks).
***************************************************************************/
static bool add_top(struct packet_unit_orders *p, lua_State *L,
                    enum actmove_res_type amrt)
{
  enum unit_orders order = ORDER_LAST;
  enum direction8 dir = DIR8_ORIGIN;
  enum unit_activity activity = ACTIVITY_LAST;
  int target = EXTRA_NONE;
  /* const */ struct extra_type *extra = NULL;
  int times;

  LUASCRIPT_CHECK_STATE(L, FALSE);
  /* stack: ordval */
  log_verbose("Trying to add %s to orders from L top", tolua_typename(L, -1));
  switch (lua_type(L, -1)) {
  case LUA_TNUMBER:
    order = lua_tointeger(L, -1); /* 0 if bad integer */
    break;
  case LUA_TSTRING:
    /* Extra name, action name, direction or order specifier */
    {
      const char *str = lua_tostring(L, -1);

      extra = extra_type_by_rule_name(str);
      if (NULL != extra) {
        /* unspeific "Mine" may be misinterpreted for extra name
         * while you actually plant forest here */
        /* TODO: inspect previous work to see what actual terrain to expect */
        const struct unit *pu = game_unit_by_number(p->unit_id);
        const struct tile *dt = index_to_tile(p->dest_tile);
        if (!fc_strcasecmp(str, "Mine")
            && !can_unit_do_activity_targeted_at(pu, ACTIVITY_MINE,
                                                 extra, dt)) {
          activity = ACTIVITY_MINE;
        } else if (!fc_strcasecmp(str, "Irrigate") /* just if it happens */
            && !can_unit_do_activity_targeted_at(pu, ACTIVITY_IRRIGATE,
                                                 extra, dt)) {
          activity = ACTIVITY_IRRIGATE;
        } else {
          target = extra_index(extra);
        }
        break;
      }

      if (try_set_activity(&activity, str)) {
        break;
      }
      dir = direction8_by_name(str, fc_strcasecmp);
      if (is_valid_dir(dir)) {
        break;
      } else { /* raw dir8 in a character */
        dir = (enum direction8) (str[0] - '0');
        if (dir < direction8_invalid() && is_valid_dir(dir)) {
          break;
        }
      }
      order = pchar2order(str);
      if (ORDER_LAST != order) {
        break;
      }
    }
    /* All checks and autofilling go later, but something must it be */
    luaL_error(L, "Unrecognized string '%s' supplied as an order",
               lua_tostring(L, -1));
    return FALSE;
  case LUA_TUSERDATA:
  case LUA_TLIGHTUSERDATA: /* FIXME: One seems to not work */
    /* Direction (TODO: or Tile) */
    if (!strcmp(tolua_typename(L, -1), "Direction")) {
      dir = *((enum direction8 *) lua_touserdata(L, -1));
    /* } else if (!strcmp(lua_tostring(L, -1), "Tile")) { */
      /* TODO: goto tile */
    } else {
      luaL_error(L, "Invalid user type %s supplied as an order",
                 tolua_typename(L, -1));
      lua_pop(L, 1); /* stack: */
      return FALSE;
    }
    break;
  case LUA_TTABLE:
    /*TODO: to_tile = Tile => execute on all tiles along auto path */
    lua_getfield(L, -1, "times"); /* stack: ordval times */
    if (lua_isinteger(L, -1)) {
      /* sequence of orders, repeat it times */
      times = lua_tointeger(L, -1);
      lua_pop(L, 1); /* stack: ordval */
      log_verbose("Processing subsequence (%d times) in L[%d]", times,
                  lua_gettop(L));
      if ((0 <= times) && (times <= MAX_LEN_ROUTE)) {
        int ptnn = p->length, ptl;

        if (!times) {
          lua_pop(L, 1);/* stack: */
          return TRUE;
        }
        sequence_iterate(L, -1) {
          /* stack: ordval ordval[i]*/
          if (p->length == MAX_LEN_ROUTE) {
            luaL_error(L, "Orders package overflow");
            lua_pop(L, 2); /* stack: */
            return FALSE;
          } /*stack: ordval ordval[i] */
          if (!add_top(p, L, amrt)) { /* stack: ordval */
            /* Invalid order in inner list.
             * Pop value and errmsg are done by the called function. */
            lua_pop(L, 1); /* stack: */
            return FALSE;
          }/* stack: ordval */
        } sequence_iterate_end(L);/* stack: ordval */
        ptl = p->length - ptnn;
        
         if ((p->length = ptnn + ptl * times) > MAX_LEN_ROUTE) {
         luaL_error(L, "Orders package overflow");
          lua_pop(L, 1); /* stack: */
          return FALSE;
        }

       for (int i = ptnn; i < p->length - ptl; i++) {
          p->orders[i + ptl] = (order = p->orders[i]);
          p->dir[i + ptl] = (dir = p->dir[i]);
          p->activity[i + ptl] = p->activity[i];
          p->target[i + ptl] = p->target[i];

          /* Shifting destintion tile. */
          if ((ORDER_MOVE == order || ORDER_ACTION_MOVE == order)
              && is_valid_dir(dir)) {
            p->dest_tile =
              tile_index(mapstep(index_to_tile(p->dest_tile), dir));
          }
        }

        return TRUE;
      } else {
        luaL_error(L, "times specifier %d out of diapazone (0-"
                   macro2str(MAX_LEN_ROUTE)")", times);
        lua_pop(L, 1);/* stack: ordval */
        return FALSE;
      }
      lua_pop(L, 1); /* stack: */
    } else if (!lua_isnil(L, -1)) {
      luaL_error(L, "Wrong times specifier (%s, must be integer)",
                 tolua_typename(L, -1));
      lua_pop(L, 2);/* stack: */
      return FALSE;
    } else {
      lua_pop(L, 1);/* stack: ordval */
    }
    /* single order */
    lua_getfield(L, -1, "order");/* stack: ordval ordval.order */
    if (lua_isinteger(L, -1)) {
      order = lua_tointeger(L, -1);
    } else if (lua_isstring(L, -1)) {
      order = pchar2order(lua_tostring(L, -1));
    } else if (!lua_isnil(L, -1)) {
      luaL_error(L, "Invalid order specifier type %s",
                 tolua_typename(L, -1));
      lua_pop(L, 2);
      return FALSE;
    }
    lua_pop(L, 1);/* stack: ordval */
    
    lua_getfield(L, -1, "dir");/* stack: ordval ordval.dir */
    dir = top2dir8(L);
    if (!lua_isnil(L, -1) && !is_valid_dir(dir)) {
      luaL_error(L, "Invalid direction specifier of type %s",
                 tolua_typename(L, -1));
      lua_pop(L, 2);
      return FALSE;
    }
    lua_pop(L, 1);/* stack: ordval */

    lua_getfield(L, -1, "activity");/* stack: ordval ordval.activity */
    if (lua_isstring(L, -1)) {
      if (!try_set_activity(&activity, lua_tostring(L, -1))) {
        luaL_error(L, "Invalid activity specifier '%s'",
                   lua_tostring(L, -1));
        lua_pop(L, 2);
        return FALSE;
      }
    } else if (!lua_isnil(L, -1)) {
      luaL_error(L, "Invalid activity specifier type %s",
                 tolua_typename(L, -1));
      lua_pop(L, 2);
      return FALSE;
    }
    lua_pop(L, 1);/* stack: ordval */
    
    lua_getfield(L, -1, "target");/* stack: ordval ordval.target */
    if (lua_isstring(L, -1)) {
      extra = extra_type_by_rule_name(lua_tostring(L, -1));
      if (NULL != extra) {
        target = extra_index(extra);
      } else {
        luaL_error(L, "Invalid target specifier '%s'",
                   lua_tostring(L, -1));
        lua_pop(L, 2);
        return FALSE;
      }
    } else if (!lua_isnil(L, -1)) {
      luaL_error(L, "Invalid target specifier type %s",
                 tolua_typename(L, -1));
      lua_pop(L, 2);
      return FALSE;
    }
    lua_pop(L, 1);/* stack: ordval */

    /* Maybe, somehow interpret sequences here? */
    break;
  default:
    luaL_error(L, "Invalid type %s supplied as an order",
               lua_typename(L, lua_type(L, -1)));
    lua_pop(L, 1);/* stack: */
    return FALSE;
  }

  /* Autofilling and checking */
  if (!unit_activity_is_valid(activity)) {
    if (target != EXTRA_NONE) {
      /* FIXME: some weird rulesets ay cause extras by different
       * activities for different units... */
      if (is_extra_caused_by(extra, EC_BASE)) {
        activity = ACTIVITY_BASE;
      } else if (is_extra_caused_by(extra, EC_ROAD)) {
        activity = ACTIVITY_GEN_ROAD;
      } else if (is_extra_caused_by(extra, EC_MINE)) {
        activity = ACTIVITY_MINE;
      } else if (is_extra_caused_by(extra, EC_IRRIGATION)) {
        activity = ACTIVITY_IRRIGATE;
      }
    }
  } else if (EXTRA_NONE == target) {
    enum extra_cause ec = activity_to_extra_cause(activity);
    if (EC_NONE != ec) { /* Pillage gets EC_NONE, it's right */
      const struct unit *pu = game_unit_by_number(p->unit_id);
      const struct player *uo = unit_owner(pu);
      const struct tile *dt = index_to_tile(p->dest_tile);
      const struct terrain *pterrain = tile_terrain(dt);

      /* Don't supply anything for transforming activities */
      if (!((EC_IRRIGATION == ec && pterrain->irrigation_result != pterrain)
            || (EC_MINE == ec &&(pterrain->mining_result != pterrain)))) {
        /* TODO: see what is built by previous orders */
        extra = next_extra_for_tile(dt, ec, uo, pu);
        if (extra) {
          /* We found a thing to put here */
          target = extra_index(extra);
        } else if (activity_requires_target(activity)) {
          /* All is already here but we have to put something */
          extra_type_by_cause_iterate(ec, ety) {
            target = extra_index(ety);
            break;
          } extra_type_by_cause_iterate_end;
        }
      }
    }
  }
  if (order < ORDER_MOVE || order >= ORDER_LAST) {
    if (dir <= direction8_invalid() && is_valid_dir(dir)) {
      if (amrt <= AMRT_AUTO) {
        order = ORDER_MOVE;
      } else if (amrt == AMRT_ALL) {
        order = ORDER_ACTION_MOVE;
      } else {
        const
          struct player *uo = unit_owner(game_unit_by_number(p->unit_id));
        const struct tile *dt = index_to_tile(p->dest_tile);

        fc_assert(amrt == AMRT_AUTO_EACH);
        if (is_non_allied_city_tile(dt, uo)
            || is_non_allied_unit_tile(dt, uo)){
          order = ORDER_ACTION_MOVE;
        } else {
          order = ORDER_MOVE;
        }
      }
    } else {
      dir = DIR8_ORIGIN;
    }
    /* Activity suppresses direction, just for if we need it for a hack */
    if (unit_activity_is_valid(activity)) {
      order = ORDER_ACTIVITY;
    }
  }
  /* You are not supposed to break the protocol */
  if (0 > order || order >= ORDER_LAST) {
    char buf[255];
    fc_snprintf(buf, sizeof(buf), "Could not identify order [%d] (%d?)",
                p->length + 1, order);
    luascript_arg_error(L, 2, buf);
    return FALSE;
  }

  /* Writing it to the package's next line */
  times = p->length;
  fc_assert_ret_val(0 <= times && times < MAX_LEN_ROUTE, FALSE);

  p->orders[times] = order;
  p->dir[times] = dir;
  p->activity[times] = activity;
  p->target[times] = target;
  p->length++;
  
  /* Shifting destintion tile. */
  if ((ORDER_MOVE == order || ORDER_ACTION_MOVE == order)
      && is_valid_dir(dir)) {
    p->dest_tile = tile_index(mapstep(index_to_tile(p->dest_tile), dir));
  }

  lua_pop(L, 1);/*stack: */
  return TRUE;
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
  LUASCRIPT_CHECK_SELF(L, tile);

  center_tile_mapcanvas(tile);
}

/**********************************************************************//***
  Centers the view at given tile
***************************************************************************/
void api_client_center_coords(lua_State *L, int x, int y)
{
  Tile *ptile = map_pos_to_tile(x, y);

  LUASCRIPT_CHECK_STATE(L);
  LUASCRIPT_CHECK(L, ptile, "Wrong coordinates supplied");

  center_tile_mapcanvas(ptile);
}

/**********************************************************************//***
  Head of focused units list
***************************************************************************/
Unit_List_Link *api_client_private_focus_head(lua_State *L)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  return unit_list_head(get_units_in_focus());
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

/**********************************************************************//***
  Add unit to the selection
***************************************************************************/
void api_client_unit_focus_add(lua_State *L, Unit *punit)
{
  LUASCRIPT_CHECK_STATE(L);
  LUASCRIPT_CHECK_ARG_NIL(L, punit, 2, Unit);

  unit_focus_add(punit);
}

/**********************************************************************//***
  Remove unit from the selection
***************************************************************************/
void api_client_unit_focus_remove(lua_State *L, Unit *punit)
{
  LUASCRIPT_CHECK_STATE(L);
  LUASCRIPT_CHECK_ARG_NIL(L, punit, 2, Unit);

  unit_focus_remove(punit);
}

/**********************************************************************//***
  Is a unit in focus
***************************************************************************/
bool api_client_unit_is_in_focus(lua_State *L, Unit *punit)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, punit, NULL);

  return unit_is_in_focus(punit);
}

/**********************************************************************//***
  Number of units in focus
***************************************************************************/
int api_client_num_units_in_focus(lua_State *L)
{
  LUASCRIPT_CHECK_STATE(L, -1);
  return get_num_units_in_focus();
}

/**********************************************************************//***
  Says if a seen city has some unit(s) inside
***************************************************************************/
bool api_client_city_occupied(lua_State *L, City *pcity)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  return pcity->client.occupied;
}

/*****************************************************************************
  Return TRUE iff city (foreign, as we remember it, or ours) is happy,
  based on server reports if the city is not virtual
*****************************************************************************/
bool api_client_is_city_happy(lua_State *L, City *pcity)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pcity, FALSE);

  return city_is_virtual(pcity) ? city_happy(pcity) : pcity->client.happy;
}

/*****************************************************************************
  Return TRUE iff city (foreign, as we remember it, or ours) is unhappy
  based on server reports if the city is not virtual
*****************************************************************************/
bool api_client_is_city_unhappy(lua_State *L, City *pcity)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pcity, FALSE);

  return city_is_virtual(pcity)
    ? city_unhappy(pcity) : pcity->client.unhappy;
}

/**********************************************************************//***
  Request city production change to the given item
***************************************************************************/
void api_client_city_change_production(lua_State *L, City *pcity,
                                       lua_Object prod)
{
  const char *argt = tolua_typename(L, prod);
  enum universals_n kind;

  LUASCRIPT_CHECK_STATE(L);
  LUASCRIPT_CHECK_SELF(L, pcity);

  if (strcmp(argt, "Unit_Type")) {
    kind = VUT_UTYPE;
  } else if (strcmp(argt, "Building_Type")) {
    kind = VUT_IMPROVEMENT;
  } else {
    luascript_arg_error(L, 3, "Wrong production type (must be Unit_Type"
                              " or Building_Type");
    return;
  }
  lua_getfield(L, prod, "id");
  dsend_packet_city_change(&client.conn, pcity->id, kind,
                           lua_tointeger(L, -1)); /* cleaned by wrap code */
}

/**********************************************************************//***
  Request city specialist change from one type to the another
***************************************************************************/
void api_client_city_change_specialist(lua_State *L, City *pcity,
                                       const char *s_from,
                                       const char *s_to)
{
  const struct specialist *from, *to;
  LUASCRIPT_CHECK_STATE(L);
  LUASCRIPT_CHECK_SELF(L, pcity);
  LUASCRIPT_CHECK_ARG(L, from = specialist_by_rule_name(s_from), 3,
                      "wrong 'from' specialist type name");
  LUASCRIPT_CHECK_ARG(L, to = specialist_by_rule_name(s_to), 4,
                      "wrong 'to' specialist type name");

  (void) city_change_specialist(pcity, specialist_index(from),
                                specialist_index(to));
}

/**********************************************************************//***
  Request placing pcity worker at ptile.
  If ptile is NULL or the pcity's center, the workers will be auto-arranged.
  Returns if the tile seems to be within the city radius
***************************************************************************/
bool api_client_city_make_worker(lua_State *L, City *pcity, Tile *ptile)
{
  int city_x, city_y;
  bool res;

  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, pcity, FALSE);
  if (!ptile) {
    city_x = 0;
    city_y = 0;
    res = TRUE;
  } else {
    res = city_base_to_city_map(&city_x, &city_y, pcity, ptile);
  }
  (void) dsend_packet_city_make_worker(&client.conn, pcity->id,
                                       city_x, city_y);
  return res;
}

/**********************************************************************//***
  Request removing pcity worker from ptile making a default specialist.
  If ptile is NULL or the pcity's center, the workers will be auto-arranged.
  Returns if the tile seems to be within the city radius
***************************************************************************/
bool api_client_city_make_specialist(lua_State *L, City *pcity, Tile *ptile)
{
  int city_x, city_y;
  bool res;

  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, pcity, FALSE);
  if (!ptile) {
    city_x = 0;
    city_y = 0;
    res = TRUE;
  } else {
    res = city_base_to_city_map(&city_x, &city_y, pcity, ptile);
  }
  (void) dsend_packet_city_make_specialist(&client.conn, pcity->id,
                                           city_x, city_y);
  return res;
}

/**********************************************************************//***
  Return CMA preset name, "custom" for custom governor, nil for none
***************************************************************************/
const char *api_client_city_cma_name(lua_State *L, City *pcity)
{
  struct cm_parameter cmpar;

  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pcity, NULL);

  if (cma_is_city_under_agent(pcity, &cmpar)) {
    return cmafec_get_short_descr(&cmpar);
  } else {
    return NULL;
  }
}

/**********************************************************************//***
  Says if a seen unit is transporting something
***************************************************************************/
bool api_client_unit_occupied(lua_State *L, Unit *punit)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  return punit->client.occupied;
}

/**********************************************************************//***
  Request airlift punit to pcity
***************************************************************************/
void api_client_unit_airlift(lua_State *L, Unit *punit, City *pcity)
{
  LUASCRIPT_CHECK_STATE(L);
  LUASCRIPT_CHECK_SELF(L, punit);
  LUASCRIPT_CHECK_ARG_NIL(L, pcity, 3, city);
  request_unit_airlift(punit, pcity);
}

/**********************************************************************//***
  Request load pcargo into ptransport (if NULL, tries to find one).
  Does nothing if the operation is surely invalid,
  changes pcargo activity to Sentry
***************************************************************************/
void api_client_unit_load(lua_State *L, Unit *pcargo, Unit *ptransport)
{
  LUASCRIPT_CHECK_STATE(L);
  LUASCRIPT_CHECK_SELF(L, pcargo);

  request_unit_load(pcargo, ptransport, unit_tile(ptransport));
}

/**********************************************************************//***
  Request unload pcargo from ptransport (you must own at least one of them).
  If ptransport is nil, uses pcargo's transporter and, if pcargo's
  activity is Sentry, sets it to Idle.
  Otherwise, does not check correctness of unloading.
***************************************************************************/
void api_client_unit_unload(lua_State *L, Unit *pcargo, Unit *ptransport)
{
  LUASCRIPT_CHECK_STATE(L);
  LUASCRIPT_CHECK_SELF(L, pcargo);

  if (NULL == ptransport) {
    request_unit_unload(pcargo);
  } else {
    dsend_packet_unit_unload(&client.conn, pcargo->id, ptransport->id);
  }
}

/**********************************************************************//***
  Requests move punit from its current location to ptile.
  Returns FALSE if no path is found.
***************************************************************************/
bool api_client_unit_move(lua_State *L, Unit *punit, Tile *ptile)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, punit, FALSE);
  LUASCRIPT_CHECK_ARG_NIL(L, ptile, 3, tile, FALSE);

  return send_goto_tile(punit, ptile);
}

/**********************************************************************//***
  Requests punit upgrade. Does not check if there is a city.
***************************************************************************/
void api_client_unit_upgrade(lua_State *L, Unit *punit)
{
  LUASCRIPT_CHECK_STATE(L);
  LUASCRIPT_CHECK_SELF(L, punit);

  dsend_packet_unit_upgrade(&client.conn, punit->id);
}

/**********************************************************************//***
  Requests building or adding to a city. If name is not given, requests it
  in a standard way
***************************************************************************/
void api_client_unit_build_city(lua_State *L, Unit *punit, const char *name)
{
  LUASCRIPT_CHECK_STATE(L);
  LUASCRIPT_CHECK_SELF(L, punit);

  if (name) {
    dsend_packet_unit_build_city(&client.conn, punit->id, name);
  } else {
    request_unit_build_city(punit);
  }
}

/**********************************************************************//***
  Gives orders to punit as given in seq. Each order is a table or
  a shortcut of it
***************************************************************************/
void api_client_unit_give_orders(lua_State *L, Unit *punit, lua_Object seq,
                                 bool vigilant, bool rep)
{
  struct packet_unit_orders p;
  struct tile *curr = unit_tile(punit);
  enum actmove_res_type amrt = AMRT_AUTO;
  int len;

  LUASCRIPT_CHECK_STATE(L);
  LUASCRIPT_CHECK_SELF(L, punit);
  LUASCRIPT_CHECK_ARG(L, lua_type(L, seq) == LUA_TTABLE, 3,
                      "Order sequence must be a table");

  memset(&p, 0, sizeof(p)); /* likely, helps compression, inits p.length=0 */
  p.unit_id = punit->id;
  p.dest_tile = (p.src_tile = tile_index(curr)); /* shifted by add_top() */
  p.repeat = rep;
  p.vigilant = vigilant;

  lua_getfield(L, seq, "actmove");
  switch (lua_type(L, -1)) {
  case LUA_TNIL:
    break;
  case LUA_TBOOLEAN:
    if (lua_toboolean(L, -1)) {
      amrt = AMRT_ALL;
    } else {
      amrt = AMRT_NO;
    }
    break;
  case LUA_TSTRING:
    {
      const char *str = lua_tostring(L, -1);
      if (!fc_strcasecmp(str, "all")) {
        amrt = AMRT_ALL;
        break;
      } else if (!fc_strcasecmp(str, "auto_each")) {
        amrt = AMRT_AUTO_EACH;
        break;
      } else if (!fc_strcasecmp(str, "auto")) {
        break;
      } else if (!fc_strcasecmp(str, "no")) {
        amrt = AMRT_NO;
        break;
      }
    }
    /* no break */
  default:
    luascript_arg_error(L, 1, "Wrong 'actmove' specifier "
                        "(must be 'auto|auto_each|all|no')");
    return;
  }
  lua_len(L, seq);
  len = lua_tointeger(L, -1);
  lua_pop(L, 2);
  sequence_iterate(L, seq) {/* push seq[i] */ 
    if (p.length <= MAX_LEN_ROUTE) {
      if (!add_top(&p, L, AMRT_AUTO == amrt && _seqno == len
                   ? AMRT_AUTO_EACH : amrt)) { /* pop seq[i] */
        break; /* for */
      }
    } else {
      lua_pop(L, 1);
      luaL_error(L, "Orders package overflow");
      return;
    }
  } sequence_iterate_end(L);
  lua_pop(L, 1); /* pop final nil */
  send_packet_unit_orders(&client.conn, &p);
}

/**********************************************************************//***
  Requests punit to do an activity. Target is optional.
  If the activity requires target and it is missing, tries to find one
***************************************************************************/
void api_client_unit_request_activity_targeted(lua_State *L, Unit *punit,
                                               const char *activity_name,
                                               const char *target)
{
  enum unit_activity act;
  /* const */ struct extra_type *tgt = NULL;

  LUASCRIPT_CHECK_STATE(L);
  LUASCRIPT_CHECK_SELF(L, punit);
  LUASCRIPT_CHECK_ARG_NIL(L, activity_name, 3, string);

  if (target) {
    tgt = extra_type_by_rule_name(target);
    if (!tgt) {
      luascript_arg_error(L, 4, "bad target extra name");
    }
  }
  act = unit_activity_by_name(activity_name, fc_strcasecmp);
  if (unit_activity_is_valid(act)) {
    if (activity_requires_target(act)) {
      enum extra_cause ec = activity_to_extra_cause(act);
      if (extra_cause_is_valid(ec)) {
        tgt = next_extra_for_tile(unit_tile(punit), ec, unit_owner(punit),
                                  punit);
        if (NULL == tgt) {
          extra_type_by_cause_iterate(ec, ety) {
            tgt = ety;
            break;
          } extra_type_by_cause_iterate_end;
        }
      }
    }
    request_new_unit_activity_targeted(punit, act, tgt);
  } else {
    luascript_arg_error(L, 3, "bad activity name");
  }
}

/**********************************************************************//***
  Requests punit to perform an action. Value is optional.
  Target id supplied as integer is not checked, otherwise
  target kind must conform to the action one.
  If the action requires target and it is missing, tries to find one
  (currently, just takes the city the unit is in).
  Value is needed for targeted diplomatic actions, it is innerly increased
  at 1 if the action is "Targeted Sabotage City" (as the protocol requires);
  a check is done for user types used for it and for "Targeted Steal Tech"
***************************************************************************/
void api_client_unit_do_action(lua_State *L, Unit *punit,
                               const Action *paction, lua_Object target,
                               lua_Object value)
{
  enum gen_action act;
  int tid = 0, ch, vid = 0;
  enum action_target_kind atk;

  LUASCRIPT_CHECK_STATE(L);
  LUASCRIPT_CHECK_SELF(L, punit);
  LUASCRIPT_CHECK_ARG_NIL(L, paction, 3, action/id/name); /* by other funcs */
  act = paction->id;
  atk = action_get_target_kind(paction);

  fc_assert(AAK_UNIT == action_get_actor_kind(paction));

  if (0 != target) {
    switch (lua_type(L, target)) {
    case LUA_TNIL:
      break;
    case LUA_TNUMBER:
      tid = lua_tointegerx(L, target, &ch);
      break;
    case LUA_TLIGHTUSERDATA:
    case LUA_TUSERDATA:
      lua_getfield(L, target, "id"); /* hope it won't fail here */
      if (LUA_TNUMBER == lua_type(L, -1)) {
        tid = lua_tointeger(L, -1);
      } /* popped by glue code */
      break;
    default:
      luascript_arg_error(L, 4, "unsupported target type");
      return;
    }
  }
  if (0 == tid) {
    const struct city *tcity = NULL;
    const struct unit *tunit = NULL;
    const struct tile *ut = unit_tile(punit);
    switch (atk) {
    case ATK_CITY:
      tcity = tile_city(ut);
      if (!tcity) {/* Do it somewhere around? */
        adjc_iterate(ut, nt) {
          const struct city *tc = tile_city(nt);
          if (tc) {
            if (tcity) { /* Too many cities around! */
              tcity = NULL;
              break;
            } else {
              tcity = tc;
            }
          }
        } adjc_iterate_end;
      }
      if (tcity) {
        tid = tcity->id;
      } else {
        luascript_arg_error(L, 4, "no city supplied as an action target");
      }
      break;
    case ATK_UNIT:
      /* Does not check if the action is valid for other units
       * or for units on the same tile */
      adjc_iterate(ut, nt) {
        bool finish = FALSE;
        switch (unit_list_size(ut->units)) {
        case 0: break;
        case 1:
          tunit = unit_list_get(ut->units, 0);
          break;
        default: finish = TRUE; /* Too many units around! */
        }
        if (finish) {
          tunit = NULL;
          break;
        }
      } adjc_iterate_end;
      if (tunit) {
        tid = tunit->id;
      } else {
        luascript_arg_error(L, 4, "no unit supplied as an action target");
      }
      break;
    default:
      fc_assert_ret_msg(FALSE, "unknown action target kind found!");
    }
  }
  if (0 == tid) {
    luascript_arg_error(L, 4, "target could not be identified");
    return;
  }
  if (value != 0) {
    switch (lua_type(L, value)) {
    case LUA_TNUMBER:
      vid = lua_tointeger(L, value);
    case LUA_TLIGHTUSERDATA:
    case LUA_TUSERDATA:
      if (ACTION_SPY_TARGETED_STEAL_TECH == act
          && strcmp(tolua_typename(L, value), "Tech_Type")) {
        luascript_arg_error(L, 5, "value must be tech type/id"
                            "for this action");
        return;
      }
      if (ACTION_SPY_TARGETED_SABOTAGE_CITY == act
          && strcmp(tolua_typename(L, value), "Building_Type")) {
        luascript_arg_error(L, 5, "value must be building type/id"
                            "for this action");
        return;
      }
      lua_getfield(L, value, "id"); /* hope it won't fail here */
      if (LUA_TNUMBER == lua_type(L, -1)) {
        vid = lua_tointeger(L, -1);
      } /* popped by glue code */
      break;
    case LUA_TSTRING: /* Guess by action what is it */
      switch (act) {
      case ACTION_SPY_TARGETED_STEAL_TECH:
        {
          const struct advance *pa
           = advance_by_rule_name(lua_tostring(L, value));
          if (pa){
            vid = advance_index(pa);
          } else if (!fc_strcasecmp("random", lua_tostring(L, value))) {
            vid = A_UNSET;
          } else {
            luascript_arg_error(L, 5,"tech type/id/name value "
                                "required for this action");
            return;
          }
        }
        break;
      case ACTION_SPY_TARGETED_SABOTAGE_CITY:
        {
          const struct impr_type *pi
           = improvement_by_rule_name(lua_tostring(L, value));
          if (pi) {
            vid = improvement_index(pi);
          } else if (!fc_strcasecmp("Production", lua_tostring(L, value))) {
            vid = -1;
          } else if (!fc_strcasecmp("random", lua_tostring(L, value))) {
            vid = B_LAST;
          } else {
            luascript_arg_error(L, 5, "building type/id/name value required "
                                "for this action");
            return;
          }
        }
        break;
      default:
        luascript_arg_error(L, 5, "can't interpret string value"
                            "for this action");
        return;
      }
      break;
    default:
      luascript_arg_error(L, 5, "unsupported value type");
      return;
    }
    if (ACTION_SPY_TARGETED_SABOTAGE_CITY == act) {
      vid++;
    }
  } else {
    switch (act) {
    case ACTION_SPY_STEAL_TECH:
    case ACTION_SPY_TARGETED_STEAL_TECH:
      vid = A_UNSET; /* at spy's choice */
      break;
    case ACTION_SPY_SABOTAGE_CITY:
    case ACTION_SPY_TARGETED_SABOTAGE_CITY:
      vid = B_LAST + 1; /* at spy's choice */
      break;
    default:
      break;/* nothing to do */
    }
  }

  request_do_action(act, punit->id, tid, vid);
}

/**********************************************************************//***
  Wrapper for named action form
***************************************************************************/
void api_client_unit_do_action_name(lua_State *L, Unit *punit,
                                    const char *name, lua_Object target,
                                    lua_Object value)
{
  enum gen_action act = gen_action_by_name(name, fc_strcasecmp);

  LUASCRIPT_CHECK_STATE(L);
  LUASCRIPT_CHECK_ARG(L, gen_action_is_valid(act), 3, "wrong action name");

  api_client_unit_do_action(L, punit, action_by_number(act), target, value);
}

/**********************************************************************//***
  Wrapper for numbered action form
***************************************************************************/
void api_client_unit_do_action_id(lua_State *L, Unit *punit,
                                  int act, lua_Object target,
                                  lua_Object value)
{
  const struct action *pact = action_by_number(act);

  LUASCRIPT_CHECK_STATE(L);
  LUASCRIPT_CHECK_ARG(L, pact, 3, "wrong action id");

  api_client_unit_do_action(L, punit, pact, target, value);
}
