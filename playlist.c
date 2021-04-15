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
static const char *find_swap_point(const char *a);
void sort_as_appropriate(void);
#ifdef HAVE_LIBID3TAG
static id3_latin1_t *get_id3tag_field(struct id3_tag *tag, char *id);
#endif
#ifdef HAVE_NFTW
static int add_directory_callback(const char *name, const struct stat *sb,
				  int flag, struct FTW *ftw);
#endif
static void kill_child(int signal);
#ifdef USEGTK2
static gboolean iter_from_position(gint item, GtkTreeIter *iter);
#endif
static void freeze_list();
static void thaw_list();
static void select_list_row(gint item);
static void clear_list();
static void remove_list_row(gint item);
static void insert_list_row(songinfo *sinfo, gint item);

/* Globals. */
pid_t childpid = 0;
guint contextid, 
  listcount = 0, 
  playing = 0, 
  paused = 0;
gint last_played = -1,
  last_row_hit = -1,
  last_row_clicked = -1;
fileaction *last_action = NULL;

#ifdef USEGTK2
/* Get a GtkTreeIter pointing at a given row in the list. */
static gboolean iter_from_position(gint item, GtkTreeIter *iter) {
  if (item < 0) return FALSE;
  return gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(filestore), iter,
				       NULL, item);
}
#endif

/* Freeze the song list. */
static void freeze_list() {
#ifdef USEGTK2
  /* TreeViews don't appear to support freezing/thawing. */
#else
  gtk_clist_freeze(GTK_CLIST(filelist));
#endif
}

/* Thaw the song list. */
static void thaw_list() {
#ifndef USEGTK2
  gtk_clist_thaw(GTK_CLIST(filelist));
#endif
}

/* Select a given row in the list and scroll to make it visible if
   necessary. */
static void select_list_row(gint item) {
#ifdef USEGTK2
  GtkTreeIter iter;
  GtkTreeSelection *selection;

  if (!iter_from_position(item, &iter)) return;
  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(filelist));
  gtk_tree_selection_select_iter(selection, &iter);
#else
  gtk_clist_select_row(GTK_CLIST(filelist), item, 0);
#endif

  if (mode_scroll_catchup && GTK_WIDGET_REALIZED(filelist)) {
#ifdef USEGTK2
    GdkRectangle row, vis;
    GtkTreePath *path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, item);
    
    gtk_tree_view_get_cell_area(GTK_TREE_VIEW(filelist), path, NULL, &row);
    gtk_tree_view_widget_to_tree_coords(GTK_TREE_VIEW(filelist),
					row.x, row.y, &row.x, &row.y);
    gtk_tree_view_get_visible_rect(GTK_TREE_VIEW(filelist), &vis);
    
    if ((row.y < vis.y)
	|| (row.y + row.height) >= (vis.y + vis.height)) {
      gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(filelist), path, NULL,
				   TRUE, 0.5, 0.0);
    }
#else
    if (gtk_clist_row_is_visible(GTK_CLIST(filelist), item)
	!= GTK_VISIBILITY_FULL) {
      gtk_clist_moveto(GTK_CLIST(filelist), item, 0, 0.5, 0.0);
    }
#endif
  }
}

/* Clear the list. */
static void clear_list() {
#ifdef USEGTK2
  GtkTreeIter iter;

  /* Destroy all the list's songinfo structures. */
  if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(filestore), &iter)) {
    do {
      gpointer sinfo;

      gtk_tree_model_get(GTK_TREE_MODEL(filestore), &iter,
			 SONGINFO_COLUMN, &sinfo,
			 -1);
      destroy_songinfo(sinfo);
    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(filestore), &iter));
  }
  
  gtk_list_store_clear(GTK_LIST_STORE(filestore));
#else
  gtk_clist_clear(GTK_CLIST(filelist));
#endif
}

/* Remove a row from the list. */
static void remove_list_row(gint item) {
#ifdef USEGTK2
  GtkTreeIter iter;
  gpointer sinfo;

  if (!iter_from_position(item, &iter)) return;
  gtk_tree_model_get(GTK_TREE_MODEL(filestore), &iter,
		     SONGINFO_COLUMN, &sinfo,
		     -1);
  destroy_songinfo(sinfo);
  gtk_list_store_remove(GTK_LIST_STORE(filestore), &iter);
#else
  gtk_clist_remove(GTK_CLIST(filelist), item);
#endif
}

