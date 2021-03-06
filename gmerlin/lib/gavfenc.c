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
#include <stdio.h>

#include <config.h>
#include <gmerlin/translation.h>
#include <gmerlin/plugin.h>
#include <gmerlin/utils.h>
#include <gmerlin/pluginfuncs.h>
#include <gmerlin/pluginregistry.h>

#include <gavfenc.h>

#include <gavl/gavf.h>

#include <gmerlin/log.h>
#define LOG_DOMAIN "gavfenc"

typedef struct
  {
  int index;
  gavl_packet_sink_t * psink;
  bg_plugin_handle_t * plugin;
  gavl_compression_info_t ci;
  gavl_metadata_t m;
  
  const gavf_stream_header_t * h;
  } stream_common_t;



typedef struct
  {
  stream_common_t com;
  gavl_audio_format_t format;
  gavl_audio_sink_t * sink;
  } audio_stream_t;

typedef struct
  {
  stream_common_t com;
  gavl_video_format_t format;
  gavl_video_sink_t * sink;
  } video_stream_t;

typedef struct
  {
  stream_common_t com;
  int timescale;
  } text_stream_t;

typedef struct
  {
  stream_common_t com;
  gavl_video_format_t format;
  gavl_video_sink_t * sink;
  } overlay_stream_t;

typedef struct
  {
  int num_audio_streams;
  int num_video_streams;
  int num_text_streams;
  int num_overlay_streams;
  
  audio_stream_t * audio_streams;
  video_stream_t * video_streams;
  text_stream_t * text_streams;
  overlay_stream_t * overlay_streams;
  
  int flags;
  
  gavf_t * enc;
  gavf_io_t * io;

  bg_encoder_callbacks_t * cb;
  gavf_options_t * opt;

  FILE * output;
  char * filename;

  bg_plugin_registry_t * plugin_reg;

  bg_parameter_info_t * audio_parameters;
  bg_parameter_info_t * video_parameters;
  bg_parameter_info_t * overlay_parameters;
  
  gavf_program_header_t * ph;
  } bg_gavf_t;

void * bg_gavfenc_create(bg_plugin_registry_t * plugin_reg)
  {
  bg_gavf_t * ret;
  ret = calloc(1, sizeof(*ret));
  ret->enc = gavf_create();

  ret->ph = gavf_get_program_header(ret->enc);
  
  ret->opt = gavf_get_options(ret->enc);
  ret->plugin_reg = plugin_reg;
  
  return ret;
  }

static void bg_gavf_destroy(void * data)
  {
  bg_gavf_t * f = data;

  if(f->audio_streams)
    free(f->audio_streams);
  if(f->video_streams)
    free(f->video_streams);
  if(f->text_streams)
    free(f->text_streams);
  if(f->overlay_streams)
    free(f->overlay_streams);
  
  if(f->enc)
    gavf_close(f->enc);
  
  free(f);
  }

static const bg_parameter_info_t parameters[] =
  {
    {
      .name      = "format",
      .long_name = TRS("Format"),
      .type      = BG_PARAMETER_STRINGLIST,
      .multi_names  = (const char*[]){ "disk", NULL },
      .multi_labels = (const char*[]){ TRS("Disk"), NULL },
    },
    {
      .name        = "sync_distance",
      .long_name   = TRS("Sync distance (ms)"),
      .type        = BG_PARAMETER_INT,
      .val_min     = { .val_i = 20 },
      .val_max     = { .val_i = 10000 },
      .val_default = { .val_i = 500 },
      .help_string = TRS("Distance between sync headers if no stream has keyframes. Smaller distances produce larger overhead but speed up seeking"),
    },
    {
      .name        = "sync_index",
      .long_name   = TRS("Write sync index"),
      .type        = BG_PARAMETER_CHECKBUTTON,
      .help_string = TRS("Write a synchronisation index to enable seeking"),
      .val_default = { .val_i = 1 },
    },
    {
      .name        = "packet_index",
      .long_name   = TRS("Write packet index"),
      .type        = BG_PARAMETER_CHECKBUTTON,
      .help_string = TRS("Write a packet index to enable faster seeking"),
      .val_default = { .val_i = 1 },
    },
    {
      .name        = "interleave",
      .long_name   = TRS("Interleave"),
      .type        = BG_PARAMETER_CHECKBUTTON,
      .help_string = TRS("Take care of proper interleaving. Enabling this increases encoding latency and memory usage. Disable this if the packets are already properly interleaved."),
      .val_default = { .val_i = 1 },
    },
    { /* End */ }
  };

