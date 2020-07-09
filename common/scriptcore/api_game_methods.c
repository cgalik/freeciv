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
#include "achievements.h"
#include "actions.h"
#include "citizens.h"
#include "culture.h"
#include "combat.h"
#include "game.h"
#include "government.h"
#include "improvement.h"
#include "map.h"
#include "movement.h"
#include "nation.h"
#include "research.h"
#include "specialist.h"
#include "tech.h"
#include "terrain.h"
#include "tile.h"
#include "unitlist.h"
#include "unittype.h"
#include "vision.h"

/* common/scriptcore */
#include "luascript.h"

#include "api_game_methods.h"

/* FIXME: Move this to some header files [[ */
#define macro2str_base(__macro__) #__macro__
#define macro2str(__macro__) macro2str_base(__macro__)

static int tile_gcdist(const struct tile *ptile, const struct player *plr);
static Output_type_id luao2otype(lua_State *L, lua_Object oty);
/* ]] */
static lua_Object ap2top(lua_State *L, struct act_prob ap, lua_Object *r2);

static /* FIXME: Move this to some common C file [[ */
int tile_gcdist(const struct tile *ptile, const struct player *plr)
{
  int res = FC_INFINITY;
  struct city *gc = tile_city(ptile);
  
  if (gc && city_owner(gc) == plr && is_gov_center(gc)) {
    return 0;
  } else {
    city_list_iterate(plr->cities, pc) {
      /* Do not recheck current city */
      if (gc != pc && is_gov_center(pc)) {
        int dist = real_map_distance(ptile, pc->tile);

        if (dist < res) {
          res = dist;
        }
      }
    } city_list_iterate_end;
  }
  return FC_INFINITY == res ? -1 : res;
}

/***********************************************************************//****
  Helper to return two values from tolua-wrapped code.
  For normal ap, the first result is lower estimation and the second one
  is upper estimation, in half-percents. For special ap, first result
  is nil and the second one is an explanation string.
*****************************************************************************/
static lua_Object ap2top(lua_State *L, struct act_prob ap, lua_Object *r2)
{
  if (ap.min <= ap.max) {
    lua_pushinteger(L, ap.max);
    *r2 = lua_gettop(L);
    lua_pushinteger(L, ap.min);
  } else {
    if (253 == ap.min && 0 == ap.max) {
      lua_pushstring(L, "not relevant");
    } else if(254 == ap.min && 0 == ap.max) {
      lua_pushstring(L, "not implemented");
    } else {
      fc_assert_msg(FALSE, "act_prob unknown special value!");
      lua_pushnil(L);
    }
    *r2 = lua_gettop(L);
    lua_pushnil(L);
  }
  return lua_gettop(L);
}

static Output_type_id luao2otype(lua_State *L, lua_Object oty)
{
  /* LUASCRIPT_CHECK_STATE(L, O_LAST); Already checked */
  switch (lua_type(L, oty)) {
  case LUA_TNUMBER:
    return lua_tointeger(L, oty);
  case LUA_TSTRING:
    return output_type_by_identifier(lua_tostring(L, oty));
  default:
    return O_LAST;
  }
}
/* ]] */

#define CITY_OUTPUTF(fname)                                              \
int api_methods_city_##fname(lua_State *L, City *pcity, lua_Object otype)\
{                                                                        \
  Output_type_id oty = luao2otype(L, otype);                             \
  LUASCRIPT_CHECK_STATE(L, 0);                                           \
  LUASCRIPT_CHECK_SELF(L, pcity, 0);                                     \
  LUASCRIPT_CHECK_ARG(L, oty >= O_FOOD && oty < O_LAST, 3,               \
                      "Wrong output type", 0);                           \
  return pcity->fname[oty];                                              \
}

CITY_OUTPUTF(surplus) /* Final surplus in each category. */
CITY_OUTPUTF(waste) /* Waste/corruption in each category. */
CITY_OUTPUTF(unhappy_penalty) /* Penalty from unhappy cities. */
CITY_OUTPUTF(prod) /* Production is total minus waste and penalty. */
CITY_OUTPUTF(citizen_base) /* Base production from citizens. */
CITY_OUTPUTF(usage) /* Amount of each resource being used. */
#undef CITY_OUTPUTF

/*****************************************************************************
  Return the current turn.
*****************************************************************************/
int api_methods_game_turn(lua_State *L)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);

  return game.info.turn;
}

/*****************************************************************************
  Return game win chance with tabular arguments
*****************************************************************************/
double api_methods_game_win_chance(lua_State *L, 
                                   int as, int ahp, int afp,
                                   int ds, int dhp, int dfp)
{
  LUASCRIPT_CHECK_STATE(L, 0.0);
  LUASCRIPT_CHECK_ARG(L, as >= 0, 2, "att.strength", 0.0);
  LUASCRIPT_CHECK_ARG(L, ahp >= 0, 3, "att.health", 0.0);
  LUASCRIPT_CHECK_ARG(L, afp > 0, 4, "att.firepower", 0.0);
  LUASCRIPT_CHECK_ARG(L, ds >= 0, 2, "def.strength", 0.0);
  LUASCRIPT_CHECK_ARG(L, dhp >= 0, 3, "def.health", 0.0);
  LUASCRIPT_CHECK_ARG(L, dfp > 0, 4, "def.firepower", 0.0);

  return win_chance(as, ahp, afp, ds, dhp, dfp);
}


/*****************************************************************************
  Return TRUE if pbuilding is a wonder.
*****************************************************************************/
bool api_methods_building_type_is_wonder(lua_State *L,
                                         Building_Type *pbuilding)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, pbuilding, FALSE);

  return is_wonder(pbuilding);
}

/*****************************************************************************
  Return TRUE if pbuilding is a great wonder.
*****************************************************************************/
bool api_methods_building_type_is_great_wonder(lua_State *L,
                                               Building_Type *pbuilding)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, pbuilding, FALSE);

  return is_great_wonder(pbuilding);
}

/*****************************************************************************
  Return TRUE if pbuilding is a small wonder.
*****************************************************************************/
bool api_methods_building_type_is_small_wonder(lua_State *L,
                                               Building_Type *pbuilding)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, pbuilding, FALSE);

  return is_small_wonder(pbuilding);
}

/*****************************************************************************
  Return TRUE if pbuilding is a building.
*****************************************************************************/
bool api_methods_building_type_is_improvement(lua_State *L,
                                              Building_Type *pbuilding)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, pbuilding, FALSE);

  return is_improvement(pbuilding);
}

/*****************************************************************************
  Return rule name for Building_Type
*****************************************************************************/
const char *api_methods_building_type_rule_name(lua_State *L,
                                                Building_Type *pbuilding)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pbuilding, NULL);

  return improvement_rule_name(pbuilding);
}

/*****************************************************************************
  Return translated name for Building_Type
*****************************************************************************/
const char
  *api_methods_building_type_name_translation(lua_State *L,
                                              Building_Type *pbuilding)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pbuilding, NULL);

  return improvement_name_translation(pbuilding);
}

