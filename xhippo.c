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

#include "xhippo.h"

/* Prototypes for static functions. */
typedef void (*ButtonFunc)(GtkButton *, gpointer);
static GtkWidget *add_button(const gchar *label, const gchar *tip,
			     ButtonFunc callback, GtkWidget *container,
			     const gchar *name, gint accel);
typedef void (*MenuFunc)(GtkMenuItem *, gpointer);
static GtkWidget *add_menu_item(const gchar *label, MenuFunc callback,
				gint accel);
static void add_menu_separator(void);

/* Globals. */
GtkTooltips *tooltips;
GtkWidget *window, *restartbutton, *stopbutton, *buttons_box, 
  *all_box, *savebutton, *sortbutton, *addbutton,
  *filelist, *statusbar, *nextbutton, *prevbutton, *pausebutton, *loadbutton,
  *minicheckbox, *randomcheckbox, *fileselector = NULL, *list_box,
  *scrollbar, *upbutton, *downbutton, *deletebutton, *popupmenu,
  *up_item, *down_item, *delete_item, *info_item, *eventbox,
  *prefs_window = NULL, *prefs_all_box;
GtkListStore *filestore;
GtkAdjustment *vadj;
GtkAccelGroup *accel_group;
int sigchld_pipe[2];
gchar startup_cwd[STRBUFSIZE];

/* Show a message on the status bar */
void status(const char *message) {
  gtk_statusbar_pop(GTK_STATUSBAR(statusbar), contextid);
  gtk_statusbar_push(GTK_STATUSBAR(statusbar), contextid, message);
}

/* Update the status bar. Called when we play a new song or we change
   in or out of mini mode. */
void update_window() {
  gchar statbuf[STRBUFSIZE];

  if (paused) {
    snprintf(statbuf, STRBUFSIZE, "%s", _("Paused"));
  } else if (playing) {
    if (attribute_mini) {
      /* Put the name of the song playing in the status bar. */
      snprintf(statbuf, STRBUFSIZE, "%s",
	       get_songinfo(last_played)->display_name);
    } else {
      /* Put the "Playing with" string in the status bar. */
      if (mode_show_pid) {
	snprintf(statbuf, STRBUFSIZE, _("Playing with %s (pid %d)"),
		 last_action->name, childpid);
      } else {
	snprintf(statbuf, STRBUFSIZE, _("Playing with %s."),
		 last_action->name);
      }
    }
  } else {
    snprintf(statbuf, STRBUFSIZE, _("Stopped."));
  }

  status(statbuf);
}

/* A timeout function to be called after we've had a SIGCHLD. This
   does stuff that we can't do in the signal handler. */
void sigchld_backend(gpointer data, gint source, GdkInputCondition cond) {
  gchar c;
  pid_t deadpid;

  /* Read the character off the pipe. */
  read(sigchld_pipe[0], &c, 1);

  /* If the child that has exited isn't the player process, we're not
     interested. */
  deadpid = waitpid(-1, NULL, WNOHANG);
  if (deadpid < 1 || deadpid != childpid) return;
  childpid = 0;

  if (playing) {
    if (mode_delete_when_played) delete_row(last_played);
    pick_next();
  }
}

/* Called when the child process quits. */
void handle_sigchld(int dummy) {
  /* Leave it to GDK to find out about. */
  write(sigchld_pipe[1], "!", 1);
}

static GtkTargetEntry target_table[] = {
  { "text/uri-list", 0, 0 },
};

void print_version() {
  g_print("GNU xhippo %s\n", VERSION);
  g_print("Copyright (c) 1998-2000 Adam Sampson\n"
"GNU xhippo comes with NO WARRANTY, to the extent permitted by law.\n"
"You may redistribute copies of GNU xhippo under the terms of the\n"
"GNU General Public License. For more information about these matters,\n"
"see the file named COPYING.\n");
}

static GtkWidget *add_button(const gchar *label, const gchar *tip,
			     ButtonFunc callback, GtkWidget *container,
			     const gchar *name, gint accel) {
  GtkWidget *new;

  /* Create and bind the Load button. */
  new = gtk_button_new_with_label(label);
  gtk_signal_connect(GTK_OBJECT(new), "clicked",
		     GTK_SIGNAL_FUNC(callback), NULL);
  gtk_box_pack_start(GTK_BOX(container), new, TRUE, TRUE, 0);
  gtk_tooltips_set_tip(tooltips, new, tip, NULL);
  if (accel) {
    gtk_widget_add_accelerator(new, "clicked", accel_group,
			       accel, 0, GTK_ACCEL_VISIBLE);
  }
  gtk_widget_set_name(new, name);
  gtk_widget_show(new);

  return new;
}

