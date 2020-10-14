/***********************************************************************
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/
#ifndef FC__UNITHAND_H
#define FC__UNITHAND_H

/* common */
#include "unit.h"

/* server */
#include "hand_gen.h"

void unit_activity_handling(struct unit *punit,
                            enum unit_activity new_activity);
void unit_activity_handling_targeted(struct unit *punit,
                                     enum unit_activity new_activity,
                                     struct extra_type **new_target);
void unit_change_homecity_handling(struct unit *punit, struct city *new_pcity,
                                   bool rehome);

bool unit_move_handling(struct unit *punit, struct tile *pdesttile,
                        bool igzoc, bool move_diplomat_city,
                        struct unit *embark_to);

bool unit_build_city(struct player *pplayer, struct unit *punit,
                     const char *name);

void unit_do_disband(struct player *pplayer, struct unit *punit, 
                     bool voluntary);

void city_add_or_build_error(struct player *pplayer, struct unit *punit,
                             enum unit_add_build_city_result res);

struct city *action_tgt_city(struct unit *actor, struct tile *target_tile);

#endif  /* FC__UNITHAND_H */
