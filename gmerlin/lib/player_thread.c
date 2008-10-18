/*****************************************************************
 * gmerlin - a general purpose multimedia framework and applications
 *
 * Copyright (c) 2001 - 2008 Members of the Gmerlin project
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
#include <stdlib.h>
#include <stdio.h>

#include <gmerlin/player.h>
#include <playerprivate.h>
#include <gmerlin/log.h>

#define LOG_DOMAIN "player"

/* Metadata */

static void msg_metadata(bg_msg_t * msg, const void * data)
  {
  bg_metadata_t * m = (bg_metadata_t *)data;
  bg_msg_set_id(msg, BG_PLAYER_MSG_METADATA);
  bg_msg_set_arg_metadata(msg, 0, m);
  }

static void msg_time(bg_msg_t * msg,
                     const void * data)
  {
  bg_msg_set_id(msg, BG_PLAYER_MSG_TIME_CHANGED);
  bg_msg_set_arg_time(msg, 0, *((const gavl_time_t*)data));
  }

static void msg_cleanup(bg_msg_t * msg,
                        const void * data)
  {
  bg_msg_set_id(msg, BG_PLAYER_MSG_CLEANUP);
  }

static void msg_interrupt(bg_msg_t * msg,
                          const void * data)
  {
  bg_msg_set_id(msg, BG_PLAYER_MSG_INTERRUPT);
  }

static void msg_interrupt_resume(bg_msg_t * msg,
                                 const void * data)
  {
  bg_msg_set_id(msg, BG_PLAYER_MSG_INTERRUPT_RESUME);
  }

static void msg_volume_changed(bg_msg_t * msg,
                               const void * data)
  {
  bg_msg_set_id(msg, BG_PLAYER_MSG_VOLUME_CHANGED);
  bg_msg_set_arg_float(msg, 0, *((float*)data));
  }

static void msg_audio_stream(bg_msg_t * msg,
                             const void * data)
  {
  const bg_player_t * player;
  player = (const bg_player_t*)data;
  bg_msg_set_id(msg, BG_PLAYER_MSG_AUDIO_STREAM);
  bg_msg_set_arg_int(msg, 0, player->current_audio_stream);
  bg_msg_set_arg_audio_format(msg, 1,
                              &(player->audio_stream.input_format));
  bg_msg_set_arg_audio_format(msg, 2,
                              &(player->audio_stream.output_format));
  
  }

static void msg_video_stream(bg_msg_t * msg,
                             const void * data)
  {
  bg_player_t * player;
  player = (bg_player_t*)data;

  bg_msg_set_id(msg, BG_PLAYER_MSG_VIDEO_STREAM);
  bg_msg_set_arg_int(msg, 0, player->current_video_stream);
  bg_msg_set_arg_video_format(msg, 1,
                              &(player->video_stream.input_format));
  bg_msg_set_arg_video_format(msg, 2,
                              &(player->video_stream.output_format));
  
  }

static void msg_subtitle_stream(bg_msg_t * msg,
                                const void * data)
  {
  bg_player_t * player;
  player = (bg_player_t*)data;

  bg_msg_set_id(msg, BG_PLAYER_MSG_SUBTITLE_STREAM);
  bg_msg_set_arg_int(msg, 0, player->current_subtitle_stream);
  bg_msg_set_arg_int(msg, 1, !!DO_SUBTITLE_TEXT(player->flags));
  
  bg_msg_set_arg_video_format(msg, 2,
                              &(player->subtitle_stream.input_format));
  bg_msg_set_arg_video_format(msg, 3,
                              &(player->subtitle_stream.output_format));
  

  }

static void msg_num_streams(bg_msg_t * msg,
                            const void * data)
  {
  bg_track_info_t * info;
  info = (bg_track_info_t *)data;
  bg_msg_set_id(msg, BG_PLAYER_MSG_TRACK_NUM_STREAMS);

  bg_msg_set_arg_int(msg, 0, info->num_audio_streams);
  bg_msg_set_arg_int(msg, 1, info->num_video_streams);
  bg_msg_set_arg_int(msg, 2, info->num_subtitle_streams);
  }

struct stream_info_s
  {
  bg_track_info_t * track;
  int index;
  };

static void msg_audio_stream_info(bg_msg_t * msg, const void * data)
  {
  const struct stream_info_s * si;
  si = (const struct stream_info_s*)data;
  bg_msg_set_id(msg, BG_PLAYER_MSG_AUDIO_STREAM_INFO);

  bg_msg_set_arg_int(msg,    0, si->index);
  bg_msg_set_arg_string(msg, 1, si->track->audio_streams[si->index].info);
  bg_msg_set_arg_string(msg, 2, si->track->audio_streams[si->index].language);
  }

static void msg_video_stream_info(bg_msg_t * msg, const void * data)
  {
  const struct stream_info_s * si;
  si = (const struct stream_info_s*)data;
  bg_msg_set_id(msg, BG_PLAYER_MSG_VIDEO_STREAM_INFO);

  bg_msg_set_arg_int(msg,    0, si->index);
  bg_msg_set_arg_string(msg, 1, si->track->video_streams[si->index].info);
  bg_msg_set_arg_string(msg, 2, si->track->video_streams[si->index].language);
  }

static void msg_subtitle_stream_info(bg_msg_t * msg, const void * data)
  {
  const struct stream_info_s * si;
  si = (const struct stream_info_s*)data;
  bg_msg_set_id(msg, BG_PLAYER_MSG_SUBTITLE_STREAM_INFO);

  bg_msg_set_arg_int(msg,    0, si->index);
  bg_msg_set_arg_string(msg, 1, si->track->subtitle_streams[si->index].info);
  bg_msg_set_arg_string(msg, 2, si->track->subtitle_streams[si->index].language);
  }

