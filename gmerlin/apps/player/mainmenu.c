/*****************************************************************
 * gmerlin - a general purpose multimedia framework and applications
 *
 * Copyright (c) 2001 - 2012 Members of the Gmerlin project
 * gmerlin-general@lists.sourceforge.net
 * http://gmerlin.sourceforge.net
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * *****************************************************************/

#include <config.h>
#include <string.h>

#include <gmerlin/translation.h>

#include "gmerlin.h"
#include <gmerlin/utils.h>
#include <gmerlin/subprocess.h>

#include <gdk/gdkkeysyms.h>

#include <gui_gtk/aboutwindow.h>
#include <gui_gtk/plugin.h>

typedef struct
  {
  int num_plugins;
  GtkWidget *  menu;
  GtkWidget ** plugin_items;
  GtkWidget *  info;
  GtkWidget *  options;

  const bg_plugin_info_t * plugin_info;
  bg_plugin_handle_t * plugin_handle;
  bg_plugin_type_t type;
  uint32_t flags;
  
  bg_cfg_section_t * section;
  int no_callback;

  gmerlin_t * g; // For convenience
  } plugin_menu_t;

typedef struct stream_menu_s
  {
  GSList * group;
  GtkWidget ** stream_items;
  guint * ids;

  GtkWidget * off_item;
  guint off_id;

  int num_streams;
  int streams_alloc;
  
  GtkWidget * menu;

  GtkWidget * options;
  GtkWidget * plugins;
  GtkWidget * filters;
  plugin_menu_t plugin_menu;
  } stream_menu_t;

typedef struct chapter_menu_s
  {
  int timescale;
  int num_chapters;
  int chapters_alloc;
  guint * ids;
  GSList * group;
  GtkWidget ** chapter_items;
  GtkWidget * menu;
  } chapter_menu_t;

typedef struct visualization_menu_s
  {
  GtkWidget * menu;
  GtkWidget * enable;
  GtkWidget * options;
  GtkWidget * plugins;
  plugin_menu_t plugin_menu;
  } visualization_menu_t;

struct windows_menu_s
  {
  GtkWidget * mediatree;
  guint       mediatree_id;
  GtkWidget * infowindow;
  guint       infowindow_id;
  GtkWidget * logwindow;
  guint       logwindow_id;
  GtkWidget * menu;
  };

struct help_menu_s
  {
  GtkWidget * about;
  GtkWidget * help;
  GtkWidget * menu;
  };

struct options_menu_s
  {
  GtkWidget * preferences;
  GtkWidget * skins;
  GtkWidget * kbd;
  GtkWidget * menu;
  };

struct seek_menu_s
  {
  GtkWidget * seek_00;
  GtkWidget * seek_10;
  GtkWidget * seek_20;
  GtkWidget * seek_30;
  GtkWidget * seek_40;
  GtkWidget * seek_50;
  GtkWidget * seek_60;
  GtkWidget * seek_70;
  GtkWidget * seek_80;
  GtkWidget * seek_90;
  GtkWidget * menu;
  };

struct command_menu_s
  {
  GtkWidget * inc_volume;
  GtkWidget * dec_volume;

  GtkWidget * mute;

  GtkWidget * seek_forward;
  GtkWidget * seek_backward;
  GtkWidget * next;
  GtkWidget * previous;

  GtkWidget * next_chapter;
  GtkWidget * previous_chapter;
  GtkWidget * current_to_favourites;
  GtkWidget * goto_current;
  
  GtkWidget * seek_item;
  GtkWidget * pause;
  
  GtkWidget * quit;

  struct seek_menu_s       seek_menu;
  
  GtkWidget * menu;
  };

struct accessories_menu_s
  {
  GtkWidget * transcoder;
  GtkWidget * visualizer;
  GtkWidget * mixer;

  GtkWidget * menu;
  };

struct main_menu_s
  {
  struct windows_menu_s       windows_menu;
  struct help_menu_s          help_menu;
  struct options_menu_s       options_menu;
  struct command_menu_s       command_menu;
  struct accessories_menu_s   accessories_menu;
  struct stream_menu_s        audio_stream_menu;
  struct stream_menu_s        video_stream_menu;
  struct stream_menu_s        subtitle_stream_menu;
  struct chapter_menu_s       chapter_menu;
  struct visualization_menu_s visualization_menu;
  
  GtkWidget * windows_item;
  GtkWidget * help_item;
  GtkWidget * options_item;
  GtkWidget * accessories_item;
  
  GtkWidget * audio_stream_item;
  GtkWidget * video_stream_item;
  GtkWidget * subtitle_stream_item;
  GtkWidget * chapter_item;
  GtkWidget * visualization_item;
  
  GtkWidget * menu;
  gmerlin_t * g;
  };

static GtkWidget * create_menu()
  {
  GtkWidget * ret;
  GtkWidget * tearoff_item;

  ret = gtk_menu_new();
  tearoff_item = gtk_tearoff_menu_item_new();
  gtk_widget_show(tearoff_item);

  gtk_menu_shell_append(GTK_MENU_SHELL(ret), tearoff_item);
  return ret;
  }

static int stream_menu_has_widget(stream_menu_t * s,
                                  GtkWidget * w, int * index)
  {
  int i;
  if((w == s->off_item) &&
     gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(s->off_item)))
    {
    *index = -1;
    return 1;
    }
  for(i = 0; i < s->num_streams; i++)
    {
    if((w == s->stream_items[i]) &&
       gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(s->stream_items[i])))
      {
      *index = i;
      return 1;
      }
    }
  return 0;
  }

static int chapter_menu_has_widget(chapter_menu_t * s,
                                   GtkWidget * w, int * index)
  {
  int i;
  for(i = 0; i < s->num_chapters; i++)
    {
    if((w == s->chapter_items[i]) &&
       gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(s->chapter_items[i])))
      {
      *index = i;
      return 1;
      }
    }
  return 0;
  }