static const bg_parameter_info_t *
bg_gavf_get_parameters(void * data)
  {
  return parameters;
  }

static void
bg_gavf_set_parameter(void * data, const char * name,
                      const bg_parameter_value_t * val)
  {
  bg_gavf_t * f = data;

  if(!name)
    {
    gavf_options_set_flags(f->opt, f->flags);
    return;
    }
  else if(!strcmp(name, "sync_distance"))
    gavf_options_set_sync_distance(f->opt, val->val_i * 1000);
  else if(!strcmp(name, "sync_index"))
    {
    if(val->val_i)
      f->flags |= GAVF_OPT_FLAG_SYNC_INDEX;
    else
      f->flags &= ~GAVF_OPT_FLAG_SYNC_INDEX;
    }
  else if(!strcmp(name, "packet_index"))
    {
    if(val->val_i)
      f->flags |= GAVF_OPT_FLAG_PACKET_INDEX;
    else
      f->flags &= ~GAVF_OPT_FLAG_PACKET_INDEX;
    }
  else if(!strcmp(name, "interleave"))
    {

    }
  }

/* Codec parameters */

static const bg_parameter_info_t *
bg_gavf_get_audio_parameters(void * data)
  {
  bg_gavf_t * f = data;

  if(!f->audio_parameters)
    {
    f->audio_parameters =
      bg_plugin_registry_create_compressor_parameters(f->plugin_reg,
                                                      BG_PLUGIN_AUDIO_COMPRESSOR);
    }
  return f->audio_parameters;
  }

static const bg_parameter_info_t *
bg_gavf_get_video_parameters(void * data)
  {
  bg_gavf_t * f = data;

  if(!f->video_parameters)
    {
    f->video_parameters =
      bg_plugin_registry_create_compressor_parameters(f->plugin_reg,
                                                      BG_PLUGIN_VIDEO_COMPRESSOR);
    }
  return f->video_parameters;
  }

static const bg_parameter_info_t *
bg_gavf_get_subtitle_overlay_parameters(void * data)
  {
  bg_gavf_t * f = data;

  if(!f->overlay_parameters)
    {
    f->overlay_parameters =
      bg_plugin_registry_create_compressor_parameters(f->plugin_reg,
                                                      BG_PLUGIN_OVERLAY_COMPRESSOR);
    }
  return f->video_parameters;
  }


/* Set parameters */

static void
bg_gavf_set_audio_parameter(void * data, int stream, const char * name,
                            const bg_parameter_value_t * val)
  {
  bg_gavf_t * f = data;

  bg_plugin_registry_set_compressor_parameter(f->plugin_reg,
                                              &f->audio_streams[stream].com.plugin,
                                              name, val);
  }

static void
bg_gavf_set_video_parameter(void * data, int stream, const char * name,
                            const bg_parameter_value_t * val)
  {
  bg_gavf_t * f = data;
  bg_plugin_registry_set_compressor_parameter(f->plugin_reg,
                                              &f->video_streams[stream].com.plugin,
                                              name, val);
  }

static void
bg_gavf_set_overlay_parameter(void * data, int stream, const char * name,
                            const bg_parameter_value_t * val)
  {
  bg_gavf_t * f = data;
  bg_plugin_registry_set_compressor_parameter(f->plugin_reg,
                                              &f->overlay_streams[stream].com.plugin,
                                              name, val);
  }

static void
bg_gavf_set_callbacks(void * data,
                      bg_encoder_callbacks_t * cb)
  {
  bg_gavf_t * f = data;
  f->cb = cb;
  }

