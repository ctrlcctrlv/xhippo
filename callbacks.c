/*
   GNU xhippo: a GTK-based playlist manager.
   Copyright 1998, 1999, 2000, 2001, 2002, 2003, 2007 Adam Sampson <ats@offog.org>

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
static void add_info_window_row(GtkWidget *table, gint pos,
				const gchar *text1, const gchar *text2);
static void popup_songinfo_window(int song);

/* Handle button presses on various widgets. */
gboolean handle_button_press(GtkWidget *widget, GdkEventButton *event, 
			     gpointer data) {
  gint row = -1;

  if (widget == filelist) {
    /* The click was on the list. Get the clicked file. */

    GtkTreePath *path;

    if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(filelist),
				      event->x, event->y,
				      &path, NULL, NULL, NULL)) {
      gint *indices = gtk_tree_path_get_indices(path);
      row = indices[0];
      gtk_tree_path_free(path);
    }
  }

  if (event->button == 3) {
    /* Right-mouse-button click anywhere in the interface. */
    gint state;

    /* Work out which row the user was interested in, and whether to
       enable to menu items. */
    if (widget == filelist && row >= 0) {
      last_row_hit = row;
      state = 1;
    } else {
      last_row_hit = -1;
      state = 0;
    }

    gtk_widget_set_sensitive(GTK_WIDGET(info_item), state);
    gtk_widget_set_sensitive(GTK_WIDGET(up_item), state);
    gtk_widget_set_sensitive(GTK_WIDGET(down_item), state);
    gtk_widget_set_sensitive(GTK_WIDGET(delete_item), state);
    
    gtk_menu_popup(GTK_MENU(popupmenu), NULL, NULL, NULL, NULL, 
		   event->button, event->time);

    return TRUE;

  } else if (event->button == 1 && widget == eventbox) {
    /* Left click on the event box. */

    popup_songinfo_window(last_played);

    return TRUE;

  } else if (event->button == 1 && widget == filelist && row != -1) {
    /* Left click on a song. */

    if (last_row_clicked != row) start_playing(row);
    last_row_clicked = row;

    return TRUE;

  } else {
    /* Another button click; use the normal handlers. */

    return FALSE;
  }
}

/* Handle the "Cancel" button in the file selector. */
void handle_fileselector_cancel(GtkButton *widget, gpointer data) {
  gtk_widget_destroy(fileselector);
  fileselector = NULL;
}

/* Handle the "OK" button in the Load Playlist file selector. */
void handle_load_ok(GtkButton *widget, gpointer data) {
  const gchar *filename;
  gchar *buf;

  clear_playlist();
  filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fileselector));
  buf = g_strdup(filename);
  read_playlist(buf, 0);
  g_free(buf);
  gtk_widget_destroy(fileselector);
  fileselector = NULL;
}

/* Handle the "OK" button in the Save Playlist file selector. */
void handle_save_ok(GtkButton *widget, gpointer data) {
  const gchar *n;

  n = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fileselector));
  if (write_playlist(n)) status("Unable to save playlist.");
  
  gtk_widget_destroy(fileselector);
  fileselector = NULL;
}

/* Handle the "OK" button in the Add file selector. */
void handle_add_ok(GtkButton *widget, gpointer data) {
  add_file(gtk_file_selection_get_filename(GTK_FILE_SELECTION(fileselector)));
  gtk_widget_destroy(fileselector);
  fileselector = NULL;
}

#ifdef HAVE_NFTW
/* Handle the "OK" button in the Add Directory file selector. */
void handle_add_dir_ok(GtkButton *widget, gpointer data) {
  add_directory(gtk_file_selection_get_filename(GTK_FILE_SELECTION(fileselector)));
  gtk_widget_destroy(fileselector);
  fileselector = NULL;
}
#endif

/* Called when "Next" is pressed. */
void handle_next(GtkButton *widget, gpointer data) {
  pick_next();
}

/* Called when "Prev" is pressed. */
void handle_prev(GtkButton *widget, gpointer data) {
  pick_prev();
}

/* Called when attribute_mini has changed, to alter the window size. */
void set_mini() {
  if (attribute_mini) {
    gtk_widget_hide(list_box);
    gtk_window_resize(GTK_WINDOW(window), attribute_width, 1);
  } else {
    gtk_widget_show(list_box);
    sync_list();

    gtk_window_resize(GTK_WINDOW(window), attribute_width, attribute_height);
  }
}

/* Called when the "Mini" checkbox is toggled. */
void handle_minitoggle(GtkToggleButton *widget, gpointer data) {
  attribute_mini = 0;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(minicheckbox))) {
    attribute_mini = 1;
  }
  set_mini();
  update_window();
}

/* Called when the "Random" checkbox is toggled. */
void handle_randomtoggle(GtkToggleButton *widget, gpointer data) {
  mode_play_ordered = 1;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(randomcheckbox))) {
    mode_play_ordered = 0;
  }
}

