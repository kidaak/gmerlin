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

#include <stdio.h>
#include <gmerlin/player.h>
#include <playerprivate.h>

void bg_player_set_oa_plugin(bg_player_t * p, bg_plugin_handle_t * handle)
  {
  bg_msg_t * msg;

  if(handle)
    bg_plugin_ref(handle);
  
  msg = bg_msg_queue_lock_write(p->command_queue);

  bg_msg_set_id(msg, BG_PLAYER_CMD_SET_OA_PLUGIN);
  bg_msg_set_arg_ptr_nocopy(msg, 0, handle);
  bg_msg_queue_unlock_write(p->command_queue);
  }


void bg_player_set_ov_plugin(bg_player_t * p, bg_plugin_handle_t * handle)
  {
  bg_msg_t * msg;
  if(handle)
    bg_plugin_ref(handle);

  msg = bg_msg_queue_lock_write(p->command_queue);
  bg_msg_set_id(msg, BG_PLAYER_CMD_SET_OV_PLUGIN);
  bg_msg_set_arg_ptr_nocopy(msg, 0, handle);
  bg_msg_queue_unlock_write(p->command_queue);
  }


void bg_player_play(bg_player_t * p, bg_plugin_handle_t * handle,
                    int track, int ignore_flags, const char * track_name)
  {
  bg_msg_t * msg;

  bg_plugin_ref(handle);
  
  msg = bg_msg_queue_lock_write(p->command_queue);
  bg_msg_set_id(msg, BG_PLAYER_CMD_PLAY);
  bg_msg_set_arg_ptr_nocopy(msg, 0, handle);
  bg_msg_set_arg_int(msg, 1, track);
  bg_msg_set_arg_int(msg, 2, ignore_flags);
  bg_msg_set_arg_string(msg, 3, track_name);
  bg_msg_queue_unlock_write(p->command_queue);
  }

void bg_player_stop(bg_player_t * p)
  {
  bg_msg_t * msg;

  msg = bg_msg_queue_lock_write(p->command_queue);
  bg_msg_set_id(msg, BG_PLAYER_CMD_STOP);
  bg_msg_queue_unlock_write(p->command_queue);
  
  }

void bg_player_toggle_mute(bg_player_t * p)
  {
  bg_msg_t * msg;

  msg = bg_msg_queue_lock_write(p->command_queue);
  bg_msg_set_id(msg, BG_PLAYER_CMD_TOGGLE_MUTE);
  bg_msg_queue_unlock_write(p->command_queue);
  
  }


void bg_player_pause(bg_player_t * p)
  {
  bg_msg_t * msg;
  msg = bg_msg_queue_lock_write(p->command_queue);
  bg_msg_set_id(msg, BG_PLAYER_CMD_PAUSE);
  bg_msg_queue_unlock_write(p->command_queue);
  
  }

void bg_player_set_audio_stream(bg_player_t * p, int index)
  {
  bg_msg_t * msg;
  msg = bg_msg_queue_lock_write(p->command_queue);
  bg_msg_set_id(msg, BG_PLAYER_CMD_SET_AUDIO_STREAM);
  bg_msg_set_arg_int(msg, 0, index);
  bg_msg_queue_unlock_write(p->command_queue);
  }

void bg_player_set_video_stream(bg_player_t * p, int index)
  {
  bg_msg_t * msg;
  msg = bg_msg_queue_lock_write(p->command_queue);
  bg_msg_set_id(msg, BG_PLAYER_CMD_SET_VIDEO_STREAM);
  bg_msg_set_arg_int(msg, 0, index);
  bg_msg_queue_unlock_write(p->command_queue);
  }

void bg_player_set_subtitle_stream(bg_player_t * p, int index)
  {
  bg_msg_t * msg;
  msg = bg_msg_queue_lock_write(p->command_queue);
  bg_msg_set_id(msg, BG_PLAYER_CMD_SET_SUBTITLE_STREAM);
  bg_msg_set_arg_int(msg, 0, index);
  bg_msg_queue_unlock_write(p->command_queue);
  }

