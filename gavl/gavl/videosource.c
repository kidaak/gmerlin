/*****************************************************************
 * gavl - a general purpose audio/video processing library
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

#include <stdlib.h>


#include <gavl/gavl.h>

struct gavl_video_source_s
  {
  gavl_video_format_t src_format;
  gavl_video_format_t dst_format;
  int src_flags;
  int dst_flags;

  /* Callback set by the client */
  
  gavl_video_source_func_t func;
  gavl_video_converter_t * cnv;
  int scale_timestamps;
  
  int do_convert;
  
  void * priv;

  /* FPS Conversion */
  int64_t next_pts;

  int64_t fps_pts;
  int64_t fps_duration;
  
  gavl_video_frame_t * fps_frame;
  
  gavl_video_frame_pool_t * src_fp;
  gavl_video_frame_pool_t * dst_fp;

  /* Callback set according to the configuration */
  gavl_source_status_t (*read_video)(gavl_video_source_t * s,
                                     gavl_video_frame_t ** frame);
  };

gavl_video_source_t *
gavl_video_source_create(gavl_video_source_func_t func,
                         void * priv,
                         int src_flags,
                         const gavl_video_format_t * src_format)
  {
  gavl_video_source_t * ret = calloc(1, sizeof(*ret));

  ret->func = func;
  ret->priv = priv;
  ret->src_flags = src_flags;
  gavl_video_format_copy(&ret->src_format, src_format);
  ret->cnv = gavl_video_converter_create();
  return ret;
  }

/* Called by the destination */

const gavl_video_format_t *
gavl_video_source_get_src_format(gavl_video_source_t * s)
  {
  return &s->src_format;
  }

const gavl_video_format_t *
gavl_video_source_get_dst_format(gavl_video_source_t * s)
  {
  return &s->dst_format;
  }

gavl_video_options_t * gavl_video_source_get_options(gavl_video_source_t * s)
  {
  return gavl_video_converter_get_options(s->cnv);
  }

GAVL_PUBLIC
void gavl_video_source_reset(gavl_video_source_t * s)
  {
  s->next_pts = GAVL_TIME_UNDEFINED;
  if(s->src_fp)
    gavl_video_frame_pool_reset(s->src_fp);
  if(s->dst_fp)
    gavl_video_frame_pool_reset(s->dst_fp);
  }

GAVL_PUBLIC
void gavl_video_source_destroy(gavl_video_source_t * s)
  {
  if(s->src_fp)
    gavl_video_frame_pool_destroy(s->src_fp);
  if(s->dst_fp)
    gavl_video_frame_pool_destroy(s->dst_fp);
  gavl_video_converter_destroy(s->cnv);
  free(s);
  }

#define SCALE_PTS(f)                                           \
  if(s->scale_timestamps)                                      \
    {                                                          \
    int64_t next_pts;                                          \
    if(s->next_pts == GAVL_TIME_UNDEFINED)                     \
      s->next_pts = gavl_time_rescale(s->src_format.timescale, \
                                      s->dst_format.timescale, \
                                      (f)->timestamp);               \
    next_pts = gavl_time_rescale(s->src_format.timescale,      \
                                 s->dst_format.timescale,      \
                                 (f)->timestamp + (f)->duration);    \
    (f)->timestamp = s->next_pts;                                    \
    (f)->duration = next_pts - (f)->timestamp;                       \
    s->next_pts = next_pts;                                    \
    }