static GtkWidget *add_menu_item(const gchar *label, MenuFunc callback,
				gint accel) {
  GtkWidget *item;

  item = gtk_menu_item_new_with_label(label);
  gtk_menu_append(GTK_MENU(popupmenu), item);
  gtk_signal_connect_object(GTK_OBJECT(item), "activate",
			    GTK_SIGNAL_FUNC(callback), NULL);
  if (accel) {
    gtk_widget_add_accelerator(item, "activate", accel_group,
                               accel, 0, GTK_ACCEL_VISIBLE);
  }
  gtk_widget_show(item);

  return item;
}

static void add_menu_separator(void) {
  GtkWidget *item;

  item = gtk_menu_item_new();
  gtk_menu_append(GTK_MENU(popupmenu), item);
  gtk_widget_show(item);
}

int main(int argc, char **argv) {
  struct sigaction act;
  gchar buf[STRBUFSIZE], wintitlestring[STRBUFSIZE];
  int c, j; /* for getopt */

  /* Set up gettext. */
  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);

  /* Check that HOME is available. */
  if (!getenv("HOME")) {
    fprintf(stderr, _("You must set $HOME to your home directory.\n"));
    return 20;
  }

  /* Get the CWD. */
  if (getcwd(startup_cwd, STRBUFSIZE - 1)) {
    /* We left 1 space above for the /. */
    strcat(startup_cwd, "/");
  } else {
    /* Leave it blank; prepending it to paths won't do any damage then. */
    *startup_cwd = '\0';
  }

  /* Set up the pipe used for the SIGCHLD handler. */
  pipe(sigchld_pipe);

  /* Seed RNG. */
  srand(time(0));

  /* Handle arguments. */
#ifdef USEGNOME
  gnome_init("xhippo", VERSION, argc, argv);
#else
  gtk_init(&argc, &argv);
#endif

  /* Read the gtkrc files. */
  snprintf(buf, STRBUFSIZE, "%s/.xhippo/gtkrc", getenv("HOME"));
  gtk_rc_parse(buf);
  gtk_rc_parse("xhippo.gtkrc");

  /* Set our response to SIGCHLD. */
  act.sa_handler = handle_sigchld;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_NOCLDSTOP;
  sigaction(SIGCHLD, &act, NULL);

  /* Create the acceleration group. */
  accel_group = gtk_accel_group_new();

  /* Create the tooltips object. */
  tooltips = gtk_tooltips_new();

  /* Create a window. */
  snprintf(wintitlestring, STRBUFSIZE, "xhippo %s", VERSION);
#ifdef USEGNOME
  /* In GNOME mode, window is actually a GNOME app, but the
     differences in use are very little. */
  window = gnome_app_new("xhippo", wintitlestring);
  gtk_widget_realize(window);
#else
  /* Standard GTK initialisation. */
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), wintitlestring);
#endif

  /* With GTK1, mini mode only works when shrinking is enabled. With
     GTK2, it only works when shrinking is disabled. Go figure. */
  gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, TRUE);

  gtk_signal_connect(GTK_OBJECT(window), "destroy",
		     GTK_SIGNAL_FUNC(handle_destroy), NULL);
  gtk_signal_connect(GTK_OBJECT(window), "configure_event",
		     GTK_SIGNAL_FUNC(handle_configure), NULL);

  /* Listen to drag-and-drop. */
  gtk_signal_connect(GTK_OBJECT(window), "drag_data_received",
		     GTK_SIGNAL_FUNC(handle_dnd_drop), NULL);
  gtk_drag_dest_set(window,
		    GTK_DEST_DEFAULT_MOTION |
		    GTK_DEST_DEFAULT_HIGHLIGHT |
		    GTK_DEST_DEFAULT_DROP,
		    target_table, 1,
		    GDK_ACTION_COPY | GDK_ACTION_MOVE);

  /* Create the container for everything. */
  all_box = gtk_vbox_new(FALSE, 0);
#ifdef USEGNOME
  gnome_app_set_contents(GNOME_APP(window), all_box);
#else
  gtk_container_add(GTK_CONTAINER(window), all_box);