static void msg_video_description(bg_msg_t * msg, const void * data)
  {
  bg_msg_set_id(msg, BG_PLAYER_MSG_VIDEO_DESCRIPTION);
  bg_msg_set_arg_string(msg, 0, (char*)data);
  }


static void msg_audio_description(bg_msg_t * msg, const void * data)
  {
  bg_msg_set_id(msg, BG_PLAYER_MSG_AUDIO_DESCRIPTION);
  bg_msg_set_arg_string(msg, 0, (char*)data);
  }

static void msg_stream_description(bg_msg_t * msg, const void * data)
  {
  bg_msg_set_id(msg, BG_PLAYER_MSG_STREAM_DESCRIPTION);
  bg_msg_set_arg_string(msg, 0, (char*)data);
  }


static void msg_mute(bg_msg_t * msg, const void * data)
  {
  bg_msg_set_id(msg, BG_PLAYER_MSG_MUTE);
  bg_msg_set_arg_int(msg, 0, *((int*)data));
  }

#if 0 

static void msg_subpicture_description(bg_msg_t * msg, const void * data)
  {
  bg_msg_set_id(msg, BG_PLAYER_MSG_SUBPICTURE_DESCRIPTION);
  bg_msg_set_arg_string(msg, 0, (char*)data);
  }

#endif

static void msg_num_chapters(bg_msg_t * msg, const void * data)
  {
  bg_chapter_list_t * l;
  l = (bg_chapter_list_t *)data;
  bg_msg_set_id(msg, BG_PLAYER_MSG_NUM_CHAPTERS);

  if(l)
    {
    bg_msg_set_arg_int(msg, 0, l->num_chapters);
    bg_msg_set_arg_int(msg, 1, l->timescale);
    }
  else
    {
    bg_msg_set_arg_int(msg, 0, 0);
    bg_msg_set_arg_int(msg, 1, 0);
    }
  }

struct chapter_info_s
  {
  bg_chapter_list_t * l;
  int index;
  };

static void msg_chapter_info(bg_msg_t * msg, const void * data)
  {
  struct chapter_info_s * d = (struct chapter_info_s*)data;
  bg_msg_set_id(msg, BG_PLAYER_MSG_CHAPTER_INFO);
  bg_msg_set_arg_int(msg, 0, d->index);
  bg_msg_set_arg_string(msg, 1, d->l->chapters[d->index].name);
  bg_msg_set_arg_time(msg, 2, d->l->chapters[d->index].time);
  }

static void msg_chapter_changed(bg_msg_t * msg, const void * data)
  {
  bg_msg_set_id(msg, BG_PLAYER_MSG_CHAPTER_CHANGED);
  //  fprintf(stderr, "New chapter: %d\n", *((int*)data));
  bg_msg_set_arg_int(msg, 0, *((int*)data));
  }

/*
 *  Interrupt playback so all plugin threads are waiting inside
 *  keep_going();
 */

static void interrupt_cmd(bg_player_t * p, int new_state)
  {
  int old_state;
  
  pthread_mutex_lock(&(p->stop_mutex));

  /* Get the old state */
  old_state = bg_player_get_state(p);
  
  /* Set the new state */
  bg_player_set_state(p, new_state, NULL, NULL);

  if(old_state == BG_PLAYER_STATE_PAUSED)
    {
    pthread_mutex_unlock(&p->stop_mutex);
    return;
    }
  /* Tell it to the fifos */

  if(DO_AUDIO(p->flags))
    bg_fifo_set_state(p->audio_stream.fifo, BG_FIFO_PAUSED);
  if(DO_VIDEO(p->flags))
    bg_fifo_set_state(p->video_stream.fifo, BG_FIFO_PAUSED);
  
  //  if(p->waiting_plugin_threads < p->total_plugin_threads)
  pthread_cond_wait(&(p->stop_cond), &(p->stop_mutex));
  
  pthread_mutex_unlock(&p->stop_mutex);
  bg_player_time_stop(p);

  if(DO_AUDIO(p->flags))
    bg_player_oa_stop(p->oa_context);
  }

/* Preload fifos */

static void preload(bg_player_t * p)
  {
  
  bg_player_input_preload(p->input_context);

  }

/* Start playback */

static void start_playback(bg_player_t * p, int new_state)
  {
  int want_new;
  if(new_state == BG_PLAYER_STATE_CHANGING)
    {
    want_new = 0;
    bg_player_set_state(p, new_state, &want_new, NULL);
    }
  else
    bg_player_set_state(p, new_state, NULL, NULL);
    

  /* Start timer */
  
  bg_player_time_start(p);

  if(DO_AUDIO(p->flags))
    bg_player_oa_start(p->oa_context);
  
  pthread_mutex_lock(&(p->start_mutex));
  pthread_cond_broadcast(&(p->start_cond));
  pthread_mutex_unlock(&(p->start_mutex));
  }

/* Pause command */