/*********************************************************************//***
  Returns how much of production of otype the city wastes.
  If gcdist is specified, considers this govcener distance, otherwise
  tries to calculate by available map data.
  Does not consider trade min. size or unhappy state.
**************************************************************************/
double api_methods_city_waste_level(lua_State *L, City *pcity,
                                    int otype, lua_Object gcd)
{
  int gcdist;

  LUASCRIPT_CHECK_STATE(L, 0.);
  LUASCRIPT_CHECK_SELF(L, pcity, 0.);
  LUASCRIPT_CHECK_ARG(L, otype >= O_FOOD && otype < O_LAST,
                      3, "Wrong output type", 0.);
  
  if (gcd && lua_isnumber(L, gcd)) {
    gcdist = lua_tointeger(L, gcd);
  } else {
    gcdist = tile_gcdist(city_tile(pcity), city_owner(pcity));
  }

  if (gcdist < 0) { /* Waste all if no capital */
    return 1.;
  }
  
  return (double)
  (get_city_output_bonus(pcity, get_output_type(otype), EFT_OUTPUT_WASTE)
   + gcdist
    * get_city_output_bonus(pcity, get_output_type(otype),
                            EFT_OUTPUT_WASTE_BY_DISTANCE)
  ) * 0.0001
  * (100 - get_city_output_bonus(pcity, get_output_type(otype),
                                 EFT_OUTPUT_WASTE_PCT));
}

/*********************************************************************//***
  Returns how much of production of otn the city wastes.
  Wrapper for the numeric otype function that understands output type names
**************************************************************************/
double
api_methods_city_waste_level_ostr(lua_State *L, City *pcity,
                                  const char* otn, lua_Object gcd)
{
  return api_methods_city_waste_level(L, pcity, 
                                      output_type_by_identifier(otn), gcd);
}

/*********************************************************************//***
  Pushes a table of pcity nationalities {[Player] = int},
  or nil if nationalities are off
**************************************************************************/
lua_Object api_methods_city_nationality(lua_State *L, City *pcity)
{
  int t;
  
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, pcity, 0);

  if (!game.info.citizen_nationality) {
    lua_pushnil(L);
    return lua_gettop(L);
  }

  lua_newtable(L);
  t = lua_gettop(L);
  citizens_iterate(pcity, psl, num) {
    tolua_pushusertype(L, player_slot_get_player(psl), "Player");
    lua_pushinteger(L, num);
    lua_settable(L, t);
  } citizens_iterate_end;

  return t;
}

/**************************************************************************
  Return number of specialists by rule name
**************************************************************************/
int api_methods_city_specialists(lua_State *L, City *pcity,
                                 const char *spec)
{
  struct specialist *specs = specialist_by_rule_name(spec);
  
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, pcity, 0);
  LUASCRIPT_CHECK_ARG(L, NULL != specs, 3, "Wrong specialist rule name", 0);
  
  return pcity->specialists[specs->item_number];
}

/*********************************************************************//***
  Returns if a city is virtual (can be got from City:trade_routes_iterate()
  etc. in the client). Such virtual cities contain few useful info.
**************************************************************************/
bool api_methods_city_is_virtual(lua_State *L, City *pcity)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, pcity, FALSE);
  
  return city_is_virtual(pcity);
}

/*********************************************************************//***
  Returns current number of established trade routes
**************************************************************************/
int api_methods_city_traderoutes_number(lua_State *L, City *pcity)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, pcity, FALSE);
  
  return city_num_trade_routes(pcity);
}

/*********************************************************************//***
  Returns a table {[trade_partner_city] = route_output} of the city.
  In client, unknown cities are virtual.
**************************************************************************/
lua_Object api_methods_city_trade_routes(lua_State *L, City *pcity)
{
  lua_Object table;

  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, pcity, 0);
  
  lua_createtable(L, 0, city_num_trade_routes(pcity));
  table = lua_gettop(L);
  
  trade_routes_iterate(pcity, tcity) {
    tolua_pushusertype(L, tcity, "City");
    lua_pushinteger(L, pcity->trade_value[_itcity]);
    lua_settable(L, table);
  } trade_routes_iterate_end;

  return table;
}


/*********************************************************************//***
  Returns how much trade pcity has or may have per turn trading with tcity
**************************************************************************/
int api_methods_city_trade_with(lua_State *L, City *pcity, City *tcity)
{
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, pcity, 0);
  LUASCRIPT_CHECK_ARG_NIL(L, tcity, 3, city, 0);

  return trade_between_cities(pcity, tcity);
}

/*********************************************************************//***
  Returns one-time bonus of a caravan from pcity acting to tcity,
  if or if not the route is newly established
**************************************************************************/
int api_methods_caravan_bonus(lua_State *L, City *pcity, City *tcity,
                              bool establish)
{
  LUASCRIPT_CHECK_SELF(L, pcity, 0);
  LUASCRIPT_CHECK_ARG_NIL(L, tcity, 3, city, 0);

  /* FIXME: for foreign cities, dumb client does not know
   * if its owner knows and thus can work a single tile => errors... */
  return get_caravan_enter_city_trade_bonus(pcity, tcity, establish);
}

/*********************************************************************//***
  Tells how many citizens are as happy as cat at given level
**************************************************************************/
int api_methods_city_happy_count(lua_State *L, City *pcity,
                                 int cat, int level)
{
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, pcity, 0);
  LUASCRIPT_CHECK_ARG(L, cat >= CITIZEN_HAPPY && cat <= CITIZEN_SPECIALIST,
                      3, "Wrong citizens category %d (max. "
                      macro2str(CITIZEN_SPECIALIST) ")", 0);
  LUASCRIPT_CHECK_ARG(L, level >= FEELING_BASE && level < FEELING_LAST, 4,
                      "Wrong feeling level %d (max. "
                      macro2str(FEELING_FINAL) ")", 0);
  return
    pcity->feel[(enum citizen_category) cat][(enum citizen_feeling) level];
}

/*****************************************************************************
  Return number of city supported units
*****************************************************************************/
int api_methods_city_supported_units_number(lua_State *L, City *pcity)
{
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, pcity, 0);

  return unit_list_size(pcity->units_supported);
}

/*****************************************************************************
  Return list head for units supported by pcity
*****************************************************************************/
Unit_List_Link *api_methods_private_city_supported_list_head(lua_State *L,
                                                             City *pcity)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pcity, NULL);

  return unit_list_head(pcity->units_supported);
}

/*****************************************************************************
  Return TRUE iff city has building
*****************************************************************************/
bool api_methods_city_has_building(lua_State *L, City *pcity,
                                   Building_Type *building)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pcity, FALSE);
  LUASCRIPT_CHECK_ARG_NIL(L, building, 3, Building_Type, FALSE);

  return city_has_building(pcity, building);
}