static int plugin_menu_has_widget(plugin_menu_t * s,
                                  GtkWidget * w, int * index)
  {
  int i;
  for(i = 0; i < s->num_plugins; i++)
    {
    if((w == s->plugin_items[i]) &&
       gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(s->plugin_items[i])))
      {
      *index = i;
      return 1;
      }
    }
  return 0;
  }

static void
plugin_menu_set_plugin(plugin_menu_t * s, int index)
  {
  if(s->no_callback)
    return;
  
  s->plugin_info = bg_plugin_find_by_index(s->g->plugin_reg, index,
                                           s->type, BG_PLUGIN_ALL);

  /* Save for future use */
  bg_plugin_registry_set_default(s->g->plugin_reg, s->type, s->flags,
                                 s->plugin_info->name);
  
  if(s->plugin_handle)
    {
    bg_plugin_unref(s->plugin_handle);
    s->plugin_handle = NULL;
    }
  if(s->plugin_info->parameters)
    gtk_widget_set_sensitive(s->options, 1);
  else
    gtk_widget_set_sensitive(s->options, 0);
  
  if(s->type == BG_PLUGIN_OUTPUT_AUDIO)
    {
    s->plugin_handle = bg_plugin_load(s->g->plugin_reg, s->plugin_info);
    bg_player_set_oa_plugin(s->g->player, s->plugin_handle);
    }
  else if(s->type == BG_PLUGIN_OUTPUT_VIDEO)
    {
    bg_ov_plugin_t * plugin;

    s->plugin_handle = bg_plugin_load(s->g->plugin_reg, s->plugin_info);
    plugin = (bg_ov_plugin_t *)s->plugin_handle->plugin;
    if(plugin->set_window_options)
      {
      gavl_video_frame_t * icon = NULL;
      gavl_video_format_t icon_format;
      char * icon_path;

      memset(&icon_format, 0, sizeof(icon_format));

      /* Load icon */
      icon_path = bg_search_file_read("icons", WINDOW_ICON);
      if(icon_path)
        {
        icon = 
          bg_plugin_registry_load_image(s->g->plugin_reg,
                                        icon_path,
                                        &icon_format,
                                        NULL);
        }
      plugin->set_window_options(s->plugin_handle->priv,
                                 WINDOW_NAME, WINDOW_CLASS,
                                 icon, &icon_format);
      if(icon)
        gavl_video_frame_destroy(icon);
      if(icon_path)
        free(icon_path);
      }
    bg_player_set_ov_plugin(s->g->player, s->plugin_handle);
    }
  else if(s->type == BG_PLUGIN_VISUALIZATION)
    {
    bg_player_set_visualization_plugin(s->g->player, s->plugin_info);
    }

  s->section = bg_plugin_registry_get_section(s->g->plugin_reg, s->plugin_info->name);
  
  }

static void set_parameter(void * data, const char * name,
                          const bg_parameter_value_t * v)
  {
  plugin_menu_t * m;
  m = (plugin_menu_t*)data;
  
  if(m->plugin_handle && m->plugin_handle->plugin->set_parameter)
    {
    bg_plugin_lock(m->plugin_handle);
    m->plugin_handle->plugin->set_parameter(m->plugin_handle->priv, name, v);
    bg_plugin_unlock(m->plugin_handle);
    }
  else if(m->type == BG_PLUGIN_VISUALIZATION)
    {
    bg_player_set_visualization_plugin_parameter(m->g->player, name, v);
    }
  }

static void plugin_menu_configure(plugin_menu_t * m)
  {
  const bg_parameter_info_t * parameters;
  bg_dialog_t * dialog;

  if(m->plugin_handle)
    parameters = m->plugin_handle->plugin->get_parameters(m->plugin_handle->priv);
  else
    parameters = m->plugin_info->parameters;
  dialog = bg_dialog_create(m->section,
                            set_parameter,
                            NULL,
                            (void*)m,
                            parameters,
                            TRD(m->plugin_info->long_name,
                                m->plugin_info->gettext_domain));
  bg_dialog_show(dialog, m->g->player_window->window);
  bg_dialog_destroy(dialog);
  
  }

static void plugin_menu_free(plugin_menu_t * s)
  {
  if(s->plugin_handle)
    bg_plugin_unref(s->plugin_handle);
  if(s->plugin_items)
    free(s->plugin_items);
  }

static void about_window_close_callback(bg_gtk_about_window_t* win, void* data)
  {
  gmerlin_t * g;
  main_menu_t * the_menu;

  g = (gmerlin_t*)data;
  the_menu = g->player_window->main_menu;
  gtk_widget_set_sensitive(the_menu->help_menu.about, 1);
  }