static void pause_cmd(bg_player_t * p)
  {
  int state;

  if(!p->can_pause)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Cannot pause live stream");
    return;
    }

  state = bg_player_get_state(p);
  
  if(state == BG_PLAYER_STATE_PLAYING)
    {
    interrupt_cmd(p, BG_PLAYER_STATE_PAUSED);

    if(p->do_bypass)
      bg_player_input_bypass_set_pause(p->input_context, 1);

    /* Now that all threads are stopped, we can go back to play mode */
    if(DO_AUDIO(p->flags))
      bg_fifo_set_state(p->audio_stream.fifo, BG_FIFO_PLAYING);
    if(DO_VIDEO(p->flags))
      bg_fifo_set_state(p->video_stream.fifo, BG_FIFO_PLAYING);
    
    }
  else if(state == BG_PLAYER_STATE_PAUSED)
    {
    preload(p);
    if(p->do_bypass)
      bg_player_input_bypass_set_pause(p->input_context, 0);

    start_playback(p, BG_PLAYER_STATE_PLAYING);
    }
  }

static int init_audio_stream(bg_player_t * p)
  {
  if(!bg_player_audio_init(p, p->current_audio_stream))
    {
    //    bg_player_set_state(p, BG_PLAYER_STATE_ERROR,
    //                    "Cannot setup audio playback", NULL);
    bg_player_set_state(p, BG_PLAYER_STATE_ERROR,
                        NULL, NULL);
    return 0;
    }
  return 1;  
  }

static int init_video_stream(bg_player_t * p)
  {
  if(!bg_player_video_init(p, p->current_video_stream))
    {
    bg_player_set_state(p, BG_PLAYER_STATE_ERROR,
                        NULL, NULL);
    return 0;
    }
  return 1;
  }

static int init_subtitle_stream(bg_player_t * p)
  {
  if(!bg_player_subtitle_init(p, p->current_subtitle_stream))
    {
    bg_player_set_state(p, BG_PLAYER_STATE_ERROR,
                        NULL, NULL);
    return 0;
    }
  return 1;
  }

/* Initialize playback pipelines */

static int init_streams(bg_player_t * p)
  {
  if(DO_SUBTITLE_ONLY(p->flags))
    {
    if(!init_audio_stream(p) ||
       !init_subtitle_stream(p) ||
       !init_video_stream(p))
      return 0;
    }
  else
    {
    if(!init_audio_stream(p) ||
       !init_video_stream(p) ||
       !init_subtitle_stream(p))
      return 0;
    }
  return 1;
  }

/* Cleanup everything */

static void cleanup_streams(bg_player_t * player)
  {
  if(DO_AUDIO(player->flags))
    bg_player_oa_cleanup(player->oa_context);
  
  if(DO_VIDEO(player->flags))
    bg_player_ov_cleanup(player->ov_context);
  
  bg_player_time_stop(player);

  /* Subtitles must be cleaned up as long as the ov plugin
     is still open */
  bg_player_subtitle_cleanup(player);
  
  bg_player_video_cleanup(player);
  bg_player_audio_cleanup(player);
  bg_player_time_reset(player);
  }

static void player_cleanup(bg_player_t * player)
  {
  gavl_time_t t = 0;
  
  bg_player_input_cleanup(player->input_context);
  player->input_handle = (bg_plugin_handle_t*)0;

  cleanup_streams(player);

  bg_msg_queue_list_send(player->message_queues,
                         msg_time,
                         &t);
  
  }

/* Initialize playback (called when playback starts or after
   streams have changed) */