/* Insert a row in the list. */
static void insert_list_row(songinfo *sinfo, gint item) {
#ifdef USEGTK2
  GtkTreeIter iter;

  gtk_list_store_insert(GTK_LIST_STORE(filestore), &iter, item);
  gtk_list_store_set(GTK_LIST_STORE(filestore), &iter,
		     SONGINFO_COLUMN, sinfo,
		     TITLE_COLUMN, sinfo->display_name,
		     -1);
  /* Note: in the CList version, we add destroy_songinfo as a hook
     with the data. In the Tree version, we call it when rows are
     removed. */
#else
  gchar *row[1];
  
  row[0] = sinfo->display_name;
  gtk_clist_insert(GTK_CLIST(filelist), item, row);
  gtk_clist_set_row_data_full(GTK_CLIST(filelist), item, sinfo,
			      destroy_songinfo);
#endif
}

/* Send a signal to the child process (or process group). */
static void kill_child(int signal) {
  kill(last_action->setgroup ? -childpid : childpid, signal);
}

/* Synchronise the list to what's currently playing. */
void sync_list() {
  select_list_row(last_played);
}

/* Stop playing. */
void stop_playing() {
  if (!playing) return;
  playing = 0;

  if (!childpid) return;

  /* Wake up a paused player. */
  if (paused) {
    kill_child(SIGCONT);
    paused = 0;
  }

  kill_child(SIGTERM);

  /* Wait for the child to exit. */
  waitpid(childpid, NULL, 0);
  childpid = 0;
}

/* Toggle paused status. */
void pause_playing() {
  if (!playing) return;
  if (!childpid) return;

  if (paused) {
    kill_child(SIGCONT);
    paused = 0;
  } else {
    kill_child(SIGSTOP);
    paused = 1;
  }
}

/* Get the action for a given filename. Return NULL if a matching
   action isn't found. */
fileaction *get_action(const gchar *name) {
  const gchar *type;

  type = name + strlen(name);
  while ((type > name) && (*type != '.') && (*type != '/')) {
    --type;
  }

  if (*type == '.') {
    fileaction *fa;

    ++type;
    for (fa = fileactions; fa; fa = fa->next) {
      if (!strcasecmp(type, fa->extension)) return fa;
    }
  }
  return NULL;
}

/* Start playing song number num. Stop the old one first. */
void start_playing(gint num) {
  gchar *name, c;
  int argcount, i;
  int pipefd[2];

  if (num < 0 || num >= listcount) return;

  if (childpid) stop_playing();

  /* Get the extension of the filename. */
  name = get_songinfo(num)->name;
  last_action = get_action(name);
  if (!last_action) {
    playing = 0;
    status(_("Unable to guess type."));
    return;
  }
  
  last_played = num;
  playing = 1;
  paused = 0;
  get_songinfo(num)->played = 1;

  /* Update the interface. */
  select_list_row(num);
  update_window();

  /* Create a pipe to synchronise with the child. */
  if (pipe(pipefd) < 0) return;

  /* Fork off a child process to run the player in. */
  childpid = fork();
  if (childpid < 0) return;

  if (!childpid) {
    gchar *command, **argptr, *p;

    /* Before we do anything else, synchonise with the main process;
       if the player dies immediately it's possible for xhippo's
       sigchld handler to get called before the child's pid has even
       made it into childpid, which makes for interesting
       deadlocks. */
    read(pipefd[0], &c, 1);

    /* Build the argptr array for execvp. */
    command = strdup(last_action->action);
    argcount = 2;
    for (p = command; p; p = strchr(p, ' ')) {
      argcount++;
      p++;
    }
    argptr = (gchar **) malloc(argcount * sizeof(gchar *));
    i = 0;
    for (p = command; p; p = strchr(p, ' ')) {
      if (p != command) *p++ = '\0';
      argptr[i++] = p;
    }
    argptr[i++] = name;
    argptr[i++] = NULL;

    /* Redirect stdout and stderr to /dev/null if necessary. */
    if (mode_hide_player_output || last_action->nullinput) {
      int fd;

      fd = open("/dev/null", O_RDWR);
      if (last_action->nullinput) dup2(fd, 0); /* stdin */
      if (mode_hide_player_output) {
	dup2(fd, 1); /* stdout */
	dup2(fd, 2); /* stderr */
      }
      close(fd);
    }

    /* Set process group so that all processes in the group get
       killed. */
    if (last_action->setgroup) setpgid(0, 0);

    /* At this point we've still got command and argptr allocated. */
    execvp(argptr[0], argptr);
    fprintf(stderr, _("Unable to start player \"%s\".\n"), argptr[0]);
    execvp("echo", argptr);
    _exit(20);
  }

  /* Synchronise with the child. */
  write(pipefd[1], "!", 1);
  close(pipefd[0]);
  close(pipefd[1]);

  if (mode_write_playing) {
    gchar csname[STRBUFSIZE];
    FILE *f;

    /* Write the title of the song that's currently playing to the
       ~/.xhippo/current_song file. */
    snprintf(csname, STRBUFSIZE, "%s/.xhippo/current_song", 
	     getenv("HOME"));
    if ((f = fopen(csname, "w"))) {
      fprintf(f, "%s\n", get_songinfo(num)->display_name);
      fclose(f);
    }
  }

  if (mode_persist_frequently) persist_playlist();
}

