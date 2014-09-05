/********************************************************************** 
 Freeciv - Copyright (C) 1996-2005 - Freeciv Development Team
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

#include <gtk/gtk.h>

/* utility */
#include "astring.h"
#include "support.h"

/* common */
#include "actions.h"
#include "game.h"
#include "movement.h"
#include "research.h"
#include "unit.h"
#include "unitlist.h"

/* client */
#include "dialogs_g.h"
#include "chatline.h"
#include "choice_dialog.h"
#include "client_main.h"
#include "climisc.h"
#include "connectdlg_common.h"
#include "control.h"
#include "gui_main.h"
#include "gui_stuff.h"
#include "mapview.h"
#include "packhand.h"

/* client/gui-gtk-2.0 */
#include "citydlg.h"
#include "dialogs.h"
#include "wldlg.h"

static GtkWidget *diplomat_dialog;
static int diplomat_id;

static GtkWidget  *spy_tech_shell;

static GtkWidget  *spy_sabotage_shell;

/* A structure to hold parameters for actions inside the GUI in stead of
 * storing the needed data in a global variable. */
struct action_data {
  int actor_unit_id;
  int target_city_id;
  int target_unit_id;
  int value;
};

/****************************************************************
  Create a new action data structure that can be stored in the
  dialogs.
*****************************************************************/
static struct action_data *act_data(int actor_unit_id,
                                    int target_city_id,
                                    int target_unit_id,
                                    int value)
{
  struct action_data *data = fc_malloc(sizeof(*data));

  data->actor_unit_id = actor_unit_id;
  data->target_city_id = target_city_id;
  data->target_unit_id = target_unit_id;
  data->value = value;

  return data;
}

/**********************************************************************
  User responded to bribe dialog
**********************************************************************/
static void bribe_response(GtkWidget *w, gint response, gpointer data)
{
  struct action_data *args = (struct action_data *)data;

  if (response == GTK_RESPONSE_YES) {
    request_diplomat_action(DIPLOMAT_BRIBE, args->actor_unit_id,
                            args->target_unit_id, 0);
  }

  gtk_widget_destroy(w);
  free(args);
}

/****************************************************************
  Ask the server how much the bribe is
*****************************************************************/
static void diplomat_bribe_callback(GtkWidget *w, gpointer data)
{
  struct action_data *args = (struct action_data *)data;

  if (NULL != game_unit_by_number(args->actor_unit_id)
      && NULL != game_unit_by_number(args->target_unit_id)) {
    request_action_details(ACTION_SPY_BRIBE_UNIT, args->actor_unit_id,
                           args->target_unit_id);
  }

  gtk_widget_destroy(diplomat_dialog);
  free(args);
}

/*************************************************************************
  Popup unit bribe dialog
**************************************************************************/
void popup_bribe_dialog(struct unit *actor, struct unit *punit, int cost)
{
  GtkWidget *shell;
  char buf[1024];

  fc_snprintf(buf, ARRAY_SIZE(buf), PL_("Treasury contains %d gold.",
                                        "Treasury contains %d gold.",
                                        client_player()->economic.gold),
              client_player()->economic.gold);

  if (cost <= client_player()->economic.gold) {
    shell = gtk_message_dialog_new(NULL, 0,
      GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
      /* TRANS: %s is pre-pluralised "Treasury contains %d gold." */
      PL_("Bribe unit for %d gold?\n%s",
          "Bribe unit for %d gold?\n%s", cost), cost, buf);
    gtk_window_set_title(GTK_WINDOW(shell), _("Bribe Enemy Unit"));
    setup_dialog(shell, toplevel);
  } else {
    shell = gtk_message_dialog_new(NULL, 0,
      GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
      /* TRANS: %s is pre-pluralised "Treasury contains %d gold." */
      PL_("Bribing the unit costs %d gold.\n%s",
          "Bribing the unit costs %d gold.\n%s", cost), cost, buf);
    gtk_window_set_title(GTK_WINDOW(shell), _("Traitors Demand Too Much!"));
    setup_dialog(shell, toplevel);
  }
  gtk_window_present(GTK_WINDOW(shell));
  
  g_signal_connect(shell, "response", G_CALLBACK(bribe_response),
                   act_data(actor->id, 0, punit->id, cost));
}