/*****************************************************************************
  Returns a building or a unit that the city is working on now, or nil
*****************************************************************************/
lua_Object api_methods_city_production(lua_State *L, City *pcity)
{
  LUASCRIPT_CHECK_STATE(L, 0); /* no state, no top */
  /* LUASCRIPT_CHECK_SELF(L, lua_gettop(L)<-nil); */
  if (!pcity) {
    luascript_arg_error(L, 2, "got 'nil' for self");
    lua_pushnil(L); 
  } else {
    switch (pcity->production.kind) {
    case VUT_IMPROVEMENT:
      tolua_pushusertype(L, pcity->production.value.building,
                         "Building_Type");
      break;
    case VUT_UTYPE:
      tolua_pushusertype(L, pcity->production.value.utype, "Unit_Type");
      break;
    default:
      /* should not be here, except very strange cases */
      log_error("City builds some wrong kind %d", pcity->production.kind);
      lua_pushnil(L);
    }
  }
  return lua_gettop(L);
}

/*****************************************************************************
  Return the square raduis of the city map.
*****************************************************************************/
int api_methods_city_map_sq_radius(lua_State *L, City *pcity)
{
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, pcity, 0);

  return city_map_radius_sq_get(pcity);
}

/*****************************************************************************
  Return the square vision raduis of the city
*****************************************************************************/
int api_methods_city_vision_sq_radius(lua_State *L, City *self, int vl)
{
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, self, 0);
  LUASCRIPT_CHECK_ARG(L, V_MAIN <= vl && vl < V_COUNT, 3,
                      "Wrong vision layer", 0);

  switch (vl) {
  case V_MAIN:
    return get_city_bonus(self, EFT_CITY_VISION_RADIUS_SQ);
  case V_INVIS:
    return 2;
  };
  fc_assert_ret_val(FALSE /* should not be here */, -1);
}

/**************************************************************************
  Return the size of the city.
**************************************************************************/
int api_methods_city_size_get(lua_State *L, City *pcity)
{
  LUASCRIPT_CHECK_STATE(L, 1);
  LUASCRIPT_CHECK_SELF(L, pcity, 1);

  return city_size_get(pcity);
}

/**************************************************************************
  Return the tile of the city.
**************************************************************************/
Tile *api_methods_city_tile_get(lua_State *L, City *pcity)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pcity, NULL);

  return pcity->tile;
}

/**************************************************************************
  How much city inspires partisans for a player.
**************************************************************************/
int api_methods_city_inspire_partisans(lua_State *L, City *self, Player *inspirer)
{
  bool inspired = FALSE;

  if (!game.info.citizen_nationality) {
    if (self->original == inspirer) {
      inspired = TRUE;
    }
  } else {
    if (game.info.citizen_partisans_pct > 0) {
      int own = citizens_nation_get(self, inspirer->slot);
      int total = 0;

      /* Not citizens_foreign_iterate() as city has already changed hands.
       * old owner would be considered foreign and new owner not. */
      citizens_iterate(self, pslot, nat) {
        total += nat;
      } citizens_iterate_end;

      if ((own * 100 / total) >= game.info.citizen_partisans_pct) {
        inspired = TRUE;
      }
    } else if (self->original == inspirer) {
      inspired = TRUE;
    }
  }

  if (inspired) {
    /* Cannot use get_city_bonus() as it would use city's current owner
     * instead of inspirer. */
    return get_target_bonus_effects(NULL, inspirer, NULL, self, NULL,
                                    city_tile(self), NULL, NULL, NULL,
                                    NULL, EFT_INSPIRE_PARTISANS);
  }

  return 0;
}

/**************************************************************************
  How much culture city has?
**************************************************************************/
int api_methods_city_culture_get(lua_State *L, City *pcity)
{
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, pcity, 0);

  return city_culture(pcity);
}

/*****************************************************************************
  Return TRUE iff city happy
*****************************************************************************/
bool api_methods_is_city_happy(lua_State *L, City *pcity)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pcity, FALSE);

  return city_happy(pcity);
}

/*****************************************************************************
  Return TRUE iff city is unhappy
*****************************************************************************/
bool api_methods_is_city_unhappy(lua_State *L, City *pcity)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pcity, FALSE);

  return city_unhappy(pcity);
}

/*****************************************************************************
  Return TRUE iff city is celebrating
*****************************************************************************/
bool api_methods_is_city_celebrating(lua_State *L, City *pcity)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pcity, FALSE);

  return city_celebrating(pcity);
}

/*****************************************************************************
  Return TRUE iff city is government center
*****************************************************************************/
bool api_methods_is_gov_center(lua_State *L, City *pcity)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pcity, FALSE);

  return is_gov_center(pcity);
}

/*****************************************************************************
  Return TRUE if city is capital
*****************************************************************************/
bool api_methods_is_capital(lua_State *L, City *pcity)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pcity, FALSE);

  return is_capital(pcity);
}

/*****************************************************************************
   Return rule name for Government
*****************************************************************************/
const char *api_methods_government_rule_name(lua_State *L,
                                             Government *pgovernment)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pgovernment, NULL);

  return government_rule_name(pgovernment);
}

/*****************************************************************************
  Return translated name for Government
*****************************************************************************/
const char *api_methods_government_name_translation(lua_State *L,
                                                    Government *pgovernment)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pgovernment, NULL);

  return government_name_translation(pgovernment);
}


/*****************************************************************************
  Return rule name for Nation_Type
*****************************************************************************/
const char *api_methods_nation_type_rule_name(lua_State *L,
                                              Nation_Type *pnation)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pnation, NULL);

  return nation_rule_name(pnation);
}

/*****************************************************************************
  Return translated adjective for Nation_Type
*****************************************************************************/
const char *api_methods_nation_type_name_translation(lua_State *L,
                                                     Nation_Type *pnation)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pnation, NULL);

  return nation_adjective_translation(pnation);
}

/*****************************************************************************
  Return translated plural noun for Nation_Type
*****************************************************************************/
const char *api_methods_nation_type_plural_translation(lua_State *L,
                                                       Nation_Type *pnation)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pnation, NULL);

  return nation_plural_translation(pnation);
}

/*****************************************************************************
  Return TRUE iff player has wonder
*****************************************************************************/
bool api_methods_player_has_wonder(lua_State *L, Player *pplayer,
                                   Building_Type *building)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, pplayer, FALSE);
  LUASCRIPT_CHECK_ARG_NIL(L, building, 3, Building_Type, FALSE);

  return wonder_is_built(pplayer, building);
}

/*****************************************************************************
  Return player number
*****************************************************************************/
int api_methods_player_number(lua_State *L, Player *pplayer)
{
  LUASCRIPT_CHECK_STATE(L, -1);
  LUASCRIPT_CHECK_SELF(L, pplayer, -1);

  return player_number(pplayer);
}

/*****************************************************************************
  Return player team number
*****************************************************************************/
int api_methods_player_team_number(lua_State *L, Player *pplayer)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, pplayer, FALSE);

  return team_number(pplayer->team);
}

/*****************************************************************************
  Return player team rule name
*****************************************************************************/
const char *api_methods_player_team_rule_name(lua_State *L, Player *pplayer)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, pplayer, FALSE);

  return team_rule_name(pplayer->team);
}