#endif
  gtk_widget_set_name(all_box, "all_box");

  /* Create the event box for the status bar. */
  eventbox = gtk_event_box_new();
  gtk_signal_connect(GTK_OBJECT(eventbox), "button_press_event",
		     GTK_SIGNAL_FUNC(handle_button_press), NULL);
  gtk_box_pack_start(GTK_BOX(all_box), eventbox, FALSE, FALSE, 0);

  /* Create the status bar. */
  statusbar = gtk_statusbar_new();
  gtk_statusbar_set_has_resize_grip(GTK_STATUSBAR(statusbar), FALSE);
  gtk_container_add (GTK_CONTAINER(eventbox), statusbar);
  contextid = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar), "xhippo");
  gtk_statusbar_push(GTK_STATUSBAR(statusbar), contextid,
		     _("Welcome to xhippo!"));
  gtk_widget_set_name(statusbar, "statusbar");

  gtk_widget_show(statusbar);
  gtk_widget_show(eventbox);

  /* Create the upper buttons container. */
  buttons_box = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(all_box), buttons_box, FALSE, FALSE, 0);
  gtk_widget_set_name(buttons_box, "buttons_box");

  /* Create the buttons and checkboxes in the upper bar. */
  restartbutton = add_button(_("Restart"), _("Restart the current song"), 
                             handle_restart, buttons_box, "restartbutton", 0);
  gtk_widget_add_accelerator(restartbutton, "clicked", accel_group,
                             GDK_r, 0, GTK_ACCEL_VISIBLE);

  stopbutton = add_button(_("Stop"), _("Stop playing"), handle_stop,
			  buttons_box, "stopbutton", GDK_KP_Divide);
  gtk_widget_add_accelerator(stopbutton, "clicked", accel_group,
                             GDK_s, 0, GTK_ACCEL_VISIBLE);

  pausebutton = add_button(_("Pause"), _("Pause playing"), handle_pause,
			   buttons_box, "pausebutton", GDK_KP_Add);
  gtk_widget_add_accelerator(pausebutton, "clicked", accel_group,
                             GDK_p, 0, GTK_ACCEL_VISIBLE);

  prevbutton = add_button(_("Prev"), _("Play the previous song"), handle_prev,
			  buttons_box, "nextbutton", GDK_b);
  
  nextbutton = add_button(_("Next"), _("Play the next song"), handle_next,
			  buttons_box, "nextbutton", GDK_KP_Subtract);
  gtk_widget_add_accelerator(nextbutton, "clicked", accel_group,
			     GDK_KP_Multiply, 0, GTK_ACCEL_VISIBLE);
  gtk_widget_add_accelerator(nextbutton, "clicked", accel_group,
                             GDK_n, 0, GTK_ACCEL_VISIBLE);
  
  minicheckbox = gtk_check_button_new_with_label(_("Mini"));
  gtk_signal_connect(GTK_OBJECT(minicheckbox), "toggled",
		     GTK_SIGNAL_FUNC(handle_minitoggle), NULL);
  gtk_widget_set_name(minicheckbox, "minicheckbox");
  gtk_box_pack_start(GTK_BOX(buttons_box), minicheckbox, FALSE, FALSE, 0);
  gtk_widget_add_accelerator(minicheckbox, "clicked", accel_group,
                             GDK_quoteleft, 0, GTK_ACCEL_VISIBLE);
  gtk_widget_show(minicheckbox);

  randomcheckbox = gtk_check_button_new_with_label(_("Random"));
  gtk_signal_connect(GTK_OBJECT(randomcheckbox), "toggled",
		     GTK_SIGNAL_FUNC(handle_randomtoggle), NULL);
  gtk_widget_set_name(randomcheckbox, "randomcheckbox");
  gtk_box_pack_start(GTK_BOX(buttons_box), randomcheckbox, FALSE, FALSE, 0);
  gtk_widget_add_accelerator(randomcheckbox, "clicked", accel_group,
                             GDK_h, 0, GTK_ACCEL_VISIBLE);
  gtk_widget_show(randomcheckbox);

  /* Show the buttons boxes. */
  gtk_widget_show(buttons_box);

  /* Create an hbox into which the list and its scrollbar fit. */
  list_box = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(all_box), list_box, TRUE, TRUE, 0);
  gtk_widget_set_name(list_box, "list_box");

  /* Create the popup menu. */
  popupmenu = gtk_menu_new();
  info_item = add_menu_item(_("Song information"), handle_menu_info, 0);
  up_item = add_menu_item(_("Move song up"), handle_menu_up, 0);
  down_item = add_menu_item(_("Move song down"), handle_menu_down, 0);
  delete_item = add_menu_item(_("Remove song"), handle_menu_delete, 0);
  add_menu_separator();
  add_menu_item(_("Add song"), handle_menu_add, GDK_a);