/****************************************************************
  User selected sabotaging from choice dialog
*****************************************************************/
static void diplomat_sabotage_callback(GtkWidget *w, gpointer data)
{
  struct action_data *args = (struct action_data *)data;

  if (NULL != game_unit_by_number(args->actor_unit_id)
      && NULL != game_city_by_number(args->target_city_id)) {
    request_diplomat_action(DIPLOMAT_SABOTAGE, args->actor_unit_id,
                            args->target_city_id, B_LAST + 1);
  }

  gtk_widget_destroy(diplomat_dialog);
  free(args);
}

/****************************************************************
  User selected investigating from choice dialog
*****************************************************************/
static void diplomat_investigate_callback(GtkWidget *w, gpointer data)
{
  struct action_data *args = (struct action_data *)data;

  if (NULL != game_city_by_number(args->target_city_id)
      && NULL != game_unit_by_number(args->actor_unit_id)) {
    request_diplomat_action(DIPLOMAT_INVESTIGATE, args->actor_unit_id,
                            args->target_city_id, 0);
  }

  gtk_widget_destroy(diplomat_dialog);
  free(args);
}

/****************************************************************
  User selected unit sabotaging from choice dialog
*****************************************************************/
static void spy_sabotage_unit_callback(GtkWidget *w, gpointer data)
{
  struct action_data *args = (struct action_data *)data;

  request_diplomat_action(SPY_SABOTAGE_UNIT, args->actor_unit_id,
                          args->target_unit_id, 0);

  gtk_widget_destroy(diplomat_dialog);
  free(args);
}

/****************************************************************
  User selected embassy establishing from choice dialog
*****************************************************************/
static void diplomat_embassy_callback(GtkWidget *w, gpointer data)
{
  struct action_data *args = (struct action_data *)data;

  if (NULL != game_unit_by_number(args->actor_unit_id)
      && NULL != game_city_by_number(args->target_city_id)) {
    request_diplomat_action(DIPLOMAT_EMBASSY, args->actor_unit_id,
                            args->target_city_id, 0);
  }

  gtk_widget_destroy(diplomat_dialog);
  free(args);
}

/****************************************************************
  User selected poisoning from choice dialog
*****************************************************************/
static void spy_poison_callback(GtkWidget *w, gpointer data)
{
  struct action_data *args = (struct action_data *)data;

  if (NULL != game_unit_by_number(args->actor_unit_id)
      && NULL != game_city_by_number(args->target_city_id)) {
    request_diplomat_action(SPY_POISON, args->actor_unit_id,
                            args->target_city_id, 0);
  }

  gtk_widget_destroy(diplomat_dialog);
  free(args);
}

/****************************************************************
  User selected stealing from choice dialog
*****************************************************************/
static void diplomat_steal_callback(GtkWidget *w, gpointer data)
{
  struct action_data *args = (struct action_data *)data;

  if (NULL != game_unit_by_number(args->actor_unit_id)
      && NULL != game_city_by_number(args->target_city_id)) {
    request_diplomat_action(DIPLOMAT_STEAL, args->actor_unit_id,
                            args->target_city_id, A_UNSET);
  }

  gtk_widget_destroy(diplomat_dialog);
  free(args);
}

/****************************************************************
  User responded to steal advances dialog
*****************************************************************/
static void spy_advances_response(GtkWidget *w, gint response,
                                  gpointer data)
{
  struct action_data *args = (struct action_data *)data;

  if (response == GTK_RESPONSE_ACCEPT && args->value > 0) {
    if (NULL != game_unit_by_number(args->actor_unit_id)
        && NULL != game_city_by_number(args->target_city_id)) {
      request_diplomat_action(DIPLOMAT_STEAL_TARGET, args->actor_unit_id,
                              args->target_city_id, args->value);
    }
  }

  gtk_widget_destroy(spy_tech_shell);
  spy_tech_shell = NULL;
  free(data);
}

/****************************************************************
  User selected entry in steal advances dialog
*****************************************************************/
static void spy_advances_callback(GtkTreeSelection *select,
                                  gpointer data)
{
  struct action_data *args = (struct action_data *)data;

  GtkTreeModel *model;
  GtkTreeIter it;

  if (gtk_tree_selection_get_selected(select, &model, &it)) {
    gtk_tree_model_get(model, &it, 1, &(args->value), -1);
    
    gtk_dialog_set_response_sensitive(GTK_DIALOG(spy_tech_shell),
      GTK_RESPONSE_ACCEPT, TRUE);
  } else {
    args->value = 0;
	  
    gtk_dialog_set_response_sensitive(GTK_DIALOG(spy_tech_shell),
      GTK_RESPONSE_ACCEPT, FALSE);
  }
}

