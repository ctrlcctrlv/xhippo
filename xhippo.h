/*
   GNU xhippo: a GTK-based playlist manager.
   Copyright 1998, 1999, 2000 Adam Sampson <ats@offog.org>

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

#ifndef _XHIPPO_H
#define _XHIPPO_H 1

/* Pull in autoconf definitions. */
#include "config.h"

/* --- Other headers --- */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <signal.h>
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <time.h>
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/stat.h>
#include <glib.h>
#ifdef USEGNOME
#include <gnome.h>
#endif
#ifndef NOGETOPTLONG
#include <getopt.h>
#endif
#ifdef HAVE_LIBID3TAG
#include <id3tag.h>
#endif
#ifdef HAVE_NFTW
#define __USE_XOPEN_EXTENDED 1
#include <ftw.h>
#undef __USE_XOPEN_EXTENDED
#endif
#include <locale.h>

/* This is necessary for MacOS X, where using the default redirection
   mechanism generates binaries with unresolved symbols due to some
   reasonably stupid kludges in gettext. */
#define _INTL_REDIRECT_MACROS 1
#include "gettext.h"
#define _(X) gettext(X)


/* --- Constants --- */

/* A generic string buffer size used throughout xhippo. */
#define STRBUFSIZE 1024

/* --- Types --- */

/* A file type linked list. */
struct _fileaction {
  gchar *extension;        /* The extension of the file. */
  gchar *action;           /* The command to run. */
  gchar *name;             /* The name of the command. */
  char setgroup;           /* Whether the player should be run in a
			      separate process group. */
  char nullinput;          /* Whether the player's stdin should be
                              connected to /dev/null. */

  struct _fileaction *next;
};
typedef struct _fileaction fileaction;

/* A user command linked list. */
struct _usercommand {
  gchar *description;      /* Description of the command for the menu. */
  gchar *command;          /* The command to run (%s being replaced by
			      the filename of the current song). */

  struct _usercommand *next;
};
typedef struct _usercommand usercommand;

/* Information about a song in the playlist. */
struct _songinfo {
  gchar *name;             /* Its filename. */
  gchar *display_name;     /* The name shown in the list. */
  char played;             /* Whether it has been played. */
  char inforead;           /* Whether the information that comes 
			      from stat (mtime, size) has been read. */
  time_t mtime;            /* When the file was last modified. */
  off_t size;              /* The size of the file. */
  GtkWidget *info_window;  /* The information window; NULL if not up. */
  int previous;            /* The index of the song played before this one
                              when this song was last picked by the
                              randomiser. -1 if no previous song was picked. */
};
typedef struct _songinfo songinfo;

enum _configtype {
  CT_END,                   /* end of list */
  CT_BOOL,                  /* gboolean */
  CT_INT,                   /* guint */
  CT_STRING,                /* gchar * */
  CT_SORTTYPE,              /* sorttype */
  CT_FUNCTION1,             /* void function(gchar *arg) */
  CT_FUNCTION2,             /* void function(gchar *arg1, gchar *arg2) */
  CT_FUNCTION23             /* void function(gchar *arg1, gchar *arg2,
			       gchar *arg3) (arg3 can be NULL) */
};
typedef enum _configtype configtype;

struct _configcommand {
  gint pass;                /* Pass upon which to read this entry. */
  configtype type;          /* Type of this command. */
  const char *command;      /* Name of this command. */
  void *target;
  const char *label;        /* Label for this command in the config dialog. */
};
typedef struct _configcommand configcommand;

enum _sorttype {
  SORT_NONE,                /* Do not sort. */
  SORT_NAME,                /* Sort by display_name. */
  SORT_SWAPPED,             /* Sort by swapped display_name. */
  SORT_MTIME                /* Sort by mtime. */
};
typedef enum _sorttype sorttype;

/* --- Globals --- */

/* UI widgets */
extern GtkTooltips *tooltips;
extern GtkWidget *window, *restartbutton, *stopbutton, *buttons_box, 
  *all_box, *savebutton, *sortbutton, *addbutton,
  *filelist, *statusbar, *nextbutton, *prevbutton, *pausebutton, *loadbutton,
  *minicheckbox, *randomcheckbox, *fileselector, *list_box,
  *scrollbar, *upbutton, *downbutton, *deletebutton, *popupmenu,
  *up_item, *down_item, *delete_item, *info_item, *eventbox,
  *prefs_window, *prefs_all_box;
extern GtkListStore *filestore;
enum {
  SONGINFO_COLUMN,
  TITLE_COLUMN,
  NUM_COLUMNS
};
extern GtkAdjustment *vadj;
extern GtkAccelGroup *accel_group;
extern int sigchld_pipe[2];
extern gchar startup_cwd[STRBUFSIZE];

/* Status variables */
extern pid_t childpid;
extern guint contextid, 
  listcount, 
  playing, 
  paused;
extern gint last_played,
  last_row_hit,
  last_row_clicked;
fileaction *last_action;

/* Configuration options */
extern gboolean mode_play_on_start, 
  mode_scroll_catchup, 
  mode_left_scroll,
  mode_must_play_all, 
  mode_one_time, 
  mode_show_pid,
  mode_read_id3_tags,
  mode_save_window_pos,
  mode_start_mini,
  mode_play_ordered,
  mode_strip_extension,
  mode_hide_player_output,
  mode_demangle_names,
  mode_playlist_title,
  mode_title_basename,
  mode_no_check_files,
  mode_write_playing,
  mode_commandline_songs,
  mode_commandline_dirs,
  mode_delete_when_played,
  mode_persist_playlist,
  mode_commandline_guess,
  mode_persist_frequently,
  attribute_mini;