static void menu_callback(GtkWidget * w, gpointer data)
  {
  int i;
  gmerlin_t * g;
  main_menu_t * the_menu;

  g = (gmerlin_t*)data;
  the_menu = g->player_window->main_menu;
  
  if(w == the_menu->options_menu.preferences)
    gmerlin_configure(g);
  else if(w == the_menu->audio_stream_menu.options)
    bg_dialog_show(g->audio_dialog, g->player_window->window);
  else if(w == the_menu->audio_stream_menu.filters)
    bg_dialog_show(g->audio_filter_dialog, g->player_window->window);
  else if(w == the_menu->video_stream_menu.options)
    bg_dialog_show(g->video_dialog, g->player_window->window);
  else if(w == the_menu->video_stream_menu.filters)
    bg_dialog_show(g->video_filter_dialog, g->player_window->window);
  else if(w == the_menu->subtitle_stream_menu.options)
    bg_dialog_show(g->subtitle_dialog, g->player_window->window);
  else if(w == the_menu->visualization_menu.options)
    bg_dialog_show(g->visualization_dialog, g->player_window->window);
  else if(w == the_menu->visualization_menu.enable)
    {
    i = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(the_menu->visualization_menu.enable));
    bg_player_set_visualization(g->player, i);
    bg_plugin_registry_set_visualize(g->plugin_reg, i);
    }
  else if(w == the_menu->options_menu.skins)
    {
    if(!g->skin_browser)
      g->skin_browser = gmerlin_skin_browser_create(g);
    gmerlin_skin_browser_show(g->skin_browser);
    }
  else if(w == the_menu->options_menu.kbd)
    {
    bg_system("gmerlin_kbd_config &");
    }
  else if(w == the_menu->windows_menu.infowindow)
    {
    if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(the_menu->windows_menu.infowindow)))
      {
      bg_gtk_info_window_show(g->info_window);
      g->show_info_window = 1;
      }
    else
      {
      bg_gtk_info_window_hide(g->info_window);
      g->show_info_window = 0;
      }
    }
  else if(w == the_menu->windows_menu.logwindow)
    {
    if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(the_menu->windows_menu.logwindow)))
      {
      bg_gtk_log_window_show(g->log_window);
      g->show_log_window = 1;
      }
    else
      {
      bg_gtk_log_window_hide(g->log_window);
      g->show_log_window = 0;
      }
    }
  else if(w == the_menu->help_menu.about)
    {
    gtk_widget_set_sensitive(the_menu->help_menu.about, 0);
    bg_gtk_about_window_create("Gmerlin player", VERSION,
                               "player_icon.png",
                               about_window_close_callback,
                               g);
    }
  else if(w == the_menu->help_menu.help)
    {
    bg_display_html_help("userguide/GUI-Player.html");
    }
  
  else if(w == the_menu->windows_menu.mediatree)
    {
    if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(the_menu->windows_menu.mediatree)))
      {
      bg_gtk_tree_window_show(g->tree_window);
      g->show_tree_window = 1;
      }
    else
      {
      bg_gtk_tree_window_hide(g->tree_window);
      g->show_tree_window = 0;
      }
    }
  else if(w == the_menu->accessories_menu.visualizer)
    {
    bg_system("gmerlin_visualizer &");
    }
  else if(w == the_menu->accessories_menu.mixer)
    {
    bg_system("gmerlin_alsamixer &");
    }
  else if(w == the_menu->accessories_menu.transcoder)
    {
    bg_system("gmerlin_transcoder_remote -launch");
    }
  /* Commands (== accelerators) */
  else if(w == the_menu->command_menu.inc_volume)
    player_window_push_accel(g->player_window, ACCEL_VOLUME_UP);
  else if(w == the_menu->command_menu.dec_volume)
    player_window_push_accel(g->player_window, ACCEL_VOLUME_DOWN);
  else if(w == the_menu->command_menu.seek_backward)
    player_window_push_accel(g->player_window, ACCEL_SEEK_BACKWARD);
  else if(w == the_menu->command_menu.seek_forward)
    player_window_push_accel(g->player_window, ACCEL_SEEK_FORWARD);
  else if(w == the_menu->command_menu.pause)
    player_window_push_accel(g->player_window, ACCEL_PAUSE);
  else if(w == the_menu->command_menu.mute)
    player_window_push_accel(g->player_window, ACCEL_MUTE);
  else if(w == the_menu->command_menu.next_chapter)
    player_window_push_accel(g->player_window, ACCEL_NEXT_CHAPTER);
  else if(w == the_menu->command_menu.previous_chapter)
    player_window_push_accel(g->player_window, ACCEL_PREV_CHAPTER);
  else if(w == the_menu->command_menu.next)
    player_window_push_accel(g->player_window, ACCEL_NEXT);
  else if(w == the_menu->command_menu.previous)
    player_window_push_accel(g->player_window, ACCEL_PREV);
  else if(w == the_menu->command_menu.current_to_favourites)
    player_window_push_accel(g->player_window, ACCEL_CURRENT_TO_FAVOURITES);
  else if(w == the_menu->command_menu.quit)
    player_window_push_accel(g->player_window, ACCEL_QUIT);

  else if(w == the_menu->command_menu.seek_menu.seek_00)
    player_window_push_accel(g->player_window, ACCEL_SEEK_START);
  else if(w == the_menu->command_menu.seek_menu.seek_10)
    player_window_push_accel(g->player_window, ACCEL_SEEK_10);
  else if(w == the_menu->command_menu.seek_menu.seek_20)
    player_window_push_accel(g->player_window, ACCEL_SEEK_20);
  else if(w == the_menu->command_menu.seek_menu.seek_30)
    player_window_push_accel(g->player_window, ACCEL_SEEK_30);
  else if(w == the_menu->command_menu.seek_menu.seek_40)
    player_window_push_accel(g->player_window, ACCEL_SEEK_40);
  else if(w == the_menu->command_menu.seek_menu.seek_50)
    player_window_push_accel(g->player_window, ACCEL_SEEK_50);
  else if(w == the_menu->command_menu.seek_menu.seek_60)
    player_window_push_accel(g->player_window, ACCEL_SEEK_60);
  else if(w == the_menu->command_menu.seek_menu.seek_70)
    player_window_push_accel(g->player_window, ACCEL_SEEK_70);
  else if(w == the_menu->command_menu.seek_menu.seek_80)
    player_window_push_accel(g->player_window, ACCEL_SEEK_80);
  else if(w == the_menu->command_menu.seek_menu.seek_90)
    player_window_push_accel(g->player_window, ACCEL_SEEK_90);
  
  /* Stream selection */
  else if(stream_menu_has_widget(&the_menu->audio_stream_menu, w, &i))
    bg_player_set_audio_stream(g->player, i);
  else if(stream_menu_has_widget(&the_menu->video_stream_menu, w, &i))
    bg_player_set_video_stream(g->player, i);
  else if(stream_menu_has_widget(&the_menu->subtitle_stream_menu, w, &i))
    bg_player_set_subtitle_stream(g->player, i);
  /* Chapters */
  else if(chapter_menu_has_widget(&the_menu->chapter_menu, w, &i))
    bg_player_set_chapter(g->player, i);

  /* Audio plugin */
  else if(plugin_menu_has_widget(&the_menu->audio_stream_menu.plugin_menu, w, &i))
    plugin_menu_set_plugin(&the_menu->audio_stream_menu.plugin_menu, i);
  else if(w == the_menu->audio_stream_menu.plugin_menu.info)
    bg_gtk_plugin_info_show(the_menu->audio_stream_menu.plugin_menu.plugin_info,
                            g->player_window->window);
  else if(w == the_menu->audio_stream_menu.plugin_menu.options)
    plugin_menu_configure(&the_menu->audio_stream_menu.plugin_menu);

  /* Video plugin */
  else if(plugin_menu_has_widget(&the_menu->video_stream_menu.plugin_menu, w, &i))
    plugin_menu_set_plugin(&the_menu->video_stream_menu.plugin_menu, i);
  else if(w == the_menu->video_stream_menu.plugin_menu.info)
    bg_gtk_plugin_info_show(the_menu->video_stream_menu.plugin_menu.plugin_info,
                            g->player_window->window);
  else if(w == the_menu->video_stream_menu.plugin_menu.options)
    plugin_menu_configure(&the_menu->video_stream_menu.plugin_menu);

  /* Visualization plugin */
  else if(plugin_menu_has_widget(&the_menu->visualization_menu.plugin_menu, w, &i))
    plugin_menu_set_plugin(&the_menu->visualization_menu.plugin_menu, i);
  else if(w == the_menu->visualization_menu.plugin_menu.info)
    bg_gtk_plugin_info_show(the_menu->visualization_menu.plugin_menu.plugin_info,
                            g->player_window->window);
  else if(w == the_menu->visualization_menu.plugin_menu.options)
    plugin_menu_configure(&the_menu->visualization_menu.plugin_menu);
  }