/*********************************************************************//******
  Return string describing space race status of pplayer, nil if no spaceship
*****************************************************************************/
const char *api_methods_player_spaceship_state(lua_State *L, Player *pplayer)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pplayer, NULL);

  switch (pplayer->spaceship.state) {
  case SSHIP_NONE:
    break;
  case SSHIP_STARTED:
    return "Started";
  case SSHIP_LAUNCHED:
    return "Launched";
  case SSHIP_ARRIVED:
    return "Arrived";
  default:
    fc_assert_msg(FALSE, "wrong spaceship state");
  }
  return NULL;
}

/*********************************************************************//******
  Return pplayer's spaceship success rate
*****************************************************************************/
double
  api_methods_player_spaceship_success_rate(lua_State *L, Player *pplayer)
{
  LUASCRIPT_CHECK_STATE(L, 0.0);
  LUASCRIPT_CHECK_SELF(L, pplayer, 0.0);

  return pplayer->spaceship.success_rate;
}

/*********************************************************************//******
  Return pplayer's spaceship travel time, 0.0 if no spaceship
*****************************************************************************/
double
  api_methods_player_spaceship_travel_time(lua_State *L, Player *pplayer)
{
  LUASCRIPT_CHECK_STATE(L, 0.0);
  LUASCRIPT_CHECK_SELF(L, pplayer, 0.0);

  return pplayer->spaceship.travel_time;
}

/*****************************************************************************
  Return the number of cities pplayer has.
*****************************************************************************/
int api_methods_player_num_cities(lua_State *L, Player *pplayer)
{
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, pplayer, 0);

  return city_list_size(pplayer->cities);
}

/*****************************************************************************
  Return the number of units pplayer has.
*****************************************************************************/
int api_methods_player_num_units(lua_State *L, Player *pplayer)
{
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, pplayer, 0);

  return unit_list_size(pplayer->units);
}

/*****************************************************************************
  Return gold for Player
*****************************************************************************/
int api_methods_player_gold(lua_State *L, Player *pplayer)
{
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, pplayer, 0);

  return pplayer->economic.gold;
}

/*****************************************************************************
  Return TRUE if Player knows advance ptech.
*****************************************************************************/
bool api_methods_player_knows_tech(lua_State *L, Player *pplayer,
                                   Tech_Type *ptech)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, pplayer, FALSE);
  LUASCRIPT_CHECK_ARG_NIL(L, ptech, 3, Tech_Type, FALSE);

  return research_invention_state(research_get(pplayer),
                                  advance_number(ptech)) == TECH_KNOWN;
}

/**************************************************************************
  How much culture player has?
**************************************************************************/
int api_methods_player_culture_get(lua_State *L, Player *pplayer)
{
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, pplayer, 0);

  return player_culture(pplayer);
}

/*****************************************************************************
  Return TRUE if players have this diplomatic relation
*****************************************************************************/
bool api_methods_player_dipl_rel(lua_State *L, Player *self,
                                 Player *other, const char *rel)
{
  int dr = diplrel_by_rule_name(rel);

  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, self, FALSE);
  LUASCRIPT_CHECK_ARG_NIL(L, other, 3, player, FALSE);
  LUASCRIPT_CHECK_ARG_NIL(L, rel, 4, string, FALSE);
  LUASCRIPT_CHECK_ARG(L, dr != diplrel_other_invalid(), 4,
                      "Wrong diplomatic state", FALSE);

  return is_diplrel_between(self, other, dr);
}

/*****************************************************************************
  Return TRUE if players share research.
*****************************************************************************/
bool api_methods_player_shares_research(lua_State *L, Player *pplayer,
                                        Player *aplayer)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, pplayer, FALSE);
  LUASCRIPT_CHECK_ARG_NIL(L, aplayer, 3, Player, FALSE);

  return research_get(pplayer) == research_get(aplayer);
}

/*****************************************************************************
  Return name of the research group player belongs to.
*****************************************************************************/
const char *api_methods_research_rule_name(lua_State *L, Player *pplayer)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, pplayer, FALSE);

  return research_rule_name(research_get(pplayer));
}

/*****************************************************************************
  Return name of the research group player belongs to.
*****************************************************************************/
const char *api_methods_research_name_translation(lua_State *L, Player *pplayer)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, pplayer, FALSE);

  return research_name_translation(research_get(pplayer));
}

/**********************************************************************//*****
  Return tech the player is researching and how many bulbs are scored
*****************************************************************************/
Tech_Type
  *api_methods_player_researching(lua_State *L, Player *self, int *bulbs)
{
  struct research *r;
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, self, FALSE);

  r = research_get(self);
  fc_assert_ret_val_msg(r, NULL, "The player research is undefined!"); 
  *bulbs  = r->bulbs_researched;
  return advance_by_number(r->researching);
}

/*****************************************************************************
  Return list head for unit list for Player
*****************************************************************************/
Unit_List_Link *api_methods_private_player_unit_list_head(lua_State *L,
                                                          Player *pplayer)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pplayer, NULL);
  return unit_list_head(pplayer->units);
}

/*****************************************************************************
  Return list head for city list for Player
*****************************************************************************/
City_List_Link *api_methods_private_player_city_list_head(lua_State *L,
                                                          Player *pplayer)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pplayer, NULL);

  return city_list_head(pplayer->cities);
}

/*****************************************************************************
  Return rule name for Tech_Type
*****************************************************************************/
const char *api_methods_tech_type_rule_name(lua_State *L, Tech_Type *ptech)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, ptech, NULL);

  return advance_rule_name(ptech);
}

/*****************************************************************************
  Return translated name for Tech_Type
*****************************************************************************/
const char *api_methods_tech_type_name_translation(lua_State *L,
                                                   Tech_Type *ptech)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, ptech, NULL);

  return advance_name_translation(ptech);
}

/*****************************************************************************
  Return rule name for Terrain
*****************************************************************************/
const char *api_methods_terrain_rule_name(lua_State *L, Terrain *pterrain)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pterrain, NULL);

  return terrain_rule_name(pterrain);
}

/*****************************************************************************
  Return translated name for Terrain
*****************************************************************************/
const char *api_methods_terrain_name_translation(lua_State *L,
                                                 Terrain *pterrain)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pterrain, NULL);

  return terrain_name_translation(pterrain);
}

/*****************************************************************************
  Return name of the terrain's class
*****************************************************************************/
const char *api_methods_terrain_class_name(lua_State *L, Terrain *pterrain)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pterrain, NULL);

  return terrain_class_name(terrain_type_terrain_class(pterrain));
}

/*****************************************************************************
  Return rule name for Disaster
*****************************************************************************/
const char *api_methods_disaster_rule_name(lua_State *L, Disaster *pdis)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pdis, NULL);

  return disaster_rule_name(pdis);
}

/*****************************************************************************
  Return translated name for Disaster
*****************************************************************************/
const char *api_methods_disaster_name_translation(lua_State *L,
                                                  Disaster *pdis)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pdis, NULL);

  return disaster_name_translation(pdis);
}

