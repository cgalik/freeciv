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

#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

/* dependencies/lua */
#include "lua.h"
#include "lualib.h"

/* dependencies/tolua */
#include "tolua.h"

/* utility */
#include "log.h"

/* common */
#include "featured_text.h"

/* common/scriptcore */
#include "api_game_specenum.h"
#include "luascript.h"
#include "tolua_common_a_gen.h"
#include "tolua_common_z_gen.h"
#include "tolua_game_gen.h"
#include "tolua_signal_gen.h"

/* client */
#include "luaconsole_common.h"

/* client/luascript */
#include "tolua_client_gen.h"

#include "script_client.h"

/*****************************************************************************
  Lua virtual machine state.
*****************************************************************************/
static struct fc_lua *main_fcl = NULL;

/*****************************************************************************
  Optional game script code (useful for scenarios).
*****************************************************************************/
static char *script_client_code = NULL;

static void script_client_vars_init(void);
static void script_client_vars_free(void);
static void script_client_vars_load(struct section_file *file);
static void script_client_vars_save(struct section_file *file);
static void script_client_code_init(void);
static void script_client_code_free(void);
static void script_client_code_load(struct section_file *file);
static void script_client_code_save(struct section_file *file);

static void script_client_output(struct fc_lua *fcl, enum log_level level,
                                 const char *format, ...)
            fc__attribute((__format__ (__printf__, 3, 4)));

static void script_client_signal_create(void);

/***********************************************************************//****
  Push an empty table into Lua stack, return its position
  To be cleared with script_client_obj_done(this_or_further_value)
*****************************************************************************/
int script_client_newtable(void) {
  lua_State *L;
  
  fc_assert_ret_val(main_fcl, 0);
  L = main_fcl->state;
  fc_assert_ret_val(L, 0);
  
  lua_newtable(L);
  return lua_gettop(L);
}

/***********************************************************************//****
  Adds (raw way) an integer key to a table.
  All values except the first are ignored.
*****************************************************************************/
void script_client_table_seti(int t, int i, enum api_types vt, ...) {
  va_list args;

  va_start(args, vt);
  luascript_rawseti(main_fcl, t, i, vt, args);
  va_end(args);
}

/***********************************************************************//****
  Adds (raw way) a string key to a table.
  All values except the first are ignored.
*****************************************************************************/
void script_client_table_setfield(int t, const char *f,
                                  enum api_types vt, ...) {
  va_list args;

  va_start(args, vt);
  luascript_rawsetfield(main_fcl, t, f, vt, args);
  va_end(args);
}

/***********************************************************************//****
  We give callback a table and it may put something inside. Check what type.
*****************************************************************************/
enum api_types script_client_table_field_type(int t, const char* f) {
  fc_assert(main_fcl);

  return luascript_field_api_type(main_fcl->state, t, f);
}

/***********************************************************************//****
  We give callback a table and it may put something inside. Check what type.
*****************************************************************************/
enum api_types script_client_table_i_type(int t, int i) {
  fc_assert(main_fcl);

  return luascript_index_api_type(main_fcl->state, t, i);
}

/***********************************************************************//****
  We give callback a table and it may change it. Check sequence length.
  Returns 0 on errors. Uses raw table access.
*****************************************************************************/
int script_client_table_len(int t) {
  fc_assert_ret_val(main_fcl, 0);

  return luascript_rawlen(main_fcl->state, t);
}

/***********************************************************************//****
  Routine to get feedback from a signal: we give signal a table,
  signal changes its contents, we check how.
  If the key type does not match apit, does not change *args and
  logs out an error using table name n if not NULL.
  Uses raw table access but for some types may use conversion metamethod.
*****************************************************************************/
void script_client_table_n_geti(int t, const char *n, int i,
                                enum api_types apit, ...) {
  va_list args;

  va_start(args, apit);
  luascript_table_rawgeti(main_fcl, t, n, i, apit, args);
  va_end(args);
}

