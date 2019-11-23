/********************************************************************** 
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

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

/* utility */
#include "fcintl.h"
#include "log.h"
#include "mem.h"                /* free() */
#include "support.h"            /* fc_snprintf() */

/* common */
#include "game.h"
#include "government.h"
#include "research.h"
#include "unitlist.h"

/* client/include */
#include "repodlgs_g.h"

/* client */
#include "client_main.h"
#include "connectdlg_common.h"  /* is_server_running() */
#include "control.h"
#include "options.h"
#include "packhand_gen.h"

#include "repodlgs_common.h"


/****************************************************************
  Fills out the array of struct improvement_entry given by
  entries. The array must be able to hold at least B_LAST entries.
*****************************************************************/
void get_economy_report_data(struct improvement_entry *entries,
			     int *num_entries_used, int *total_cost,
			     int *total_income)
{
  *num_entries_used = 0;
  *total_cost = 0;
  *total_income = 0;

  if (NULL == client.conn.playing) {
    return;
  }

  improvement_iterate(pimprove) {
    if (is_improvement(pimprove)) {
      int count = 0, redundant = 0, cost = 0;
      city_list_iterate(client.conn.playing->cities, pcity) {
	if (city_has_building(pcity, pimprove)) {
	  count++;
	  cost += city_improvement_upkeep(pcity, pimprove);
          if (is_improvement_redundant(pcity, pimprove)) {
            redundant++;
          }
	}
      }
      city_list_iterate_end;

      if (count == 0) {
	continue;
      }

      entries[*num_entries_used].type = pimprove;
      entries[*num_entries_used].count = count;
      entries[*num_entries_used].redundant = redundant;
      entries[*num_entries_used].total_cost = cost;
      entries[*num_entries_used].cost = cost / count;
      (*num_entries_used)++;

      /* Currently there is no building expense under anarchy.  It's
       * not a good idea to hard-code this in the client, but what
       * else can we do? */
      if (government_of_player(client.conn.playing) !=
          game.government_during_revolution) {
        *total_cost += cost;
      }
    }
  } improvement_iterate_end;

  city_list_iterate(client.conn.playing->cities, pcity) {
    *total_income += pcity->prod[O_GOLD];
    if (city_production_has_flag(pcity, IF_GOLD)) {
      *total_income += MAX(0, pcity->surplus[O_SHIELD]);
    }
  } city_list_iterate_end;
}

/******************************************************************
  Returns an array of units with gold_upkeep. Number of units in 
  the array is added to num_entries_used.
******************************************************************/
void get_economy_report_units_data(struct unit_entry *entries,
				   int *num_entries_used, int *total_cost)
{
  int count, cost, partial_cost;

  *num_entries_used = 0;
  *total_cost = 0;

  if (NULL == client.conn.playing) {
    return;
  }

  unit_type_iterate(unittype) {
    cost = utype_upkeep_cost(unittype, client.conn.playing, O_GOLD);

    if (cost == 0) {
      /* Short-circuit all of the following checks. */
      continue;
    }

    count = 0;
    partial_cost = 0;

    city_list_iterate(client.conn.playing->cities, pcity) {
      unit_list_iterate(pcity->units_supported, punit) {
	if (unit_type_get(punit) == unittype) {
	  count++;
	  partial_cost += punit->upkeep[O_GOLD];
	}

      } unit_list_iterate_end;
    } city_list_iterate_end;

    if (count == 0) {
      continue;
    }

    (*total_cost) += partial_cost;

    entries[*num_entries_used].type = unittype;
    entries[*num_entries_used].count = count;
    entries[*num_entries_used].cost = cost;
    entries[*num_entries_used].total_cost = partial_cost;
    (*num_entries_used)++;

  } unit_type_iterate_end;
}

/****************************************************************************
  Sell all improvements of the given type in all cities.  If "redundant_only"
  is specified then only those improvements that are replaced will be sold.

  The "message" string will be filled with a GUI-friendly message about
  what was sold.
****************************************************************************/
void sell_all_improvements(struct impr_type *pimprove, bool redundant_only,
			   char *message, size_t message_sz)
{
  int count = 0, gold = 0;

  if (!can_client_issue_orders()) {
    fc_snprintf(message, message_sz, _("You cannot sell improvements."));
    return;
  }

  city_list_iterate(client.conn.playing->cities, pcity) {
    if (!pcity->did_sell && city_has_building(pcity, pimprove)
	&& (!redundant_only
	    || is_improvement_redundant(pcity, pimprove))) {
      count++;
      gold += impr_sell_gold(pimprove);
      city_sell_improvement(pcity, improvement_number(pimprove));
    }
  } city_list_iterate_end;

  if (count > 0) {
    /* FIXME: plurality of count is ignored! */
    /* TRANS: "Sold 3 Harbor for 90 gold." (Pluralisation is in gold --
     * second %d -- not in buildings.) */
    fc_snprintf(message, message_sz, PL_("Sold %d %s for %d gold.",
                                         "Sold %d %s for %d gold.", gold),
                count, improvement_name_translation(pimprove), gold);
  } else {
    fc_snprintf(message, message_sz, _("No %s could be sold."),
                improvement_name_translation(pimprove));
  }
}