/* Called when "Restart" is pressed. */
void handle_restart(GtkButton *widget, gpointer data) {
  start_playing(last_played);
  update_window();
}

/* Called when "Stop" is pressed. */
void handle_stop(GtkButton *widget, gpointer data) {
  stop_playing();
  update_window();
}

/* Called when "Pause" is pressed. */
void handle_pause(GtkButton *widget, gpointer data) {
  pause_playing();
  update_window();
}

/* Handle the "destroy" event on an info window. data points to the
   songinfo structure. */
gboolean handle_info_destroy(GtkWidget *widget, GdkEvent *event,
			     gpointer data) {
  /* Clear the pointer to the window. */
  ((songinfo *)data)->info_window = NULL;

  return FALSE;
}

/* Add a row to the info window. */
static void add_info_window_row(GtkWidget *table, gint pos,
				const gchar *text1, const gchar *text2) {
  GtkWidget *label, *text;

  label = gtk_label_new(text1);
  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
  gtk_table_attach(GTK_TABLE(table), label,
		   0, 1, pos, pos + 1,
		   GTK_FILL, GTK_FILL, 4, 0);
  gtk_widget_show(label);

  text = gtk_label_new(text2);
  gtk_label_set_justify(GTK_LABEL(text), GTK_JUSTIFY_LEFT);
  gtk_table_attach(GTK_TABLE(table), text,
		   1, 2, pos, pos + 1,
		   GTK_EXPAND, GTK_EXPAND, 4, 0);
  gtk_widget_show(text);
}

/* Handle "Info" in the popup menu. */
void handle_menu_info(GtkMenuItem *widget, gpointer data) {
  popup_songinfo_window(last_row_hit);
}

/* Pop up the song info window for a given song. */
static void popup_songinfo_window(int song) {
  songinfo *sinfo;
  GtkWidget *table;
  gchar buf[STRBUFSIZE], *p;

  sinfo = get_songinfo(song);
  if (!sinfo) return;

  /* If the window already exists, don't bother. */
  if (sinfo->info_window) return;

  /* Make sure we've got the information we need. */
  read_song_info(sinfo, NULL);

  sinfo->info_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(sinfo->info_window), _("Song information"));
  gtk_signal_connect(GTK_OBJECT(sinfo->info_window), "destroy",
		     GTK_SIGNAL_FUNC(handle_info_destroy), sinfo);

  table = gtk_table_new(4, 2, FALSE);
  gtk_container_add(GTK_CONTAINER(sinfo->info_window), table);

  add_info_window_row(table, 0, _("Name:"), sinfo->display_name);
  add_info_window_row(table, 1, _("Filename:"), sinfo->name);
  snprintf(buf, STRBUFSIZE, "%s", ctime(&sinfo->mtime));
  /* ctime appends a carriage return; strip it off. */
  while ((p = strrchr(buf, '\n'))) *p = '\0';
  add_info_window_row(table, 2, _("Last modified:"), buf);
  snprintf(buf, STRBUFSIZE, "%lu", sinfo->size);
  add_info_window_row(table, 3, _("File size:"), buf);
  gtk_widget_show(table);
  gtk_widget_show(sinfo->info_window);
}

void handle_menu_up(GtkMenuItem *widget, gpointer data) {
  if (last_row_hit > 0) move_row(last_row_hit, last_row_hit - 1);
}

void handle_menu_down(GtkMenuItem *widget, gpointer data) {
  if (last_row_hit < listcount - 1) move_row(last_row_hit, last_row_hit + 1);
}

void handle_menu_delete(GtkMenuItem *widget, gpointer data) {
  if (last_row_hit == last_played) stop_playing();
  delete_row(last_row_hit);
  update_window();
}

/* Handle user-supplied items in the menu. */
void handle_menu_user(GtkMenuItem *widget, gpointer data) {
  usercommand *uc = (usercommand *)data;
  gchar cmdbuf[STRBUFSIZE];

  snprintf(cmdbuf, STRBUFSIZE, uc->command,
	   last_row_hit == -1 ? "" : get_songinfo(last_row_hit)->name);
  system(cmdbuf);
}

/* Called when "Load" is pressed. */
void handle_menu_load(GtkMenuItem *widget, gpointer data) {
  gchar name[STRBUFSIZE];

  get_playlist_dir(name, STRBUFSIZE);
  if (fileselector) return;
  fileselector = gtk_file_selection_new(_("Load playlist"));
  gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileselector), name);
  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fileselector)
				->ok_button),
		     "clicked", (GtkSignalFunc) handle_load_ok,
		     fileselector);
  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fileselector)
				->cancel_button),
		     "clicked", (GtkSignalFunc) handle_fileselector_cancel,
		     fileselector);
  gtk_widget_show(fileselector);
}