/* Pick and play the previous song. */
void pick_prev() {
  int newitem = -1;
  
  if (listcount) {
    if ((!mode_play_ordered) && last_played != -1) {
      newitem = get_songinfo(last_played)->previous;
    }

    if (newitem == -1) {
      if (last_played > 0) {
	newitem = last_played - 1;
      } else {
	newitem = listcount - 1;
      }
    }
    start_playing(newitem);
  } else {
    status(_("No items to pick from!"));
  }
}

/* Pick and play a random song. */
void pick_next() {
  int newitem, first, i;

  if (listcount) {
    if (mode_play_ordered) {
      newitem = last_played + 1;
      if (newitem == listcount) {
	newitem = 0;
        if (mode_one_time) {
          stop_playing();
	  status(_("All items played."));
          return;
        }
      }
    } else {
      /* This is the random function recommended by the GNU rand()
	 manual page. */
      newitem = (int) (((1.0 * listcount) * rand()) / (RAND_MAX + 1.0));
      if (mode_must_play_all) {
	first = newitem;
	while (get_songinfo(newitem)->played) {
	  newitem++;
	  if (newitem==listcount) newitem=0;
	  if (newitem==first) {
	    for (i=0; i<listcount; i++) get_songinfo(i)->played = 0;
            if (mode_one_time) {
              stop_playing();
	      status(_("All items played."));
              return;
            }
	  }
	}
      }

      /* Fill in the previous entry on the last song played. */
      if (last_played >= 0 && last_played < listcount) {
	get_songinfo(newitem)->previous = last_played;
      }
    }
    start_playing(newitem);
  } else {
    status(_("No items to pick from!"));
  }
}

#ifdef HAVE_LIBID3TAG
static id3_latin1_t *get_id3tag_field(struct id3_tag *tag, char *id) {
  struct id3_frame const *frame;
  union id3_field const *field;
  id3_ucs4_t const *ucs4;

  frame = id3_tag_findframe(tag, id, 0);
  if (!frame) return NULL;
  field = &frame->fields[1];
  if (id3_field_getnstrings(field) == 0) return NULL;
  ucs4 = id3_field_getstrings(field, 0);
  if (!ucs4) return NULL;
  /* FIXME GTK2 expects UTF8. */
  return id3_ucs4_latin1duplicate(ucs4);
}
#endif

/* Return the "Title - Artist" form of a song's title from its ID3
   tag; return NULL if one can't be found. */