static int
bg_gavf_open(void * data, const char * filename,
             const gavl_metadata_t * metadata,
             const gavl_chapter_list_t * chapter_list)
  {
  bg_gavf_t * f = data;

  f->filename = bg_filename_ensure_extension(filename, "gavf");

  if(!bg_encoder_cb_create_output_file(f->cb, f->filename))
    return 0;
  
  if(!(f->output = fopen(f->filename, "wb")))
    return 0;

  f->io = gavf_io_create_file(f->output, 1, 1, 1);

  if(!gavf_open_write(f->enc, f->io, metadata, chapter_list))
    return 0;
  
  return 1;
  }

static int
bg_gavf_writes_compressed_audio(void * priv,
                                const gavl_audio_format_t * format,
                                const gavl_compression_info_t * info)
  {
  return 1;
  }

static int
bg_gavf_writes_compressed_video(void * priv,
                                const gavl_video_format_t * format,
                                const gavl_compression_info_t * info)
  {
  return 1;
  }

static int
bg_gavf_writes_compressed_overlay(void * priv,
                                  const gavl_video_format_t * format,
                                  const gavl_compression_info_t * info)
  {
  return 1;
  }

static audio_stream_t * append_audio_stream(bg_gavf_t * f, const gavl_metadata_t * m,
                                            const gavl_audio_format_t * format)
  {
  audio_stream_t * ret;

  f->audio_streams =
    realloc(f->audio_streams,
            (f->num_audio_streams+1)*sizeof(*f->audio_streams));
  ret = f->audio_streams + f->num_audio_streams;
  f->num_audio_streams++;
  memset(ret, 0, sizeof(*ret));
  gavl_audio_format_copy(&ret->format, format);
  if(m)
    gavl_metadata_copy(&ret->com.m, m);
  return ret;
  }

static video_stream_t * append_video_stream(bg_gavf_t * f, const gavl_metadata_t * m,
                                            const gavl_video_format_t * format)
  {
  video_stream_t * ret;

  f->video_streams =
    realloc(f->video_streams,
            (f->num_video_streams+1)*sizeof(*f->video_streams));

  ret = f->video_streams + f->num_video_streams;
  f->num_video_streams++;
  memset(ret, 0, sizeof(*ret));
  gavl_video_format_copy(&ret->format, format);
  if(m)
    gavl_metadata_copy(&ret->com.m, m);
  return ret;
  }

static text_stream_t *
append_text_stream(bg_gavf_t * f, const gavl_metadata_t * m)
  {
  text_stream_t * ret;

  f->text_streams =
    realloc(f->text_streams,
            (f->num_text_streams+1)*sizeof(*f->text_streams));

  ret = f->text_streams + f->num_text_streams;
  f->num_text_streams++;
  memset(ret, 0, sizeof(*ret));
  if(m)
    gavl_metadata_copy(&ret->com.m, m);
  return ret;
  }

static overlay_stream_t *
append_overlay_stream(bg_gavf_t * f, const gavl_metadata_t * m,
                      const gavl_video_format_t * format)
  {
  overlay_stream_t * ret;

  f->overlay_streams =
    realloc(f->overlay_streams,
            (f->num_overlay_streams+1)*sizeof(*f->overlay_streams));

  ret = f->overlay_streams + f->num_overlay_streams;
  f->num_overlay_streams++;
  memset(ret, 0, sizeof(*ret));
  gavl_video_format_copy(&ret->format, format);
  if(m)
    gavl_metadata_copy(&ret->com.m, m);
  return ret;
  }

static int
bg_gavf_add_audio_stream(void * data,
                         const gavl_metadata_t * m,
                         const gavl_audio_format_t * format)
  {
  audio_stream_t * s;
  bg_gavf_t * f = data;
  s = append_audio_stream(f, m, format);
  gavl_metadata_delete_compression_fields(&s->com.m);
  return f->num_audio_streams-1;
  }

static int
bg_gavf_add_video_stream(void * data,
                         const gavl_metadata_t * m,
                         const gavl_video_format_t * format)
  {
  video_stream_t * s;
  bg_gavf_t * f = data;
  s = append_video_stream(f, m, format);
  gavl_metadata_delete_compression_fields(&s->com.m);
  return f->num_video_streams-1;
  }