typedef struct
  {
  gavl_time_t duration;
  int can_seek;
  } duration_struct;

static void msg_duration(bg_msg_t * msg,
                         const void * data)
  {
  duration_struct * d;
  d = (duration_struct*)data;
  bg_msg_set_id(msg, BG_PLAYER_MSG_TRACK_DURATION);
  bg_msg_set_arg_time(msg, 0, d->duration);
  bg_msg_set_arg_int(msg, 1, d->can_seek);
  }

void bg_player_set_duration(bg_player_t * p, gavl_time_t duration, int can_seek)
  {
  duration_struct d;
  d.duration = duration;
  d.can_seek = can_seek;
  bg_msg_queue_list_send(p->message_queues,
                         msg_duration, &d);
  }

static void msg_metadata(bg_msg_t * msg,
                         const void * data)
  {
  const gavl_metadata_t * m = data;
  bg_msg_set_id(msg, BG_PLAYER_MSG_METADATA);
  bg_msg_set_arg_metadata(msg, 0, m);
  }

void bg_player_set_metadata(bg_player_t * p, const gavl_metadata_t * m)
  {
  bg_msg_queue_list_send(p->message_queues,
                         msg_metadata, m);
  }

typedef struct
  {
  int id;
  } key_struct;

static void msg_accel(bg_msg_t * msg,
                      const void * data)
  {
  const key_struct * s = (const key_struct *)(data);
  bg_msg_set_id(msg, BG_PLAYER_MSG_ACCEL);
  bg_msg_set_arg_int(msg, 0, s->id);
  }

void bg_player_accel_pressed(bg_player_t * p, int id)
  {
  key_struct s;
  s.id = id;
  bg_msg_queue_list_send(p->message_queues,
                         msg_accel, &s);
  }

void bg_player_seek(bg_player_t * p, gavl_time_t time, int scale)
  {
  bg_msg_t * msg;
  
  msg = bg_msg_queue_lock_write(p->command_queue);
  bg_msg_set_id(msg, BG_PLAYER_CMD_SEEK);
  bg_msg_set_arg_time(msg, 0, time);
  bg_msg_set_arg_int(msg, 1, scale);
  bg_msg_queue_unlock_write(p->command_queue);
  }

void bg_player_seek_rel(bg_player_t * p, gavl_time_t t)
  {
  bg_msg_t * msg;
  
  msg = bg_msg_queue_lock_write(p->command_queue);
  bg_msg_set_id(msg, BG_PLAYER_CMD_SEEK_REL);
  bg_msg_set_arg_time(msg, 0, t);
  bg_msg_queue_unlock_write(p->command_queue);
  }

void bg_player_set_volume(bg_player_t * p, float volume)
  {
  bg_msg_t * msg;
  
  msg = bg_msg_queue_lock_write(p->command_queue);
  bg_msg_set_id(msg, BG_PLAYER_CMD_SET_VOLUME);
  bg_msg_set_arg_float(msg, 0, volume);
  bg_msg_queue_unlock_write(p->command_queue);
  }

void bg_player_set_volume_rel(bg_player_t * p, float volume)
  {
  bg_msg_t * msg;
  
  msg = bg_msg_queue_lock_write(p->command_queue);
  bg_msg_set_id(msg, BG_PLAYER_CMD_SET_VOLUME_REL);
  bg_msg_set_arg_float(msg, 0, volume);
  bg_msg_queue_unlock_write(p->command_queue);
  }


void bg_player_error(bg_player_t * p)
  {
  bg_msg_t * msg;
  
  msg = bg_msg_queue_lock_write(p->command_queue);
  bg_msg_set_id(msg, BG_PLAYER_CMD_SETSTATE);
  bg_msg_set_arg_int(msg, 0, BG_PLAYER_STATE_ERROR);
  bg_msg_queue_unlock_write(p->command_queue);
  }