/****************************************************************
  Create spy's tech stealing dialog
*****************************************************************/
static void create_advances_list(struct player *pplayer,
				 struct player *pvictim,
				 struct action_data *args)
{  
  GtkWidget *sw, *label, *vbox, *view;
  GtkListStore *store;
  GtkCellRenderer *rend;
  GtkTreeViewColumn *col;

  spy_tech_shell = gtk_dialog_new_with_buttons(_("Steal Technology"),
    NULL,
    0,
    GTK_STOCK_CANCEL,
    GTK_RESPONSE_CANCEL,
    _("_Steal"),
    GTK_RESPONSE_ACCEPT,
    NULL);
  setup_dialog(spy_tech_shell, toplevel);
  gtk_window_set_position(GTK_WINDOW(spy_tech_shell), GTK_WIN_POS_MOUSE);

  gtk_dialog_set_default_response(GTK_DIALOG(spy_tech_shell),
				  GTK_RESPONSE_ACCEPT);

  label = gtk_frame_new(_("Select Advance to Steal"));
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(spy_tech_shell)->vbox), label);

  vbox = gtk_vbox_new(FALSE, 6);
  gtk_container_add(GTK_CONTAINER(label), vbox);
      
  store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);

  view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
  g_object_unref(store);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);

  rend = gtk_cell_renderer_text_new();
  col = gtk_tree_view_column_new_with_attributes(NULL, rend,
						 "text", 0, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

  label = g_object_new(GTK_TYPE_LABEL,
    "use-underline", TRUE,
    "mnemonic-widget", view,
    "label", _("_Advances:"),
    "xalign", 0.0,
    "yalign", 0.5,
    NULL);
  gtk_container_add(GTK_CONTAINER(vbox), label);
  
  sw = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
				      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(sw), view);

  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
    GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
  gtk_widget_set_size_request(sw, -1, 200);
  
  gtk_container_add(GTK_CONTAINER(vbox), sw);

  /* Now populate the list */
  if (pvictim) { /* you don't want to know what lag can do -- Syela */
    const struct research *presearch = research_get(pplayer);
    const struct research *vresearch = research_get(pvictim);
    GtkTreeIter it;
    GValue value = { 0, };

    advance_index_iterate(A_FIRST, i) {
      if (research_invention_state(vresearch, i) == TECH_KNOWN
          && (research_invention_state(presearch, i) == TECH_UNKNOWN
              || research_invention_state(presearch, i)
                 == TECH_PREREQS_KNOWN)) {
	gtk_list_store_append(store, &it);

	g_value_init(&value, G_TYPE_STRING);
        g_value_set_static_string(&value, research_advance_name_translation
                                              (presearch, i));
	gtk_list_store_set_value(store, &it, 0, &value);
	g_value_unset(&value);
	gtk_list_store_set(store, &it, 1, i, -1);
      }
    } advance_index_iterate_end;

    gtk_list_store_append(store, &it);

    g_value_init(&value, G_TYPE_STRING);
    {
      struct astring str = ASTRING_INIT;
      /* TRANS: %s is a unit name, e.g., Spy */
      astr_set(&str, _("At %s's Discretion"),
               unit_name_translation(game_unit_by_number(
                                         args->actor_unit_id)));
      g_value_set_string(&value, astr_str(&str));
      astr_free(&str);
    }
    gtk_list_store_set_value(store, &it, 0, &value);
    g_value_unset(&value);
    gtk_list_store_set(store, &it, 1, A_UNSET, -1);
  }

  gtk_dialog_set_response_sensitive(GTK_DIALOG(spy_tech_shell),
    GTK_RESPONSE_ACCEPT, FALSE);
  
  gtk_widget_show_all(GTK_DIALOG(spy_tech_shell)->vbox);

  g_signal_connect(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), "changed",
                   G_CALLBACK(spy_advances_callback), args);
  g_signal_connect(spy_tech_shell, "response",
                   G_CALLBACK(spy_advances_response), args);
  
  args->value = 0;

  gtk_tree_view_focus(GTK_TREE_VIEW(view));
}