static int
bg_gavf_add_text_stream(void * data,
                        const gavl_metadata_t * m,
                        uint32_t * timescale)
  {
  bg_gavf_t * f = data;

  text_stream_t * s = append_text_stream(f, m);
  s->timescale = *timescale;
  return f->num_text_streams-1;
  }

static int
bg_gavf_add_overlay_stream(void * data,
                           const gavl_metadata_t * m,
                           const gavl_video_format_t * format)
  {
  overlay_stream_t * s;
  bg_gavf_t * f = data;
  s = append_overlay_stream(f, m, format);
  gavl_metadata_delete_compression_fields(&s->com.m);
  return f->num_overlay_streams-1;
  }


static int
bg_gavf_add_audio_stream_compressed(void * data,
                                    const gavl_metadata_t * m,
                                    const gavl_audio_format_t * format,
                                    const gavl_compression_info_t * info)
  {
  audio_stream_t * s;
  bg_gavf_t * f = data;
  s = append_audio_stream(f, m, format);
  gavl_compression_info_copy(&s->com.ci, info);
  return f->num_audio_streams-1;
  }

static int
bg_gavf_add_video_stream_compressed(void * data,
                                    const gavl_metadata_t * m,
                                    const gavl_video_format_t * format,
                                    const gavl_compression_info_t * info)
  {
  video_stream_t * s;
  bg_gavf_t * f = data;
  s = append_video_stream(f, m, format);
  gavl_compression_info_copy(&s->com.ci, info);
  return f->num_video_streams-1;
  }

static int
bg_gavf_add_overlay_stream_compressed(void * data,
                                      const gavl_metadata_t * m,
                                      const gavl_video_format_t * format,
                                      const gavl_compression_info_t * info)
  {
  overlay_stream_t * s;
  bg_gavf_t * f = data;
  s = append_overlay_stream(f, m, format);
  gavl_compression_info_copy(&s->com.ci, info);
  return f->num_overlay_streams-1;
  }

