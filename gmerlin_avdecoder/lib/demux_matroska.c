/*****************************************************************
 * gmerlin-avdecoder - a general purpose multimedia decoding library
 *
 * Copyright (c) 2001 - 2010 Members of the Gmerlin project
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
#include <stdlib.h>
#include <string.h>

#include <avdec_private.h>
#include <matroska.h>
#include <nanosoft.h>

#define LOG_DOMAIN "demux_matroska"

typedef struct
  {
  bgav_mkv_ebml_header_t ebml_header;
  bgav_mkv_meta_seek_info_t meta_seek_info;
  
  bgav_mkv_segment_info_t segment_info;
  bgav_mkv_cues_t cues;
  int have_cues;
  
  bgav_mkv_track_t * tracks;
  int num_tracks;

  int64_t segment_start;

  bgav_mkv_cluster_t cluster;

  int64_t pts_offset;
  
  } mkv_t;
 
static int probe_matroska(bgav_input_context_t * input)
  {
  bgav_mkv_ebml_header_t h;
  bgav_input_context_t * input_mem;
  int ret = 0;
  
  /* We want a complete EBML header in the first 64 bits
   * with DocType either "matroska" or "webm"
   */
  uint8_t header[64];
  
  if(bgav_input_get_data(input, header, 64) < 64)
    return 0;

  if((header[0] != 0x1a) ||
     (header[1] != 0x45) ||
     (header[2] != 0xdf) ||
     (header[3] != 0xa3)) // No EBML signature
    return 0; 

  input_mem = bgav_input_open_memory(header, 64, input->opt);

  if(!bgav_mkv_ebml_header_read(input_mem, &h))
    return 0;

  if(!h.DocType)
    return 0;

  if(!strcmp(h.DocType, "matroska") ||
     !strcmp(h.DocType, "webm"))
    {
    ret = 1;
    }
  
  bgav_mkv_ebml_header_free(&h);
  bgav_input_close(input_mem);
  return ret;
  }

#define CODEC_FLAG_INCOMPLETE (1<<0)

typedef struct
  {
  const char * id;
  uint32_t fourcc;
  void (*init_func)(bgav_stream_t * s);
  int flags;
  } codec_info_t;

static void init_vfw(bgav_stream_t * s)
  {
  uint8_t * data;
  uint8_t * end;
  
  bgav_BITMAPINFOHEADER_t bh;
  bgav_mkv_track_t * p = s->priv;
  
  data = p->CodecPrivate;
  end = data + p->CodecPrivateLen;
  bgav_BITMAPINFOHEADER_read(&bh, &data);
  s->fourcc = bgav_BITMAPINFOHEADER_get_fourcc(&bh);

  if(data < end)
    {
    s->ext_size = end - data;
    s->ext_data = malloc(s->ext_size);
    memcpy(s->ext_data, data, s->ext_size);
    }
  }

static const codec_info_t video_codecs[] =
  {
    { "V_MS/VFW/FOURCC", 0x00,                            init_vfw, 0 },
    { "V_MPEG4/ISO/SP",  BGAV_MK_FOURCC('m','p','4','v'), NULL,     0 },
    { "V_MPEG4/ISO/ASP", BGAV_MK_FOURCC('m','p','4','v'), NULL,     0 },
    { "V_MPEG4/ISO/AP",  BGAV_MK_FOURCC('m','p','4','v'), NULL,     0 },
    { "V_MPEG4/MS/V1",   BGAV_MK_FOURCC('M','P','G','4'), NULL,     0 },
    { "V_MPEG4/MS/V2",   BGAV_MK_FOURCC('M','P','4','2'), NULL,     0 },
    { "V_MPEG4/MS/V3",   BGAV_MK_FOURCC('M','P','4','3'), NULL,     0 },
    { "V_REAL/RV10",     BGAV_MK_FOURCC('R','V','1','0'), NULL,     0 },
    { "V_REAL/RV20",     BGAV_MK_FOURCC('R','V','2','0'), NULL,     0 },
    { "V_REAL/RV30",     BGAV_MK_FOURCC('R','V','3','0'), NULL,     0 },
    { "V_REAL/RV40",     BGAV_MK_FOURCC('R','V','4','0'), NULL,     0 },
    { "V_VP8",           BGAV_MK_FOURCC('V','P','8','0'), NULL,     0 },
    { /* End */ }
  };