/****************************************************************
  User has responded to spy's sabotage building dialog
*****************************************************************/
static void spy_improvements_response(GtkWidget *w, gint response, gpointer data)
{
  struct action_data *args = (struct action_data *)data;

  if (response == GTK_RESPONSE_ACCEPT && args->value > -2) {
    if (NULL != game_unit_by_number(args->actor_unit_id)
        && NULL != game_city_by_number(args->target_city_id)) {
      request_diplomat_action(DIPLOMAT_SABOTAGE_TARGET,
                              args->actor_unit_id,
                              args->target_city_id,
                              args->value + 1);
    }
  }

  gtk_widget_destroy(spy_sabotage_shell);
  spy_sabotage_shell = NULL;
  free(args);
}

/****************************************************************
  User has selected new building from spy's sabotage dialog
*****************************************************************/
static void spy_improvements_callback(GtkTreeSelection *select, gpointer data)
{
  struct action_data *args = (struct action_data *)data;

  GtkTreeModel *model;
  GtkTreeIter it;

  if (gtk_tree_selection_get_selected(select, &model, &it)) {
    gtk_tree_model_get(model, &it, 1, &(args->value), -1);
    
    gtk_dialog_set_response_sensitive(GTK_DIALOG(spy_sabotage_shell),
      GTK_RESPONSE_ACCEPT, TRUE);
  } else {
    args->value = -2;
	  
    gtk_dialog_set_response_sensitive(GTK_DIALOG(spy_sabotage_shell),
      GTK_RESPONSE_ACCEPT, FALSE);
  }
}

/****************************************************************
  Creates spy's building sabotaging dialog
*****************************************************************/
static void create_improvements_list(struct player *pplayer,
				     struct city *pcity,
				     struct action_data *args)
{  
  GtkWidget *sw, *label, *vbox, *view;
  GtkListStore *store;
  GtkCellRenderer *rend;
  GtkTreeViewColumn *col;
  GtkTreeIter it;
  
  spy_sabotage_shell = gtk_dialog_new_with_buttons(_("Sabotage Improvements"),
    NULL,
    0,
    GTK_STOCK_CANCEL,
    GTK_RESPONSE_CANCEL,
    _("_Sabotage"), 
    GTK_RESPONSE_ACCEPT,
    NULL);
  setup_dialog(spy_sabotage_shell, toplevel);
  gtk_window_set_position(GTK_WINDOW(spy_sabotage_shell), GTK_WIN_POS_MOUSE);

  gtk_dialog_set_default_response(GTK_DIALOG(spy_sabotage_shell),
				  GTK_RESPONSE_ACCEPT);

  label = gtk_frame_new(_("Select Improvement to Sabotage"));
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(spy_sabotage_shell)->vbox), label);

  vbox = gtk_vbox_new(FALSE, 6);
  gtk_container_add(GTK_CONTAINER(label), vbox);
      
  store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);

  view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
  g_object_unref(store);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);

  rend = gtk_cell_renderer_text_new();
  col = gtk_tree_view_column_new_with_attributes(NULL, rend,
						 "text", 0, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

  label = g_object_new(GTK_TYPE_LABEL,
    "use-underline", TRUE,
    "mnemonic-widget", view,
    "label", _("_Improvements:"),
    "xalign", 0.0,
    "yalign", 0.5,
    NULL);
  gtk_container_add(GTK_CONTAINER(vbox), label);
  
  sw = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
				      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(sw), view);

  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
    GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
  gtk_widget_set_size_request(sw, -1, 200);
  
  gtk_container_add(GTK_CONTAINER(vbox), sw);

  /* Now populate the list */
  gtk_list_store_append(store, &it);
  gtk_list_store_set(store, &it, 0, _("City Production"), 1, -1, -1);

  city_built_iterate(pcity, pimprove) {
    if (pimprove->sabotage > 0) {
      gtk_list_store_append(store, &it);
      gtk_list_store_set(store, &it,
                         0, city_improvement_name_translation(pcity, pimprove),
                         1, improvement_number(pimprove),
                         -1);
    }  
  } city_built_iterate_end;

  gtk_list_store_append(store, &it);
  {
    struct astring str = ASTRING_INIT;
    /* TRANS: %s is a unit name, e.g., Spy */
    astr_set(&str, _("At %s's Discretion"),
             unit_name_translation(game_unit_by_number(args->actor_unit_id)));
    gtk_list_store_set(store, &it, 0, astr_str(&str), 1, B_LAST, -1);
    astr_free(&str);
  }

  gtk_dialog_set_response_sensitive(GTK_DIALOG(spy_sabotage_shell),
    GTK_RESPONSE_ACCEPT, FALSE);
  
  gtk_widget_show_all(GTK_DIALOG(spy_sabotage_shell)->vbox);

  g_signal_connect(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), "changed",
                   G_CALLBACK(spy_improvements_callback), args);
  g_signal_connect(spy_sabotage_shell, "response",
                   G_CALLBACK(spy_improvements_response), args);

  args->value = -2;
	  
  gtk_tree_view_focus(GTK_TREE_VIEW(view));
}