static int bg_gavf_start(void * data)
  {
  bg_gavf_t * priv;
  int i;
  priv = data;
  for(i = 0; i < priv->num_audio_streams; i++)
    {
    audio_stream_t * s = priv->audio_streams + i;

    if(s->com.plugin && (s->com.ci.id == GAVL_CODEC_ID_NONE))
      {
      bg_codec_plugin_t * p = (bg_codec_plugin_t*)s->com.plugin->plugin;
      /* Switch on codec */
      
      s->sink = p->open_encode_audio(s->com.plugin->priv,
                                     &s->com.ci,
                                     &s->format,
                                     &s->com.m);
      if(!s->sink)
        return 0;
      }

    s->com.index =
      gavf_add_audio_stream(priv->enc, &s->com.ci, &s->format, &s->com.m);
    
    }
  for(i = 0; i < priv->num_video_streams; i++)
    {
    video_stream_t * s = priv->video_streams + i;

    if(s->com.plugin && (s->com.ci.id == GAVL_CODEC_ID_NONE))
      {
      bg_codec_plugin_t * p = (bg_codec_plugin_t*)s->com.plugin->plugin;
      /* Switch on codec */
      s->sink = p->open_encode_video(s->com.plugin->priv,
                                     &s->com.ci,
                                     &s->format,
                                     &s->com.m);
      if(!s->sink)
        return 0;
      }
    
    s->com.index = gavf_add_video_stream(priv->enc, &s->com.ci,
                                         &s->format, &s->com.m);

    
    }
  for(i = 0; i < priv->num_text_streams; i++)
    {
    text_stream_t * s = priv->text_streams + i;
    s->com.index = gavf_add_text_stream(priv->enc, s->timescale, &s->com.m);
    }

  for(i = 0; i < priv->num_overlay_streams; i++)
    {
    overlay_stream_t * s = priv->overlay_streams + i;

    if(s->com.plugin && (s->com.ci.id == GAVL_CODEC_ID_NONE))
      {
      bg_codec_plugin_t * p = (bg_codec_plugin_t*)s->com.plugin->plugin;
      /* Switch on codec */
      s->sink = p->open_encode_overlay(s->com.plugin->priv,
                                       &s->com.ci,
                                       &s->format,
                                       &s->com.m);
      if(!s->sink)
        return 0;
      }
    s->com.index = gavf_add_overlay_stream(priv->enc, &s->com.ci,
                                           &s->format, &s->com.m);
    }
  
  if(!gavf_start(priv->enc))
    return 0;

  for(i = 0; i < priv->num_audio_streams; i++)
    {
    audio_stream_t * s = priv->audio_streams + i;
    s->com.h = priv->ph->streams + s->com.index;
    if(s->sink) // Codec present and active
      {
      bg_codec_plugin_t * p = (bg_codec_plugin_t*)s->com.plugin->plugin;
      s->com.psink = gavf_get_packet_sink(priv->enc, s->com.h->id);
      p->set_packet_sink(s->com.plugin->priv,
                         s->com.psink);
      }
    else if(s->com.ci.id == GAVL_CODEC_ID_NONE) // Write uncompressed
      s->sink = gavf_get_audio_sink(priv->enc, s->com.h->id);
    else // Write compressed
      s->com.psink = gavf_get_packet_sink(priv->enc, s->com.h->id);
    }
  for(i = 0; i < priv->num_video_streams; i++)
    {
    video_stream_t * s = priv->video_streams + i;
    s->com.h = priv->ph->streams + s->com.index;
    if(s->sink) // Codec present and active
      {
      bg_codec_plugin_t * p = (bg_codec_plugin_t*)s->com.plugin->plugin;
      s->com.psink = gavf_get_packet_sink(priv->enc, s->com.h->id);
      p->set_packet_sink(s->com.plugin->priv,s->com.psink);
      }
    else if(s->com.ci.id == GAVL_CODEC_ID_NONE) // Write uncompressed
      s->sink = gavf_get_video_sink(priv->enc, s->com.h->id);
    else // Write compressed
      s->com.psink = gavf_get_packet_sink(priv->enc, s->com.h->id);
    }
  for(i = 0; i < priv->num_overlay_streams; i++)
    {
    overlay_stream_t * s = priv->overlay_streams + i;
    s->com.h = priv->ph->streams + s->com.index;
    if(s->sink) // Codec present and active
      {
      bg_codec_plugin_t * p = (bg_codec_plugin_t*)s->com.plugin->plugin;
      s->com.psink = gavf_get_packet_sink(priv->enc, s->com.h->id);
      p->set_packet_sink(s->com.plugin->priv,s->com.psink);
      }
    else if(s->com.ci.id == GAVL_CODEC_ID_NONE) // Write uncompressed
      s->sink = gavf_get_video_sink(priv->enc, s->com.h->id);
    else // Write compressed
      s->com.psink = gavf_get_packet_sink(priv->enc, s->com.h->id);
    }
  for(i = 0; i < priv->num_text_streams; i++)
    {
    text_stream_t * s = priv->text_streams + i;
    s->com.h = priv->ph->streams + s->com.index;
    s->com.psink = gavf_get_packet_sink(priv->enc, s->com.h->id);
    }
  
  
  return 1;
  }

/* LEGACY */
#if 0
static void
bg_gavf_get_audio_format(void * data, int stream,
                         gavl_audio_format_t*ret)
  {
  bg_gavf_t * priv;
  priv = data;

  gavl_audio_format_copy(ret, &priv->audio_streams[stream].format);
  }

static void
bg_gavf_get_video_format(void * data, int stream,
                         gavl_video_format_t*ret)
  {
  bg_gavf_t * priv;
  priv = data;

  gavl_video_format_copy(ret, &priv->video_streams[stream].format);
  }

static int
bg_gavf_write_audio_frame(void * data,
                          gavl_audio_frame_t * frame, int stream)
  {
  bg_gavf_t * f = data;
  return gavl_audio_sink_put_frame(f->audio_streams[stream].sink,
                                   frame) == GAVL_SINK_OK;
  }

