/*
   GNU xhippo: a GTK-based playlist manager.
   Copyright 1998, 1999, 2000, 2007 Adam Sampson <ats@offog.org>

   This file is part of GNU xhippo.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "xhippo.h"

/* Prototypes for static functions. */
static void add_file_action(gchar *extension, gchar *action, gchar *options);
static void add_user_command(gchar *description, gchar *command);
static void conf_read_playlist(gchar *name);
static void prefs_bool_callback(GtkWidget *widget, gpointer data);

/* Globals. */
gboolean mode_play_on_start = 0, 
  mode_scroll_catchup = 0, 
  mode_left_scroll = 0,
  mode_must_play_all = 0, 
  mode_one_time = 0, 
  mode_show_pid = 0,
  mode_read_id3_tags = 0,
  mode_save_window_pos = 0,
  mode_start_mini = 0,
  mode_play_ordered = 0,
  mode_strip_extension = 0,
  mode_hide_player_output = 0,
  mode_demangle_names = 0,
  mode_playlist_title = 0,
  mode_title_basename = 0,
  mode_no_check_files = 0,
  mode_write_playing = 0,
  mode_commandline_songs = 0,
  mode_commandline_dirs = 0,
  mode_delete_when_played = 0,
  mode_persist_playlist = 0,
  mode_commandline_guess = 0,
  mode_persist_frequently = 0,
  attribute_mini = 0;
guint mode_skip_path = 0;
gchar *attribute_playlist_dir = NULL;
sorttype attribute_sort_on_load = SORT_NONE;
gint attribute_xpos = 100, 
  attribute_ypos = 100, 
  attribute_width = 325, 
  attribute_height = 325;
fileaction *fileactions = NULL;
usercommand *usercommands = NULL;

/* Configuration commands. */
static configcommand *configcommands;
static int num_configcommands, size_configcommands;

/* Add a config command to the list. */
static void add_config_command(gint pass, configtype type, char *command,
			       void *target, const char *label) {

  /* Grow the array if it's not big enough. */
  if (num_configcommands >= size_configcommands) {
    size_configcommands += 50;
    configcommands = realloc(configcommands,
			     size_configcommands * sizeof *configcommands);
  }

  configcommands[num_configcommands].pass = pass;
  configcommands[num_configcommands].type = type;
  configcommands[num_configcommands].command = command;
  configcommands[num_configcommands].target = target;
  configcommands[num_configcommands].label = label;

  num_configcommands++;
}