static void init_playback(bg_player_t * p, gavl_time_t time,
                          int flags)
  {
  int i;
  struct stream_info_s si;
  struct chapter_info_s ci;

  bg_player_set_state(p, BG_PLAYER_STATE_STARTING, NULL, NULL);

  /* Close previous visualization before we init the streams
     because it might close the ov_plugin as well */
  
  if(DO_VISUALIZE(p->old_flags) && !DO_VISUALIZE(p->flags))
    bg_visualizer_close(p->visualizer);
  
  /* Initialize audio and video streams if not in bypass mode */
  
  if(!p->do_bypass)
    {
    if(!init_streams(p))
      return;
    }
  
  /* Set up visualizations */
  
  if(DO_VISUALIZE(p->flags))
    {
    if(!DO_VISUALIZE(p->old_flags) ||
       bg_visualizer_need_restart(p->visualizer))
      {
      if(DO_VISUALIZE(p->old_flags))
        bg_visualizer_close(p->visualizer);
      /* Initialize visualizer */
      bg_visualizer_open_plugin(p->visualizer, &p->audio_stream.fifo_format,
                                bg_player_ov_get_plugin(p->ov_context));
      }
    else
      {
      /* Update audio format */
      bg_visualizer_set_audio_format(p->visualizer, &p->audio_stream.fifo_format);
      }
    }
  
  /* Send input messages */
  bg_player_input_send_messages(p->input_context);

  
  if(p->track_info->description)
    bg_msg_queue_list_send(p->message_queues,
                           msg_stream_description,
                           p->track_info->description);

  /* Send metadata */

  bg_msg_queue_list_send(p->message_queues,
                         msg_metadata,
                         &(p->track_info->metadata));


  bg_player_set_duration(p, p->track_info->duration, p->can_seek);
  
  bg_msg_queue_list_send(p->message_queues,
                         msg_num_streams,
                         p->track_info);

  /* Send chapter info */

  bg_msg_queue_list_send(p->message_queues,
                         msg_num_chapters,
                         p->track_info->chapter_list);
  
  ci.l = p->track_info->chapter_list;

  if(ci.l)
    {
    for(ci.index = 0; ci.index < ci.l->num_chapters; ci.index++)
      {
      bg_msg_queue_list_send(p->message_queues,
                             msg_chapter_info,
                             &(ci));
      }

    p->current_chapter = 0;
    bg_msg_queue_list_send(p->message_queues,
                           msg_chapter_changed,
                           &(p->current_chapter));
    }
  
  /* Send infos about the streams we have */
  si.track = p->track_info;
  for(i = 0; i < p->track_info->num_audio_streams; i++)
    {
    si.index = i;
    bg_msg_queue_list_send(p->message_queues,
                           msg_audio_stream_info,
                           &si);
    }
  for(i = 0; i < p->track_info->num_video_streams; i++)
    {
    si.index = i;
    bg_msg_queue_list_send(p->message_queues,
                           msg_video_stream_info,
                           &si);
    }
  for(i = 0; i < p->track_info->num_subtitle_streams; i++)
    {
    si.index = i;
    bg_msg_queue_list_send(p->message_queues,
                           msg_subtitle_stream_info,
                           &si);
    }
  
  /* Send messages about formats */
  if(DO_AUDIO(p->flags))
    {
    if(p->track_info->audio_streams[p->current_audio_stream].description)
      bg_msg_queue_list_send(p->message_queues,
                             msg_audio_description,
                             p->track_info->audio_streams[p->current_audio_stream].description);
    bg_msg_queue_list_send(p->message_queues,
                           msg_audio_stream,
                           p);
    }
  if(DO_VIDEO(p->flags))
    {
    if(!DO_SUBTITLE_ONLY(p->flags) &&
       p->track_info->video_streams[p->current_video_stream].description)
      bg_msg_queue_list_send(p->message_queues,
                             msg_video_description,
                             p->track_info->video_streams[p->current_video_stream].description);
    bg_msg_queue_list_send(p->message_queues,
                           msg_video_stream,
                           p);
    }
  else if(!DO_VISUALIZE(p->flags))
    bg_player_ov_standby(p->ov_context);
  
  if(DO_SUBTITLE(p->flags))
    {
    bg_msg_queue_list_send(p->message_queues, msg_subtitle_stream, p);
    }
  
  /* Count the threads */

  p->total_plugin_threads = 1;
  if(DO_AUDIO(p->flags))
    p->total_plugin_threads++;
  if(DO_VIDEO(p->flags))
    p->total_plugin_threads++;

  /* Reset variables */

  p->waiting_plugin_threads = 0;
    
  /* Start playback */
  
  pthread_mutex_lock(&(p->stop_mutex));

  if(p->do_bypass)
    pthread_create(&(p->input_thread),
                   (pthread_attr_t*)0,
                   bg_player_input_thread_bypass, p->input_context);
  else
    pthread_create(&(p->input_thread),
                   (pthread_attr_t*)0,
                   bg_player_input_thread, p->input_context);
    
  
  if(DO_AUDIO(p->flags))
    pthread_create(&(p->oa_thread),
                   (pthread_attr_t*)0,
                   bg_player_oa_thread, p->oa_context);

  if(DO_VIDEO(p->flags))
    pthread_create(&(p->ov_thread),
                   (pthread_attr_t*)0,
                   bg_player_ov_thread, p->ov_context);
  
  //  if(p->waiting_plugin_threads < p->total_plugin_threads)
  pthread_cond_wait(&(p->stop_cond), &(p->stop_mutex));
  pthread_mutex_unlock(&p->stop_mutex);
  
  bg_player_time_init(p);

  if((time > 0) && (p->can_seek))
    {
    bg_player_input_seek(p->input_context, &time);
    bg_player_time_set(p, time);
    }
  else
    {
    if(DO_AUDIO(p->flags))
      bg_audio_filter_chain_reset(p->audio_stream.fc);
    if(DO_VIDEO(p->flags))
      bg_video_filter_chain_reset(p->video_stream.fc);
    }
  
  if(!p->do_bypass)
    {
  
    /* Fire up the fifos */
    if(DO_AUDIO(p->flags))
      bg_fifo_set_state(p->audio_stream.fifo, BG_FIFO_PLAYING);
    if(DO_VIDEO(p->flags))
      bg_fifo_set_state(p->video_stream.fifo, BG_FIFO_PLAYING);
    preload(p);
    }
  
  if(flags & BG_PLAY_FLAG_INIT_THEN_PAUSE)
    {
    bg_player_set_state(p, BG_PLAYER_STATE_PAUSED, NULL, NULL);
    if(p->do_bypass)
      bg_player_input_bypass_set_pause(p->input_context, 1);
    if(DO_VIDEO(p->flags))
      bg_player_ov_update_still(p->ov_context);

    }
  else
    start_playback(p, BG_PLAYER_STATE_PLAYING);
  
  /* Set start time to zero */

  bg_msg_queue_list_send(p->message_queues,
                         msg_time,
                         &time);
  }


/* Play a file. This must be called ONLY if the player is in
   a defined stopped state
*/

static void play_cmd(bg_player_t * p,
                     bg_plugin_handle_t * handle,
                     int track_index, char * track_name, int flags)
  {
  /* Shut down from last playback if necessary */
  
  if(p->input_handle && !bg_plugin_equal(p->input_handle, handle))
    player_cleanup(p);
  
  bg_player_set_track_name(p, track_name);
  
  p->input_handle = handle;
  if(!bg_player_input_init(p->input_context,
                           handle, track_index))
    {
    bg_player_set_state(p, BG_PLAYER_STATE_ERROR, NULL, NULL);
    bg_player_ov_standby(p->ov_context);
    return;
    }
  init_playback(p, 0, flags);
  }