#ifdef HAVE_NFTW
  add_menu_item(_("Add directory"), handle_menu_add_dir, GDK_d);
#endif
  add_menu_separator();
  add_menu_item(_("Load playlist"), handle_menu_load, GDK_l);
  add_menu_item(_("Save playlist"), handle_menu_save, GDK_v);
  add_menu_item(_("Sort playlist by name"), handle_menu_sort_name, GDK_o);
  add_menu_item(_("Sort playlist by swapped name"),
                handle_menu_sort_swapped_name, GDK_w);
  add_menu_item(_("Sort playlist by mtime"), handle_menu_sort_mtime, GDK_t);
  add_menu_item(_("Clear playlist"), handle_menu_clear, GDK_c);
  add_menu_separator();
#ifdef USEPREFS
  add_menu_item(_("Preferences"), handle_menu_prefs, 0);
#endif
  add_menu_item(_("Quit"), handle_menu_quit, GDK_q);
  
  /* Initialise the config system. */
  init_config_commands();

  /* Read the config file for the first time. */
  read_config_files(0);

  /* If the user specified any user menu items, add them to the menu. */
  if (usercommands) {
    usercommand *uc;
    gint acceltab[] = { GDK_1, GDK_2, GDK_3, GDK_4, GDK_5,
			GDK_6, GDK_7, GDK_8, GDK_9, GDK_0, -1 };
    gint *accels = acceltab;
    
    add_menu_separator();
    for (uc = usercommands; uc; uc = uc->next) {
      GtkWidget *item;

      item = gtk_menu_item_new_with_label(uc->description);
      gtk_menu_append(GTK_MENU(popupmenu), item);
      gtk_signal_connect_object(GTK_OBJECT(item), "activate",
				GTK_SIGNAL_FUNC(handle_menu_user),
				(gpointer)uc);
      
      /* Assign an accelerator if we haven't used them all up. */
      if (*accels != -1) {
	gtk_widget_add_accelerator(item, "activate", accel_group,
				   *accels++, 0, GTK_ACCEL_VISIBLE);
      }

      gtk_widget_show(item);
    }
  }

  /* Parse arguments. */
  while (1) {
    char *argstring = "aslmpiwtVhoqOd1TbcWk:fDxPg";
#ifndef NOGETOPTLONG
    static struct option long_options[] = {
      {"autostart",0,0,0}, /* -a */
      {"scroll",0,0,0}, /* -s */
      {"leftscroll",0,0,0}, /* -l */
      {"mustplayall",0,0,0}, /* -m */
      {"showpid",0,0,0}, /* -p */
      {"readid3",0,0,0}, /* -i */
      {"savewinstate",0,0,0}, /* -w */
      {"startmini",0,0,0}, /* -t */
      {"version",0,0,0}, /* -V */
      {"help",0,0,0}, /* -h */
      {"ordered",0,0,0}, /* -o */
      {"stripextension",0,0,0}, /* -S */
      {"hideplayeroutput",0,0,0}, /* -q */
      {"sortonload",0,0,0}, /* -O */
      {"onetime",0,0,0}, /* -1 */
      {"playlisttitle",0,0,0}, /* -T */
      {"titlebasename",0,0,0}, /* -b */
      {"nocheckfiles",0,0,0}, /* -c */
      {"writeplaying",0,0,0}, /* -W */
      {"skippath", required_argument, 0, 0}, /* -k */
      {"commandlinesongs",0,0,0}, /* -f */
      {"commandlinedirs",0,0,0}, /* -D */
      {"deletewhenplayed",0,0,0}, /* -x */
      {"persistplaylist",0,0,0}, /* -P */
      {"commandlineguess",0,0,0}, /* -g */
      {0,0,0,0}
    };
    j = 0;
    c = getopt_long(argc, argv, argstring, long_options, &j);
#else
    c = getopt(argc, argv, argstring);
#endif
    if (c == -1) break; /* out of the while; done with the args */

#ifndef NOGETOPTLONG
    /* A long option; look it up in the arguments string. */
    if (c == 0) {
      char *p = argstring;
      while (j--) {
	p++;
	if (*p == ':') p++;
      }
      c = *p;
    }
#endif

    switch(c) {
    case 'a': /* -a: automatic play on start */
      mode_play_on_start = 1;
      break;
    case 's': /* -s: scroll catchup */
      mode_scroll_catchup = 1;
      break;
    case 'l': /* -l: left scrollbar */
      mode_left_scroll = 1;
      break;
    case 'm': /* -m: must play all */
      mode_must_play_all = 1;
      break;
    case 'p': /* -p: show PIDs */
      mode_show_pid = 1;
      break;
    case 'i': /* -i: read ID3 tags */
      mode_read_id3_tags = 1;
      break;
    case 'w': /* -w: save window position */
      mode_save_window_pos = 1;
      break;
    case 't': /* -t: start minified */
      mode_start_mini = 1;
      break;
    case 'S': /* -S: strip extension */
      mode_strip_extension = 1;
      break;
    case 'o': /* -o: play ordered */
      mode_play_ordered = 1;
      break;
    case 'q': /* -q: hide player output */
      mode_hide_player_output = 1;
      break;
    case 'O': /* -O: sort on load */
      attribute_sort_on_load = SORT_NAME;
      break;
    case 'd': /* -d: demangle names */
      mode_demangle_names = 1;
      break;
    case 'T': /* -T: playlist title */
      mode_playlist_title = 1;
      break;
    case 'b': /* -b: playlist title basename */
      mode_title_basename = 1;
      break;
    case 'c': /* -c: don't bother stat()ing files on startup */
      mode_no_check_files = 1;
      break;
    case 'W': /* -w: write playing song to a file */
      mode_write_playing = 1;
      break;
    case '1': /* -1: one time */
      mode_one_time = 1;
      break;
    case 'k': /* -k: skip path */
      mode_skip_path = atoi(optarg);
      break;
    case 'f': /* -f: songs (rather than playlist) on command line */
      mode_commandline_songs = 1;
      break;
    case 'D': /* -d: directories (rather than playlists) on command line */
      mode_commandline_dirs = 1;
      break;
    case 'x': /* -x: delete when played */
      mode_delete_when_played = 1;
      break;
    case 'P': /* -P: persist playlist */
      mode_persist_playlist = 1;
      break;
    case 'g': /* -g: guess command line arguments */
      mode_commandline_guess = 1;
      break;
    case 'V':
      print_version();
      return 0;
    case 'h':
      print_version();
      g_print("\nUsage: xhippo [options] [playlist [playlist ...]]\n"
"or:    xhippo -f [options] [song [song ...]]\n"
"-a  --autostart    Start playing automatically once the playlists have\n"
"                   been loaded.\n"
"-s  --scroll       Automatically scroll the list when a random song\n"
"                   is picked.\n"
"-l  --leftscroll   Place scrollbar on the left side of the window.\n"
"-m  --mustplayall  When picking a random song, pick one that has not\n"
"                   already been played if possible.\n"
"-1  --onetime      Stop once the entire list has been played.\n"
"-p  --showpid      Show the PIDs of player processes in the status bar.\n"
"-i  --readid3      Attempt to read ID3 tags from files and use the titles\n"
"                   stored in the list.\n"
"-S  --stripextension\n"
"                   Strip extensions from files shown in the playlist.\n"
"-d  --demanglenames\n"
"                   Replace underscores and %%20s in names with spaces.\n"
"-w  --savewinstate Save the window position and state upon exit to\n"
"                   $HOME/.xhippo/winstate.\n"
"-T  --playlisttitle\n"
"                   Make the window title the name of the loaded playlist.\n"
"-b  --titlebasename\n"
"                   If -T is set, use the basename of the playlist.\n"
"-t  --startmini    Force xhippo to start with its window in the \"mini\"\n"
"                   state (with the list hidden).\n"
"-q  --hideplayeroutput\n"
"                   Redirect player stdout and stderr to /dev/null.\n"
"-O  --sortonload   Sort the playlist by name immediately after loading it.\n"
"-W  --writeplaying Write the name of the currently-playing song to\n"
"                   $HOME/.xhippo/current_song.\n"
"-V  --version      Print xhippo's version and exit.\n"
"-c  --nocheckfiles Don't bother checking whether the files in the playlist\n"
"                   actually exist.\n"
"-o  --ordered      Play the files in the order specified in the playlist,\n"
"                   rather than in random order.\n"
"-k  --skippath <n> Skip <n> elements of the path of files and display the rest.\n"
"-f  --commandlinesongs\n"
"                   Take the non-option arguments as songs to add to the\n"
"                   playlist rather than playlists to load.\n"
"-D  --commandlinedirs\n"
"                   Take the non-option arguments as directories to search\n"
"                   for files to add to the playlist rather than playlists\n"
"                   to load.\n"
"-x  --deletewhenplayed\n"
"                   Delete played songs from the playlist.\n"
"-P  --persistplaylist\n"
"                   Automatically load and save the current playlist.\n"
"-g  --commandlineguess\n"
"                   Attempt to guess based on the config settings whether\n"
"                   non-option arguments are songs, dirs or playlists.\n"
"-h  --help         This help.\n"
"\n"
"Report bugs to bug-xhippo@gnu.org.\n");
      return 0;
    default:
      return 20;
    }
  }

  /* Create the list. We have to do this after we've read the options,
     because we need to know the state of mode_left_scroll. */
  filelist = gtk_clist_new(1);

  gtk_clist_set_selection_mode(GTK_CLIST(filelist), GTK_SELECTION_SINGLE);
  gtk_clist_column_titles_hide(GTK_CLIST(filelist));
  gtk_clist_set_column_width(GTK_CLIST(filelist), 0, 200);
  gtk_clist_set_shadow_type(GTK_CLIST(filelist), GTK_SHADOW_ETCHED_IN);

  gtk_signal_connect(GTK_OBJECT(filelist), "button_press_event",
  		     GTK_SIGNAL_FUNC(handle_button_press), NULL);

  gtk_widget_set_name(filelist, "filelist");

  vadj = (GtkAdjustment *)gtk_adjustment_new(0, 0, 0, 0, 0, 0);
  gtk_tree_view_set_vadjustment(GTK_TREE_VIEW(filelist),
				GTK_ADJUSTMENT(vadj));
  scrollbar = gtk_vscrollbar_new(GTK_ADJUSTMENT(vadj));

  if (mode_left_scroll) { 
    gtk_box_pack_start(GTK_BOX(list_box), scrollbar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(list_box), filelist, TRUE, TRUE, 0); 
  } else { 
    gtk_box_pack_start(GTK_BOX(list_box), filelist, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(list_box), scrollbar, FALSE, FALSE, 0); 
  } 

  /* Load the playlist if no other args are given. */
  snprintf(buf, STRBUFSIZE, "%s/.xhippo/saved_playlist", getenv("HOME"));
  if (mode_persist_playlist && optind >= argc) read_playlist(buf, 1);

  /* Deal with non-options */
  while (optind < argc) {
    char *name = argv[optind++];
    if (mode_commandline_songs) {
      add_file(name);
    } else if (mode_commandline_dirs) {
      add_directory(name);
    } else {
      if (mode_commandline_guess) {
	fileaction *fa = get_action(name);
	if (fa) {
	  add_file(name);
	} else if (filetype(name) == 2) {
	  add_directory(name);
	} else {
	  read_playlist(name, 0);
	}
      } else {
	read_playlist(name, 0);
      }
    }
  }
  
  /* Show the list and scrollbar. */
  gtk_widget_show(filelist);
  gtk_widget_show(scrollbar);

  /* Show the list box. */
  gtk_widget_show(list_box);

  /* Update the checkbox state. */
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(randomcheckbox),
			       1 - mode_play_ordered);

  /* Show the everything box. */
  gtk_widget_show(all_box);

  /* Attach the acceleration group to the window. */
  gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

  /* Read the config file for the second time. */
  read_config_files(1);

  /* Set the status. */
  snprintf(buf, STRBUFSIZE, _("%d files in list."), listcount);
  status(buf);

  /* Read the window attributes as appropriate. */
  if (mode_save_window_pos) {
    read_window_state();

    gtk_window_move(GTK_WINDOW(window), attribute_xpos, attribute_ypos);
    if (attribute_width && attribute_height)
      gtk_window_resize(GTK_WINDOW(window), attribute_width, attribute_height);
  }

  /* Set the window attributes and show it. */
  if (mode_start_mini) attribute_mini = 1;
  if (attribute_mini) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(minicheckbox), 1);
    set_mini();
  }
  gtk_widget_show(window);

  /* If mode_play_on_start, start playing. */
  if (mode_play_on_start) pick_next();

  /* Make GDK monitor the pipe. */
  gdk_input_add(sigchld_pipe[0], GDK_INPUT_READ, sigchld_backend, NULL);

  /* Main loop. */
  gtk_main();

  /* Note that the interface doesn't exist any more, so you can't do
     anything interesting here. */
  return 0;
}