extern guint mode_skip_path;
extern gchar *attribute_playlist_dir;
extern sorttype attribute_sort_on_load;
extern gint attribute_xpos, 
  attribute_ypos, 
  attribute_width, 
  attribute_height;
extern fileaction *fileactions;
extern usercommand *usercommands;

/* --- Prototypes --- */

songinfo *copy_songinfo(songinfo *src);
void destroy_songinfo(gpointer sinfo);
void delete_row(gint location);
void insert_row(songinfo *sinfo, gint location);
songinfo *get_songinfo(gint number);
void move_row(gint src, gint dest);
void persist_playlist(void);
gint filetype(char *file);
void chomp(gchar *line);
void status(const char *message);
void update_window(void);
void sync_list(void);
void stop_playing(void);
void pause_playing(void);
fileaction *get_action(const gchar *name);
void start_playing(gint num);
void pick_prev(void);
void pick_next(void);
gchar *read_mp3_tag (const char *filename );
void read_song_info(songinfo *sinfo, struct stat *st);
void add_file(const gchar *name);
void add_directory(const gchar *name);
void save_window_state(void);
void read_window_state(void);
void clear_playlist(void);
int sort_compare_name(const void *a, const void *b);
int sort_compare_swapped_name(const void *a, const void *b);
int sort_compare_mtime(const void *a, const void *b);
void sortplaylist(int (*func)(const void *a, const void *b),
		  char needinfo);
void read_playlist(gchar *name, int use_cwd);
int write_playlist(const gchar *name);
void init_config_commands();
void execute_command(gchar *command, gint pass);
void read_config(gchar *name, gint pass);
void read_config_files(gint pass);
void show_prefs_window();
gboolean handle_button_press(GtkWidget *widget, GdkEventButton *event,
			     gpointer data);
void handle_fileselector_cancel(GtkButton *widget, gpointer data);
void handle_load_ok(GtkButton *widget, gpointer data);
void handle_save_ok(GtkButton *widget, gpointer data);
void handle_add_ok(GtkButton *widget, gpointer data);
#ifdef HAVE_NFTW
void handle_add_dir_ok(GtkButton *widget, gpointer data);
#endif
void handle_next(GtkButton *widget, gpointer data);
void handle_prev(GtkButton *widget, gpointer data);
void set_mini(void);
void handle_minitoggle(GtkToggleButton *widget, gpointer data);
void handle_randomtoggle(GtkToggleButton *widget, gpointer data);
void handle_restart(GtkButton *widget, gpointer data);
void handle_stop(GtkButton *widget, gpointer data);
void handle_pause(GtkButton *widget, gpointer data);
gboolean handle_info_destroy(GtkWidget *widget, GdkEvent *event, 
			     gpointer data);
void handle_menu_info(GtkMenuItem *widget, gpointer data);
void handle_menu_up(GtkMenuItem *widget, gpointer data);
void handle_menu_down(GtkMenuItem *widget, gpointer data);
void handle_menu_delete(GtkMenuItem *widget, gpointer data);
void handle_menu_user(GtkMenuItem *widget, gpointer data);
void get_playlist_dir(gchar *buf, int buflen);
void handle_menu_load(GtkMenuItem *widget, gpointer data);
void handle_menu_save(GtkMenuItem *widget, gpointer data);
void handle_menu_add(GtkMenuItem *widget, gpointer data);
#ifdef HAVE_NFTW
void handle_menu_add_dir(GtkMenuItem *widget, gpointer data);
#endif
void handle_menu_sort_name(GtkMenuItem *widget, gpointer data);
void handle_menu_sort_swapped_name(GtkMenuItem *widget, gpointer data);
void handle_menu_sort_mtime(GtkMenuItem *widget, gpointer data);
void handle_menu_clear(GtkMenuItem *widget, gpointer data);
void handle_menu_prefs(GtkMenuItem *widget, gpointer data);
void handle_menu_quit(GtkMenuItem *widget, gpointer data);
gboolean handle_destroy(GtkWidget *widget, GdkEvent *event, gpointer data);
gboolean handle_configure(GtkWidget *widget, GdkEventConfigure *event,
			  gpointer data);
void sigchld_backend(gpointer data, gint source, GdkInputCondition cond);
void handle_sigchld(int dummy);
void handle_dnd_drop(GtkWidget *widget, GdkDragContext *context,
		     gint x, gint y,
		     GtkSelectionData *data, guint info,
		     guint time, gpointer user_data);
gint handle_prefs_delete_event(GtkWidget *widget, GdkEvent *event,
			       gpointer data);
void handle_prefs_destroy(GtkWidget *widget, gpointer data);
void print_version(void);
int main(int argc, char **argv);

/* --- Compatibility stuff --- */

#ifndef HAVE_SNPRINTF
/* If the user doesn't have snprintf, use sprintf instead. 
   This is a GNU cpp extension. */
#define snprintf(s, l, f, a...) sprintf(s, f, ##a)
#endif

#ifdef NOSTRCASECMP
/* The system doesn't provide strcasecmp, so we will instead. */
int strcasecmp(char *a, char *b);
#endif

#endif /* _XHIPPO_H */