static void cleanup_playback(bg_player_t * player,
                             int old_state, int new_state, int want_new,
                             int stop_input)
  {
  if(old_state == BG_PLAYER_STATE_STOPPED)
    return;
  
  if(new_state == BG_PLAYER_STATE_CHANGING)
    bg_player_set_state(player, new_state, &want_new, NULL);
  else
    bg_player_set_state(player, new_state, NULL, NULL);
  
  switch(old_state)
    {
    case BG_PLAYER_STATE_CHANGING:
      break;
    case BG_PLAYER_STATE_STARTING:
    case BG_PLAYER_STATE_PAUSED:
    case BG_PLAYER_STATE_SEEKING:
    case BG_PLAYER_STATE_BUFFERING:
      /* If the threads are sleeping, wake them up now so they'll end */
      start_playback(player, new_state);
    case BG_PLAYER_STATE_PLAYING: // Fall through
      /* Set the stop flag */
      if(DO_AUDIO(player->flags))
        bg_fifo_set_state(player->audio_stream.fifo, BG_FIFO_STOPPED);
      if(DO_VIDEO(player->flags))
        bg_fifo_set_state(player->video_stream.fifo, BG_FIFO_STOPPED);

      if(stop_input)
        {
        bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "Joining input thread...");
        pthread_join(player->input_thread, (void**)0);
        bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "Joining input thread done");
        }
      
      if(DO_AUDIO(player->flags))
        {
        bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "Joining audio thread...");
        pthread_join(player->oa_thread, (void**)0);
        bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "Joining audio thread done");
        }
      if(DO_VIDEO(player->flags))
        {
        bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "Joining video thread...");
        pthread_join(player->ov_thread, (void**)0);
        bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "Joining video thread done");
        }
      if(DO_AUDIO(player->flags))
        bg_player_oa_stop(player->oa_context);
    default:
      break;
    }
  
  if(new_state == BG_PLAYER_STATE_STOPPED)
    {
    if(DO_VISUALIZE(player->flags))
      bg_visualizer_close(player->visualizer);
    bg_player_ov_standby(player->ov_context);
    }
  return;
  }

static void stop_cmd(bg_player_t * player, int new_state, int want_new)
  {
  int old_state;
  
  old_state = bg_player_get_state(player);

  cleanup_playback(player, old_state, new_state, want_new, 1);
  
  if((old_state == BG_PLAYER_STATE_PLAYING) ||
     (old_state == BG_PLAYER_STATE_PAUSED))
    {
    if((new_state == BG_PLAYER_STATE_STOPPED) ||
       !(player->do_bypass) ||
       !(player->input_handle->info->flags & BG_PLUGIN_KEEP_RUNNING))
      player_cleanup(player);
    }
  player->old_flags = player->flags;
  player->flags = 0;
  }

static void stream_change_init(bg_player_t * player)
  {
  int old_state;
  old_state = bg_player_get_state(player);
  
  if((old_state == BG_PLAYER_STATE_STOPPED)  ||
     (old_state == BG_PLAYER_STATE_CHANGING) ||
     (old_state == BG_PLAYER_STATE_ERROR))
    player->saved_state.playing = 0;
  else
    player->saved_state.playing = 1;
  
  if(player->saved_state.playing)
    {
    bg_player_time_get(player, 1, &player->saved_state.time);
    /* Interrupt and pretend we are seeking */
    
    cleanup_playback(player, old_state, BG_PLAYER_STATE_CHANGING, 0, 1);
    cleanup_streams(player);
    bg_player_input_stop(player->input_context);
    player->old_flags = player->flags;
    }
  }

static int stream_change_done(bg_player_t * player)
  {
  gavl_time_t t = 0;
  if(player->saved_state.playing)
    {
    if(!bg_player_input_set_track(player->input_context))
      {
      bg_player_set_state(player, BG_PLAYER_STATE_ERROR,
                          NULL, NULL);
      goto fail;
      }
    bg_player_input_select_streams(player->input_context);
    
    if(!bg_player_input_start(player->input_context))
      {
      bg_player_set_state(player, BG_PLAYER_STATE_ERROR,
                          NULL, NULL);
      goto fail;
      }
    init_playback(player, player->saved_state.time, 0);
    }
  return 1;
  fail:
  bg_player_ov_standby(player->ov_context);
  bg_player_time_reset(player);
  
  bg_player_input_cleanup(player->input_context);
  player->input_handle = (bg_plugin_handle_t*)0;
  bg_msg_queue_list_send(player->message_queues,
                         msg_time,
                         &t);
  
  return 0;
  }


static void set_ov_plugin_cmd(bg_player_t * player,
                              bg_plugin_handle_t * handle)
  {
  stream_change_init(player);
  bg_player_ov_set_plugin(player, handle);
  bg_player_ov_standby(player->ov_context);
  stream_change_done(player);
  }

static void set_oa_plugin_cmd(bg_player_t * player,
                              bg_plugin_handle_t * handle)
  {
  stream_change_init(player);
  bg_player_oa_set_plugin(player, handle);
  stream_change_done(player);
  }