gchar *read_mp3_tag (const char *filename) {
  gchar *trackname = NULL;

#ifdef HAVE_LIBID3TAG
  /* Use libid3tag to read the tag. */
  struct id3_file *f;
  struct id3_tag *t;
  id3_latin1_t *title = NULL, *artist = NULL;

  f = id3_file_open(filename, ID3_FILE_MODE_READONLY);
  if (!f) return NULL;

  t = id3_file_tag(f);
  if (!t) goto out;

  title = get_id3tag_field(t, ID3_FRAME_TITLE);
  if (!title) goto out;
  artist = get_id3tag_field(t, ID3_FRAME_ARTIST);
  if (!artist) goto out;

  trackname = malloc(strlen((const char *) artist) + 3
                     + strlen((const char *) title) + 1);
  sprintf(trackname, "%s - %s", (const char *) artist, (const char *) title);

 out: 
  if (title) free(title);
  if (artist) free(artist);
  id3_file_close(f);
  return trackname;
#else
  /* Use our simple version. Routine to read the ID3 tag from an MP3
     file, originally supplied by Craig Knudsen. */
  int fd;
  gchar data[100];
  int ret;
  
  fd = open(filename, O_RDONLY);
  ret = lseek(fd, -128, SEEK_END);
  if (ret > 0) {
    ret = read(fd, data, 3);
    if (ret == 3) {
      data[3] = '\0';
      if (strcmp(data, "TAG") == 0) {
        ret = read(fd, data, 26);
        if (ret > 0) {
          trackname = strdup(data);
        }
      }
    }
  }
  close(fd);
  return trackname;
#endif /* HAVE_LIBID3TAG */
}

/* Fill in entries in a songinfo structure that need a stat first. Use
   the information in the provided struct stat if it's not NULL. */
void read_song_info(songinfo *sinfo, struct stat *st) {
  struct stat stnew;

  /* If we've already done this, then don't bother. */
  if (sinfo->inforead) return;

  if (!st) {
    st = &stnew;
    if (stat(sinfo->name, st) < 0) return;
  }
  sinfo->size = st->st_size;
  sinfo->mtime = st->st_mtime;
  sinfo->inforead = 1;
}

/* Called to add a new file to the playlist. */
void add_file(const gchar *initname) {
  gchar *name;
  const gchar *basename;
  gchar *dispname, *tag;
  songinfo *sinfo;
  struct stat st;

  /* Prepend the cwd if the name doesn't start with a slash. */
  if (initname[0] == '/') {
    name = strdup(initname);
  } else {
    name = malloc(strlen(startup_cwd) + strlen(initname) + 1);
    sprintf(name, "%s%s", startup_cwd, initname);
  }

  if (!mode_no_check_files) {
    /* Note: this uses stat rather than filetype() because we need the
       other information from the stat structure and this avoids
       having to stat every file in the playlist twice. */
    if (stat(name, &st) < 0) {
      g_print(_("Couldn't find \"%s\".\n"), name);
      return;
    }

    if (!S_ISREG(st.st_mode)) {
      g_print(_("\"%s\" isn't a regular file.\n"), name);
      return;
    }
  }

  if (mode_skip_path) {
    /* Skip the first mode_skip_path components of the name. */
    const gchar *e = name + strlen(name);
    int i = mode_skip_path;

    if (*name == '/') i++;
    for (basename = name; i > 0 && basename < e; basename++) {
      if (*basename == '/') i--;
    }
    if (basename == e) basename = name;
  } else {
    /* Take the basename. */
    basename = name + strlen(name);
    while((basename > name) && (*basename != '/')) --basename; 
  }

  tag = NULL;
  if (mode_read_id3_tags) tag = read_mp3_tag(name);
  if (tag) {
    dispname = strdup(tag);
  } else {
    gchar *p;

    if (basename[0] == '/') ++basename;
    dispname = strdup(basename);

    if (mode_demangle_names) {
      /* Replace underscores with spaces. */
      while ((p = strchr(dispname, '_'))) *p = ' ';

      /* Replace '%20's with spaces. */
      while ((p = strstr(dispname, "%20"))) {
	*p = ' ';
	p += 2;
	while (*(++p - 1)) *(p - 2) = *p;
      }
    }

    if (mode_strip_extension) {
      /* Strip off the extension. */
      p = dispname + strlen(dispname);
      while (p > dispname && *p != '.') --p;
      if (*p == '.') *p = '\0';
    }
  }

  /* Build a songinfo struct. */
  sinfo = malloc(sizeof(songinfo));
  sinfo->name = name;
  sinfo->display_name = dispname;
  sinfo->played = 0;
  sinfo->inforead = 0;
  sinfo->info_window = NULL;
  sinfo->previous = -1;
  if (!mode_no_check_files) read_song_info(sinfo, &st);

  insert_row(sinfo, listcount);

  if (tag) free(tag);
}