/*****************************************************************************
  Return rule name for Achievement
*****************************************************************************/
const char *api_methods_achievement_rule_name(lua_State *L, Achievement *pach)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pach, NULL);

  return achievement_rule_name(pach);
}

/*****************************************************************************
  Return translated name for Achievement
*****************************************************************************/
const char *api_methods_achievement_name_translation(lua_State *L,
                                                     Achievement *pach)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pach, NULL);

  return achievement_name_translation(pach);
}

/*****************************************************************************
  Return rule name for Action
*****************************************************************************/
const char *api_methods_action_rule_name(lua_State *L, Action *pact)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pact, NULL);

  return action_id_rule_name(pact->id);
}

/*****************************************************************************
  Return translated name for Action
*****************************************************************************/
const char *api_methods_action_name_translation(lua_State *L, Action *pact)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pact, NULL);

  return action_id_name_translation(pact->id);
}

/*****************************************************************************
  Return action target kind
*****************************************************************************/
const char *api_methods_action_target_kind(lua_State *L, Action *self)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, self, NULL);

  switch (action_get_target_kind(self)) {
  case ATK_CITY:
    return "City";
  case ATK_UNIT:
    return "Unit";
  default:
    fc_assert_ret_val_msg(FALSE, NULL, "wrong actor kind for an action!");
  }
}

/*****************************************************************************
  Return the native x coordinate of the tile.
*****************************************************************************/
int api_methods_tile_nat_x(lua_State *L, Tile *ptile)
{
  LUASCRIPT_CHECK_STATE(L, -1);
  LUASCRIPT_CHECK_SELF(L, ptile, -1);

  return index_to_native_pos_x(tile_index(ptile));
}

/*****************************************************************************
  Return the native y coordinate of the tile.
*****************************************************************************/
int api_methods_tile_nat_y(lua_State *L, Tile *ptile)
{
  LUASCRIPT_CHECK_STATE(L, -1);
  LUASCRIPT_CHECK_SELF(L, ptile, -1);

  return index_to_native_pos_y(tile_index(ptile));
}

/*****************************************************************************
  Return the map x coordinate of the tile.
*****************************************************************************/
int api_methods_tile_map_x(lua_State *L, Tile *ptile)
{
  LUASCRIPT_CHECK_STATE(L, -1);
  LUASCRIPT_CHECK_SELF(L, ptile, -1);

  return index_to_map_pos_x(tile_index(ptile));
}

/*****************************************************************************
  Return the map y coordinate of the tile.
*****************************************************************************/
int api_methods_tile_map_y(lua_State *L, Tile *ptile)
{
  LUASCRIPT_CHECK_STATE(L, -1);
  LUASCRIPT_CHECK_SELF(L, ptile, -1);

  return index_to_map_pos_y(tile_index(ptile));
}

/***********************************************************************//****
  Return real map distance (diagonal=1) between tiles 1 and 2
*****************************************************************************/
int api_methods_tile_real_map_distance(lua_State *L,
                                       Tile *ptile1, Tile *ptile2)
{
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, ptile1, 0);
  LUASCRIPT_CHECK_ARG_NIL(L, ptile2, 2, Tile, 0);

  return real_map_distance(ptile1, ptile2);
}

/***********************************************************************//****
  Return Manhattan map distance (diagonal=2) between tiles 1 and 2
*****************************************************************************/
int api_methods_tile_map_distance(lua_State *L, Tile *ptile1, Tile *ptile2)
{
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, ptile1, 0);
  LUASCRIPT_CHECK_ARG_NIL(L, ptile2, 2, Tile, 0);

  return map_distance(ptile1, ptile2);
}

/*****************************************************************************
  Return City on ptile, else NULL
*****************************************************************************/
City *api_methods_tile_city(lua_State *L, Tile *ptile)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, ptile, NULL);

  return tile_city(ptile);
}

/*****************************************************************************
  Return TRUE if there is a city inside the maximum city radius from ptile.
*****************************************************************************/
bool api_methods_tile_city_exists_within_max_city_map(lua_State *L,
                                                      Tile *ptile,
                                                      bool may_be_on_center)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, ptile, FALSE);

  return city_exists_within_max_city_map(ptile, may_be_on_center);
}

/*****************************************************************************
  Return TRUE if there is a extra with rule name name on ptile.
  If no name is specified return true if there is a extra on ptile.
*****************************************************************************/
bool api_methods_tile_has_extra(lua_State *L, Tile *ptile, const char *name)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, ptile, FALSE);

  if (!name) {
    extra_type_iterate(pextra) {
      if (tile_has_extra(ptile, pextra)) {
        return TRUE;
      }
    } extra_type_iterate_end;

    return FALSE;
  } else {
    struct extra_type *pextra;

    pextra = extra_type_by_rule_name(name);

    return (NULL != pextra && tile_has_extra(ptile, pextra));
  }
}

/*****************************************************************************
  Return TRUE if there is a base with rule name name on ptile.
  If no name is specified return true if there is any base on ptile.
*****************************************************************************/
bool api_methods_tile_has_base(lua_State *L, Tile *ptile, const char *name)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, ptile, FALSE);

  if (!name) {
    extra_type_by_cause_iterate(EC_BASE, pextra) {
      if (tile_has_extra(ptile, pextra)) {
        return TRUE;
      }
    } extra_type_by_cause_iterate_end;

    return FALSE;
  } else {
    struct extra_type *pextra;

    pextra = extra_type_by_rule_name(name);

    return (NULL != pextra && is_extra_caused_by(pextra, EC_BASE)
            && tile_has_extra(ptile, pextra));
  }
}

/*****************************************************************************
  Return TRUE if there is a road with rule name name on ptile.
  If no name is specified return true if there is any road on ptile.
*****************************************************************************/
bool api_methods_tile_has_road(lua_State *L, Tile *ptile, const char *name)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, ptile, FALSE);

  if (!name) {
    extra_type_by_cause_iterate(EC_ROAD, pextra) {
      if (tile_has_extra(ptile, pextra)) {
        return TRUE;
      }
    } extra_type_by_cause_iterate_end;

    return FALSE;
  } else {
    struct extra_type *pextra;
 
    pextra = extra_type_by_rule_name(name);

    return (NULL != pextra && is_extra_caused_by(pextra, EC_ROAD)
            && tile_has_extra(ptile, pextra));
  }
}

/*****************************************************************************
  Return number of units on tile
*****************************************************************************/
int api_methods_tile_num_units(lua_State *L, Tile *ptile)
{
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, ptile, 0);

  return unit_list_size(ptile->units);
}

/****************************************************************//*********
  Return tile's output of otype, for the city if it is specified.
***************************************************************************/
int api_methods_tile_output(lua_State *L, Tile *self, int otype, City *city)
{
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, self, 0);
  LUASCRIPT_CHECK_ARG(L, otype >= O_FOOD && otype < O_LAST,
                      3, "Wrong output type", 0);
  
  return city ? city_tile_output_now(city, self, otype)
              : city_tile_output(NULL, self, FALSE, otype);
}