static GtkWidget *
create_pixmap_item(const char * label, const char * pixmap,
                   gmerlin_t * gmerlin,
                   GtkWidget * menu)
  {
  GtkWidget * ret, *image;
  char * path;
  
  
  if(pixmap)
    {
    path = bg_search_file_read("icons", pixmap);
    if(path)
      {
      image = gtk_image_new_from_file(path);
      free(path);
      }
    else
      image = gtk_image_new();
    gtk_widget_show(image);
    ret = gtk_image_menu_item_new_with_label(label);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(ret), image);
    }
  else
    {
    ret = gtk_menu_item_new_with_label(label);
    }
  
  g_signal_connect(G_OBJECT(ret), "activate", G_CALLBACK(menu_callback),
                   (gpointer)gmerlin);
  gtk_widget_show(ret);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), ret);
  return ret;
  }

static GtkWidget * create_item(const char * label,
                               gmerlin_t * gmerlin,
                               GtkWidget * menu)
  {
  GtkWidget * ret;
  ret = gtk_menu_item_new_with_label(label);
  g_signal_connect(G_OBJECT(ret), "activate",
                   G_CALLBACK(menu_callback),
                   gmerlin);
  gtk_widget_show(ret);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), ret);
  return ret;
  }

static GtkWidget * create_toggle_item(const char * label,
                                      gmerlin_t * gmerlin,
                                      GtkWidget * menu, guint * id)
  {
  guint32 handler_id;
  GtkWidget * ret;
  ret = gtk_check_menu_item_new_with_label(label);
  handler_id = g_signal_connect(G_OBJECT(ret), "toggled",
                   G_CALLBACK(menu_callback),
                   gmerlin);
  if(id)
    *id = handler_id;
  gtk_widget_show(ret);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), ret);
  return ret;
  }

static GtkWidget * create_stream_item(gmerlin_t * gmerlin,
                                      stream_menu_t * m,
                                      guint * id)
  {
  GtkWidget * ret;
  ret = gtk_radio_menu_item_new_with_label(m->group, "");
  m->group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(ret));
  
  *id = g_signal_connect(G_OBJECT(ret), "activate",
                         G_CALLBACK(menu_callback),
                         gmerlin);
  gtk_menu_shell_insert(GTK_MENU_SHELL(m->menu), ret, (int)(id - m->ids) + 2);
  return ret;
  }

static GtkWidget * create_chapter_item(gmerlin_t * gmerlin,
                                       chapter_menu_t * m,
                                       guint * id)
  {
  GtkWidget * ret;
  ret = gtk_radio_menu_item_new_with_label(m->group, "");
  m->group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(ret));
  
  *id = g_signal_connect(G_OBJECT(ret), "activate",
                         G_CALLBACK(menu_callback),
                         gmerlin);
  gtk_menu_shell_append(GTK_MENU_SHELL(m->menu), ret);
  return ret;
  }

static GtkWidget * create_submenu_item(const char * label,
                                       GtkWidget * child_menu,
                                       GtkWidget * parent_menu)
  {
  GtkWidget * ret;
  ret = gtk_menu_item_new_with_label(label);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(ret), child_menu);
  gtk_widget_show(ret);

  gtk_menu_shell_append(GTK_MENU_SHELL(parent_menu), ret);
  return ret;
  }

#if 0
typedef struct stream_menu_s
  {
  GSList * group;
  GtkWidget ** stream_items;
  guint * ids;

  GtkWidget * off_item;
  guint off_id;

  int num_streams;
  int streams_alloc;
  
  GtkWidget * menu;
  } stream_menu_t;
#endif