static void init_acm(bgav_stream_t * s)
  {
  bgav_WAVEFORMAT_t wf;
  bgav_mkv_track_t * p = s->priv;
  
  bgav_WAVEFORMAT_read(&wf, p->CodecPrivate, p->CodecPrivateLen);
  bgav_WAVEFORMAT_get_format(&wf, s);
  bgav_WAVEFORMAT_free(&wf);
  }

static void append_vorbis_extradata(bgav_stream_t * s,
                                    uint8_t * data, int len)
  {
  uint8_t * ptr;
  s->ext_data = realloc(s->ext_data, s->ext_size + len + 4);
  ptr = s->ext_data + s->ext_size;
  BGAV_32BE_2_PTR(len, ptr); ptr+=4;
  memcpy(ptr, data, len);
  s->ext_size += len + 4;
  }

static void init_vorbis(bgav_stream_t * s)
  {
  uint8_t * ptr;
  int len1;
  int len2;
  int len3;
  
  bgav_mkv_track_t * p = s->priv;
  
  ptr = p->CodecPrivate;
  if(*ptr != 0x02)
    {
    bgav_log(s->opt, BGAV_LOG_ERROR, LOG_DOMAIN,
             "Vorbis extradata must start with 0x02n");
    return;
    }
  ptr++;

  /* 1st packet */
  len1 = 0;
  while(*ptr == 255)
    {
    len1 += 255;
    ptr++;
    }
  len1 += *ptr;
  ptr++;

  /* 2nd packet */
  len2 = 0;
  while(*ptr == 255)
    {
    len2 += 255;
    ptr++;
    }
  len2 += *ptr;
  ptr++;

  /* 3rd packet */
  len3 = p->CodecPrivateLen - (ptr - p->CodecPrivate) - len1 - len2;

  /* Append header packets */
  append_vorbis_extradata(s, ptr, len1);
  ptr += len1;
  
  append_vorbis_extradata(s, ptr, len2);
  ptr += len2;

  append_vorbis_extradata(s, ptr, len3);
  
  s->fourcc = BGAV_MK_FOURCC('V','B','I','S');
  s->flags |= STREAM_LACING;
  }


static const codec_info_t audio_codecs[] =
  {
    { "A_MS/ACM",        0x00,                            init_acm,    0 },
    { "A_VORBIS",        0x00,                            init_vorbis, 0 },
    { /* End */ }
  };

static void init_stream_common(mkv_t * m,
                               bgav_stream_t * s,
                               bgav_mkv_track_t * track,
                               const codec_info_t * codecs)
  {
  int i = 0;
  const codec_info_t * info = NULL;
  
  s->priv = track;

  while(codecs[i].id)
    {
    if(((codecs[i].flags & CODEC_FLAG_INCOMPLETE) &&
        !strncmp(codecs[i].id, track->CodecID, strlen(codecs[i].id))) ||
       !strcmp(codecs[i].id, track->CodecID))
      {
      info = &codecs[i];
      break;
      }
    i++;
    }

  if(info)
    {
    s->fourcc = info->fourcc;
    if(info->init_func)
      info->init_func(s);
    else if(track->CodecPrivateLen)
      {
      s->ext_data = malloc(track->CodecPrivateLen);
      memcpy(s->ext_data, track->CodecPrivate, track->CodecPrivateLen);
      s->ext_size = track->CodecPrivateLen;
      }
    }
  s->stream_id = track->TrackNumber;
  s->timescale = 1000000000 / m->segment_info.TimecodeScale;
  }