static void do_seek(bg_player_t * player, gavl_time_t t, int old_state)
  {
  int new_chapter;
  gavl_time_t sync_time = t;

  if(player->can_seek)
    bg_player_input_seek(player->input_context, &sync_time);
  
  /* Clear fifos and filter chains */

  if(DO_AUDIO(player->flags))
    {
    bg_audio_filter_chain_reset(player->audio_stream.fc);
    bg_fifo_clear(player->audio_stream.fifo);
    }
  if(DO_VIDEO(player->flags))
    {
    bg_video_filter_chain_reset(player->video_stream.fc);
    bg_fifo_clear(player->video_stream.fifo);
    }
  if(DO_SUBTITLE(player->flags))
    {
    bg_fifo_clear(player->subtitle_stream.fifo);
    }
  
  /* Resync */

  /* Fire up the fifos */
  if(DO_AUDIO(player->flags))
    bg_fifo_set_state(player->audio_stream.fifo, BG_FIFO_PLAYING);
  if(DO_VIDEO(player->flags))
    bg_fifo_set_state(player->video_stream.fifo, BG_FIFO_PLAYING);
  
  preload(player);
  
  if(player->can_seek)
    bg_player_time_set(player, sync_time);
  else
    bg_player_time_set(player, 0);
    
  bg_player_ov_reset(player);

  if(player->track_info->chapter_list)
    {
    new_chapter =
      bg_chapter_list_get_current(player->track_info->chapter_list,
                                  sync_time);
    if(new_chapter != player->current_chapter)
      {
      player->current_chapter = new_chapter;
      bg_msg_queue_list_send(player->message_queues,
                             msg_chapter_changed,
                             &(player->current_chapter));
      }
    }
  
  if(old_state == BG_PLAYER_STATE_PAUSED)
    {
    bg_player_set_state(player, BG_PLAYER_STATE_PAUSED, NULL, NULL);

    /* Need to update slider and time for seeking case */

    bg_msg_queue_list_send(player->message_queues,
                           msg_time,
                           &sync_time);
    
    if(DO_VIDEO(player->flags))
      bg_player_ov_update_still(player->ov_context);
    
    // if(p->do_bypass)
    // bg_player_input_bypass_set_pause(p->input_context, 1);
    }
  else
    start_playback(player, BG_PLAYER_STATE_PLAYING);
  }

static void seek_cmd(bg_player_t * player, gavl_time_t t)
  {
  int old_state;

  old_state = bg_player_get_state(player);
  
  //  gavl_video_frame_t * vf;
  interrupt_cmd(player, BG_PLAYER_STATE_SEEKING);
  
  do_seek(player, t, old_state);  
  }


static void set_audio_stream_cmd(bg_player_t * player, int stream)
  {
  
  if(stream == player->current_audio_stream)
    return;

  stream_change_init(player);
  player->current_audio_stream = stream;
  stream_change_done(player);
  }

static void set_video_stream_cmd(bg_player_t * player, int stream)
  {
  
  if(stream == player->current_video_stream)
    return;

  stream_change_init(player);
  player->current_video_stream = stream;
  stream_change_done(player);
  }


static void set_subtitle_stream_cmd(bg_player_t * player, int stream)
  {
  
  if(stream == player->current_subtitle_stream)
    return;

  stream_change_init(player);
  player->current_subtitle_stream = stream;
  stream_change_done(player);
  }

static void chapter_cmd(bg_player_t * player, int chapter)
  {
  int state;

  if(!player->can_seek)
    return;
  
  state = bg_player_get_state(player);
  
  if((state != BG_PLAYER_STATE_PLAYING) &&
     (state != BG_PLAYER_STATE_PAUSED))
    return;
  
  if(!player->track_info->chapter_list ||
     (chapter < 0) ||
     (chapter >= player->track_info->chapter_list->num_chapters))
    return;
  seek_cmd(player,
           gavl_time_unscale(player->track_info->chapter_list->timescale,
                             player->track_info->chapter_list->chapters[chapter].time));
  }

/* Process command, return FALSE if thread should be ended */