/****************************************************************************
  Disband all supported units of the given type.  If in_cities_only is
  specified then only units inside our cities will be disbanded.

  The "message" string will be filled with a GUI-friendly message about
  what was sold.
****************************************************************************/
void disband_all_units(struct unit_type *punittype, bool in_cities_only,
		       char *message, size_t message_sz)
{
  int count = 0;

  if (!can_client_issue_orders()) {
    /* TRANS: Obscure observer error. */
    fc_snprintf(message, message_sz, _("You cannot disband units."));
    return;
  }

  if (utype_has_flag(punittype, UTYF_UNDISBANDABLE)) {
    fc_snprintf(message, message_sz, _("%s cannot be disbanded."),
                utype_name_translation(punittype));
    return;
  }

  city_list_iterate(client.conn.playing->cities, pcity) {
    /* Only supported units are disbanded.  Units with no homecity have no
     * cost and are not disbanded. */
    unit_list_iterate(pcity->units_supported, punit) {
      struct city *incity = tile_city(unit_tile(punit));

      if (unit_type_get(punit) == punittype
	  && (!in_cities_only
	      || (incity && city_owner(incity) == client.conn.playing))) {
	count++;
	request_unit_disband(punit);
      }
    } unit_list_iterate_end;
  } city_list_iterate_end;

  if (count > 0) {
    fc_snprintf(message, message_sz, _("Disbanded %d %s."),
                count, utype_name_translation(punittype));
  } else {
    fc_snprintf(message, message_sz, _("No %s could be disbanded."),
                utype_name_translation(punittype));
  }
}

/**********************************************************************//**
  Fills the commentary and all for immediate research confirmation popup.
  *certain is if we are sure the tech can be immediately researched
**************************************************************************/
bool immediate_research_may_happen(char *buf, int buf_sz, Tech_type_id tech,
                                   bool *certain)
{
  struct research *presearch =
    (client_has_player() ? research_get(client_player()) : NULL);
  int cost = research_total_bulbs_required(presearch, tech, FALSE);
  int bulbs, loss;
  int min_cost = (int) (cost * min_leakage_ratio(client_player(), tech));
  struct option *tpo;
  struct advance *vap = valid_advance_by_number(tech);

  fc_assert_ret_val(presearch, FALSE);
  fc_assert_ret_val(cost, FALSE);
  fc_assert_ret_val(vap, FALSE);

  bulbs = (tech == presearch->researching_saved)
          ? presearch->bulbs_researching_saved
          : presearch->bulbs_researched;

  /* In some cases you have more bulbs than your current tech costs... */
  if (tech != presearch->researching_saved) {
    tpo = optset_option_by_name(server_optset, "techpenalty");

    if (!tpo) {
      log_error("techpenalty server option unknown");
      loss = 0;
    } else {
      /* Loss, if it is a probability that we can't change for free */
      loss = option_int_get(tpo) * bulbs / 100;
    }
  } else {
    loss = 0;
  }
    fc_assert(bulbs >= loss);
  if(!min_cost) {
    min_cost = 1;
  }
  log_debug("Consider switching to %s with %d bulbs (%d|%d), %d losing, "
            "%d..%d expecting to spend",
            advance_rule_name(advance_by_number(tech)), bulbs,
            presearch->bulbs_researching_saved, presearch->bulbs_researched,
            loss, min_cost, cost);
  
  fc_snprintf(buf, buf_sz, PL_("Research %s for %d bulb?\n",
                               "Research %s for %d bulbs?\n", cost),
              advance_name_translation(vap), cost);
    
  if (bulbs >= min_cost) { /* bulbs - loss, but how we know there is loss? */
    if (min_cost != cost) {
      cat_snprintf(buf, buf_sz,
                   PL_("(The cost might be down to %d bulb"
                       " due to tech leakage.)\n",
                       "(The cost might be down to %d bulbs"
                       " due to tech leakage.)\n",
                       min_cost),
                   min_cost);
    }
    if (loss > 0) { /* TODO: may we know if there is loss for sure? */
      cat_snprintf(buf, buf_sz,
                   PL_("%d bulb may be lost "
                       "if you have got no advance this turn.\n",
                       "%d bulbs may be lost "
                       "if you have got no advance this turn.\n", loss),
                   loss);
    }

    if (min_cost == cost && !loss) {
      cat_snprintf(buf, buf_sz,
                   PL_("%d bulb will remain.\n",
                       "%d bulbs will remain.\n", bulbs - cost - loss),
                   bulbs - cost - loss);
    } else {
      int max_rem = bulbs - min_cost;
      int min_rem = bulbs - cost - loss;
      cat_snprintf(buf, buf_sz,
                   /* FIXME: in some languages does PL_ act good here? */
                   PL_("%d to %d bulb will remain.\n",
                       "%d to %d bulbs will remain.\n",
                       max_rem),
                   MAX(0, min_rem),
                   max_rem
                   );
    }

    *certain = bulbs - loss >= cost;
    return TRUE;
  }

  return FALSE;
}