/****************************************************************
  Popup tech stealing dialog with list of possible techs
*****************************************************************/
static void spy_steal_popup(GtkWidget *w, gpointer data)
{
  struct action_data *args = (struct action_data *)data;

  struct city *pvcity = game_city_by_number(args->target_city_id);
  struct player *pvictim = NULL;

  if (pvcity) {
    pvictim = city_owner(pvcity);
  }

/* it is concievable that pvcity will not be found, because something
has happened to the city during latency.  Therefore we must initialize
pvictim to NULL and account for !pvictim in create_advances_list. -- Syela */

  /* FIXME: Don't discard the second tech choice dialog. */
  if (!spy_tech_shell) {
    create_advances_list(client.conn.playing, pvictim, args);
    gtk_window_present(GTK_WINDOW(spy_tech_shell));
  } else {
    free(args);
  }

  gtk_widget_destroy(diplomat_dialog);
}

/****************************************************************
 Requests up-to-date list of improvements, the return of
 which will trigger the popup_sabotage_dialog() function.
*****************************************************************/
static void spy_request_sabotage_list(GtkWidget *w, gpointer data)
{
  struct action_data *args = (struct action_data *)data;

  if (NULL != game_unit_by_number(args->actor_unit_id)
      && NULL != game_city_by_number(args->target_city_id)) {
    request_action_details(ACTION_SPY_TARGETED_SABOTAGE_CITY,
                           args->actor_unit_id,
                           args->target_city_id);
  }

  gtk_widget_destroy(diplomat_dialog);
  free(args);
}

/*************************************************************************
 Pops-up the Spy sabotage dialog, upon return of list of
 available improvements requested by the above function.
**************************************************************************/
void popup_sabotage_dialog(struct unit *actor, struct city *pcity)
{
  /* FIXME: Don't discard the second target choice dialog. */
  if (!spy_sabotage_shell) {
    create_improvements_list(client.conn.playing, pcity,
                             act_data(actor->id, pcity->id, 0, 0));
    gtk_window_present(GTK_WINDOW(spy_sabotage_shell));
  }
}

/****************************************************************
...  Ask the server how much the revolt is going to cost us
*****************************************************************/
static void diplomat_incite_callback(GtkWidget *w, gpointer data)
{
  struct action_data *args = (struct action_data *)data;

  if (NULL != game_unit_by_number(args->actor_unit_id)
      && NULL != game_city_by_number(args->target_city_id)) {
    request_action_details(ACTION_SPY_INCITE_CITY, args->actor_unit_id,
                           args->target_city_id);
  }

  gtk_widget_destroy(diplomat_dialog);
  free(args);
}

/************************************************************************
  User has responded to incite dialog
************************************************************************/
static void incite_response(GtkWidget *w, gint response, gpointer data)
{
  struct action_data *args = (struct action_data *)data;

  if (response == GTK_RESPONSE_YES) {
    request_diplomat_action(DIPLOMAT_INCITE, args->actor_unit_id,
                            args->target_city_id, 0);
  }

  gtk_widget_destroy(w);
  free(args);
}

