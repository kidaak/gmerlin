/*****************************************************************
 
  tree.h
 
  Copyright (c) 2003-2004 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de
 
  http://gmerlin.sourceforge.net
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 
*****************************************************************/

#ifndef __BG_GTK_TREE_H_
#define __BG_GTK_TREE_H_

/* Atom definitions for drag_n_drop */

void bg_gtk_tree_create_atoms();

#define bg_gtk_atom_entries_name   "gmerlin_entries"

/* Entries corresponding to removable devices can be dragged
   into the transcoder window but not between albums */

#define bg_gtk_atom_entries_name_r "gmerlin_entries_r"

#define bg_gtk_atom_album_name   "gmerlin_album"

extern GdkAtom bg_gtk_atom_entries;
extern GdkAtom bg_gtk_atom_entries_r;
extern GdkAtom bg_gtk_atom_album;

typedef struct bg_gtk_tree_window_s bg_gtk_tree_window_t;
typedef struct bg_gtk_album_widget_s bg_gtk_album_widget_t;
typedef struct bg_gtk_tree_widget_s bg_gtk_tree_widget_t;
typedef struct bg_gtk_album_window_s bg_gtk_album_window_t;

/* Tree widget */

bg_gtk_tree_widget_t * bg_gtk_tree_widget_create(bg_media_tree_t * tree);

bg_media_tree_t      * bg_gtk_tree_widget_get_tree(bg_gtk_tree_widget_t *);

void bg_gtk_tree_widget_destroy(bg_gtk_tree_widget_t *);

GtkWidget * bg_gtk_tree_widget_get_widget(bg_gtk_tree_widget_t *);

void
bg_gtk_tree_widget_close_album(bg_gtk_tree_widget_t * widget,
                               bg_gtk_album_window_t * win);

void bg_gtk_tree_widget_set_tooltips(bg_gtk_tree_widget_t*, int enable);


/* Tree window */

bg_gtk_tree_window_t *
bg_gtk_tree_window_create(bg_media_tree_t * tree,
                          void (*close_callback)(bg_gtk_tree_window_t*,void*),
                          void * close_callback_data);

void bg_gtk_tree_window_destroy(bg_gtk_tree_window_t *);

void bg_gtk_tree_window_show(bg_gtk_tree_window_t*);

void bg_gtk_tree_window_hide(bg_gtk_tree_window_t*);

void bg_gtk_tree_window_set_tooltips(bg_gtk_tree_window_t*, int enable);


/* Album widget */

bg_gtk_album_widget_t * bg_gtk_album_widget_create(bg_album_t * album,
                                                   GtkWidget * parent);
void bg_gtk_album_widget_destroy(bg_gtk_album_widget_t*);

GtkWidget * bg_gtk_album_widget_get_widget(bg_gtk_album_widget_t*);
bg_album_t * bg_gtk_album_widget_get_album(bg_gtk_album_widget_t*);

void bg_gtk_album_widget_delete_drag();

void bg_gtk_album_widget_update(bg_gtk_album_widget_t * w);

void bg_gtk_album_widget_put_config(bg_gtk_album_widget_t * w);

void bg_gtk_album_widget_set_tooltips(bg_gtk_album_widget_t * w, int enable);


/* Album window */

bg_gtk_album_window_t * bg_gtk_album_window_create(bg_album_t * album,
                                                   bg_gtk_tree_widget_t*);

void bg_gtk_album_window_destroy(bg_gtk_album_window_t*, int notify);

void bg_gtk_album_window_raise(bg_gtk_album_window_t*);
void bg_gtk_album_window_update(bg_gtk_album_window_t * w);

bg_album_t * bg_gtk_album_window_get_album(bg_gtk_album_window_t*);

void bg_gtk_album_window_set_tooltips(bg_gtk_album_window_t * w, int enable);


#endif // __BG_GTK_TREE_H_