/****************************************************************//*********
  Return tile's output of otype, for the city if it is specified and
  for the case it is (not) celebrating
***************************************************************************/
int api_methods_tile_output_full(lua_State *L, Tile *self, int otype,
                                 City *city, bool celeb)
{
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, self, 0);
  LUASCRIPT_CHECK_ARG(L, otype >= O_FOOD && otype < O_LAST,
                      3, "Wrong output type", 0);
  
  return city_tile_output(city, self, celeb, otype);
}

/*****************************************************************************
  Return the distance to the nearest gov. center of plr, or -1 if none.
  FIXME: move the calculator to a common header file
*****************************************************************************/
int api_methods_tile_gcdist(lua_State *L, Tile *ptile, Player *plr)
{
  LUASCRIPT_CHECK_STATE(L, -1);
  LUASCRIPT_CHECK_SELF(L, ptile, -1);
  LUASCRIPT_CHECK_ARG_NIL(L, plr, 3, Player, -1);

 return tile_gcdist(ptile, plr);
}

/*****************************************************************************
  Return list head for unit list for Tile
*****************************************************************************/
Unit_List_Link *api_methods_private_tile_unit_list_head(lua_State *L,
                                                        Tile *ptile)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, ptile, NULL);

  return unit_list_head(ptile->units);
}

/*****************************************************************************
  Return nth tile iteration index (for internal use)
  Will return the next index, or an index < 0 when done
*****************************************************************************/
int api_methods_private_tile_next_outward_index(lua_State *L, Tile *pstart,
                                                int tindex, int max_dist)
{
  int dx, dy;
  int newx, newy;
  int startx, starty;

  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, pstart, 0);

  if (tindex < 0) {
    return 0;
  }

  index_to_map_pos(&startx, &starty, tile_index(pstart));

  tindex++;
  while (tindex < game.map.num_iterate_outwards_indices) {
    if (game.map.iterate_outwards_indices[tindex].dist > max_dist) {
      return -1;
    }
    dx = game.map.iterate_outwards_indices[tindex].dx;
    dy = game.map.iterate_outwards_indices[tindex].dy;
    newx = dx + startx;
    newy = dy + starty;
    if (!normalize_map_pos(&newx, &newy)) {
      tindex++;
      continue;
    }
    return tindex;
  }
  return -1;
}

/*****************************************************************************
  Return tile for nth iteration index (for internal use)
*****************************************************************************/
Tile *api_methods_private_tile_for_outward_index(lua_State *L, Tile *pstart,
                                                 int tindex)
{
  int newx, newy;

  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, pstart, NULL);
  LUASCRIPT_CHECK_ARG(L,
                      tindex >= 0 && tindex < game.map.num_iterate_outwards_indices,
                      3, "index out of bounds", NULL);

  index_to_map_pos(&newx, &newy, tile_index(pstart));
  newx += game.map.iterate_outwards_indices[tindex].dx;
  newy += game.map.iterate_outwards_indices[tindex].dy;

  if (!normalize_map_pos(&newx, &newy)) {
    return NULL;
  }
  return map_pos_to_tile(newx, newy);
}

/*****************************************************************************
  Return squared distance between tiles 1 and 2
*****************************************************************************/
int api_methods_tile_sq_distance(lua_State *L, Tile *ptile1, Tile *ptile2)
{
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, ptile1, 0);
  LUASCRIPT_CHECK_ARG_NIL(L, ptile2, 3, Tile, 0);

  return sq_map_distance(ptile1, ptile2);
}

/*****************************************************************************
  Can punit found a city on its tile?
*****************************************************************************/
bool api_methods_unit_city_can_be_built_here(lua_State *L, Unit *punit)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, punit, FALSE);

  return city_can_be_built_here(unit_tile(punit), punit);
}

/**************************************************************************
  Return the tile of the unit.
**************************************************************************/
Tile *api_methods_unit_tile_get(lua_State *L, Unit *punit)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, punit, NULL);

  return unit_tile(punit);
}

/*****************************************************************************
  Get unit orientation
*****************************************************************************/
Direction api_methods_unit_orientation_get(lua_State *L, Unit *punit)
{
  LUASCRIPT_CHECK_STATE(L, direction8_invalid());
  LUASCRIPT_CHECK_ARG_NIL(L, punit, 2, Unit, direction8_invalid());

  return punit->facing;
}

/************************************************************************//***
  Get punit attacking tunit power (in current state)
*****************************************************************************/
int api_methods_unit_attack_power(lua_State *L, Unit *punit, Unit *tunit)
{
  LUASCRIPT_CHECK_STATE(L, 0.);
  LUASCRIPT_CHECK_SELF(L, punit, 0.);
  LUASCRIPT_CHECK_ARG_NIL(L, tunit, 2, Unit, 0.);

  return get_total_attack_power(punit, tunit);
}

/************************************************************************//***
  Get punit defending from tunit power (in current state)
*****************************************************************************/
int api_methods_unit_defense_power(lua_State *L, Unit *punit, Unit *tunit)
{
  LUASCRIPT_CHECK_STATE(L, 0.);
  LUASCRIPT_CHECK_SELF(L, punit, 0.);
  LUASCRIPT_CHECK_ARG_NIL(L, tunit, 2, Unit, 0.);

  return get_total_defense_power(tunit, punit);
}

/************************************************************************//***
  Get punit attacking tunit win chance (in current state)
*****************************************************************************/
double api_methods_unit_win_chance(lua_State *L, Unit *punit, Unit *tunit)
{
  LUASCRIPT_CHECK_STATE(L, 0.);
  LUASCRIPT_CHECK_SELF(L, punit, 0.);
  LUASCRIPT_CHECK_ARG_NIL(L, tunit, 2, Unit, 0.);

  return unit_win_chance(punit, tunit);
}

/*****************************************************************************
  Get unit activity target rule name
*****************************************************************************/
const char *api_methods_unit_activity_target(lua_State *L, Unit *punit)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, punit, NULL);

  if (punit->activity_target) {
    return extra_rule_name(punit->activity_target);
  } else {
    return NULL;
  }
}