#ifdef HAVE_NFTW
static int add_directory_callback(const char *name, const struct stat *sb,
				  int flag, struct FTW *ftw) {

  /* Only add the file if a player can be found for it. */
  if (flag == FTW_F && get_action(name)) add_file(name);
  return 0;
}
#endif

/* Called to add all the playable files in a directory to a
   playlist. */
void add_directory(const gchar *name) {
#ifdef HAVE_NFTW
  freeze_list();

  /* We use an arbitrary maximum depth of 100, since nftw (at least in
     the glibc implementation) doesn't have an option for "unlimited
     depth". Specifying FTW_DEPTH without FTW_PHYS means that it won't
     get stuck in loops. */
  nftw(name, add_directory_callback, 100, FTW_DEPTH);

  sort_as_appropriate();
  thaw_list();
#else
  /* We don't have nftw; just don't do anything (for now). */
#endif
}

/* Clear the playlist. */
void clear_playlist() {
  stop_playing();
  clear_list();
  last_played = -1;
  update_window();
}

/* Sort the playlist as specified. */
void sort_as_appropriate() {
  switch (attribute_sort_on_load) {
  case SORT_NONE:
    /* do nothing */
    break;
  case SORT_NAME:
    sortplaylist(sort_compare_name, FALSE);
    break;
  case SORT_SWAPPED:
    sortplaylist(sort_compare_swapped_name, FALSE);
    break;
  case SORT_MTIME:
    sortplaylist(sort_compare_mtime, FALSE);
    break;
  }
}  

/* Sorting function for playlist entries by display name. */
int sort_compare_name(const void *a, const void *b) {
  return strcmp((*((songinfo **)a))->display_name,
		(*((songinfo **)b))->display_name);
}

/* Return the other half of the name in a string, or failing that the
   string itself. */
static const char *find_swap_point(const char *a) {
  const char *p = strstr(a, " - ");
  if (!p) return a;
  if (!*++p) return a;
  if (!*++p) return a;
  return p;
}

/* Sorting function for playlist entries by swapped display name. */
int sort_compare_swapped_name(const void *a, const void *b) {
  const char *na = (*((songinfo **)a))->display_name,
    *nb = (*((songinfo **)b))->display_name,
    *nas = find_swap_point(na),
    *nbs = find_swap_point(nb);
  int c = strcmp(nas, nbs);
  if (c != 0) return c;
  return strcmp(na, nb);
}

/* Sorting function for playlist entries by mtime. */
int sort_compare_mtime(const void *a, const void *b) {
  return (*((songinfo **)a))->mtime - (*((songinfo **)b))->mtime;
}

/* Sort the playlist by the display name using the specified function,
   and getting the song info first if required. */
void sortplaylist(int (*func)(const void *a, const void *b),
		  char needinfo) {
  songinfo **sl;
  gint i, lc = listcount;

  if (needinfo) {
    for (i=0; i<lc; i++) read_song_info(get_songinfo(i), NULL);
  }

  /* Copy the songinfo structs from the playlist into a new list,
     and then sort that. */
  sl = (songinfo **)malloc(lc * sizeof(songinfo *));
  for (i=0; i<lc; i++) sl[i] = copy_songinfo(get_songinfo(i));
  qsort(sl, lc, sizeof(songinfo *), func);
  
  freeze_list();
  clear_playlist();
  for (i=0; i<lc; i++) insert_row(sl[i], i);
  thaw_list();

  free(sl);
}

/* Write a playlist. Returns 1 on error, 0 on success. */
int write_playlist(const gchar *name) {
  int i;
  FILE *f = fopen(name, "w");

  if (!f) return 1;
  for (i=0; i<listcount; i++) fprintf(f, "%s\n", get_songinfo(i)->name);
  fclose(f);
  return 0;
}

/* Read a playlist. If use_cwd is 1, use the cwd as the dirname rather
   than the dirname of the playlist. */
