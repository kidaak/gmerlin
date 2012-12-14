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

#include <string.h>


#include "gavftools.h"

#include <gmerlin/ov.h>

#define LOG_DOMAIN "gavf-play"

static bg_plug_t * in_plug = NULL;

static char * infile = NULL;

static bg_cfg_section_t * audio_section = NULL;
static bg_cfg_section_t * video_section = NULL;

static int audio_stream = -1;
static int video_stream = -1;

typedef struct
  {
  bg_plugin_handle_t * h;
  bg_oa_plugin_t * plugin;
  bg_parameter_info_t * parameters;
  } audio_stream_t;

typedef struct
  {
  bg_plugin_handle_t * h;
  bg_ov_plugin_t * plugin;
  bg_ov_t * ov;
  bg_parameter_info_t * parameters;
  } video_stream_t;

static const bg_parameter_info_t audio_parameters[] =
  {
    {
      .name      = "plugin",
      .long_name = TRS("Plugin"),
      .type      = BG_PARAMETER_MULTI_MENU,
      .flags     = BG_PARAMETER_PLUGIN,
    },
    { /* End */ },
  };

static const bg_parameter_info_t video_parameters[] =
  {
    {
      .name      = "plugin",
      .long_name = TRS("Plugin"),
      .type      = BG_PARAMETER_MULTI_MENU,
      .flags     = BG_PARAMETER_PLUGIN,
    },
    { /* End */ },
  };

static void init_audio(audio_stream_t * as)
  {
  as->parameters =
    bg_parameter_info_copy_array(audio_parameters);
  
  bg_plugin_registry_set_parameter_info(plugin_reg,
                                        BG_PLUGIN_OUTPUT_AUDIO,
                                        BG_PLUGIN_PLAYBACK,
                                        &as->parameters[0]);
  }

static void init_video(video_stream_t * vs)
  {
  vs->parameters =
    bg_parameter_info_copy_array(video_parameters);
  
  bg_plugin_registry_set_parameter_info(plugin_reg,
                                        BG_PLUGIN_OUTPUT_VIDEO,
                                        BG_PLUGIN_PLAYBACK,
                                        &vs->parameters[0]);
  }

static void set_audio_parameter(void * data,
                                const char * name,
                                const bg_parameter_value_t * val)
  {
  audio_stream_t * s = data;
  
  if(!name)
    return;

  if(!strcmp(name, "plugin"))
    {
    const bg_plugin_info_t * info;
    
    if(s->h &&
       !strcmp(s->h->info->name, val->val_str))
      return;
    
    if(s->h)
      bg_plugin_unref(s->h);
    
    info = bg_plugin_find_by_name(plugin_reg, val->val_str);
    s->h = bg_plugin_load(plugin_reg, info);
    s->plugin = (bg_oa_plugin_t*)(s->h->plugin);
    // if(s->input_plugin->set_callbacks)
    // s->input_plugin->set_callbacks(s->h->priv, &rec->recorder_cb);
    }


  }

static void set_video_parameter(void * data,
                                const char * name,
                                const bg_parameter_value_t * val)
  {
  video_stream_t * s = data;
  
  if(!name)
    return;
  
  if(!strcmp(name, "plugin"))
    {
    const bg_plugin_info_t * info;
    
    if(s->h &&
       !strcmp(s->h->info->name, val->val_str))
      return;
    
    if(s->h)
      bg_plugin_unref(s->h);
    
    info = bg_plugin_find_by_name(plugin_reg, val->val_str);
    s->h = bg_plugin_load(plugin_reg, info);
    s->plugin = (bg_ov_plugin_t*)(s->h->plugin);
    // if(s->input_plugin->set_callbacks)
    // s->input_plugin->set_callbacks(s->h->priv, &rec->recorder_cb);
    }
  }

typedef struct
  {
  audio_stream_t as;
  video_stream_t vs;
  } player_t;

static player_t player;

static void player_init(player_t * p)
  {
  init_audio(&p->as);
  init_video(&p->vs);
  }

static void opt_aud(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    fprintf(stderr, "Option -aud requires an argument\n");
    exit(-1);
    }
  if(!bg_cmdline_apply_options(audio_section,
                               set_audio_parameter,
                               &player.as,
                               player.as.parameters,
                               (*_argv)[arg]))
    exit(-1);
  bg_cmdline_remove_arg(argc, _argv, arg);
  }

static void opt_vid(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    fprintf(stderr, "Option -vid requires an argument\n");
    exit(-1);
    }
  if(!bg_cmdline_apply_options(video_section,
                               set_video_parameter,
                               &player.vs,
                               player.vs.parameters,
                               (*_argv)[arg]))
    exit(-1);
  bg_cmdline_remove_arg(argc, _argv, arg);
  }

static void opt_i(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    fprintf(stderr, "Option -i requires an argument\n");
    exit(-1);
    }
  infile = (*_argv)[arg];
  bg_cmdline_remove_arg(argc, _argv, arg);
  }