/***********************************************************************//****
  Routine to get feedback from a signal: we give it a table,
  it changes its contents, we check how.
  If the key type does not match apit, does not change v and
  logs out an error using table name n if not NULL.
  Uses raw table access but for some types may use conversion metamethod.
*****************************************************************************/
void script_client_table_n_getfield(int t, const char *n, const char* f, 
                                    enum api_types apit, ...) {
  va_list args;

  va_start(args, apit);
  luascript_table_rawgetfield(main_fcl, t, n, f, apit, args);
  va_end(args);
}

/***********************************************************************//****
  Clears Lua stack top starting from the value obj (including it)
*****************************************************************************/
void script_client_obj_done(int obj) {
  fc_assert_ret(main_fcl);
  luascript_trunc(main_fcl->state, obj);
}

/*****************************************************************************
  Parse and execute the script in str
*****************************************************************************/
bool script_client_do_string(const char *str)
{
  int status = luascript_do_string(main_fcl, str, "cmd");

  return (status == 0);
}

/*****************************************************************************
  Parse and execute the script at filename.
*****************************************************************************/
bool script_client_do_file(const char *filename)
{
  int status = luascript_do_file(main_fcl, filename);

  return (status == 0);
}

/*****************************************************************************
  Invoke the 'callback_name' Lua function.
*****************************************************************************/
bool script_client_callback_invoke(const char *callback_name, int nargs,
                                   enum api_types *parg_types, va_list args)
{
  return luascript_callback_invoke(main_fcl, callback_name, nargs, parg_types,
                                   args);
}

/*****************************************************************************
  Mark any, if exported, full userdata representing 'object' in
  the current script state as 'Nonexistent'.
  This changes the type of the lua variable.
*****************************************************************************/
void script_client_remove_exported_object(void *object)
{
  luascript_remove_exported_object(main_fcl, object);
}

/*****************************************************************************
  Initialize the game script variables.
*****************************************************************************/
static void script_client_vars_init(void)
{
  /* nothing */
}

/*****************************************************************************
  Free the game script variables.
*****************************************************************************/
static void script_client_vars_free(void)
{
  /* nothing */
}

/*****************************************************************************
  Load the game script variables in file.
*****************************************************************************/
static void script_client_vars_load(struct section_file *file)
{
  luascript_vars_load(main_fcl, file, "script.vars");
}

/*****************************************************************************
  Save the game script variables to file.
*****************************************************************************/
static void script_client_vars_save(struct section_file *file)
{
  luascript_vars_save(main_fcl, file, "script.vars");
}

/*****************************************************************************
  Initialize the optional game script code (useful for scenarios).
*****************************************************************************/
static void script_client_code_init(void)
{
  script_client_code = NULL;
}

/*****************************************************************************
  Free the optional game script code (useful for scenarios).
*****************************************************************************/
static void script_client_code_free(void)
{
  if (script_client_code) {
    free(script_client_code);
    script_client_code = NULL;
  }
}

/*****************************************************************************
  Load the optional game script code from file (useful for scenarios).
*****************************************************************************/
static void script_client_code_load(struct section_file *file)
{
  if (!script_client_code) {
    const char *code;
    const char *section = "script.code";

    code = secfile_lookup_str_default(file, "", "%s", section);
    script_client_code = fc_strdup(code);
    luascript_do_string(main_fcl, script_client_code, section);
  }
}

/*****************************************************************************
  Save the optional game script code to file (useful for scenarios).
*****************************************************************************/
static void script_client_code_save(struct section_file *file)
{
  if (script_client_code) {
    secfile_insert_str_noescape(file, script_client_code, "script.code");
  }
}

/*****************************************************************************
  Initialize the scripting state.
*****************************************************************************/
bool script_client_init(void)
{
  if (main_fcl != NULL) {
    fc_assert_ret_val(main_fcl->state != NULL, FALSE);

    return TRUE;
  }

  main_fcl = luascript_new(script_client_output);
  if (main_fcl == NULL) {
    luascript_destroy(main_fcl); /* TODO: main_fcl is NULL here... */
    main_fcl = NULL;

    return FALSE;
  }

  tolua_common_a_open(main_fcl->state);
  api_specenum_open(main_fcl->state);
  tolua_game_open(main_fcl->state);
  tolua_signal_open(main_fcl->state);
  tolua_client_open(main_fcl->state);
  tolua_common_z_open(main_fcl->state);

  script_client_code_init();
  script_client_vars_init();

  luascript_signal_init(main_fcl);
  script_client_signal_create();

  return TRUE;
}