void read_playlist(gchar *name, int use_cwd) {
  FILE *f;
  char line[STRBUFSIZE], fullname[STRBUFSIZE], dirname[STRBUFSIZE], 
    *slash, *filename;

  freeze_list();

  f = fopen(name, "r");
  if (!f) {
    g_print(_("Couldn't open playlist \"%s\"!\n"), name);
    return;
  }

  /* Work out the dirname if we've got one. */
  slash = strrchr(name, '/');
  if (use_cwd) {
    snprintf(dirname, STRBUFSIZE, "%s", startup_cwd);
  } else if (slash) {
    *slash = '\0';
    snprintf(dirname, STRBUFSIZE, "%s/", name);
    *slash = '/';
  } else {
    *dirname = '\0';
  }
  
  while (fgets(line, STRBUFSIZE, f)) {
    chomp(line);

    /* Work out the full filename including the extra path. */
    filename = (*line == '!') ? line + 1 : line;
    if (*filename != '/') {
      snprintf(fullname, STRBUFSIZE, "%s%s", dirname, filename);
    } else {
      snprintf(fullname, STRBUFSIZE, "%s", filename);
    }
    
    if (*line == '!') {
      /* Allow inclusion of a playlist from a playlist. */
      read_playlist(fullname, use_cwd);
    } else {
      add_file(fullname);
    }
  }

  fclose(f);

  sort_as_appropriate();
  
  /* Set the window title if required. We do this at the end of this
     function so that the name of the highest nested playlist is
     used. */
  if (mode_playlist_title) {
    gchar *s, *t, *prefix = "xhippo " VERSION ": ";

    if (mode_title_basename) {
      s = name + strlen(name);
      while (s > name && *s != '/') s--;
      if (*s == '/') s++;
    } else {
      s = name;
    }

    t = malloc(strlen(s) + strlen(prefix) + 1);
    sprintf(t, "%s%s", prefix, s);
    gtk_window_set_title(GTK_WINDOW(window), t);
    free(t);
  }

  thaw_list();
}

/* Copy a songinfo structure. */
songinfo *copy_songinfo(songinfo *src) {
  songinfo *dest = (songinfo *)malloc(sizeof(songinfo));

  dest->name = strdup(src->name);
  dest->display_name = strdup(src->display_name);
  dest->played = src->played;
  dest->inforead = src->inforead;
  dest->mtime = src->mtime;
  dest->size = src->size;
  dest->info_window = src->info_window;
  dest->previous = src->previous;

  return dest;
}

/* Destructor for songinfo, and therefore for the row as well. We can
   safely decrement the list count when this gets called. */
void destroy_songinfo(gpointer sinfo) {
  songinfo *s = (songinfo *)sinfo;
  free((gchar *) s->name);
  free((gchar *) s->display_name);
  if (s->info_window) gtk_widget_destroy(s->info_window);

  free(sinfo);
  --listcount;
}

/* Delete a row from the list. */
void delete_row(gint location) {
  int i;

  /* Adjust the current song number. We decrement this even if we just
     played the deleted song, because that makes non-random mode do
     sensible things when moving to the next song. */
  if (last_played >= location) --last_played;
  
  /* Adjust all the previous links to match the list. */
  for (i = 0; i < listcount; i++) {
    songinfo *s = get_songinfo(i);
    if (s->previous > location) --s->previous;
  }

  remove_list_row(location);
}

/* Insert a row into the list based on a filled-in songinfo structure. 
   Call as insert_row(X, listcount); to append to the end. */
void insert_row(songinfo *sinfo, gint location) {
  insert_list_row(sinfo, location);
  listcount++;
}

/* Get a pointer to a song's songinfo. */
songinfo *get_songinfo(gint number) {
#ifdef USEGTK2
  GtkTreeIter iter;
  gpointer sinfo;

  if (!iter_from_position(number, &iter)) return NULL;
  gtk_tree_model_get(GTK_TREE_MODEL(filestore), &iter,
		     SONGINFO_COLUMN, &sinfo,
		     -1);
  return (songinfo *)sinfo;
#else
  return (songinfo *)gtk_clist_get_row_data(GTK_CLIST(filelist), number);
#endif
}

/* Move a row from one location to another within the list. */
void move_row(gint src, gint dest) {
  songinfo *new;

  freeze_list();

  new = copy_songinfo(get_songinfo(src));
  remove_list_row(src);
  insert_row(new, dest);

  thaw_list();
}

/* Save the persistent playlist. */
void persist_playlist() {
  gchar buf[STRBUFSIZE];
  
  /* Save the playlist. */
  snprintf(buf, STRBUFSIZE, "%s/.xhippo/saved_playlist", getenv("HOME"));
  if (mode_persist_playlist) write_playlist(buf);
}