/* Initialise the list of config commands. */
void init_config_commands() {
  num_configcommands = 0;
  size_configcommands = 0;
  configcommands = NULL;

  add_config_command(0, CT_BOOL, "autostart", &mode_play_on_start,
		     _("Start playing automatically"));
  add_config_command(0, CT_BOOL, "scroll", &mode_scroll_catchup,
		     _("Scroll the list when moving to a new song"));
  add_config_command(0, CT_BOOL, "leftscroll", &mode_left_scroll,
		     _("Place the scrollbar on the left of the window"));
  add_config_command(0, CT_BOOL, "mustplayall", &mode_must_play_all,
		     _("Avoid repeating songs when picking randomly"));
  add_config_command(0, CT_BOOL, "onetime", &mode_one_time,
		     _("Stop at end of playlist"));
  add_config_command(0, CT_BOOL, "showpid", &mode_show_pid,
		     _("Show the PID of the player process"));
  add_config_command(0, CT_BOOL, "readid3", &mode_read_id3_tags,
		     _("Read ID3 tags from MP3 files"));
  add_config_command(0, CT_BOOL, "savewinstate", &mode_save_window_pos,
		     _("Save window position and size on exit"));
  add_config_command(0, CT_BOOL, "startmini", &mode_start_mini,
		     _("Start up with Mini turned on"));
  add_config_command(0, CT_BOOL, "ordered", &mode_play_ordered,
		     _("Start up with Random turned off"));
  add_config_command(0, CT_BOOL, "stripextension", &mode_strip_extension,
		     _("Don't display the extension of files in the "
		       "playlist"));
  add_config_command(0, CT_BOOL, "hideplayeroutput", &mode_hide_player_output,
		     _("Hide the output of all players"));
  add_config_command(0, CT_BOOL, "demanglenames", &mode_demangle_names,
		     _("Demangle file names"));
  add_config_command(0, CT_BOOL, "playlisttitle", &mode_playlist_title,
		     _("Show the name of the current playlist in the window "
		       "title"));
  add_config_command(0, CT_BOOL, "titlebasename", &mode_title_basename,
		     _("Use the basename of the playlist when showing "
		       "playlist name"));
  add_config_command(0, CT_BOOL, "nocheckfiles", &mode_no_check_files,
		     _("Don't check that files in the playlist exist"));
  add_config_command(0, CT_BOOL, "writeplaying", &mode_write_playing,
		     _("Write the currently-playing song to "
		       "$HOME/.xhippo/current_song"));
  add_config_command(0, CT_BOOL, "commandlinesongs", &mode_commandline_songs,
		     _("On startup, command-line arguments are songs"));
  add_config_command(0, CT_BOOL, "commandlinedirs", &mode_commandline_dirs,
		     _("On startup, command-line arguments are directories"));
  add_config_command(0, CT_BOOL, "deletewhenplayed", &mode_delete_when_played,
		     _("Remove played songs from the playlist"));
  add_config_command(0, CT_BOOL, "persistplaylist", &mode_persist_playlist,
		     _("Autosave the playlist on exit and autoload it on "
		       "startup"));
  add_config_command(0, CT_BOOL, "commandlineguess", &mode_commandline_guess,
		     _("On startup, guess the types of command-line "
		       "arguments"));
  add_config_command(0, CT_BOOL, "persistfrequently", &mode_persist_frequently,
		     _("When autosaving, autosave playlist whenever a new "
		       "song starts"));
  add_config_command(0, CT_INT, "skippath", &mode_skip_path,
		     _("Number of path elements to strip (0 to strip all)"));
  add_config_command(0, CT_STRING, "playlistdir", &attribute_playlist_dir,
		     _("Directory to save playlists in"));
  add_config_command(0, CT_SORTTYPE, "sortonload", &attribute_sort_on_load,
		     _("Type of sort to perform whenever a playlist is "
		       "loaded"));
  add_config_command(0, CT_FUNCTION23, "type", add_file_action, NULL);
  add_config_command(0, CT_FUNCTION2, "usercommand", add_user_command, NULL);
  add_config_command(1, CT_FUNCTION1, "load", conf_read_playlist, NULL);
  add_config_command(1, CT_FUNCTION1, "exec", system, NULL);
  add_config_command(0, CT_END, NULL, NULL, NULL);
}

/* Read a playlist. */
static void conf_read_playlist(gchar *name) {
  read_playlist(name, 0);
}

/* Add a file action to the list. */
static void add_file_action(gchar *extension, gchar *action, gchar *options) {
  char *p;
  fileaction *fa = (fileaction *)malloc(sizeof(fileaction));

  fa->extension = strdup(extension);
  fa->action = strdup(action);

  /* Make fa->name contain the name up to the first space. */
  fa->name = strdup(action);
  p = strchr(fa->name, ' ');
  if (p) *p = '\0';

  fa->setgroup = fa->nullinput = 0;
  if (options) {
    if (strchr(options, 'g')) fa->setgroup = 1;
    if (strchr(options, 'i')) fa->nullinput = 1;
  }
  
  fa->next = fileactions;
  fileactions = fa;
}

/* Add a user command to the list. This must be done in order in order
   to avoid confusing the user, so we have to append to the end of the
   list. */
static void add_user_command(gchar *description, gchar *command) {
  usercommand *uc = (usercommand *)malloc(sizeof(usercommand)), *p;
  
  uc->description = strdup(description);
  uc->command = strdup(command);
  uc->next = NULL;

  if (!usercommands) {
    usercommands = uc;
  } else {
    for (p = usercommands; p->next; p = p->next) /* empty */;
    p->next = uc;
  }
}