static int init_audio(bgav_demuxer_context_t * ctx,
                      bgav_mkv_track_t * track)
  {
  bgav_stream_t * s;
  bgav_mkv_track_audio_t * a;
  gavl_audio_format_t * fmt;
  mkv_t * m = ctx->priv;
  
  s = bgav_track_add_audio_stream(ctx->tt->cur, ctx->opt);
  init_stream_common(m, s, track, audio_codecs);

  fmt = &s->data.audio.format;
  a = &track->audio;

  if(a->SamplingFrequency > 0.0)
    {
    fmt->samplerate = (int)a->SamplingFrequency;

    if(a->OutputSamplingFrequency > a->SamplingFrequency)
      {
      fmt->samplerate = (int)a->OutputSamplingFrequency;
      s->flags |= STREAM_SBR;
      }
    }
  if(a->Channels > 0)
    fmt->num_channels = a->Channels;
  if(a->BitDepth > 0)
    s->data.audio.bits_per_sample = a->BitDepth;
  return 1;
  }

static int init_video(bgav_demuxer_context_t * ctx,
                      bgav_mkv_track_t * track)
  {
  bgav_stream_t * s;
  bgav_mkv_track_video_t * v;
  gavl_video_format_t * fmt;
  mkv_t * m = ctx->priv;
  
  s = bgav_track_add_video_stream(ctx->tt->cur, ctx->opt);
  init_stream_common(m, s, track, video_codecs);

  fmt = &s->data.video.format;
  v = &track->video;
  
  if(v->PixelWidth)
    fmt->image_width = v->PixelWidth;
  if(v->PixelHeight)
    fmt->image_height = v->PixelHeight;

#if 0  
  if(v->DisplayWidth && v->DisplayHeight &&
     ((v->DisplayWidth != v->PixelWidth) ||
      (v->DisplayHeight != v->PixelHeight)))
    {
    fmt->pixel_width = 
    }
#else
  fmt->pixel_width = 1;
  fmt->pixel_height = 1;
#endif

  fmt->frame_width = fmt->image_width;
  fmt->frame_height = fmt->image_height;
  fmt->timescale = s->timescale;
  fmt->framerate_mode = GAVL_FRAMERATE_VARIABLE;
  
  return 1;
  }

#define MAX_HEADER_LEN 16