static gavl_source_status_t
read_video_simple(gavl_video_source_t * s,
                  gavl_video_frame_t ** frame)
  {
  gavl_source_status_t st;
  gavl_video_frame_t * in_frame;
  int direct = 0;
  
  /* Pass from src to dst */

  if(!(*frame) && (s->src_flags & GAVL_SOURCE_SRC_ALLOC) &&
     (!(s->dst_flags & GAVL_SOURCE_DST_OVERWRITES) ||
      !(s->src_flags & GAVL_SOURCE_SRC_REF)))
    direct = 1;
  
  /* Pass from dst to src (this is the legacy behavior) */

  if(*frame && !(s->src_flags & GAVL_SOURCE_SRC_ALLOC))
    direct = 1;
  
  if(direct)
    {
    if((st = s->func(s->priv, frame)) != GAVL_SOURCE_OK)
      return st;
    SCALE_PTS(*frame);
    return GAVL_SOURCE_OK;
    }
  
  /* memcpy */

  if(!(s->src_flags & GAVL_SOURCE_SRC_ALLOC))
    in_frame = gavl_video_frame_pool_get(s->src_fp);
  else
    in_frame = NULL;
  
  if(!(*frame))
    {
    if(!s->dst_fp)
      s->dst_fp = gavl_video_frame_pool_create(NULL, &s->dst_format);
    *frame = gavl_video_frame_pool_get(s->dst_fp);
    }
  if((st = s->func(s->priv, &in_frame)) != GAVL_SOURCE_OK)
    return st;

  gavl_video_frame_copy(&s->src_format, *frame, in_frame);
  gavl_video_frame_copy_metadata(*frame, in_frame);
  
  SCALE_PTS(*frame);
  return GAVL_SOURCE_OK;
  }

static gavl_source_status_t
read_video_cnv(gavl_video_source_t * s,
               gavl_video_frame_t ** frame)
  {
  gavl_source_status_t st;
  gavl_video_frame_t * in_frame = NULL;
  
  if(!(s->src_flags & GAVL_SOURCE_SRC_ALLOC))
    in_frame = gavl_video_frame_pool_get(s->src_fp);

  if((st = s->func(s->priv, &in_frame)) != GAVL_SOURCE_OK)
    return st;

  if(!(*frame))
    {
    if(!s->dst_fp)
      s->dst_fp = gavl_video_frame_pool_create(NULL, &s->dst_format);
    *frame = gavl_video_frame_pool_get(s->dst_fp);
    }
  gavl_video_convert(s->cnv, in_frame, *frame);
  SCALE_PTS(*frame);
  return GAVL_SOURCE_OK;
  }

static gavl_source_status_t
read_frame_fps(gavl_video_source_t * s,
               gavl_video_frame_t ** frame)
  {
  gavl_source_status_t st;
  if(!(s->src_flags & GAVL_SOURCE_SRC_ALLOC))
    {
    if(s->fps_frame)
      s->fps_frame->refcount = 0;
    s->fps_frame = gavl_video_frame_pool_get(s->src_fp);
    }
  if((st = s->func(s->priv, &s->fps_frame)) != GAVL_SOURCE_OK)
    return st;
    
  s->fps_pts      = s->fps_frame->timestamp;
  s->fps_duration = s->fps_frame->duration;
  return GAVL_SOURCE_OK;
  }