/*****************************************************************************
  Return three values: orders, bool repeat and bool vigilant
  {{order=int, dir=Direction, activity=string, target=string}}
*****************************************************************************/
lua_Object api_methods_unit_orders(lua_State *L, Unit *punit,
                                   bool *repeat, bool *vigilant)
{
  int t, o;
  void* tolua_obj;
  
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, punit, 0);
  
  if (!unit_has_orders(punit)) {
    lua_pushnil(L);
    return lua_gettop(L); /* boolean values are FALSE by tolua_game.pkg */
  }
  *repeat = punit->orders.repeat;
  *vigilant = punit->orders.vigilant;
  lua_createtable(L, punit->orders.length, 0);
  t = lua_gettop(L);
  for (int i = 0; i < punit->orders.length; i++) {
    int tgt = punit->orders.list[i].target;

    lua_pushinteger(L, i + 1);
    lua_createtable(L, 0, 2); /* We mostly just move around */
    o = lua_gettop(L);
    lua_pushinteger(L, punit->orders.list[i].order);
    lua_setfield(L, o, "order");
    if (is_valid_dir(punit->orders.list[i].dir)) {
      tolua_obj = tolua_copy(L, (void*) &punit->orders.list[i].dir,
                             sizeof(Direction));
      tolua_pushusertype(L, tolua_clone(L, tolua_obj, NULL), "Direction");
      lua_setfield(L, o, "dir");
    }
    if (unit_activity_is_valid(punit->orders.list[i].activity)) {
      lua_pushstring(L, unit_activity_name(punit->orders.list[i].activity));
      lua_setfield(L, o, "activity");
    }
    if (tgt >= 0 && tgt < MAX_EXTRA_TYPES) { /* assertion */
      tolua_obj = (void *) extra_by_number(tgt);
      if (tolua_obj) {
        lua_pushstring(L, extra_rule_name((struct extra_type *) tolua_obj));
        lua_setfield(L, o, "target");
      }
    }
    lua_settable(L, t);
  }
  return t;
}

/*****************************************************************************
  Return current index of punit's orders (starting at 1), nil if no orders
*****************************************************************************/
lua_Object api_methods_unit_orders_index(lua_State *L, Unit *punit)
{
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, punit, 0);
  
  if (unit_has_orders(punit)) {
    lua_pushinteger(L, punit->orders.index + 1);
  } else {
    lua_pushnil(L);
  }
  return lua_gettop(L);
}

/*****************************************************************************
  Return Unit that transports punit, if any.
*****************************************************************************/
Unit *api_methods_unit_transporter(lua_State *L, Unit *punit)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, punit, NULL);

  return punit->transporter;
}

/*****************************************************************************
  Return punit's nationality, if any
*****************************************************************************/
Player *api_methods_unit_nationality(lua_State *L, Unit *punit)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, punit, NULL);

  return unit_nationality(punit);
}

/*****************************************************************************
  Return list head for cargo list for Unit
*****************************************************************************/
Unit_List_Link *api_methods_private_unit_cargo_list_head(lua_State *L,
                                                         Unit *punit)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, punit, NULL);
  return unit_list_head(punit->transporting);
}

/*****************************************************************************
  Get punit full mp from all known bonuses
*****************************************************************************/
int api_methods_unit_move_rate(lua_State *L, Unit *punit)
{
  LUASCRIPT_CHECK_STATE(L, -1);
  LUASCRIPT_CHECK_SELF(L, punit, -1);

  return unit_move_rate(punit);
}

/*****************************************************************************
  Get punit full work speed (does not check if it has any mp)
*****************************************************************************/
int api_methods_unit_activity_rate(lua_State *L, Unit *punit)
{
  LUASCRIPT_CHECK_STATE(L, -1);
  LUASCRIPT_CHECK_SELF(L, punit, -1);

  return get_activity_rate(punit);
}

/*****************************************************************************
  Get unit moves
*****************************************************************************/
int api_methods_unit_moves_left_get(lua_State *L, Unit *punit)
{
  LUASCRIPT_CHECK_STATE(L, -1);
  LUASCRIPT_CHECK_SELF(L, punit, -1);

  return punit->moves_left;
}

/*****************************************************************************
  Get unit veteranship rank
*****************************************************************************/
int api_methods_unit_vet_get(lua_State *L, Unit *punit)
{
  LUASCRIPT_CHECK_STATE(L, -1);
  LUASCRIPT_CHECK_SELF(L, punit, -1);

  return punit->veteran;
}

/***********************************************************************//****
  Get unit veteranship rank untranslated name
*****************************************************************************/
const char *api_methods_unit_vet_name(lua_State *L, Unit *punit)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, punit, NULL);

  return untranslated_name(&utype_veteran_level(unit_type_get(punit), 
                                                punit->veteran)->name);
}

/********************************************************************//*******
  Returns current unit activity name
*****************************************************************************/
const char *api_methods_unit_activity(lua_State *L, Unit *punit)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, punit, NULL);
  
  return unit_activity_name(punit->activity);
}

/*****************************************************************************
  Return the square vision raduis of the unit
  (clone of server get_unit_vision_at(), in client, works with known info)
*****************************************************************************/
int api_methods_unit_vision_sq_radius(lua_State *L, Unit *punit, Tile *ptile,
                                      int vl)
{
  int base;
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, punit, 0);
  LUASCRIPT_CHECK_ARG(L, V_MAIN <= vl && vl < V_COUNT, 3,
                      "Wrong vision layer", 0);
  if (!ptile) {
    ptile = unit_tile(punit);
  }
  base = (unit_type_get(punit)->vision_radius_sq
          + get_unittype_bonus(unit_owner(punit), ptile,
                               unit_type_get(punit),
                               EFT_UNIT_VISION_RADIUS_SQ));
  switch (vl) {
  case V_MAIN:
    return MAX(0, base);
  case V_INVIS:
    return CLIP(0, base, 2);
  case V_COUNT:
    break;
  }

  log_error("Unsupported vision layer variant: %d.", vl);
  return 0;
}

/********************************************************************//*******
  Return action ptobability (in two values, see ap2top()) for a unit
  versus a target (unit or city). Returns nil, "wrong target"
  if the target kind is wrong.
*****************************************************************************/
lua_Object api_methods_unit_ap_vs(lua_State *L, Unit *punit,
                                  lua_Object actn, lua_Object target,
                                  lua_Object *r2)
{
  enum gen_action act;
  tolua_Error err;

  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, punit, 0);
  LUASCRIPT_CHECK_ARG(L, LUA_TUSERDATA == lua_type(L, target), 4,
                      "target must be an object (unit or city)", 0);

  switch (lua_type(L, actn)) {
  case LUA_TUSERDATA:
    LUASCRIPT_CHECK_ARG(L, !tolua_isusertype(L, actn, "Action", 0, &err), 3,
                        "wrong object type supplied as an action", 0);
    act = ((Action *) tolua_tousertype(L, actn, NULL))->id;
    break;
  case LUA_TNUMBER:
    {
      int ch;
      act = lua_tointegerx(L, actn, &ch);
      LUASCRIPT_CHECK_ARG(L, ch && action_id_is_valid(act), 3,
                          "wrong action id specified", 0);
    }
    break;
  case LUA_TSTRING:
    act = gen_action_by_name(lua_tostring(L, actn), fc_strcasecmp);
    LUASCRIPT_CHECK_ARG(L, action_id_is_valid(act), 3,
                        "wrong action name specified", 0);
    break;
  default:
    luascript_arg_error(L, 3, "wrong type supplied as an action");
    return 0;
  }

  switch (action_id_get_target_kind(act)) {
  case ATK_UNIT:
    if (tolua_isusertype(L, target, "Unit", 0, &err)) {
      return
      ap2top(L, action_prob_vs_unit(punit, act,
                                    (Unit *) tolua_tousertype(L, target,
                                                              NULL)), r2);
    }
    break;
  case ATK_CITY:
    if (tolua_isusertype(L, target, "City", 0, &err)) {
      return
      ap2top(L, action_prob_vs_city(punit, act,
                                    (City *) tolua_tousertype(L, target,
                                                              NULL)), r2);
    }
    break;
  default:
    fc_assert_msg(FALSE, "wrong action target kind!");
  }
  lua_pushstring(L, "wrong target");
  *r2 = lua_gettop(L);
  lua_pushnil(L);
  return lua_gettop(L);
}