/* Execute a configuration command. This may have uses other than
   reading the config file in the future (perhaps for remote
   control). */
void execute_command(gchar *command, gint pass) {
  gchar *cmd = strdup(command), *opta, *optb = NULL, *optc = NULL;
  configcommand *cc;

  if (!*cmd) return;
  if (*cmd == '#') return;

  opta = strchr(cmd, ':');
  if (opta) {
    *opta++ = '\0';
    optb = strchr(opta, ':');
    if (optb) {
      *optb++ = '\0';
      optc = strchr(optb, ':');
      if (optc) *optc++ = '\0';
    }
  }

  for (cc = configcommands; cc->type != CT_END; cc++) {
    if (strcmp(cmd, cc->command) != 0) continue;
    if (cc->pass != pass) continue;
    
    switch (cc->type) {
    case CT_BOOL: {
      if (!opta) {
	fprintf(stderr, "Command '%s' needs an argument.", cmd);
	goto out;
      }
      if (strcmp(opta, "yes") == 0
	  || strcmp(opta, "on") == 0
	  || strcmp(opta, "true") == 0
	  || atoi(opta) != 0) {
	*(gboolean *)cc->target = 1;
      } else {
	*(gboolean *)cc->target = 0;
      }
      break;
    }
    case CT_INT: {
      if (!opta) {
	fprintf(stderr, "Command '%s' needs an argument.", cmd);
	goto out;
      }
      *(guint *)cc->target = atoi(opta);
      break;
    }
    case CT_STRING: {
      if (!opta) {
	fprintf(stderr, "Command '%s' needs an argument.", cmd);
	goto out;
      }
      *(gchar **)cc->target = strdup(opta);
      break;
    }
    case CT_SORTTYPE: {
      sorttype *a = (sorttype *)cc->target;

      if (!opta) {
	fprintf(stderr, "Command '%s' needs an argument.", cmd);
	goto out;
      }
      *a = SORT_NONE;
      if (strcmp(opta, "name") == 0) *a = SORT_NAME;
      else if (strcmp(opta, "swapped") == 0) *a = SORT_SWAPPED;
      else if (strcmp(opta, "mtime") == 0) *a = SORT_MTIME;
      /* For compatibility, allow users to specify a non-zero numeric
	 value to mean SORT_NAME. */
      else if (atoi(opta) > 0) *a = SORT_NAME;
      break;
    }
    case CT_FUNCTION1: {
      void (*func)(gchar *a) = cc->target;

      if (!opta) {
	fprintf(stderr, "Command '%s' needs an argument.", cmd);
	goto out;
      }
      func(opta);
      break;
    }
    case CT_FUNCTION2: {
      void (*func)(gchar *a, gchar *b) = cc->target;

      if (!opta || !optb) {
	fprintf(stderr, "Command '%s' needs two arguments.", cmd);
	goto out;
      }
      func(opta, optb);
      break;
    }
    case CT_FUNCTION23: {
      void (*func)(gchar *a, gchar *b, gchar *c) = cc->target;

      if (!opta || !optb) {
	fprintf(stderr, "Command '%s' needs at least two arguments.", cmd);
	goto out;
      }
      func(opta, optb, optc);
      break;
    }
    case CT_END:
      /* This will not happen. */
      break;
    }
    
    /* And, since we've matched the command, there's no point
       in looking any further. */
    break;
  }
  
 out:
  free(cmd);
}

/* Read a config file, named in name. Pass 0 is before the GUI is
   shown, pass 1 is after. */
void read_config(gchar *name, gint pass) {
  FILE *f;
  gchar line[STRBUFSIZE];

  f = fopen(name, "r");
  if (!f) return;

  while (fgets(line, STRBUFSIZE, f)) {
    chomp(line);
    execute_command(line, pass);
  }
  
  fclose(f);
}