/* LEGACY */
static int
bg_gavf_write_video_frame(void * data,
                          gavl_video_frame_t * frame, int stream)
  {
  bg_gavf_t * f = data;
  return gavl_video_sink_put_frame(f->video_streams[stream].sink,
                                   frame) == GAVL_SINK_OK;
  }

static int
bg_gavf_write_subtitle_text(void * data, const char * text,
                            int64_t start,
                            int64_t duration, int stream)
  {
  bg_gavf_t * f = data;
  gavl_packet_t p;
  int ret;
  int len;
  
  gavl_packet_init(&p);
  len = strlen(text);

  gavl_packet_alloc(&p, len);
  memcpy(p.data, text, len);
  p.data_len = len;
  p.pts = start;
  p.duration = duration;
  
  ret = gavl_packet_sink_put_packet(f->text_streams[stream].com.psink, &p);
  gavl_packet_free(&p);
  return ret;
  }
#endif

static void free_stream_common(stream_common_t * s)
  {
  if(s->plugin)
    bg_plugin_unref(s->plugin);
  gavl_metadata_free(&s->m);
  gavl_compression_info_free(&s->ci);
  }

static int
bg_gavf_close(void * data, int do_delete)
  {
  int i;
  bg_gavf_t * priv = data;

  /* TODO: Clarify when sinks have to be destroyed */
  
  /* Close some encoders, this might flush some more packets */
  for(i = 0; i < priv->num_audio_streams; i++)
    {
    audio_stream_t * s = priv->audio_streams + i;
    free_stream_common(&s->com);
    }
  for(i = 0; i < priv->num_video_streams; i++)
    {
    video_stream_t * s = priv->video_streams + i;
    free_stream_common(&s->com);
    }
  for(i = 0; i < priv->num_text_streams; i++)
    {
    text_stream_t * s = priv->text_streams + i;
    free_stream_common(&s->com);
    }
  
  priv->num_audio_streams = 0;
  priv->num_video_streams = 0;
  priv->num_text_streams = 0;
  
  gavf_close(priv->enc);
  priv->enc = NULL;
  
  gavf_io_destroy(priv->io);
  
  return 1;
  }

#if 0
/* LEGACY */
static int
bg_gavf_write_audio_packet(void * data, gavl_packet_t * packet,
                           int stream)
  {
  bg_gavf_t * f = data;
  return gavl_packet_sink_put_packet(f->audio_streams[stream].com.psink,
                                     packet) == GAVL_SINK_OK;
  }

/* LEGACY */
static int
bg_gavf_write_video_packet(void * data, gavl_packet_t * packet,
                           int stream)
  {
  bg_gavf_t * f = data;
  return gavl_packet_sink_put_packet(f->video_streams[stream].com.psink,
                                     packet) == GAVL_SINK_OK;
  }
#endif

static gavl_audio_sink_t * bg_gavf_get_audio_sink(void * data, int stream)
  {
  bg_gavf_t * f = data;
  return f->audio_streams[stream].sink;
  }

static gavl_video_sink_t * bg_gavf_get_video_sink(void * data, int stream)
  {
  bg_gavf_t * f = data;
  return f->video_streams[stream].sink;
  }

static gavl_video_sink_t * bg_gavf_get_overlay_sink(void * data, int stream)
  {
  bg_gavf_t * f = data;
  return f->overlay_streams[stream].sink;
  }

static gavl_packet_sink_t *
bg_gavf_get_audio_packet_sink(void * data, int stream)
  {
  bg_gavf_t * f = data;
  return f->audio_streams[stream].com.psink;
  }

static gavl_packet_sink_t *
bg_gavf_get_video_packet_sink(void * data, int stream)
  {
  bg_gavf_t * f = data;
  return f->video_streams[stream].com.psink;
  }

static gavl_packet_sink_t *
bg_gavf_get_overlay_packet_sink(void * data, int stream)
  {
  bg_gavf_t * f = data;
  return f->overlay_streams[stream].com.psink;
  }

