/*****************************************************************
 
  cfg_font.c
 
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

#include <stdio.h>
#include <string.h>

#include "gtk_dialog.h"

typedef struct
  {
  GtkWidget * entry;
  GtkWidget * label;
  GtkWidget * button;
  GtkWidget * fontselect;
  } font_t;

static void get_value(bg_gtk_widget_t * w)
  {
  font_t * priv;
  priv = (font_t*)(w->priv);

  if(!w->value.val_str || (*w->value.val_str == '\0'))
    {
    gtk_entry_set_text(GTK_ENTRY(priv->entry), "");
    return;
    }
  gtk_entry_set_text(GTK_ENTRY(priv->entry), w->value.val_str);
  }

static void set_value(bg_gtk_widget_t * w)
  {
  font_t * priv;
  const char * filename;
  
  priv = (font_t*)(w->priv);

  filename = gtk_entry_get_text(GTK_ENTRY(priv->entry));

  if(w->value.val_str)
    {
    free(w->value.val_str);
    w->value.val_str = (char*)0;
    }
  
  if(*filename != '\0')
    {
    w->value.val_str = malloc(strlen(filename)+1);
    strcpy(w->value.val_str, filename);
    }
  }

static void destroy(bg_gtk_widget_t * w)
  {
  font_t * priv = (font_t*)(w->priv);
  if(priv->fontselect)
    gtk_widget_destroy(priv->fontselect);
  if(w->value.val_str)
    free(w->value.val_str);
  free(priv);
  }

static void attach(void * priv, GtkWidget * table,
                   int * row,
                   int * num_columns)
  {
  font_t * f = (font_t*)priv;

  if(*num_columns < 3)
    *num_columns = 3;
  
  gtk_table_resize(GTK_TABLE(table), *row+1, *num_columns);

  gtk_table_attach(GTK_TABLE(table), f->label,
                    0, 1, *row, *row+1, GTK_FILL, GTK_SHRINK, 0, 0);

  gtk_table_attach(GTK_TABLE(table), f->entry,
                   1, 2, *row, *row+1, GTK_FILL | GTK_EXPAND, GTK_SHRINK, 0, 0);
  
  gtk_table_attach(GTK_TABLE(table), f->button,
                   2, 3, *row, *row+1, GTK_FILL, GTK_SHRINK, 0, 0);

  (*row)++;
  }

static gtk_widget_funcs_t funcs =
  {
    get_value: get_value,
    set_value: set_value,
    destroy:   destroy,
    attach:    attach
  };

static void button_callback(GtkWidget * w, gpointer data)
  {
  char * fontname;
  font_t * priv = (font_t*)data;

  if(w == priv->button)
    {
    if(!priv->fontselect)
      {
      priv->fontselect =  gtk_font_selection_dialog_new("Select a font");
      gtk_window_set_modal(GTK_WINDOW(priv->fontselect), TRUE);
      g_signal_connect(G_OBJECT(GTK_FONT_SELECTION_DIALOG(priv->fontselect)->ok_button),
                         "clicked", G_CALLBACK(button_callback),
                         (gpointer)priv);
      g_signal_connect(G_OBJECT(GTK_FONT_SELECTION_DIALOG(priv->fontselect)->cancel_button),
                         "clicked", G_CALLBACK(button_callback),
                         (gpointer)priv);
      }

    gtk_font_selection_set_font_name(GTK_FONT_SELECTION(GTK_FONT_SELECTION_DIALOG(priv->fontselect)->fontsel), gtk_entry_get_text(GTK_ENTRY(priv->entry)));
    
    gtk_widget_show(priv->fontselect);
    gtk_main();
    }
  else if(priv->fontselect)
    {
    if(w == GTK_FONT_SELECTION_DIALOG(priv->fontselect)->ok_button)
      {
      gtk_widget_hide(priv->fontselect);
      gtk_main_quit();
      fontname = gtk_font_selection_get_font_name(GTK_FONT_SELECTION(GTK_FONT_SELECTION_DIALOG(priv->fontselect)->fontsel));
      
      gtk_entry_set_text(GTK_ENTRY(priv->entry), fontname);
      g_free(fontname);
      }
    if(w == GTK_FONT_SELECTION_DIALOG(priv->fontselect)->cancel_button)
      {
      gtk_widget_hide(priv->fontselect);
      gtk_main_quit();
      }
    }
  
  }

void bg_gtk_create_font(bg_gtk_widget_t * w, bg_parameter_info_t * info)
  {
  font_t * priv = calloc(1, sizeof(*priv));

  priv->entry = gtk_entry_new();

  if(info->help_string)
    {
    gtk_tooltips_set_tip(w->tooltips, priv->entry, info->help_string, info->help_string);
    }

  gtk_widget_show(priv->entry);

  priv->label = gtk_label_new(info->long_name);
  gtk_misc_set_alignment(GTK_MISC(priv->label), 0.0, 0.5);

  gtk_widget_show(priv->label);

  priv->button = gtk_button_new_with_label("Browse...");

  g_signal_connect(G_OBJECT(priv->button),
                     "clicked", G_CALLBACK(button_callback),
                     (gpointer)priv);
  gtk_widget_show(priv->button);
  
  w->funcs = &funcs;
  w->priv = priv;
  }