static void opt_as(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    fprintf(stderr, "Option -as requires an argument\n");
    exit(-1);
    }
  audio_stream = atoi((*_argv)[arg]) - 1;
  bg_cmdline_remove_arg(argc, _argv, arg);
  }

static void opt_vs(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    fprintf(stderr, "Option -vs requires an argument\n");
    exit(-1);
    }
  video_stream = atoi((*_argv)[arg]) - 1;
  bg_cmdline_remove_arg(argc, _argv, arg);
  }

static bg_cmdline_arg_t global_options[] =
  {
    {
      .arg =         "-aud",
      .help_arg =    "<audio_options>",
      .help_string = "Set audio recording options",
      .callback =    opt_aud,
    },
    {
      .arg =         "-vid",
      .help_arg =    "<video_options>",
      .help_string = "Set video recording options",
      .callback =    opt_vid,
    },
    {
      .arg =         "-as",
      .help_arg =    "<stream>",
      .help_string = "Select audio stream (default: 1)",
      .callback =    opt_as,
    },
    {
      .arg =         "-vs",
      .help_arg =    "<stream>",
      .help_string = "Select video stream (default: 1)",
      .callback =    opt_vs,
    },
    {
      .arg =         "-i",
      .help_arg =    "<location>",
      .help_string = "Set input file or location",
      .callback =    opt_i,
    },
    {
      /* End */
    }
  };

const bg_cmdline_app_data_t app_data =
  {
    .package =  PACKAGE,
    .version =  VERSION,
    .name =     "gavf-play",
    .long_name = TRS("Playback of a gavf stream"),
    .synopsis = TRS("[options]\n"),
    .help_before = TRS("gavf player\n"),
    .args = (bg_cmdline_arg_array_t[]) { { TRS("Options"), global_options },
                                       {  } },
    .files = (bg_cmdline_ext_doc_t[])
    { { "~/.gmerlin/plugins.xml",
        TRS("Cache of the plugin registry (shared by all applicatons)") },
      { "~/.gmerlin/generic/config.xml",
        TRS("Default plugin parameters are read from there. Use gmerlin_plugincfg to change them.") },
      { /* End */ }
    },
    
  };

static const gavf_stream_header_t *
find_stream(gavf_t * g, int type, int index)
  {
  const gavf_stream_header_t * ret;
  if(index < 0)
    ret = gavf_get_stream(g, 0, type);
  else
    {
    ret = gavf_get_stream(g, audio_stream, type);
    if(!ret)
      {
      bg_log(BG_LOG_ERROR, LOG_DOMAIN, "No %s stream %d",
             (type == GAVF_STREAM_AUDIO ? "audio" : "video"),
             index + 1);
      }
    }
  return ret;
  }

int main(int argc, char ** argv)
  {
  const gavf_stream_header_t * as;
  const gavf_stream_header_t * vs;
  const gavf_program_header_t * ph;
  
  gavf_t * g;
  int ret = 1;
  int i;
  
  bg_mediaconnector_t conn;
  
  bg_mediaconnector_init(&conn);
  gavftools_init_registries();

  player_init(&player);

  global_options[0].parameters = player.as.parameters;
  global_options[1].parameters = player.vs.parameters;
  
  bg_cmdline_init(&app_data);
  bg_cmdline_parse(global_options, &argc, &argv, NULL);
  
  in_plug = bg_plug_create_reader(plugin_reg);

  /* Open */

  if(!bg_plug_open_location(in_plug, infile, NULL, NULL))
    return ret;
  
  /* Select streams */
  
  g = bg_plug_get_gavf(in_plug);

  as = find_stream(g, GAVF_STREAM_AUDIO, audio_stream);
  vs = find_stream(g, GAVF_STREAM_VIDEO, video_stream);
  
  if(!player.as.plugin && as)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN,
           "Won't play audio: No plugin specified");
    as = NULL;
    }
  
  if(!player.vs.plugin && vs)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN,
           "Won't play video: No plugin specified");
    vs = NULL;
    }

  if(!as && !vs)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN,
           "Neither audio nor video can be played, exiting");
    return ret;
    }

  ph = gavf_get_program_header(g);
  
  for(i = 0; i < ph->num_streams; i++)
    {
    if((as && (as->id == ph->streams[i].id)) ||
       (vs && (vs->id == ph->streams[i].id)))
      bg_plug_set_stream_action(in_plug, &ph->streams[i],
                                BG_STREAM_ACTION_DECODE);
    else
      bg_plug_set_stream_action(in_plug, &ph->streams[i],
                                BG_STREAM_ACTION_OFF);
    }
  
  
  /* Start plug and set up media converter */
  
  if(!bg_plug_setup_reader(in_plug, &conn))
    return ret;

  /* Initialize output plugins */

  if(as)
    {
    
    }
  if(vs)
    {
    
    }
  
  /* Start media connector */

  

  /* Main loop */
  while(1)
    {
    
    }

  /* Cleanup */

  ret = 0;
  return ret;
  }