static void plugin_menu_init(plugin_menu_t * m, gmerlin_t * gmerlin,
                             bg_plugin_type_t plugin_type, uint32_t flags)
  {
  int i;
  const bg_plugin_info_t * info;
  
  GtkWidget * w;
  GSList * group = NULL;

  m->type = plugin_type;
  m->flags = flags;
  m->menu = create_menu();
  m->g = gmerlin;
  
  m->num_plugins = bg_plugin_registry_get_num_plugins(gmerlin->plugin_reg,
                                                      plugin_type, BG_PLUGIN_ALL);
  m->plugin_items = calloc(m->num_plugins, sizeof(*m->plugin_items));
  
  for(i = 0; i < m->num_plugins; i++)
    {
    info = bg_plugin_find_by_index(gmerlin->plugin_reg, i, plugin_type, BG_PLUGIN_ALL);
    
    m->plugin_items[i] =
      gtk_radio_menu_item_new_with_label(group,
                                         TRD(info->long_name, info->gettext_domain));
    
    g_signal_connect(G_OBJECT(m->plugin_items[i]), "activate",
                     G_CALLBACK(menu_callback), gmerlin);
    
    gtk_widget_show(m->plugin_items[i]);
    group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(m->plugin_items[i]));
    gtk_menu_shell_append(GTK_MENU_SHELL(m->menu), m->plugin_items[i]);
    }
  w = gtk_separator_menu_item_new();
  gtk_widget_show(w);
  gtk_menu_shell_append(GTK_MENU_SHELL(m->menu), w);

  m->options = create_pixmap_item("Options...", "config_16.png",
                                  gmerlin, m->menu);

  m->info = create_pixmap_item("Info...", "info_16.png",
                               gmerlin, m->menu);
  
  }

static void plugin_menu_finalize(plugin_menu_t * m)
  {
  const bg_plugin_info_t * default_info;
  const bg_plugin_info_t * info;
  int default_index = 0, i;
  default_info = bg_plugin_registry_get_default(m->g->plugin_reg,
                                                m->type, m->flags);

  m->no_callback = 1;
  for(i = 0; i < m->num_plugins; i++)
    {
    info = bg_plugin_find_by_index(m->g->plugin_reg, i, m->type, BG_PLUGIN_ALL);
    
    if(info == default_info)
      {
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(m->plugin_items[i]), 1);
      default_index = i;
      }
    }
  m->no_callback = 0;
  /* Set default plugin and send it to the player */
  plugin_menu_set_plugin(m, default_index);
  }

static void stream_menu_init(stream_menu_t * s, gmerlin_t * gmerlin,
                             int has_plugins, int has_filters,
                             bg_plugin_type_t plugin_type)
  {
  GtkWidget * separator;
  s->menu = create_menu();
  s->off_item = gtk_radio_menu_item_new_with_label(NULL, TR("Off"));
  
  s->off_id = g_signal_connect(G_OBJECT(s->off_item), "activate",
                               G_CALLBACK(menu_callback),
                               gmerlin);
  
  s->group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(s->off_item));
  gtk_widget_show(s->off_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(s->menu), s->off_item);

  separator = gtk_separator_menu_item_new();
  gtk_widget_show(separator);
  gtk_menu_shell_append(GTK_MENU_SHELL(s->menu), separator);

  s->options = create_pixmap_item(TR("Options..."), "config_16.png",
                                  gmerlin,
                                  s->menu);
  if(has_filters)
    s->filters = create_pixmap_item(TR("Filters..."), "filter_16.png", gmerlin, s->menu);
  if(has_plugins)
    {
    s->plugins = create_pixmap_item(TR("Output plugin..."), "plugin_16.png",
                                    gmerlin, s->menu);
    plugin_menu_init(&s->plugin_menu, gmerlin, plugin_type, BG_PLUGIN_PLAYBACK);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(s->plugins), s->plugin_menu.menu);
    }
  }

static void stream_menu_free(stream_menu_t * s)
  {
  if(s->stream_items)
    free(s->stream_items);
  if(s->ids)
    free(s->ids);
  }

static void stream_menu_finalize(stream_menu_t * s)
  {
  if(s->plugin_menu.menu)
    plugin_menu_finalize(&s->plugin_menu);
  }

static void visualization_menu_init(visualization_menu_t * s, gmerlin_t * gmerlin)
  {
  s->menu = create_menu();
  s->enable =
    create_toggle_item(TR("Enable visualizations"),
                       gmerlin,
                       s->menu, NULL);
  gtk_widget_show(s->enable);

  s->options = create_pixmap_item(TR("Options..."), "config_16.png",
                                  gmerlin,
                                  s->menu);
  
  s->plugins = create_pixmap_item(TR("Plugin..."), "plugin_16.png",
                                  gmerlin, s->menu);
  plugin_menu_init(&s->plugin_menu, gmerlin, BG_PLUGIN_VISUALIZATION,
                   BG_PLUGIN_VISUALIZE_FRAME | BG_PLUGIN_VISUALIZE_GL);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(s->plugins), s->plugin_menu.menu);
  
  }

static void visualization_menu_finalize(visualization_menu_t * s, gmerlin_t * gmerlin)
  {
  plugin_menu_finalize(&s->plugin_menu);

  if(bg_plugin_registry_get_visualize(gmerlin->plugin_reg))
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(s->enable), 1);
  }


static void stream_menu_set_num(gmerlin_t * g, stream_menu_t * s, int num)
  {
  int i;
  if(num > s->streams_alloc)
    {
    s->stream_items = realloc(s->stream_items, num * sizeof(*s->stream_items));
    s->ids = realloc(s->ids, num * sizeof(*s->ids));

    for(i = s->streams_alloc; i < num; i++)
      s->stream_items[i] = create_stream_item(g, s, &s->ids[i]);
    s->streams_alloc = num;
    }
  for(i = 0; i < num; i++)
    gtk_widget_show(s->stream_items[i]);
  for(i = num; i < s->streams_alloc; i++)
    gtk_widget_hide(s->stream_items[i]);
  s->num_streams = num;
  }