/*****************************************************************************
  Ouput a message on the client lua console.
*****************************************************************************/
static void script_client_output(struct fc_lua *fcl, enum log_level level,
                                 const char *format, ...)
{
  va_list args;
  struct ft_color ftc_luaconsole = ftc_luaconsole_error;

  switch (level) {
  case LOG_FATAL:
    /* Special case - will quit the client. */
    {
      char buf[1024];

      va_start(args, format);
      fc_vsnprintf(buf, sizeof(buf), format, args);
      va_end(args);

      log_fatal("%s", buf);
    }
    break;
  case LOG_ERROR:
    ftc_luaconsole = ftc_luaconsole_error;
    break;
  case LOG_NORMAL:
    ftc_luaconsole = ftc_luaconsole_normal;
    break;
  case LOG_VERBOSE:
    ftc_luaconsole = ftc_luaconsole_verbose;
    break;
  case LOG_DEBUG:
    ftc_luaconsole = ftc_luaconsole_debug;
    break;
  }

  va_start(args, format);
  luaconsole_vprintf(ftc_luaconsole, format, args);
  va_end(args);
}

/*****************************************************************************
  Free the scripting data.
*****************************************************************************/
void script_client_free(void)
{
  if (main_fcl != NULL) {
    script_client_code_free();
    script_client_vars_free();

    luascript_signal_free(main_fcl);

    luascript_destroy(main_fcl);
    main_fcl = NULL;
  }
}

/*****************************************************************************
  Load the scripting state from file.
*****************************************************************************/
void script_client_state_load(struct section_file *file)
{
  script_client_code_load(file);

  /* Variables must be loaded after code is loaded and executed,
   * so we restore their saved state properly */
  script_client_vars_load(file);
}

/*****************************************************************************
  Save the scripting state to file.
*****************************************************************************/
void script_client_state_save(struct section_file *file)
{
  script_client_code_save(file);
  script_client_vars_save(file);
}

/*****************************************************************************
  Invoke all the callback functions attached to a given signal.
*****************************************************************************/
void script_client_signal_emit(const char *signal_name, ...)
{
  va_list args;

  va_start(args, signal_name);
  luascript_signal_emit_valist(main_fcl, signal_name, args);
  va_end(args);
}

/*****************************************************************************
  Declare any new signal types you need here.
*****************************************************************************/
static void script_client_signal_create(void)
{
  luascript_signal_create(main_fcl, "new_tech", 0);
  /* Unit peekers */
  luascript_signal_create(main_fcl, "unit_create", 1, API_TYPE_UNIT);
  luascript_signal_create(main_fcl, "unit_remove", 1, API_TYPE_UNIT);
  luascript_signal_create(main_fcl, "unit_moved", 3, API_TYPE_UNIT,
                          API_TYPE_TILE, API_TYPE_TILE);
  luascript_signal_create(main_fcl, "unit_captured", 2,
                          API_TYPE_UNIT, API_TYPE_PLAYER);
  luascript_signal_create(main_fcl, "combat_info", 5,
                          API_TYPE_UNIT, API_TYPE_UNIT,
                          API_TYPE_INT, API_TYPE_INT, API_TYPE_BOOL);
  /* City peekers */
  luascript_signal_create(main_fcl, "city_create", 1, API_TYPE_CITY);
  luascript_signal_create(main_fcl, "city_transferred", 1, API_TYPE_CITY,
                          API_TYPE_PLAYER, API_TYPE_PLAYER);
  luascript_signal_create(main_fcl, "city_remove", 1, API_TYPE_CITY);
  /* Message peeker */
  luascript_signal_create(main_fcl, "event", 6,
                          API_TYPE_STRING, API_TYPE_TILE, API_TYPE_INT,
                          API_TYPE_INT, API_TYPE_PLAYER, API_TYPE_TABLE);
}