static int process_commands(bg_player_t * player)
  {
  int play_flags;
  int arg_i1;
  float arg_f1;
  int state;
  void * arg_ptr1;
  char * arg_str1;
  int next_track;
  gavl_time_t time;
  gavl_time_t current_time;
  bg_msg_t * command;
  int queue_locked = 0;
  uint32_t id;
  
  while(1)
    {
    command = bg_msg_queue_try_lock_read(player->command_queue);
    if(!command)
      return 1;
    
    /* Process commands */

    queue_locked = 1;
    
    switch(bg_msg_get_id(command))
      {
      case BG_PLAYER_CMD_QUIT:
        state = bg_player_get_state(player);
        switch(state)
          {
          case BG_PLAYER_STATE_PLAYING:
          case BG_PLAYER_STATE_CHANGING:
          case BG_PLAYER_STATE_PAUSED:
            stop_cmd(player, BG_PLAYER_STATE_STOPPED, 0);
            break;
          }
        bg_msg_queue_unlock_read(player->command_queue);
        queue_locked = 0;
        return 0;
        break;
      case BG_PLAYER_CMD_PLAY:
        play_flags = bg_msg_get_arg_int(command, 2);
        state = bg_player_get_state(player);
        arg_ptr1 = bg_msg_get_arg_ptr_nocopy(command, 0);

        if(play_flags)
          {
          if((state == BG_PLAYER_STATE_PLAYING) &&
             (play_flags & BG_PLAY_FLAG_IGNORE_IF_PLAYING))
            {
            if(arg_ptr1)
              bg_plugin_unref((bg_plugin_handle_t*)arg_ptr1);
            break;
            }
          else if((state == BG_PLAYER_STATE_STOPPED) &&
                  (play_flags & BG_PLAY_FLAG_IGNORE_IF_STOPPED))
            {
            if(arg_ptr1)
              bg_plugin_unref((bg_plugin_handle_t*)arg_ptr1);
            break;
            }
          }
        if(state == BG_PLAYER_STATE_PAUSED)
          {
          if(play_flags & BG_PLAY_FLAG_RESUME)
            {
            pause_cmd(player);
            if(arg_ptr1)
              bg_plugin_unref((bg_plugin_handle_t*)arg_ptr1);
            break;
            }
          else
            play_flags |= BG_PLAY_FLAG_INIT_THEN_PAUSE;
          }
        
        arg_i1   = bg_msg_get_arg_int(command, 1);
        arg_str1 = bg_msg_get_arg_string(command, 3);
      
        if((state == BG_PLAYER_STATE_PLAYING) ||
           (state == BG_PLAYER_STATE_PAUSED))
          {
          stop_cmd(player, BG_PLAYER_STATE_CHANGING, 0);
          }
        if(!arg_ptr1)
          {
          bg_player_set_state(player, BG_PLAYER_STATE_ERROR,
                              "No Track selected", NULL);
          }
        else
          play_cmd(player, arg_ptr1, arg_i1, arg_str1, play_flags);

        if(arg_str1)
          free(arg_str1);
      
                  
        break;
      case BG_PLAYER_CMD_STOP:
        state = bg_player_get_state(player);
        switch(state)
          {
          case BG_PLAYER_STATE_PLAYING:
          case BG_PLAYER_STATE_PAUSED:
          case BG_PLAYER_STATE_CHANGING:
            stop_cmd(player, BG_PLAYER_STATE_STOPPED, 0);
            break;
          }
        break;
      case BG_PLAYER_CMD_CHANGE:
        state = bg_player_get_state(player);
        arg_i1 = bg_msg_get_arg_int(command, 0);
        
        switch(state)
          {
          case BG_PLAYER_STATE_PLAYING:
          case BG_PLAYER_STATE_PAUSED:
          case BG_PLAYER_STATE_SEEKING:
            if(!(arg_i1 & BG_PLAY_FLAG_IGNORE_IF_PLAYING))
              stop_cmd(player, BG_PLAYER_STATE_CHANGING, 0);
            break;
          case BG_PLAYER_STATE_STOPPED:
            if(!(arg_i1 & BG_PLAY_FLAG_IGNORE_IF_STOPPED))
              stop_cmd(player, BG_PLAYER_STATE_CHANGING, 0);
            break;
          case BG_PLAYER_STATE_CHANGING:
            stop_cmd(player, BG_PLAYER_STATE_CHANGING, 0);
            break;
          }
        bg_msg_queue_list_send(player->message_queues,
                               msg_cleanup,
                               &player);
        break;
      case BG_PLAYER_CMD_SEEK:
        if(!player->can_seek)
          break;

        state = bg_player_get_state(player);

        if((state != BG_PLAYER_STATE_PLAYING) &&
           (state != BG_PLAYER_STATE_PAUSED))
          break;           

        time = bg_msg_get_arg_time(command, 0);
        seek_cmd(player, time);
        break;
      case BG_PLAYER_CMD_SET_CHAPTER:
        arg_i1 = bg_msg_get_arg_int(command, 0);
        chapter_cmd(player, arg_i1);
        break;
      case BG_PLAYER_CMD_NEXT_CHAPTER:
        chapter_cmd(player, player->current_chapter + 1);
        break;
      case BG_PLAYER_CMD_PREV_CHAPTER:
        chapter_cmd(player, player->current_chapter - 1);
        break;
      case BG_PLAYER_CMD_SEEK_REL:
        if(!player->can_seek)
          break;

        state = bg_player_get_state(player);

        
        if((state != BG_PLAYER_STATE_PLAYING) &&
           (state != BG_PLAYER_STATE_PAUSED))
          break;           
        
        time = bg_msg_get_arg_time(command, 0);

        bg_msg_queue_unlock_read(player->command_queue);
        queue_locked = 0;
        
        /* Check if there are more messages */
        while(bg_msg_queue_peek(player->command_queue, &id) && 
              (id == BG_PLAYER_CMD_SEEK_REL))
          {
          command = bg_msg_queue_lock_read(player->command_queue);
          queue_locked = 1;
          time += bg_msg_get_arg_time(command, 0);
          bg_msg_queue_unlock_read(player->command_queue);
          queue_locked = 0;
          }
        
        bg_player_time_get(player, 1, &current_time);
        time += current_time;
        
        if(time < 0)
          time = 0;
        else if(time > player->track_info->duration)
          {
          /* Seeked beyond end -> finish track */
          stop_cmd(player, BG_PLAYER_STATE_CHANGING, 1);
          break;
          }
        else
          seek_cmd(player, time);
        break;
      case BG_PLAYER_CMD_SET_VOLUME:
        arg_f1 = bg_msg_get_arg_float(command, 0);
        player->volume = arg_f1;
        if(player->do_bypass)
          bg_player_input_bypass_set_volume(player->input_context,
                                            player->volume);
        else
          bg_player_oa_set_volume(player->oa_context, player->volume);

        bg_msg_queue_list_send(player->message_queues,
                               msg_volume_changed,
                               &(player->volume));

        break;
      case BG_PLAYER_CMD_SET_VOLUME_REL:

        arg_f1 = bg_msg_get_arg_float(command, 0);

        player->volume += arg_f1;
        if(player->volume > 1.0)
          player->volume = 1.0;
        if(player->volume < BG_PLAYER_VOLUME_MIN)
          player->volume = BG_PLAYER_VOLUME_MIN;
        if(player->do_bypass)
          bg_player_input_bypass_set_volume(player->input_context,
                                            player->volume);
        else
          bg_player_oa_set_volume(player->oa_context, player->volume);

        bg_msg_queue_list_send(player->message_queues,
                               msg_volume_changed,
                               &(player->volume));
        break;
      case BG_PLAYER_CMD_SETSTATE:
        arg_i1 = bg_msg_get_arg_int(command, 0);
        switch(arg_i1)
          {
          case BG_PLAYER_STATE_FINISHING:
            state = bg_player_get_state(player);
          
            /* Close down everything */
            bg_player_set_state(player, BG_PLAYER_STATE_FINISHING, NULL, NULL);
          
            bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "Joining input thread...");
            pthread_join(player->input_thread, (void**)0);
            bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "Joining input thread done");
            if(DO_AUDIO(player->flags))
              {
              bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "Joining audio thread...");
              pthread_join(player->oa_thread, (void**)0);
              bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "Joining audio thread done");
              }
            if(DO_VIDEO(player->flags))
              {
              bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "Joining video thread...");
              pthread_join(player->ov_thread, (void**)0);
              bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "Joining video thread done");
              }
            player_cleanup(player);
            next_track = 1;
            bg_player_set_state(player, BG_PLAYER_STATE_CHANGING, &next_track, NULL);
            break;
          case BG_PLAYER_STATE_ERROR:
            stop_cmd(player, BG_PLAYER_STATE_STOPPED, 0);
            bg_player_set_state(player, BG_PLAYER_STATE_ERROR,
                                NULL, NULL);
          }
        break;
      case BG_PLAYER_CMD_SET_OA_PLUGIN:
        arg_ptr1 = bg_msg_get_arg_ptr_nocopy(command, 0);
        set_oa_plugin_cmd(player, arg_ptr1);
        break;
      case BG_PLAYER_CMD_SET_OV_PLUGIN:
        arg_ptr1 = bg_msg_get_arg_ptr_nocopy(command, 0);
        set_ov_plugin_cmd(player, arg_ptr1);
        break;

      case BG_PLAYER_CMD_SET_AUDIO_STREAM:
        arg_i1 = bg_msg_get_arg_int(command, 0);
        set_audio_stream_cmd(player, arg_i1);
        break;
      case BG_PLAYER_CMD_SET_VIDEO_STREAM:
        arg_i1 = bg_msg_get_arg_int(command, 0);
        set_video_stream_cmd(player, arg_i1);
        break;
      case BG_PLAYER_CMD_SET_SUBTITLE_STREAM:
        arg_i1 = bg_msg_get_arg_int(command, 0);
        set_subtitle_stream_cmd(player, arg_i1);
        break;
      case BG_PLAYER_CMD_PAUSE:
        pause_cmd(player);
        break;
      case BG_PLAYER_CMD_INTERRUPT:
        /* Interrupt playback and restart */
        stream_change_init(player);
        bg_msg_queue_list_send(player->message_queues,
                               msg_interrupt,
                               &player);

        bg_msg_queue_unlock_read(player->command_queue);
        queue_locked = 0;
        
        while(1)
          {
          command = bg_msg_queue_lock_read(player->command_queue);
          queue_locked = 1;
          if(bg_msg_get_id(command) != BG_PLAYER_CMD_INTERRUPT_RESUME)
            bg_log(BG_LOG_WARNING, LOG_DOMAIN,
                   "Ignoring command while playback is interrupted");
          else
            break;
          bg_msg_queue_unlock_read(player->command_queue);
          queue_locked = 0;
          }
        stream_change_done(player);
        bg_msg_queue_list_send(player->message_queues,
                               msg_interrupt_resume,
                               &player);
        break;
      case BG_PLAYER_CMD_TOGGLE_MUTE:
        pthread_mutex_lock(&player->mute_mutex);
        player->mute = !player->mute;

        bg_msg_queue_list_send(player->message_queues,
                               msg_mute,
                               &player->mute);
        pthread_mutex_unlock(&player->mute_mutex);
        break;
      }
    if(queue_locked)
      bg_msg_queue_unlock_read(player->command_queue);
  }
  
  return 1;
  }

