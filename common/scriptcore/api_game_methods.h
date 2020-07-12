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

#ifndef FC__API_GAME_METHODS_H
#define FC__API_GAME_METHODS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* common/scriptcore */
#include "luascript_types.h"

struct lua_State;

/* Game */
int api_methods_game_turn(lua_State *L);
double api_methods_game_win_chance(lua_State *L,
                                   int as, int ahp, int afp,
                                   int ds, int dhp, int dfp);

/* Building Type */
bool api_methods_building_type_is_wonder(lua_State *L,
                                         Building_Type *pbuilding);
bool api_methods_building_type_is_great_wonder(lua_State *L,
                                               Building_Type *pbuilding);
bool api_methods_building_type_is_small_wonder(lua_State *L,
                                               Building_Type *pbuilding);
bool api_methods_building_type_is_improvement(lua_State *L,
                                              Building_Type *pbuilding);
const char *api_methods_building_type_rule_name(lua_State *L,
                                                Building_Type *pbuilding);
const char *api_methods_building_type_name_translation(lua_State *L,
                                                       Building_Type *pbuilding);

/* City */
int api_methods_city_supported_units_number(lua_State *L, City *pcity);
bool api_methods_city_has_building(lua_State *L, City *pcity,
                                   Building_Type *building);
lua_Object api_methods_city_production(lua_State *L, City *pcity);
int api_methods_city_supported_units_number(lua_State *L, City *pcity);
Unit_List_Link *api_methods_private_city_supported_list_head(lua_State *L,
                                                             City *pcity);
lua_Object api_methods_city_nationality(lua_State *L, City *pcity);
int api_methods_city_specialists(lua_State *L, City *pcity,
                                 const char *spec);
bool api_methods_city_is_virtual(lua_State *L, City *pcity);
int api_methods_city_traderoutes_number(lua_State *L, City *pcity);
lua_Object api_methods_city_trade_routes(lua_State *L, City *pcity);
int api_methods_city_trade_with(lua_State *L, City *pcity, City *tcity);
int api_methods_caravan_bonus(lua_State *L, City *pcity, City *tcity,
                              bool establish);
int api_methods_city_happy_count(lua_State *L, City *pcity,
                                 int cat, int level);
int api_methods_city_map_sq_radius(lua_State *L, City *pcity);
int api_methods_city_vision_sq_radius(lua_State *L, City *self, int vl);
int api_methods_city_size_get(lua_State *L, City *pcity);
Tile *api_methods_city_tile_get(lua_State *L, City *pcity);
int api_methods_city_inspire_partisans(lua_State *L, City *self, Player *inspirer);

int api_methods_city_culture_get(lua_State *L, City *pcity);

bool api_methods_is_city_happy(lua_State *L, City *pcity);
bool api_methods_is_city_unhappy(lua_State *L, City *pcity);
bool api_methods_is_city_celebrating(lua_State *L, City *pcity);
bool api_methods_is_gov_center(lua_State *L, City *pcity);
bool api_methods_is_capital(lua_State *L, City *pcity);
double api_methods_city_waste_level(lua_State *L, City *pcity,
                                    int otype, lua_Object gcd);
double
api_methods_city_waste_level_ostr(lua_State *L, City *pcity,
                                  const char* otn, lua_Object gcd);
int api_methods_city_surplus(lua_State *L, City *pcity, lua_Object otype);
int api_methods_city_waste(lua_State *L, City *pcity, lua_Object otype);
int api_methods_city_unhappy_penalty(lua_State *L, City *pcity,
                                     lua_Object otype);
int api_methods_city_prod(lua_State *L, City *pcity, lua_Object otype);
int api_methods_city_citizen_base(lua_State *L, City *pcity,
                                  lua_Object otype);
int api_methods_city_usage(lua_State *L, City *pcity, lua_Object otype);

/* Government */
const char *api_methods_government_rule_name(lua_State *L,
                                             Government *pgovernment);
const char *api_methods_government_name_translation(lua_State *L,
                                                    Government *pgovernment);

/* Nation */
const char *api_methods_nation_type_rule_name(lua_State *L,
                                              Nation_Type *pnation);