static int open_matroska(bgav_demuxer_context_t * ctx)
  {
  bgav_mkv_element_t e;
  int done;
  int64_t pos;
  mkv_t * p;
  int i;
  bgav_input_context_t * input_mem;

  uint8_t buf[MAX_HEADER_LEN];
  int buf_len;
  int head_len;
  
  input_mem = bgav_input_open_memory(NULL, 0, ctx->opt);
    
  p = calloc(1, sizeof(*p));
  ctx->priv = p;
  p->pts_offset = BGAV_TIMESTAMP_UNDEFINED;
  
  if(!bgav_mkv_ebml_header_read(ctx->input, &p->ebml_header))
    return 0;

  //  bgav_mkv_ebml_header_dump(&p->ebml_header);
  
  /* Get the first segment */
  
  while(1)
    {
    if(!bgav_mkv_element_read(ctx->input, &e))
      return 0;

    if(e.id == MKV_ID_Segment)
      {
      p->segment_start = ctx->input->position;
      break;
      }
    else
      bgav_input_skip(ctx->input, e.size);
    }
  done = 0;
  while(!done)
    {
    pos = ctx->input->position;
    
    buf_len =
      bgav_input_get_data(ctx->input, buf, MAX_HEADER_LEN);

    if(buf_len <= 0)
      return 0;
    
    bgav_input_reopen_memory(input_mem, buf, buf_len);
    
    if(!bgav_mkv_element_read(input_mem, &e))
      return 0;
    head_len = input_mem->position;

    //    fprintf(stderr, "Got element (%d header bytes)\n",
    //            head_len);
    //    bgav_mkv_element_dump(&e);
    
    switch(e.id)
      {
      case MKV_ID_SeekHead:
        bgav_input_skip(ctx->input, head_len);
        e.end += pos;
        if(!bgav_mkv_meta_seek_info_read(ctx->input,
                                         &p->meta_seek_info,
                                         &e))
          return 0;
        // bgav_mkv_meta_seek_info_dump(&p->meta_seek_info);
        break;
      case MKV_ID_Info:
        bgav_input_skip(ctx->input, head_len);
        e.end += pos;
        if(!bgav_mkv_segment_info_read(ctx->input, &p->segment_info, &e))
          return 0;
        bgav_mkv_segment_info_dump(&p->segment_info);
        break;
      case MKV_ID_Tracks:
        bgav_input_skip(ctx->input, head_len);
        e.end += pos;
        if(!bgav_mkv_tracks_read(ctx->input, &p->tracks, &p->num_tracks, &e))
          return 0;
        break;
      case MKV_ID_Cues:
        if(!bgav_mkv_cues_read(ctx->input, &p->cues, p->num_tracks))
          return 0;
        fprintf(stderr, "Got cues before clusters\n");
        p->have_cues = 1;
        break;
      case MKV_ID_Cluster:
        done = 1;
        ctx->data_start = pos;
        ctx->flags |= BGAV_DEMUXER_HAS_DATA_START;
        break;

        
      default:
        bgav_log(ctx->opt, BGAV_LOG_WARNING, LOG_DOMAIN,
                 "Skipping %"PRId64" bytes of element %x in segment\n",
                 e.size, e.id);
        bgav_input_skip(ctx->input, head_len + e.size);
      }
    }

  /* Create track table */
  ctx->tt = bgav_track_table_create(1);
  
  for(i = 0; i < p->num_tracks; i++)
    {
    switch(p->tracks[i].TrackType)
      {
      case MKV_TRACK_VIDEO:
        if(!init_video(ctx, &p->tracks[i]))
          return 0;
        break;
      case MKV_TRACK_AUDIO:
        if(!init_audio(ctx, &p->tracks[i]))
          return 0;
        break;
      case MKV_TRACK_COMPLEX:
        bgav_log(ctx->opt, BGAV_LOG_WARNING, LOG_DOMAIN,
                 "Complex tracks not supported yet\n");
        break;
      case MKV_TRACK_LOGO:
        bgav_log(ctx->opt, BGAV_LOG_WARNING, LOG_DOMAIN,
                 "Logo tracks not supported yet\n");
        break;
      case MKV_TRACK_SUBTITLE:
        bgav_log(ctx->opt, BGAV_LOG_WARNING, LOG_DOMAIN,
                 "Subtitle tracks not supported yet\n");
        break;
      case MKV_TRACK_BUTTONS:
        bgav_log(ctx->opt, BGAV_LOG_WARNING, LOG_DOMAIN,
                 "Button tracks not supported yet\n");
        break;
      case MKV_TRACK_CONTROL:
        bgav_log(ctx->opt, BGAV_LOG_WARNING, LOG_DOMAIN,
                 "Control tracks not supported yet\n");
        break;
      }
    }

  /* Look for file index (cues) */
  if(p->meta_seek_info.num_entries && ctx->input->input->seek_byte &&
     !p->have_cues)
    {
    for(i = 0; i < p->meta_seek_info.num_entries; i++)
      {
      if(p->meta_seek_info.entries[i].SeekID == MKV_ID_Cues)
        {
        fprintf(stderr, "Found index at %"PRId64"\n",
                p->meta_seek_info.entries[i].SeekPosition);

        pos = ctx->input->position;

        bgav_input_seek(ctx->input,
                        p->segment_start +
                        p->meta_seek_info.entries[i].SeekPosition, SEEK_SET);
        
        if(bgav_mkv_cues_read(ctx->input, &p->cues, p->num_tracks))
          p->have_cues = 1;
        bgav_input_seek(ctx->input, pos, SEEK_SET);
        }
      }
    }

  /* get duration */
  if(p->segment_info.Duration > 0.0)
    ctx->tt->cur->duration =
      gavl_seconds_to_time(p->segment_info.Duration * 
                           p->segment_info.TimecodeScale * 1.0e-9);
  return 1;
  }