/********************************************************************//*******
  Check if unit can do activity activity_name (by specenum unit_activity)
  Returns false for activities that have targets if one is not specified
  and the activity is neither "Mine" nor "Irrigate"
*****************************************************************************/
bool api_methods_unit_can_do_activity(lua_State *L, Unit *punit,
                                      const char *activity_name,
                                      const char *target,
                                      const Tile *ptile)
{
  enum unit_activity act;
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, punit, FALSE);
  LUASCRIPT_CHECK_ARG_NIL(L, activity_name, 3, string, FALSE);
  
  act = unit_activity_by_name(activity_name, fc_strcasecmp);
  LUASCRIPT_CHECK_ARG(L, act != unit_activity_invalid(), 3,
                      "invalid activity", FALSE);
  if (!target) {
    if (activity_requires_target(act)) {
      switch (act) {
      case ACTIVITY_MINE:
      case ACTIVITY_IRRIGATE:
        return can_unit_do_activity(punit, act);
      default:
        return FALSE;
      }
    }
  } else {
    /* const */ struct extra_type *tgt = extra_type_by_rule_name(target);
    LUASCRIPT_CHECK_ARG(L, tgt, 4, "extra name must be valid", FALSE);
    return can_unit_do_activity_targeted_at(punit, act, tgt, ptile);
  }
  return FALSE;
}

/*****************************************************************************
  Return TRUE if punit_type has flag.
*****************************************************************************/
bool api_methods_unit_type_has_flag(lua_State *L, Unit_Type *punit_type,
                                    const char *flag)
{
  enum unit_type_flag_id id;

  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, punit_type, FALSE);
  LUASCRIPT_CHECK_ARG_NIL(L, flag, 3, string, FALSE);

  id = unit_type_flag_id_by_name(flag, fc_strcasecmp);
  if (unit_type_flag_id_is_valid(id)) {
    return utype_has_flag(punit_type, id);
  } else {
    luascript_error(L, "Unit type flag \"%s\" does not exist", flag);
    return FALSE;
  }
}

/*****************************************************************************
  Return TRUE if punit_type has role.
*****************************************************************************/
bool api_methods_unit_type_has_role(lua_State *L, Unit_Type *punit_type,
                                    const char *role)
{
  enum unit_role_id id;

  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, punit_type, FALSE);
  LUASCRIPT_CHECK_ARG_NIL(L, role, 3, string, FALSE);

  id = unit_role_id_by_name(role, fc_strcasecmp);
  if (unit_role_id_is_valid(id)) {
    return utype_has_role(punit_type, id);
  } else {
    luascript_error(L, "Unit role \"%s\" does not exist", role);
    return FALSE;
  }
}

/*****************************************************************************
  Return TRUE iff the unit type can exist on the tile.
*****************************************************************************/
bool api_methods_unit_type_can_exist_at_tile(lua_State *L,
                                             Unit_Type *punit_type,
                                             Tile *ptile)
{
  LUASCRIPT_CHECK_STATE(L, FALSE);
  LUASCRIPT_CHECK_SELF(L, punit_type, FALSE);
  LUASCRIPT_CHECK_ARG_NIL(L, ptile, 3, Tile, FALSE);

  return can_exist_at_tile(punit_type, ptile);
}

/*****************************************************************************
  Return rule name for Unit_Type
*****************************************************************************/
const char *api_methods_unit_type_rule_name(lua_State *L,
                                            Unit_Type *punit_type)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, punit_type, NULL);

  return utype_rule_name(punit_type);
}

/*****************************************************************************
  Return translated name for Unit_Type
*****************************************************************************/
const char *api_methods_unit_type_name_translation(lua_State *L,
                                                   Unit_Type *punit_type)
{
  LUASCRIPT_CHECK_STATE(L, NULL);
  LUASCRIPT_CHECK_SELF(L, punit_type, NULL);

  return utype_name_translation(punit_type);
}

/*********************************************************************//******
  Construct and return a 0-baseed sequence describing Unit_Type
   veteran system:
  {{rule_name = string, power_fact, move_bonus = int[%]}}
  Alas, [work_]raise_chance is not sent to a client
*****************************************************************************/
lua_Object api_methods_unit_type_veteran_system(lua_State *L,
                                                Unit_Type *self)
{
  int lc;
  lua_Object s, t;
  LUASCRIPT_CHECK_STATE(L, 0);
  LUASCRIPT_CHECK_SELF(L, self, 0);

  lc = utype_veteran_levels(self);
  lua_createtable(L, lc, 0);
  s = lua_gettop(L);
  for (int i = 0; i < lc; i++) {
    const struct veteran_level *vl = utype_veteran_level(self, i);
    lua_pushinteger(L, i);
    lua_createtable(L, 0, is_server() ? 5 : 3);
    t = lua_gettop(L);
    lua_pushstring(L, untranslated_name(&vl->name));
    lua_setfield(L, t, "rule_name");
    lua_pushinteger(L, vl->power_fact);
    lua_setfield(L, t, "power_fact");
    lua_pushinteger(L, vl->move_bonus);
    lua_setfield(L, t, "move_bonus");
    if (is_server()) {
      lua_pushinteger(L, vl->raise_chance);
      lua_setfield(L, t, "raise_chance");
      lua_pushinteger(L, vl->work_raise_chance);
      lua_setfield(L, t, "work_raise_chance");
    }
    lua_settable(L, s);
  }
  return s;
}

/*****************************************************************************
  Return Unit for list link
*****************************************************************************/
Unit *api_methods_unit_list_link_data(lua_State *L,
                                      Unit_List_Link *ul_link)
{
  LUASCRIPT_CHECK_STATE(L, NULL);

  return unit_list_link_data(ul_link);
}

/*****************************************************************************
  Return next list link or NULL when link is the last link
*****************************************************************************/
Unit_List_Link *api_methods_unit_list_next_link(lua_State *L,
                                                Unit_List_Link *ul_link)
{
  LUASCRIPT_CHECK_STATE(L, NULL);

  return unit_list_link_next(ul_link);
}

/*****************************************************************************
  Return City for list link
*****************************************************************************/
City *api_methods_city_list_link_data(lua_State *L,
                                      City_List_Link *cl_link)
{
  LUASCRIPT_CHECK_STATE(L, NULL);

  return city_list_link_data(cl_link);
}

/*****************************************************************************
  Return next list link or NULL when link is the last link
*****************************************************************************/
City_List_Link *api_methods_city_list_next_link(lua_State *L,
                                                City_List_Link *cl_link)
{
  LUASCRIPT_CHECK_STATE(L, NULL);

  return city_list_link_next(cl_link);
}