/*************************************************************************
Popup the yes/no dialog for inciting, since we know the cost now
**************************************************************************/
void popup_incite_dialog(struct unit *actor, struct city *pcity, int cost)
{
  GtkWidget *shell;
  char buf[1024];

  fc_snprintf(buf, ARRAY_SIZE(buf), PL_("Treasury contains %d gold.",
                                        "Treasury contains %d gold.",
                                        client_player()->economic.gold),
              client_player()->economic.gold);

  if (INCITE_IMPOSSIBLE_COST == cost) {
    shell = gtk_message_dialog_new(NULL,
      0,
      GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
      _("You can't incite a revolt in %s."),
      city_name(pcity));
    gtk_window_set_title(GTK_WINDOW(shell), _("City can't be incited!"));
  setup_dialog(shell, toplevel);
  } else if (cost <= client_player()->economic.gold) {
    shell = gtk_message_dialog_new(NULL, 0,
      GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
      /* TRANS: %s is pre-pluralised "Treasury contains %d gold." */
      PL_("Incite a revolt for %d gold?\n%s",
          "Incite a revolt for %d gold?\n%s", cost), cost, buf);
    gtk_window_set_title(GTK_WINDOW(shell), _("Incite a Revolt!"));
    setup_dialog(shell, toplevel);
  } else {
    shell = gtk_message_dialog_new(NULL,
      0,
      GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
      /* TRANS: %s is pre-pluralised "Treasury contains %d gold." */
      PL_("Inciting a revolt costs %d gold.\n%s",
          "Inciting a revolt costs %d gold.\n%s", cost), cost, buf);
    gtk_window_set_title(GTK_WINDOW(shell), _("Traitors Demand Too Much!"));
    setup_dialog(shell, toplevel);
  }
  gtk_window_present(GTK_WINDOW(shell));
  
  g_signal_connect(shell, "response", G_CALLBACK(incite_response),
                   act_data(actor->id, pcity->id, 0, cost));
}


/****************************************************************
  Callback from diplomat/spy dialog for "keep moving".
  (This should only occur when entering allied city.)
*****************************************************************/
static void diplomat_keep_moving_city_callback(GtkWidget *w, gpointer data)
{
  struct action_data *args = (struct action_data *)data;

  struct unit *punit;
  struct city *pcity;

  if ((punit = game_unit_by_number(args->actor_unit_id))
      && (pcity = game_city_by_number(args->target_city_id))
      && !same_pos(unit_tile(punit), city_tile(pcity))) {
    request_diplomat_action(DIPLOMAT_MOVE, args->actor_unit_id,
                            args->target_city_id, ATK_CITY);
  }

  gtk_widget_destroy(diplomat_dialog);
  free(args);
}

/*************************************************************************
  Callback from diplomat/spy dialog for "keep moving".
  (This should only occur when entering the tile of an allied unit.)
**************************************************************************/
static void diplomat_keep_moving_unit_callback(GtkWidget *w, gpointer data)
{
  struct action_data *args = (struct action_data *)data;

  struct unit *punit;
  struct unit *tunit;

  if ((punit = game_unit_by_number(args->actor_unit_id))
      && (tunit = game_unit_by_number(args->target_unit_id))
      && !same_pos(unit_tile(punit), unit_tile(tunit))) {
    request_diplomat_action(DIPLOMAT_MOVE, args->actor_unit_id,
                            args->target_unit_id, ATK_UNIT);
  }

  gtk_widget_destroy(diplomat_dialog);
  free(args);
}

/****************************************************************
  Diplomat dialog has been destoryed
*****************************************************************/
static void diplomat_destroy_callback(GtkWidget *w, gpointer data)
{
  diplomat_dialog = NULL;
  choose_action_queue_next();
}

/****************************************************************
  Diplomat dialog has been canceled
*****************************************************************/
static void diplomat_cancel_callback(GtkWidget *w, gpointer data)
{
  gtk_widget_destroy(diplomat_dialog);
  free(data);
}

/******************************************************************
  Show the user the action if it is enabled.
*******************************************************************/
static void action_entry(GtkWidget *shl,
                         int action_id,
                         const action_probability *action_probabilities,
                         GCallback handler,
                         struct action_data *handler_args)
{
  const gchar *label;
  const gchar *tooltip;

  /* Don't show disabled actions. */
  if (action_probabilities[action_id] == ACTPROB_IMPOSSIBLE) {
    return;
  }

  label = action_prepare_ui_name(action_id, "_",
                                 action_probabilities[action_id]);

  switch (action_probabilities[action_id]) {
  case ACTPROB_NOT_KNOWN:
    /* Missing in game knowledge. An in game action can change this. */
    tooltip =
        _("Starting to do this may currently be impossible.");
    break;
  case ACTPROB_NOT_IMPLEMENTED:
    /* Missing server support. No in game action will change this. */
    tooltip = NULL;
    break;
  default:
    tooltip = g_strdup_printf(_("The probability of success is %.1f%%."),
                              (double)action_probabilities[action_id] / 2);
    break;
  }

  choice_dialog_add(shl, label, handler, handler_args, tooltip);
}