const char *api_methods_nation_type_name_translation(lua_State *L,
                                                     Nation_Type *pnation);
const char *api_methods_nation_type_plural_translation(lua_State *L,
                                                       Nation_Type *pnation);

/* Player */
bool api_methods_player_has_wonder(lua_State *L, Player *pplayer,
                                   Building_Type *building);
int api_methods_player_number(lua_State *L, Player *pplayer);
int api_methods_player_team_number(lua_State *L, Player *pplayer);
const char *api_methods_player_team_rule_name(lua_State *L, Player *pplayer);
const char
  *api_methods_player_spaceship_state(lua_State *L, Player *pplayer);
double
  api_methods_player_spaceship_success_rate(lua_State *L, Player *pplayer);
double
  api_methods_player_spaceship_travel_time(lua_State *L, Player *pplayer);
int api_methods_player_num_cities(lua_State *L, Player *pplayer);
int api_methods_player_num_units(lua_State *L, Player *pplayer);
int api_methods_player_gold(lua_State *L, Player *pplayer);
bool api_methods_player_knows_tech(lua_State *L, Player *pplayer,
                                   Tech_Type *ptech);
bool api_methods_player_shares_research(lua_State *L, Player *pplayer,
                                        Player *aplayer);
const char *api_methods_research_rule_name(lua_State *L, Player *pplayer);
const char 
  *api_methods_research_name_translation(lua_State *L, Player *pplayer);
Tech_Type
  *api_methods_player_researching(lua_State *L, Player *self, int *bulbs);
Unit_List_Link *api_methods_private_player_unit_list_head(lua_State *L,
                                                          Player *pplayer);
City_List_Link *api_methods_private_player_city_list_head(lua_State *L,
                                                          Player *pplayer);
int api_methods_player_culture_get(lua_State *L, Player *pplayer);
bool api_methods_player_dipl_rel(lua_State *L, Player *self,
                                 Player *other, const char *rel);

/* Tech Type */
const char *api_methods_tech_type_rule_name(lua_State *L, Tech_Type *ptech);
const char *api_methods_tech_type_name_translation(lua_State *L, Tech_Type *ptech);

/* Terrain */
const char *api_methods_terrain_rule_name(lua_State *L, Terrain *pterrain);
const char *api_methods_terrain_name_translation(lua_State *L, Terrain *pterrain);
const char *api_methods_terrain_class_name(lua_State *L, Terrain *pterrain);

/* Disaster */
const char *api_methods_disaster_rule_name(lua_State *L, Disaster *pdis);
const char *api_methods_disaster_name_translation(lua_State *L,
                                                  Disaster *pdis);

/* Achievement */
const char *api_methods_achievement_rule_name(lua_State *L, Achievement *pach);
const char *api_methods_achievement_name_translation(lua_State *L,
                                                     Achievement *pach);

/* Action */
const char *api_methods_action_rule_name(lua_State *L, Action *pact);
const char *api_methods_action_name_translation(lua_State *L,
                                                Action *pact);
const char *api_methods_action_target_kind(lua_State *L, Action *self);

/* Tile */
int api_methods_tile_nat_x(lua_State *L, Tile *ptile);
int api_methods_tile_nat_y(lua_State *L, Tile *ptile);
int api_methods_tile_map_x(lua_State *L, Tile *ptile);
int api_methods_tile_map_y(lua_State *L, Tile *ptile);
City *api_methods_tile_city(lua_State *L, Tile *ptile);
bool api_methods_tile_city_exists_within_max_city_map(lua_State *L,
                                                      Tile *ptile,
                                                      bool may_be_on_center);
bool api_methods_tile_has_extra(lua_State *L, Tile *ptile, const char *name);
bool api_methods_tile_has_base(lua_State *L, Tile *ptile, const char *name);
bool api_methods_tile_has_road(lua_State *L, Tile *ptile, const char *name);
int api_methods_tile_num_units(lua_State *L, Tile *ptile);
int api_methods_tile_sq_distance(lua_State *L, Tile *ptile1, Tile *ptile2);
int api_methods_tile_real_map_distance(lua_State *L,
                                       Tile *ptile1, Tile *ptile2);