static gavl_packet_sink_t *
bg_gavf_get_text_sink(void * data, int stream)
  {
  bg_gavf_t * f = data;
  return f->text_streams[stream].com.psink;
  }


const bg_encoder_plugin_t the_plugin =
  {
    .common =
    {
      BG_LOCALE,
      .name =           bg_gavfenc_name,       /* Unique short name */
      .long_name =      TRS("GAVF encoder"),
      .description =    TRS("Plugin for encoding the Gmerlin audio video format."),
      .type =           BG_PLUGIN_ENCODER,
      .flags =          BG_PLUGIN_FILE,
      .priority =       5,
      //      .create =         bg_gavf_create,
      .destroy =        bg_gavf_destroy,
      .get_parameters = bg_gavf_get_parameters,
      .set_parameter =  bg_gavf_set_parameter,
    },
    
    .max_audio_streams =         -1,
    .max_video_streams =         -1,
    .max_text_streams = -1,
    .max_overlay_streams = -1,
    
    .get_audio_parameters = bg_gavf_get_audio_parameters,
    .get_video_parameters = bg_gavf_get_video_parameters,
    .get_overlay_parameters = bg_gavf_get_subtitle_overlay_parameters,
    
    .set_callbacks =        bg_gavf_set_callbacks,
    
    .open =                 bg_gavf_open,

    .writes_compressed_audio = bg_gavf_writes_compressed_audio,
    .writes_compressed_video = bg_gavf_writes_compressed_video,
    .writes_compressed_overlay = bg_gavf_writes_compressed_overlay,
    
    .add_audio_stream =     bg_gavf_add_audio_stream,
    .add_video_stream =     bg_gavf_add_video_stream,
    .add_text_stream =     bg_gavf_add_text_stream,
    .add_overlay_stream =     bg_gavf_add_overlay_stream,

    .add_audio_stream_compressed =     bg_gavf_add_audio_stream_compressed,
    .add_video_stream_compressed =     bg_gavf_add_video_stream_compressed,
    .add_overlay_stream_compressed =   bg_gavf_add_overlay_stream_compressed,

    // .set_video_pass =       bg_gavf_set_video_pass,
    .set_audio_parameter =  bg_gavf_set_audio_parameter,
    .set_video_parameter =  bg_gavf_set_video_parameter,
    .set_overlay_parameter =  bg_gavf_set_overlay_parameter,
    
    //    .get_audio_format =     bg_gavf_get_audio_format,
    //    .get_video_format =     bg_gavf_get_video_format,

    .start =                bg_gavf_start,

    .get_audio_sink =        bg_gavf_get_audio_sink,
    .get_video_sink =        bg_gavf_get_video_sink,
    .get_audio_packet_sink = bg_gavf_get_audio_packet_sink,
    .get_video_packet_sink = bg_gavf_get_video_packet_sink,
    .get_overlay_packet_sink = bg_gavf_get_overlay_packet_sink,
    .get_text_sink = bg_gavf_get_text_sink,
    .get_overlay_sink = bg_gavf_get_overlay_sink,
    //    .get_subtitle__sink = bg_gavf_get_text_sink,
    
    .close =                bg_gavf_close,
  };

bg_plugin_info_t * bg_gavfenc_info(bg_plugin_registry_t * reg)
  {
  bg_plugin_info_t * ret;

  ret = bg_plugin_info_create(&the_plugin.common);
  /* TODO: Create audio and video codec parameters */

  ret->audio_parameters =
    bg_plugin_registry_create_compressor_parameters(reg, BG_PLUGIN_AUDIO_COMPRESSOR);
  ret->video_parameters =
    bg_plugin_registry_create_compressor_parameters(reg, BG_PLUGIN_VIDEO_COMPRESSOR);
  ret->overlay_parameters =
    bg_plugin_registry_create_compressor_parameters(reg, BG_PLUGIN_OVERLAY_COMPRESSOR);
  
  return ret;

  }

const bg_plugin_common_t * bg_gavfenc_get()
  {
  return (const bg_plugin_common_t*)(&the_plugin);
  }