/****************************************************************
 Popup new diplomat dialog.
*****************************************************************/
void popup_diplomat_dialog(struct unit *punit, struct city *pcity,
                           struct unit *ptunit, struct tile *dest_tile,
                           const action_probability *action_probabilities)
{
  GtkWidget *shl;
  struct astring title = ASTRING_INIT, text = ASTRING_INIT;

  struct action_data *data = act_data(punit->id,
                                      (pcity) ? pcity->id : 0,
                                      (ptunit) ? ptunit->id : 0,
                                      0);

  diplomat_id = punit->id;

  astr_set(&title,
           /* TRANS: %s is a unit name, e.g., Spy */
           _("Choose Your %s's Strategy"), unit_name_translation(punit));
  astr_set(&text,
           /* TRANS: %s is a unit name, e.g., Diplomat, Spy */
           _("Your %s is waiting for your command."),
           unit_name_translation(punit));

  shl = choice_dialog_start(GTK_WINDOW(toplevel), astr_str(&title),
                            astr_str(&text));

  if (pcity) {
    /* Spy/Diplomat acting against a city */

    action_entry(shl,
                 ACTION_ESTABLISH_EMBASSY,
                 action_probabilities,
                 (GCallback)diplomat_embassy_callback,
                 data);

    action_entry(shl,
                 ACTION_SPY_INVESTIGATE_CITY,
                 action_probabilities,
                 (GCallback)diplomat_investigate_callback,
                 data);

    action_entry(shl,
                 ACTION_SPY_POISON,
                 action_probabilities,
                 (GCallback)spy_poison_callback,
                 data);

    action_entry(shl,
                 ACTION_SPY_SABOTAGE_CITY,
                 action_probabilities,
                 (GCallback)diplomat_sabotage_callback,
                 data);

    action_entry(shl,
                 ACTION_SPY_TARGETED_SABOTAGE_CITY,
                 action_probabilities,
                 (GCallback)spy_request_sabotage_list,
                 data);

    action_entry(shl,
                 ACTION_SPY_STEAL_TECH,
                 action_probabilities,
                 (GCallback)diplomat_steal_callback,
                 data);

    action_entry(shl,
                 ACTION_SPY_TARGETED_STEAL_TECH,
                 action_probabilities,
                 (GCallback)spy_steal_popup,
                 data);

    action_entry(shl,
                 ACTION_SPY_INCITE_CITY,
                 action_probabilities,
                 (GCallback)diplomat_incite_callback,
                 data);
  }

  if (ptunit) {
    /* Spy/Diplomat acting against a unit */

    action_entry(shl,
                 ACTION_SPY_BRIBE_UNIT,
                 action_probabilities,
                 (GCallback)diplomat_bribe_callback,
                 data);

    action_entry(shl,
                 ACTION_SPY_SABOTAGE_UNIT,
                 action_probabilities,
                 (GCallback)spy_sabotage_unit_callback,
                 data);
  }

  if (unit_can_move_to_tile(punit, dest_tile, FALSE)) {
    if (pcity) {
      choice_dialog_add(shl, _("_Keep moving"),
                        (GCallback)diplomat_keep_moving_city_callback,
                        data, NULL);
    } else {
      choice_dialog_add(shl, _("_Keep moving"),
                        (GCallback)diplomat_keep_moving_unit_callback,
                        data, NULL);
    }
  }

  choice_dialog_add(shl, GTK_STOCK_CANCEL,
                    (GCallback)diplomat_cancel_callback, data, NULL);

  choice_dialog_end(shl);

  diplomat_dialog = shl;

  choice_dialog_set_hide(shl, TRUE);
  g_signal_connect(shl, "destroy",
                   G_CALLBACK(diplomat_destroy_callback), NULL);
  g_signal_connect(shl, "delete_event",
                   G_CALLBACK(diplomat_cancel_callback), NULL);
  astr_free(&title);
  astr_free(&text);
}

/****************************************************************
  Returns id of a diplomat currently handled in diplomat dialog
*****************************************************************/
int diplomat_handled_in_diplomat_dialog(void)
{
  if (diplomat_dialog == NULL) {
    return -1;
  }
  return diplomat_id;
}

/****************************************************************
  Closes the diplomat dialog
****************************************************************/
void close_diplomat_dialog(void)
{
  if (diplomat_dialog != NULL) {
    gtk_widget_destroy(diplomat_dialog);
  }
}