static gavl_source_status_t
read_video_fps(gavl_video_source_t * s,
               gavl_video_frame_t ** frame)
  {
  int new_frame = 0;
  int expired = 0;
  gavl_source_status_t st;
  
  /* Read frame if necessary */
  if(!s->fps_frame)
    {
    if((st = read_frame_fps(s, &s->fps_frame)))
      return st;
    new_frame = 1;
    
    s->next_pts = gavl_time_rescale(s->src_format.timescale,
                                    s->dst_format.timescale,
                                    s->fps_frame->timestamp);
    }
  
  /* Check if frame expired */
  while(gavl_time_rescale(s->src_format.timescale,
                          s->dst_format.timescale,
                          s->fps_pts + s->fps_duration) <= s->next_pts)
    {
    if((st = read_frame_fps(s, &s->fps_frame)))
      return st;
    new_frame = 1;
    }

  /* Set pts / duration */
  s->fps_frame->timestamp = s->next_pts;
  s->fps_frame->duration  = s->dst_format.frame_duration;
  s->next_pts += s->dst_format.frame_duration;

  /* Check if frame will be expired next time */
  if(gavl_time_rescale(s->src_format.timescale,
                       s->dst_format.timescale,
                       s->fps_pts + s->fps_duration) <= s->next_pts)
    expired = 1;
  
  /* Now check what to do */

  /* Convert into output buffer */
  if(*frame && s->do_convert && new_frame && expired)
    {
    gavl_video_convert(s->cnv, s->fps_frame, *frame);
    return GAVL_SOURCE_OK;
    }

  /* Convert into local buffer */
  if(s->do_convert && new_frame)
    {
    gavl_video_frame_t * tmp_frame;
    if(!s->dst_fp)
      s->dst_fp = gavl_video_frame_pool_create(NULL, &s->dst_format);
    tmp_frame = gavl_video_frame_pool_get(s->dst_fp);
    gavl_video_convert(s->cnv, s->fps_frame, tmp_frame);
    s->fps_frame = tmp_frame;
    s->fps_frame->refcount = 1;
    }
  
  /* Copy into output buffer */
  if(*frame)
    {
    gavl_video_frame_copy(&s->dst_format, *frame, s->fps_frame);
    gavl_video_frame_copy_metadata(*frame, s->fps_frame);
    return GAVL_SOURCE_OK;
    }

  /* Pass frame directly */
  if(!(s->dst_flags & GAVL_SOURCE_DST_OVERWRITES) || expired)
    {
    *frame = s->fps_frame;
    return GAVL_SOURCE_OK;
    }

  /* Copy to tmp frame and output this */
  if(!s->dst_fp)
    s->dst_fp = gavl_video_frame_pool_create(NULL, &s->dst_format);
  *frame = gavl_video_frame_pool_get(s->dst_fp);
  gavl_video_frame_copy(&s->dst_format, *frame, s->fps_frame);
  return GAVL_SOURCE_OK;
  }
  
void gavl_video_source_set_dst(gavl_video_source_t * s, int dst_flags,
                               const gavl_video_format_t * dst_format)
  {
  int convert_fps;
  gavl_video_format_t dst_fmt;
  
  s->dst_flags = dst_flags;
  if(dst_format)
    gavl_video_format_copy(&s->dst_format, dst_format);
  else
    gavl_video_format_copy(&s->dst_format, &s->src_format);
  
  gavl_video_format_copy(&dst_fmt, &s->dst_format);
  
  dst_fmt.framerate_mode = s->src_format.framerate_mode;
  dst_fmt.timescale      = s->src_format.timescale;
  dst_fmt.frame_duration = s->src_format.frame_duration;
    
  s->do_convert =
    gavl_video_converter_init(s->cnv, &s->src_format, &dst_fmt);
  
  s->scale_timestamps = 0;

  convert_fps = 0;
  
  if(s->dst_format.framerate_mode == GAVL_FRAMERATE_CONSTANT)
    {
    if((s->src_format.framerate_mode != GAVL_FRAMERATE_CONSTANT) ||
       (s->src_format.timescale * s->dst_format.frame_duration !=
        s->dst_format.timescale * s->src_format.frame_duration))
      {
      convert_fps = 1;
      }
    }

  if(!convert_fps)
    {
    if(s->src_format.timescale != s->src_format.timescale)
      s->scale_timestamps = 1;
    }
  
  if(convert_fps)
    s->read_video = read_video_fps;
  else if(s->do_convert)
    s->read_video = read_video_cnv;
  else
    s->read_video = read_video_simple;
  

  if(!(s->src_flags & GAVL_SOURCE_SRC_ALLOC))
    s->src_fp = gavl_video_frame_pool_create(NULL, &s->src_format);
  
  }
  
gavl_source_status_t
gavl_video_source_read_frame(void * sp, gavl_video_frame_t ** frame)
  {
  gavl_video_source_t * s = sp;
  
  if(!frame)
    {
    /* Forget our status */
    gavl_video_source_reset(s);
    
    /* Skip one frame as cheaply as possible */
    return s->func(s->priv, NULL);
    }
  else
    return s->read_video(s, frame);
  }