void bg_player_change(bg_player_t * p, int flags)
  {
  bg_msg_queue_t * q;
  bg_msg_t * msg;
  
  q = bg_msg_queue_create();
  bg_player_add_message_queue(p, q);

  msg = bg_msg_queue_lock_write(p->command_queue);
  bg_msg_set_id(msg, BG_PLAYER_CMD_CHANGE);
  bg_msg_set_arg_int(msg, 0, flags);
  
  bg_msg_queue_unlock_write(p->command_queue);

  while((msg = bg_msg_queue_lock_read(q)) &&
        (bg_msg_get_id(msg) != BG_PLAYER_MSG_CLEANUP))
    {
    bg_msg_queue_unlock_read(q);
    }
  bg_player_delete_message_queue(p, q);
  bg_msg_queue_destroy(q);
  }

void bg_player_interrupt(bg_player_t * p)
  {
  bg_msg_queue_t * q;
  bg_msg_t * msg;

  /* Create message queue and connect to player */
  q = bg_msg_queue_create();
  bg_player_add_message_queue(p, q);

  /* Tell player to interrupt */
  msg = bg_msg_queue_lock_write(p->command_queue);
  bg_msg_set_id(msg, BG_PLAYER_CMD_INTERRUPT);
  bg_msg_queue_unlock_write(p->command_queue);

  /* Wait for confirmation */
  while((msg = bg_msg_queue_lock_read(q)) &&
        (bg_msg_get_id(msg) != BG_PLAYER_MSG_INTERRUPT))
    {
    bg_msg_queue_unlock_read(q);
    }
  bg_msg_queue_unlock_read(q);

  /* Cleanup */
  bg_player_delete_message_queue(p, q);
  bg_msg_queue_destroy(q);
  }

void bg_player_interrupt_resume(bg_player_t * p)
  {
  bg_msg_t * msg;
  bg_msg_queue_t * q;

  /* Create message queue and connect to player */
  q = bg_msg_queue_create();
  bg_player_add_message_queue(p, q);

  /* Tell player to resume */
  msg = bg_msg_queue_lock_write(p->command_queue);
  bg_msg_set_id(msg, BG_PLAYER_CMD_INTERRUPT_RESUME);
  bg_msg_queue_unlock_write(p->command_queue);

  /* Wait for confirmation */
  while((msg = bg_msg_queue_lock_read(q)) &&
        (bg_msg_get_id(msg) != BG_PLAYER_MSG_INTERRUPT_RESUME))
    {
    bg_msg_queue_unlock_read(q);
    }
  bg_msg_queue_unlock_read(q);
  
  /* Cleanup */
  bg_player_delete_message_queue(p, q);
  bg_msg_queue_destroy(q);
  }

void bg_player_next_chapter(bg_player_t * p)
  {
  bg_msg_t * msg;
  msg = bg_msg_queue_lock_write(p->command_queue);
  bg_msg_set_id(msg, BG_PLAYER_CMD_NEXT_CHAPTER);
  bg_msg_queue_unlock_write(p->command_queue);
  }

void bg_player_prev_chapter(bg_player_t * p)
  {
  bg_msg_t * msg;
  msg = bg_msg_queue_lock_write(p->command_queue);
  bg_msg_set_id(msg, BG_PLAYER_CMD_PREV_CHAPTER);
  bg_msg_queue_unlock_write(p->command_queue);
  }

void bg_player_set_chapter(bg_player_t * p, int chapter)
  {
  bg_msg_t * msg;
  msg = bg_msg_queue_lock_write(p->command_queue);
  bg_msg_set_id(msg, BG_PLAYER_CMD_SET_CHAPTER);
  bg_msg_set_arg_int(msg, 0, chapter);
  bg_msg_queue_unlock_write(p->command_queue);
  }