int api_methods_tile_map_distance(lua_State *L, Tile *ptile1, Tile *ptile2);
int api_methods_tile_output(lua_State *L, Tile *self, int otype, City *city);
int api_methods_tile_output_full(lua_State *L, Tile *self, int otype,
                                 City *city, bool celeb);
int api_methods_tile_gcdist(lua_State *L, Tile *ptile, Player *plr);
int api_methods_private_tile_next_outward_index(lua_State *L, Tile *pstart,
                                                int tindex, int max_dist);
Tile *api_methods_private_tile_for_outward_index(lua_State *L, Tile *pstart,
                                                 int tindex);
Unit_List_Link *api_methods_private_tile_unit_list_head(lua_State *L,
                                                        Tile *ptile);

/* Unit */
bool api_methods_unit_city_can_be_built_here(lua_State *L, Unit *punit);
Tile *api_methods_unit_tile_get(lua_State *L, Unit * punit);
lua_Object api_methods_unit_orientation_get(lua_State *L, Unit *punit);
int api_methods_unit_attack_power(lua_State *L, Unit *punit, Unit *tunit);
int api_methods_unit_defense_power(lua_State *L, Unit *punit, Unit *tunit);
double api_methods_unit_win_chance(lua_State *L, Unit *punit, Unit *tunit);
Unit *api_methods_unit_transporter(lua_State *L, Unit *punit);
Player *api_methods_unit_nationality(lua_State *L, Unit *punit);
Unit_List_Link *api_methods_private_unit_cargo_list_head(lua_State *L,
                                                         Unit *punit);
int api_methods_unit_moves_left_get(lua_State *L, Unit *punit);
int api_methods_unit_vet_get(lua_State *L, Unit *punit);
const char *api_methods_unit_vet_name(lua_State *L, Unit *punit);
const char *api_methods_unit_activity(lua_State *L, Unit *punit);
const char *api_methods_unit_activity_target(lua_State *L, Unit *punit);
int api_methods_unit_vision_sq_radius(lua_State *L, Unit *self, Tile *ptile,
                                      int vl);
lua_Object api_methods_unit_orders(lua_State *L, Unit *punit,
                                   bool *repeat, bool *vigilant);
lua_Object api_methods_unit_orders_index(lua_State *L, Unit *punit);
int api_methods_unit_move_rate(lua_State *L, Unit *punit);
int api_methods_unit_activity_rate(lua_State *L, Unit *punit);
lua_Object api_methods_unit_ap_vs(lua_State *L, Unit *punit,
                                  lua_Object actn, lua_Object target,
                                  lua_Object *r2);
bool api_methods_unit_can_do_activity(lua_State *L, Unit *punit,
                                      const char *activity_name,
                                      const char *target,
                                      const Tile *ptile);

/* Unit Type */
bool api_methods_unit_type_has_flag(lua_State *L, Unit_Type *punit_type,
                                    const char *flag);
bool api_methods_unit_type_has_role(lua_State *L, Unit_Type *punit_type,
                                    const char *role);
bool api_methods_unit_type_can_exist_at_tile(lua_State *L,
                                             Unit_Type *punit_type,
                                             Tile *ptile);
const char *api_methods_unit_type_rule_name(lua_State *L,
                                            Unit_Type *punit_type);
const char *api_methods_unit_type_name_translation(lua_State *L,
                                                   Unit_Type *punit_type);
lua_Object api_methods_unit_type_veteran_system(lua_State *L,
                                                Unit_Type *self);

/* Unit_List_Link Type */
Unit *api_methods_unit_list_link_data(lua_State *L, Unit_List_Link *link);
Unit_List_Link *api_methods_unit_list_next_link(lua_State *L,
                                                Unit_List_Link *link);

/* City_List_Link Type */
City *api_methods_city_list_link_data(lua_State *L, City_List_Link *link);
City_List_Link *api_methods_city_list_next_link(lua_State *L,
                                                City_List_Link *link);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FC__API_GAME_METHODS_H */