static void stream_menu_set_index(stream_menu_t * s, int index)
  {
  int i;
  /* Block event handlers */
  g_signal_handler_block(G_OBJECT(s->off_item), s->off_id);
  for(i = 0; i < s->streams_alloc; i++)
    g_signal_handler_block(G_OBJECT(s->stream_items[i]), s->ids[i]);

  /* Select item */
  if(index == -1)
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(s->off_item), 1);
  else
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(s->stream_items[index]), 1);

  /* Unblock event handlers */
  g_signal_handler_unblock(G_OBJECT(s->off_item), s->off_id);
  for(i = 0; i < s->streams_alloc; i++)
    g_signal_handler_unblock(G_OBJECT(s->stream_items[i]), s->ids[i]);

  }

static void chapter_menu_set_num(gmerlin_t * g, chapter_menu_t * s,
                                 int num, int timescale)
  {
  int i;
  if(num > s->chapters_alloc)
    {
    s->chapter_items = realloc(s->chapter_items,
                               num * sizeof(*s->chapter_items));
    s->ids = realloc(s->ids, num * sizeof(*s->ids));

    for(i = s->chapters_alloc; i < num; i++)
      s->chapter_items[i] = create_chapter_item(g, s, &s->ids[i]);
    s->chapters_alloc = num;
    }
  s->timescale = timescale;
  for(i = 0; i < num; i++)
    gtk_widget_show(s->chapter_items[i]);
  for(i = num; i < s->chapters_alloc; i++)
    gtk_widget_hide(s->chapter_items[i]);
  s->num_chapters = num;
  }



void
main_menu_set_audio_index(main_menu_t * m, int index)
  {
  stream_menu_set_index(&m->audio_stream_menu, index);
  }

void
main_menu_set_video_index(main_menu_t * m, int index)
  {
  stream_menu_set_index(&m->video_stream_menu, index);
  }

void
main_menu_set_subtitle_index(main_menu_t * m, int index)
  {
  stream_menu_set_index(&m->subtitle_stream_menu, index);
  }


void main_menu_set_num_streams(main_menu_t * m,
                               int audio_streams,
                               int video_streams,
                               int subtitle_streams)
  {
  stream_menu_set_num(m->g, &m->audio_stream_menu, audio_streams);
  stream_menu_set_num(m->g, &m->video_stream_menu, video_streams);
  stream_menu_set_num(m->g, &m->subtitle_stream_menu, subtitle_streams);
  }

void main_menu_set_num_chapters(main_menu_t * m,
                                int num, int timescale)
  {
  if(!num)
    gtk_widget_set_sensitive(m->chapter_item, 0);
  else
    gtk_widget_set_sensitive(m->chapter_item, 1);
  chapter_menu_set_num(m->g, &m->chapter_menu, num, timescale);
  }


void main_menu_set_chapter_info(main_menu_t * m, int chapter,
                                const char * name,
                                gavl_time_t time)
  {
  char * label;
  char time_string[GAVL_TIME_STRING_LEN];
  GtkWidget * w;

  gavl_time_prettyprint(gavl_time_unscale(m->chapter_menu.timescale, time),
                        time_string);
  
  if(name)
    label = bg_sprintf("%s [%s]", name, time_string);
  else
    label = bg_sprintf(TR("Chapter %d [%s]"),
                       chapter+1, time_string);
  
  w = m->chapter_menu.chapter_items[chapter];
  gtk_label_set_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(w))), label);
  free(label);
  }

void main_menu_chapter_changed(main_menu_t * m, int chapter)
  {
  GtkWidget * w;
  if(chapter >= m->chapter_menu.num_chapters)
    return;

  w = m->chapter_menu.chapter_items[chapter];
  g_signal_handler_block(G_OBJECT(w), m->chapter_menu.ids[chapter]);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w), 1);
  g_signal_handler_unblock(G_OBJECT(w), m->chapter_menu.ids[chapter]);
  }


void main_menu_set_audio_info(main_menu_t * m, int stream,
                              const gavl_metadata_t * metadata)
  {
  char * label;
  GtkWidget * w;
  label = bg_get_stream_label(stream, metadata);
  w = m->audio_stream_menu.stream_items[stream];
  gtk_label_set_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(w))), label);
  free(label);
  }


void main_menu_set_video_info(main_menu_t * m, int stream,
                              const gavl_metadata_t * metadata)
  {
  char * label;
  GtkWidget * w;
  label = bg_get_stream_label(stream, metadata);
  w = m->video_stream_menu.stream_items[stream];
  gtk_label_set_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(w))), label);

  free(label);

  }

void main_menu_set_subtitle_info(main_menu_t * m, int stream,
                                 const gavl_metadata_t * metadata)
  {
  char * label;
  GtkWidget * w;
  label = bg_get_stream_label(stream, metadata);
  w = m->subtitle_stream_menu.stream_items[stream];
  gtk_label_set_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(w))), label);
  free(label);
  
  }