static void * player_thread(void * data)
  {
  bg_player_t * player;
  gavl_time_t wait_time;
  int old_seconds = -1;
  int seconds;
  int do_exit;
  int state;
  gavl_time_t time;
  
  player = (bg_player_t*)data;

  bg_player_set_state(player, BG_PLAYER_STATE_STOPPED, NULL, NULL);

  wait_time = 10000;
  do_exit = 0;
  while(1)
    {
    /* Process commands */
    if(!process_commands(player))
      do_exit = 1;

    if(do_exit)
      break;

    state = bg_player_get_state(player);
    switch(state)
      {
      case BG_PLAYER_STATE_PLAYING:
      case BG_PLAYER_STATE_FINISHING:
        bg_player_time_get(player, 1, &time);
        seconds = time / GAVL_TIME_SCALE;
        if(seconds != old_seconds)
          {
          old_seconds = seconds;
          bg_msg_queue_list_send(player->message_queues,
                                 msg_time,
                                 &time);
          }
        if(player->track_info->chapter_list &&
           bg_chapter_list_changed(player->track_info->chapter_list,
                                   time, &player->current_chapter))
          {
          bg_msg_queue_list_send(player->message_queues,
                                 msg_chapter_changed,
                                 &(player->current_chapter));
          }
        
        break;
      }
    gavl_time_delay(&wait_time);
    }
  return (void*)0;
  }

void bg_player_run(bg_player_t * player)
  {
  pthread_create(&(player->player_thread),
                 (pthread_attr_t*)0,
                 player_thread, player);

  }

void bg_player_quit(bg_player_t *player)
  {
  bg_msg_t * msg;
  msg = bg_msg_queue_lock_write(player->command_queue);
  bg_msg_set_id(msg, BG_PLAYER_CMD_QUIT);
  bg_msg_queue_unlock_write(player->command_queue);
  
  //  pthread_cancel(player->player_thread);
  pthread_join(player->player_thread, (void**)0);
  }
