#include <stdlib.h>
#include <string.h>

#include <pluginregistry.h>
#include <utils.h>

#include <cfg_dialog.h>
#include <transcoder_track.h>
#include "trackdialog.h"

#include <textrenderer.h>


struct track_dialog_s
  {
  /* Config dialog */

  bg_dialog_t * cfg_dialog;

  void (*update_callback)(void * priv);
  void * update_priv;
  };

static void set_parameter_general(void * priv, char * name, bg_parameter_value_t * val)
  {
  track_dialog_t * d;
  d = (track_dialog_t *)priv;

  if(!name)
    {
    if(d->update_callback)
      d->update_callback(d->update_priv);
    }
  }

track_dialog_t * track_dialog_create(bg_transcoder_track_t * t,
                                     void (*update_callback)(void * priv),
                                     void * update_priv, int show_tooltips,
                                     bg_plugin_registry_t * plugin_reg)
  {
  int i;
  char * label;
  track_dialog_t * ret;
  void * parent;
  bg_transcoder_encoder_info_t encoder_info;
  
  bg_transcoder_encoder_info_get_from_track(plugin_reg, t, &encoder_info);
                                            
  ret = calloc(1, sizeof(*ret));

  ret->update_callback = update_callback;
  ret->update_priv     = update_priv;
  
  ret->cfg_dialog = bg_dialog_create_multi("Track options");
  
  /* General */
  
  bg_dialog_add(ret->cfg_dialog,
                "General",
                t->general_section,
                set_parameter_general, ret,
                t->general_parameters);

  
  /* Metadata */
  
  bg_dialog_add(ret->cfg_dialog,
                "Metadata",
                t->metadata_section,
                NULL,
                NULL,
                t->metadata_parameters);

  /* Audio encoder */

  if(encoder_info.audio_encoder_parameters && t->num_audio_streams)
    {
    bg_dialog_add(ret->cfg_dialog,
                  bg_cfg_section_get_name(t->audio_encoder_section),
                  t->audio_encoder_section,
                  NULL,
                  NULL,
                  encoder_info.audio_encoder_parameters);
    }

  /* Video encoder */

  if(encoder_info.video_encoder_parameters &&
     (!(encoder_info.audio_info) || t->num_video_streams))
    {
    bg_dialog_add(ret->cfg_dialog,
                  bg_cfg_section_get_name(t->video_encoder_section),
                  t->video_encoder_section,
                  NULL,
                  NULL,
                  encoder_info.video_encoder_parameters);
    }

  /* Subtitle text encoder */

  if(encoder_info.subtitle_text_encoder_parameters && t->num_subtitle_text_streams)
    {
    bg_dialog_add(ret->cfg_dialog,
                  bg_cfg_section_get_name(t->subtitle_text_encoder_section),
                  t->subtitle_text_encoder_section,
                  NULL,
                  NULL,
                  encoder_info.subtitle_text_encoder_parameters);
    }

  /* Subtitle overlay encoder */

  if(encoder_info.subtitle_overlay_encoder_parameters &&
     (t->num_subtitle_text_streams || t->num_subtitle_overlay_streams))
    {
    bg_dialog_add(ret->cfg_dialog,
                  bg_cfg_section_get_name(t->subtitle_overlay_encoder_section),
                  t->subtitle_overlay_encoder_section,
                  NULL,
                  NULL,
                  encoder_info.subtitle_overlay_encoder_parameters);
    }

  
  /* Audio streams */

  for(i = 0; i < t->num_audio_streams; i++)
    {
    if(t->num_audio_streams > 1)
      {
      if(t->audio_streams[i].label)
        label = bg_sprintf("Audio #%d: %s", i+1, t->audio_streams[i].label);
      else
        label = bg_sprintf("Audio #%d", i+1);
      }
    else
      {
      if(t->audio_streams[i].label)
        label = bg_sprintf("Audio: %s", t->audio_streams[i].label);
      else
        label = bg_sprintf("Audio");
      }
    
    
    parent = bg_dialog_add_parent(ret->cfg_dialog, NULL,
                                  label);
    free(label);
    
    bg_dialog_add_child(ret->cfg_dialog, parent,
                        "General",
                        t->audio_streams[i].general_section,
                        NULL,
                        NULL,
                        bg_transcoder_track_audio_get_general_parameters());

    if(encoder_info.audio_stream_parameters)
      bg_dialog_add_child(ret->cfg_dialog, parent,
                          bg_cfg_section_get_name(t->audio_streams[i].encoder_section),
                          t->audio_streams[i].encoder_section,
                          NULL,
                          NULL,
                          encoder_info.audio_stream_parameters);
    }

  /* Video streams */

  for(i = 0; i < t->num_video_streams; i++)
    {
    if(t->num_video_streams > 1)
      {
      if(t->video_streams[i].label)
        label = bg_sprintf("Video #%d: %s", i+1, t->video_streams[i].label);
      else
        label = bg_sprintf("Video #%d", i+1);
      }
    else
      {
      if(t->video_streams[i].label)
        label = bg_sprintf("Video: %s", t->video_streams[i].label);
      else
        label = bg_sprintf("Video");
      }

    parent = bg_dialog_add_parent(ret->cfg_dialog, NULL,
                                  label);
    free(label);
    
    bg_dialog_add_child(ret->cfg_dialog, parent,
                        "General",
                        t->video_streams[i].general_section,
                        NULL,
                        NULL,
                        bg_transcoder_track_video_get_general_parameters());

    if(encoder_info.video_stream_parameters)
      bg_dialog_add_child(ret->cfg_dialog, parent,
                          bg_cfg_section_get_name(t->video_streams[i].encoder_section),
                          t->video_streams[i].encoder_section,
                          NULL,
                          NULL,
                          encoder_info.video_stream_parameters);
    }

  /* Subtitle streams */

  for(i = 0; i < t->num_subtitle_text_streams; i++)
    {
    if(t->num_subtitle_text_streams > 1)
      {
      if(t->subtitle_text_streams[i].label)
        label = bg_sprintf("Subtitles #%d: %s", i+1, t->subtitle_text_streams[i].label);
      else
        label = bg_sprintf("Subtitles #%d", i+1);
      }
    else
      {
      if(t->subtitle_text_streams[i].label)
        label = bg_sprintf("Subtitles: %s", t->subtitle_text_streams[i].label);
      else
        label = bg_sprintf("Subtitles");
      }
    
    parent = bg_dialog_add_parent(ret->cfg_dialog, NULL,
                                  label);
    free(label);
    
    bg_dialog_add_child(ret->cfg_dialog, parent,
                        "General",
                        t->subtitle_text_streams[i].general_section,
                        NULL,
                        NULL,
                        t->subtitle_text_streams[i].general_parameters);

    bg_dialog_add_child(ret->cfg_dialog, parent,
                        "Textrenderer",
                        t->subtitle_text_streams[i].textrenderer_section,
                        NULL,
                        NULL,
                        bg_text_renderer_get_parameters());
    }

  for(i = 0; i < t->num_subtitle_overlay_streams; i++)
    {
    if(t->num_subtitle_overlay_streams > 1)
      {
      if(t->subtitle_overlay_streams[i].label)
        label = bg_sprintf("Subtitles #%d: %s", i+1+t->num_subtitle_text_streams,
                           t->subtitle_overlay_streams[i].label);
      else
        label = bg_sprintf("Subtitles #%d", i+1+t->num_subtitle_text_streams);
      }
    else
      {
      if(t->subtitle_overlay_streams[i].label)
        label = bg_sprintf("Subtitles: %s", t->subtitle_overlay_streams[i].label);
      else
        label = bg_sprintf("Subtitles");
      }
    
    parent = bg_dialog_add_parent(ret->cfg_dialog, NULL,
                                  label);
    free(label);
    
    bg_dialog_add_child(ret->cfg_dialog, parent,
                        "General",
                        t->subtitle_overlay_streams[i].general_section,
                        NULL,
                        NULL,
                        t->subtitle_overlay_streams[i].general_parameters);
    }
  
  bg_dialog_set_tooltips(ret->cfg_dialog, show_tooltips);

  
  return ret;
  
  }

void track_dialog_run(track_dialog_t * d)
  {
  bg_dialog_show(d->cfg_dialog);
  }

void track_dialog_destroy(track_dialog_t * d)
  {
  bg_dialog_destroy(d->cfg_dialog);
  free(d);
  }