main_menu_t * main_menu_create(gmerlin_t * gmerlin)
  {
  main_menu_t * ret;
  ret = calloc(1, sizeof(*ret));
  ret->g = gmerlin;
  /* Windows */
    
  ret->windows_menu.menu = create_menu();
  ret->windows_menu.mediatree =
    create_toggle_item(TR("Media Tree"), gmerlin, ret->windows_menu.menu,
                       &ret->windows_menu.mediatree_id);
  ret->windows_menu.infowindow =
    create_toggle_item(TR("Info window"), gmerlin, ret->windows_menu.menu,
                       &ret->windows_menu.infowindow_id);
  ret->windows_menu.logwindow =
    create_toggle_item(TR("Log window"), gmerlin, ret->windows_menu.menu,
                       &ret->windows_menu.logwindow_id);
  gtk_widget_show(ret->windows_menu.menu);

  /* Help */
  
  ret->help_menu.menu = create_menu();
  ret->help_menu.about = create_pixmap_item(TR("About..."), "about_16.png",
                                            gmerlin, ret->help_menu.menu);
  ret->help_menu.help = create_pixmap_item(TR("Userguide"), "help_16.png",
                                           gmerlin, ret->help_menu.menu);
  
  gtk_widget_show(ret->help_menu.menu);
    
  /* Streams */

  stream_menu_init(&ret->audio_stream_menu, gmerlin, 1, 1, BG_PLUGIN_OUTPUT_AUDIO);
  stream_menu_init(&ret->video_stream_menu, gmerlin, 1, 1, BG_PLUGIN_OUTPUT_VIDEO);
  stream_menu_init(&ret->subtitle_stream_menu, gmerlin, 0, 0, BG_PLUGIN_NONE);

  /* Chapters */
  ret->chapter_menu.menu = create_menu();

  /* Visualization */
  visualization_menu_init(&ret->visualization_menu, gmerlin);
  
  /* Options */
  
  ret->options_menu.menu = create_menu();
  ret->options_menu.preferences =
    create_pixmap_item(TR("Preferences..."), "config_16.png", gmerlin, ret->options_menu.menu);

  gtk_widget_add_accelerator(ret->options_menu.preferences, "activate", ret->g->accel_group,
                             GDK_o, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

  
  ret->options_menu.skins =
    create_item(TR("Skins..."), gmerlin, ret->options_menu.menu);

  if(bg_search_file_exec("gmerlin_kbd_config", NULL))
    ret->options_menu.kbd =
      create_item(TR("Multimedia keys..."), gmerlin, ret->options_menu.menu);
  
  /* Commands */
  ret->command_menu.menu = create_menu();

  ret->command_menu.seek_forward =
    create_item(TR("Seek forward"), gmerlin, ret->command_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.seek_forward, "activate",
                             ret->g->accel_group,
                             GDK_Right, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
  
  
  ret->command_menu.seek_backward =
    create_item(TR("Seek backward"), gmerlin, ret->command_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.seek_backward, "activate",
                             ret->g->accel_group,
                             GDK_Left, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
  

  ret->command_menu.inc_volume =
    create_item(TR("Increase volume"), gmerlin, ret->command_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.inc_volume, "activate",
                             ret->g->accel_group,
                             GDK_Right, GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);


  ret->command_menu.dec_volume =
    create_item(TR("Decrease volume"), gmerlin, ret->command_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.dec_volume, "activate",
                             ret->g->accel_group,
                             GDK_Left, GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);

  ret->command_menu.mute =
    create_item(TR("Toggle mute"), gmerlin, ret->command_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.mute, "activate",
                             ret->g->accel_group,
                             GDK_m, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
  
  ret->command_menu.next =
    create_item(TR("Next track"), gmerlin, ret->command_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.next, "activate", ret->g->accel_group,
                             GDK_Page_Down, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

  ret->command_menu.previous =
    create_item(TR("Previous track"), gmerlin, ret->command_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.previous, "activate", ret->g->accel_group,
                             GDK_Page_Up, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

  ret->command_menu.next_chapter =
    create_item(TR("Next chapter"), gmerlin, ret->command_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.next_chapter, "activate", ret->g->accel_group,
                             GDK_Page_Down,  GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                             GTK_ACCEL_VISIBLE);

  ret->command_menu.previous_chapter =
    create_item(TR("Previous chapter"), gmerlin, ret->command_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.previous_chapter, "activate", ret->g->accel_group,
                             GDK_Page_Up, GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                             GTK_ACCEL_VISIBLE);
  
  ret->command_menu.goto_current = create_item(TR("Goto current track"),
                                               gmerlin, ret->command_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.goto_current, "activate",
                             ret->g->accel_group,
                             GDK_g, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
  
  ret->command_menu.current_to_favourites =
    create_item(TR("Copy current track to favourites"), gmerlin, ret->command_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.current_to_favourites,
                             "activate", ret->g->accel_group,
                             GDK_F9, 0, GTK_ACCEL_VISIBLE);
  
  ret->command_menu.pause =
    create_item(TR("Pause"), gmerlin, ret->command_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.pause, "activate", ret->g->player_window->accel_group,
                             GDK_space, 0, GTK_ACCEL_VISIBLE);

  ret->command_menu.quit =
    create_item(TR("Quit gmerlin"), gmerlin, ret->command_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.quit, "activate",
                             ret->g->accel_group,
                             GDK_q, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

  /* Command -> Seek */

  ret->command_menu.seek_menu.menu = create_menu();

  
  
  ret->command_menu.seek_menu.seek_00 =
    create_item(TR("Seek to start"), gmerlin, ret->command_menu.seek_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.seek_menu.seek_00,
                             "activate", ret->g->player_window->accel_group,
                             GDK_0, 0, GTK_ACCEL_VISIBLE);

  ret->command_menu.seek_menu.seek_10 =
    create_item(TR("Seek to 10%"), gmerlin, ret->command_menu.seek_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.seek_menu.seek_10,
                             "activate", ret->g->player_window->accel_group,
                             GDK_1, 0, GTK_ACCEL_VISIBLE);
  ret->command_menu.seek_menu.seek_20 =
    create_item(TR("Seek to 20%"), gmerlin, ret->command_menu.seek_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.seek_menu.seek_20,
                             "activate", ret->g->player_window->accel_group,
                             GDK_2, 0, GTK_ACCEL_VISIBLE);
  ret->command_menu.seek_menu.seek_30 =
    create_item(TR("Seek to 30%"), gmerlin, ret->command_menu.seek_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.seek_menu.seek_30,
                             "activate", ret->g->player_window->accel_group,
                             GDK_3, 0, GTK_ACCEL_VISIBLE);
  ret->command_menu.seek_menu.seek_40 =
    create_item(TR("Seek to 40%"), gmerlin, ret->command_menu.seek_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.seek_menu.seek_40,
                             "activate", ret->g->player_window->accel_group,
                             GDK_4, 0, GTK_ACCEL_VISIBLE);
  ret->command_menu.seek_menu.seek_50 =
    create_item(TR("Seek to 50%"), gmerlin, ret->command_menu.seek_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.seek_menu.seek_50,
                             "activate", ret->g->player_window->accel_group,
                             GDK_5, 0, GTK_ACCEL_VISIBLE);
  ret->command_menu.seek_menu.seek_60 =
    create_item(TR("Seek to 60%"), gmerlin, ret->command_menu.seek_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.seek_menu.seek_60,
                             "activate", ret->g->player_window->accel_group,
                             GDK_6, 0, GTK_ACCEL_VISIBLE);
  ret->command_menu.seek_menu.seek_70 =
    create_item(TR("Seek to 70%"), gmerlin, ret->command_menu.seek_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.seek_menu.seek_70,
                             "activate", ret->g->player_window->accel_group,
                             GDK_7, 0, GTK_ACCEL_VISIBLE);
  ret->command_menu.seek_menu.seek_80 =
    create_item(TR("Seek to 80%"), gmerlin, ret->command_menu.seek_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.seek_menu.seek_80,
                             "activate", ret->g->player_window->accel_group,
                             GDK_8, 0, GTK_ACCEL_VISIBLE);
  ret->command_menu.seek_menu.seek_90 =
    create_item(TR("Seek to 90%"), gmerlin, ret->command_menu.seek_menu.menu);
  gtk_widget_add_accelerator(ret->command_menu.seek_menu.seek_90,
                             "activate", ret->g->player_window->accel_group,
                             GDK_9, 0, GTK_ACCEL_VISIBLE);

  ret->command_menu.seek_item =
    create_submenu_item(TR("Seek..."),
                        ret->command_menu.seek_menu.menu,
                        ret->command_menu.menu);

  
  /* Accessories */

  ret->accessories_menu.menu = create_menu();

  if(bg_search_file_exec("gmerlin_transcoder", NULL))
    ret->accessories_menu.transcoder =
      create_item(TR("Transcoder"), gmerlin, ret->accessories_menu.menu);

  if(bg_search_file_exec("gmerlin_visualizer", NULL))
    ret->accessories_menu.visualizer =
      create_item(TR("Visualizer"), gmerlin, ret->accessories_menu.menu);

  if(bg_search_file_exec("gmerlin_alsamixer", NULL))
    ret->accessories_menu.mixer =
      create_item(TR("Mixer"), gmerlin, ret->accessories_menu.menu);

  
  /* Main menu */
    
  ret->menu = create_menu();

  ret->audio_stream_item = create_submenu_item(TR("Audio..."),
                                               ret->audio_stream_menu.menu,
                                               ret->menu);

  ret->video_stream_item = create_submenu_item(TR("Video..."),
                                               ret->video_stream_menu.menu,
                                               ret->menu);

  ret->subtitle_stream_item = create_submenu_item(TR("Subtitles..."),
                                                  ret->subtitle_stream_menu.menu,
                                                  ret->menu);
  
  ret->chapter_item = create_submenu_item(TR("Chapters..."),
                                          ret->chapter_menu.menu,
                                          ret->menu);

      
  ret->visualization_item = create_submenu_item(TR("Visualization..."),
                                                ret->visualization_menu.menu,
                                                ret->menu);
  
  ret->windows_item = create_submenu_item(TR("Windows..."),
                                          ret->windows_menu.menu,
                                          ret->menu);

  ret->options_item = create_submenu_item(TR("Options..."),
                                          ret->options_menu.menu,
                                          ret->menu);

  ret->options_item = create_submenu_item(TR("Commands..."),
                                          ret->command_menu.menu,
                                          ret->menu);

  ret->accessories_item = create_submenu_item(TR("Accessories..."),
                                              ret->accessories_menu.menu,
                                              ret->menu);

  ret->help_item = create_submenu_item(TR("Help..."),
                                       ret->help_menu.menu,
                                       ret->menu);
  
  gtk_widget_show(ret->menu);

  
  return ret;
  }

void main_menu_finalize(main_menu_t * ret, gmerlin_t * gmerlin)
  {
  /* Streams */
  
  stream_menu_finalize(&ret->audio_stream_menu);
  stream_menu_finalize(&ret->video_stream_menu);
  stream_menu_finalize(&ret->subtitle_stream_menu);

  /* Visualization */
  visualization_menu_finalize(&ret->visualization_menu, gmerlin);
    
  }

void main_menu_destroy(main_menu_t * m)
  {
  plugin_menu_free(&m->audio_stream_menu.plugin_menu);
  plugin_menu_free(&m->video_stream_menu.plugin_menu);
  plugin_menu_free(&m->visualization_menu.plugin_menu);

  stream_menu_free(&m->audio_stream_menu);
  stream_menu_free(&m->video_stream_menu);
  stream_menu_free(&m->subtitle_stream_menu);
  
  free(m);
  }

GtkWidget * main_menu_get_widget(main_menu_t * m)
  {
  return m->menu;
  }

void main_menu_set_tree_window_item(main_menu_t * m, int state)
  {
  g_signal_handler_block(G_OBJECT(m->windows_menu.mediatree),
                         m->windows_menu.mediatree_id);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(m->windows_menu.mediatree), state);
  g_signal_handler_unblock(G_OBJECT(m->windows_menu.mediatree),
                           m->windows_menu.mediatree_id);
  
  }

void main_menu_set_info_window_item(main_menu_t * m, int state)
  {
  g_signal_handler_block(G_OBJECT(m->windows_menu.infowindow),
                         m->windows_menu.infowindow_id);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(m->windows_menu.infowindow), state);
  g_signal_handler_unblock(G_OBJECT(m->windows_menu.infowindow),
                           m->windows_menu.infowindow_id);
  }

void main_menu_set_log_window_item(main_menu_t * m, int state)
  {
  g_signal_handler_block(G_OBJECT(m->windows_menu.logwindow),
                         m->windows_menu.logwindow_id);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(m->windows_menu.logwindow), state);
  g_signal_handler_unblock(G_OBJECT(m->windows_menu.logwindow),
                           m->windows_menu.logwindow_id);
  }