void handle_menu_save(GtkMenuItem *widget, gpointer data) {
  gchar name[STRBUFSIZE];

  get_playlist_dir(name, STRBUFSIZE);
  if (fileselector) return;
  fileselector = gtk_file_selection_new(_("Save playlist"));
  gtk_file_selection_set_filename(GTK_FILE_SELECTION(fileselector), name);
  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fileselector)
				->ok_button),
		     "clicked", (GtkSignalFunc) handle_save_ok,
		     fileselector);
  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fileselector)
				->cancel_button),
		     "clicked", (GtkSignalFunc) handle_fileselector_cancel,
		     fileselector);
  gtk_widget_show(fileselector);
}

void handle_menu_add(GtkMenuItem *widget, gpointer data) {
  if (fileselector) return;
  fileselector = gtk_file_selection_new(_("Add song"));
  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fileselector)
				->ok_button),
		     "clicked", (GtkSignalFunc) handle_add_ok,
		     fileselector);
  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fileselector)
				->cancel_button),
		     "clicked", (GtkSignalFunc) handle_fileselector_cancel,
		     fileselector);
  gtk_widget_show(fileselector);
}

#ifdef HAVE_NFTW
void handle_menu_add_dir(GtkMenuItem *widget, gpointer data) {
  if (fileselector) return;
  fileselector = gtk_file_selection_new(_("Add directory"));
  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fileselector)
				->ok_button),
		     "clicked", (GtkSignalFunc) handle_add_dir_ok,
		     fileselector);
  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fileselector)
				->cancel_button),
		     "clicked", (GtkSignalFunc) handle_fileselector_cancel,
		     fileselector);
  gtk_widget_show(fileselector);
}
#endif

/* Handlers for the sort menu items. */
void handle_menu_sort_name(GtkMenuItem *widget, gpointer data) {
  sortplaylist(sort_compare_name, FALSE);
}
void handle_menu_sort_swapped_name(GtkMenuItem *widget, gpointer data) {
  sortplaylist(sort_compare_swapped_name, FALSE);
}
void handle_menu_sort_mtime(GtkMenuItem *widget, gpointer data) {
  sortplaylist(sort_compare_mtime, TRUE);
}

void handle_menu_clear(GtkMenuItem *widget, gpointer data) {
  clear_playlist();
}

void handle_menu_prefs(GtkMenuItem *widget, gpointer data) {
  show_prefs_window();
}

void handle_menu_quit(GtkMenuItem *widget, gpointer data) {
  gtk_widget_destroy(GTK_WIDGET(window));
}

/* Handle the "destroy" event. */
gboolean handle_destroy(GtkWidget *widget, GdkEvent *event, gpointer data) {
  /* Save the window position, if necessary. */
  if (mode_save_window_pos) save_window_state();

  /* Stop the player. */
  stop_playing();

  persist_playlist();

  /* Then quit the interface. */
  gtk_main_quit();

  return FALSE;
}

/* Handle "configure" events (moves & resizes) of the main window. */
gboolean handle_configure(GtkWidget *widget, GdkEventConfigure *event,
			  gpointer data) {
  gtk_window_get_position(GTK_WINDOW(widget),
			  &attribute_xpos, &attribute_ypos);

  /* Don't bother if we're making the window small */
  if (attribute_mini) return FALSE;

  gtk_window_get_size(GTK_WINDOW(widget),
		      &attribute_width, &attribute_height);
  return FALSE;
}

/* Handle files dropped on our window. */
void handle_dnd_drop(GtkWidget *widget, GdkDragContext *context,
		     gint x, gint y,
		     GtkSelectionData *data, guint info,
		     guint time, gpointer user_data) {
  if (data->data) {
    /* The format of data->data is a list of URIs, seperated by
       and terminating in \r\n. We're only interested in the file:
       URIs.

       The URIs can look like "file:///home/azz/x" (from ROX-Filer in
       non-hostname mode), "file://myhost.org/home/azz/x" (from
       ROX-Filer in hostname mode), or "file:/home/azz/x" (from
       Konqueror). */
    gchar *s = strdup((const char *) data->data), *p, *q;

    p = s;
    while ((q = strchr(p, '\r'))) {
      char *file;

      *q = '\0';
      if (!(*++q == '\n')) break;
      *q = '\0';

      file = NULL;
      if (strncmp(p, "file://", 7) == 0) {
	file = strchr(p + 7, '/');
      } else if (strncmp(p, "file:", 5) == 0) {
	file = p + 5;
      }

      if (file != NULL) {
	if (filetype(file) == 2) {
	  add_directory(file);
	} else {
	  add_file(file);
	}
      }
      p = q + 1;
    }
    
    free(s);
  }   
}

/* Handle the "delete_event" event on the prefs window. */
gint handle_prefs_delete_event(GtkWidget *widget, GdkEvent *event,
			       gpointer data) {
  /* Close the window. */
  return(FALSE);
}

/* Handle the "destroy" event on the prefs window. */
void handle_prefs_destroy(GtkWidget *widget, gpointer data) {
  prefs_window = NULL;
}