/* Read config files. */
void read_config_files(gint pass) {
  gchar rcname[STRBUFSIZE];

  read_config(SYSTEMXHIPPOCONFIG, pass);
  snprintf(rcname, STRBUFSIZE, "%s/.xhippo/config", getenv("HOME"));
  read_config(rcname, pass);
  read_config("xhippo.config", pass);
}

/* Get the directory in which the user wants to store playlists. */
void get_playlist_dir(gchar *buf, int buflen) {
  /* If attribute_playlist_dir is set, use that. */
  if (attribute_playlist_dir) {
    snprintf(buf, buflen, "%s", attribute_playlist_dir);
    return;
  }
  
  /* If $HOME/.xhippo/playlists is a directory, use that. */
  snprintf(buf, buflen, "%s/.xhippo/playlists/", getenv("HOME"));
  if (filetype(buf) == 2) return;

  /* Else use nothing. */
  strcpy(buf, "");
}

/* Save the window position. */
void save_window_state() {
  char filename[STRBUFSIZE];
  FILE *f;

  snprintf(filename, STRBUFSIZE, "%s/.xhippo/winstate", getenv("HOME"));
  if ((f = fopen(filename, "w"))) {
    fprintf(f, "%d\n%d\n%d\n%d\n%d\n", attribute_xpos, attribute_ypos, 
	    attribute_mini, attribute_width, attribute_height);
    fclose(f);
  } else {
    g_print(_("Couldn't write to $HOME/.xhippo/winstate.\n"));
  }
}

/* Read the window position. */
void read_window_state() {
  char filename[STRBUFSIZE];
  char line[STRBUFSIZE];
  FILE *f;

  snprintf(filename, STRBUFSIZE, "%s/.xhippo/winstate", getenv("HOME"));
  if ((f = fopen(filename, "r"))) {
    fgets(line, STRBUFSIZE, f);
    attribute_xpos = atoi(line);
    fgets(line, STRBUFSIZE, f);
    attribute_ypos = atoi(line);
    fgets(line, STRBUFSIZE, f);
    attribute_mini = atoi(line);
    fgets(line, STRBUFSIZE, f);
    attribute_width = atoi(line);
    fgets(line, STRBUFSIZE, f);
    attribute_height = atoi(line);
    fclose(f);
  } /* else fail silently, it happens all the time */
}

/* Handle a click on a preferences checkbox that represents a CF_BOOL. */
static void prefs_bool_callback(GtkWidget *widget, gpointer data) {
  gboolean *value = (gboolean *) data;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    *value = 1;
  } else {
    *value = 0;
  }
}

/* Show the prefs window. */
void show_prefs_window() {
  configcommand *cc;

  if (prefs_window) return;

  prefs_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(prefs_window), _("Preferences"));
  gtk_signal_connect(GTK_OBJECT(prefs_window), "delete_event",
		     GTK_SIGNAL_FUNC(handle_prefs_delete_event), NULL);
  gtk_signal_connect(GTK_OBJECT(prefs_window), "destroy",
		     GTK_SIGNAL_FUNC(handle_prefs_destroy), NULL);
  prefs_all_box = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(prefs_window), prefs_all_box);
  
  for (cc = configcommands; cc->type != CT_END; cc++) {
    switch (cc->type) {
    case CT_BOOL: {
      GtkWidget *w = gtk_check_button_new_with_label(cc->label);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),
				   *(gboolean *)cc->target);
      gtk_signal_connect(GTK_OBJECT(w), "toggled",
			 GTK_SIGNAL_FUNC(prefs_bool_callback),
			 (gpointer)cc->target);
      gtk_widget_show(w);
      gtk_box_pack_start(GTK_BOX(prefs_all_box), w, FALSE, FALSE, 0);
      break;
    }
    default:
      /* Do nothing. */
      break;
    }
  }

  gtk_widget_show(prefs_all_box);
  gtk_widget_show(prefs_window);
}