static int process_block(bgav_demuxer_context_t * ctx,
                         bgav_mkv_element_t * parent)
  {
  bgav_mkv_block_t b;
  bgav_stream_t * s;
  bgav_packet_t * p;
  mkv_t * m = ctx->priv;
  
  if(!bgav_mkv_block_read(ctx->input, &b, parent))
    return 0;
  
  //  bgav_mkv_block_dump(&b);
  
  s = bgav_track_find_stream(ctx, b.track);

  if(!s)
    {
    bgav_input_skip(ctx->input, b.data_size);
    return 1;
    }

  switch(b.flags & MKV_LACING_MASK)
    {
    case MKV_LACING_NONE:
      p = bgav_stream_get_packet_write(s);
      p->data_size = 0;

      if(!(s->flags & STREAM_LACING))
        {
        bgav_packet_alloc(p, b.data_size);
        if(bgav_input_read_data(ctx->input, p->data, b.data_size) <
           b.data_size)
          return 0;
        p->data_size = b.data_size;
        p->pts = b.timecode + m->cluster.Timecode - m->pts_offset;
        }
      else
        {
        if(!bgav_packet_read_segment(p, ctx->input, b.data_size))
          return 0;
        }
      bgav_packet_done_write(p);
      break;
    default:
      fprintf(stderr, "Lacing not supported yet\n");
      bgav_input_skip(ctx->input, b.data_size);
      return 0;
      break;
      
    }
  return 1;
  }

/* next packet: Processes a whole cluster at once */

static int next_packet_matroska(bgav_demuxer_context_t * ctx)
  {
  int num_blocks = 0;
  bgav_mkv_element_t e;
  bgav_mkv_element_t e1;
  mkv_t * priv = ctx->priv;
  fprintf(stderr, "next_packet_matroska\n");
  
  if(!bgav_mkv_element_read(ctx->input, &e))
    return 0;

  if(e.id != MKV_ID_Cluster)
    return 0;
  
  if(!bgav_mkv_cluster_read(ctx->input, &priv->cluster, &e, &e1))
    return 0;
  bgav_mkv_cluster_dump(&priv->cluster);

  if(priv->pts_offset == BGAV_TIMESTAMP_UNDEFINED)
    priv->pts_offset = priv->cluster.Timecode;
  
  while(1)
    {
    if(e1.id == MKV_ID_BlockGroup)
      {
      fprintf(stderr, "Got Block group\n");

      if(!bgav_mkv_element_read(ctx->input, &e1))
        return 0;
      continue;
      }

    else if((e1.id == MKV_ID_Block) ||
            (e1.id == MKV_ID_SimpleBlock))
      {
      if(!process_block(ctx, &e1))
        return 0;
      num_blocks++;
      }
    else
      {
      fprintf(stderr, "Unknown element %x in cluster\n", e1.id);
      return 0;
      }
    
    
    if(ctx->input->position < e.end)
      {
      if(!bgav_mkv_element_read(ctx->input, &e1))
        return !!num_blocks;
      }
    else
      break;
    }
  return 1;
  }


static void close_matroska(bgav_demuxer_context_t * ctx)
  {
  int i;
  mkv_t * priv = ctx->priv;

  bgav_mkv_ebml_header_free(&priv->ebml_header);  
  bgav_mkv_segment_info_free(&priv->segment_info);

  for(i = 0; i < priv->num_tracks; i++)
    bgav_mkv_track_free(&priv->tracks[i]);
  if(priv->tracks)
    free(priv->tracks);
  bgav_mkv_meta_seek_info_free(&priv->meta_seek_info);
  
  bgav_mkv_cues_free(&priv->cues);

  bgav_mkv_cluster_free(&priv->cluster);

  free(priv);
  }

const const bgav_demuxer_t bgav_demuxer_matroska =
  {
    .probe =       probe_matroska,
    .open =        open_matroska,
    // .select_track = select_track_matroska,
    .next_packet = next_packet_matroska,
    // .seek =        seek_matroska,
    // .resync  =     resync_matroska,
    .close =       close_matroska
  };