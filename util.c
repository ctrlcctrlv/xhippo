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

/* Type of file referred to. 0=unknown/nothing, 1=file, 2=dir. */
gint filetype(char *file) {
  struct stat st;

  if ((!file) || (!*file)) return 0;

  if (stat(file, &st) < 0) return 0;

  if (S_ISREG(st.st_mode)) return 1;
  if (S_ISDIR(st.st_mode)) return 2;
 
  return 0;
}

/* Remove a trailing \n from a line. */
void chomp(gchar *line) {
  char *p;

  /* Remove \n. */
  if (*line) {
    p = line + strlen(line) - 1;
    if (*p == '\n') *p = '\0';
  }
}

#ifdef NOSTRCASECMP
/* The system doesn't provide strcasecmp, so we will instead. */
int strcasecmp(char *a, char *b) {
  char *la, *lb, *p;
  int r;

  la = strdup(a);
  lb = strdup(b);
  p = la; 
  while (*p) {
    *p = tolower(*p);
    p++;
  }
  p = lb;
  while (*p) {
    *p = tolower(*p);
    p++;
  }

  r = strcmp(la, lb);

  free(la);
  free(lb);

  return r;
}
#endif

