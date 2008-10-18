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

#include <unistd.h>

#include <config.h>
#include <gmerlin/translation.h>

#include "cdaudio.h"
#include <gmerlin/utils.h>
#include <gmerlin/log.h>

#define LOG_DOMAIN "i_cdaudio"

typedef struct
  {
  bg_parameter_info_t * parameters;
  char * device_name;
  bg_track_info_t * track_info;

  void * ripper;
  gavl_audio_frame_t * frame;
  int last_read_samples;
  int read_sectors; /* Sectors to read at once */

  char disc_id[DISCID_SIZE];

  //  int fd;

  CdIo_t *cdio;
  
  bg_cdaudio_index_t * index;

  char * trackname_template;
  int use_cdtext;
  int use_local;

  /* We initialize ripping on demand to speed up CD loading in the
     transcoder */
  int rip_initialized;
  
  /* Configuration stuff */

#ifdef HAVE_MUSICBRAINZ
  int use_musicbrainz;
  char * musicbrainz_host;
  int    musicbrainz_port;
  char * musicbrainz_proxy_host;
  int    musicbrainz_proxy_port;
#endif

#ifdef HAVE_LIBCDDB
  int    use_cddb;
  char * cddb_host;
  int    cddb_port;
  char * cddb_path;
  char * cddb_proxy_host;
  int    cddb_proxy_port;
  char * cddb_proxy_user;
  char * cddb_proxy_pass;
  int cddb_timeout;
#endif
  
  
  int current_track;
  int current_sector; /* For ripping only */
      
  int first_sector;
  
  int do_bypass;

  bg_input_callbacks_t * callbacks;

  int old_seconds;
  bg_cdaudio_status_t status;

  uint32_t samples_written;

  int paused;
    
  const char * disc_name;
  } cdaudio_t;

static void destroy_cd_data(cdaudio_t* cd)
  {
  int i;
  if(cd->track_info && cd->index)
    {
    for(i = 0; i < cd->index->num_audio_tracks; i++)
      bg_track_info_free(&(cd->track_info[i]));
    free(cd->track_info);
    cd->track_info = (bg_track_info_t*)0;
    }
  if(cd->index)
    {
    bg_cdaudio_index_destroy(cd->index);
    cd->index = (bg_cdaudio_index_t*)0;
    }

  }


static const char * get_disc_name_cdaudio(void* priv)
  {
  cdaudio_t * cd;
  cd = (cdaudio_t *)priv;
  return cd->disc_name;
  }

static void close_cdaudio(void * priv);

static void * create_cdaudio()
  {
  cdaudio_t * ret;
  ret = calloc(1, sizeof(*ret));
  ret->ripper = bg_cdaudio_rip_create();
  return ret;
  }

static void set_callbacks_cdaudio(void * data,
                           bg_input_callbacks_t * callbacks)
  {
  cdaudio_t * cd;
  cd = (cdaudio_t *)data;
  cd->callbacks = callbacks;
  }

static void destroy_cdaudio(void * data)
  {
  cdaudio_t * cd;
  cd = (cdaudio_t *)data;

  destroy_cd_data(cd);
  
  if(cd->device_name)
    free(cd->device_name);

  if(cd->ripper)
    bg_cdaudio_rip_destroy(cd->ripper);

  if(cd->parameters)
    bg_parameter_info_destroy_array(cd->parameters);
  
  free(data);
  }

static int open_cdaudio(void * data, const char * arg)
  {
  int have_local_metadata = 0;
  int have_metadata = 0;
  int i, j;

  char * tmp_filename;
    
  cdaudio_t * cd = (cdaudio_t*)data;

  /* Destroy data from previous open */
  destroy_cd_data(cd);
  
  cd->device_name = bg_strdup(cd->device_name, arg);

  cd->cdio = bg_cdaudio_open(cd->device_name);
  if(!cd->cdio)
    return 0;

  cd->index = bg_cdaudio_get_index(cd->cdio);
  if(!cd->index)
    return 0;

  //  bg_cdaudio_index_dump(cd->index);
  
  /* Create track infos */

  cd->track_info = calloc(cd->index->num_audio_tracks, sizeof(*(cd->track_info)));
  
  for(i = 0; i < cd->index->num_tracks; i++)
    {
    if(cd->index->tracks[i].is_audio)
      {
      j = cd->index->tracks[i].index;
      
      cd->track_info[j].num_audio_streams = 1;
      cd->track_info[j].audio_streams =
        calloc(1, sizeof(*(cd->track_info[j].audio_streams)));
      
      cd->track_info[j].audio_streams[0].format.samplerate = 44100;
      cd->track_info[j].audio_streams[0].format.num_channels = 2;
      cd->track_info[j].audio_streams[0].format.sample_format = GAVL_SAMPLE_S16;
      cd->track_info[j].audio_streams[0].format.interleave_mode = GAVL_INTERLEAVE_ALL;
      cd->track_info[j].audio_streams[0].description = bg_strdup(NULL, "CD audio");
      
      gavl_set_channel_setup(&(cd->track_info[j].audio_streams[0].format));
      
      cd->track_info[j].duration =
        ((int64_t)(cd->index->tracks[i].last_sector -
                   cd->index->tracks[i].first_sector + 1) *
         GAVL_TIME_SCALE) / 75;
      cd->track_info[j].description = bg_strdup(NULL, TR("CD audio track"));
      cd->track_info[j].metadata.track = j+1;
      cd->track_info[j].flags = BG_TRACK_SEEKABLE | BG_TRACK_PAUSABLE;
      }
    }

  /* Create the disc ID */

  bg_cdaudio_get_disc_id(cd->index, cd->disc_id);
  
  /* Now, try to get the metadata */

  /* 1st try: Check for cdtext */

  if(cd->use_cdtext)
    {
    if(bg_cdaudio_get_metadata_cdtext(cd->cdio,
                                      cd->track_info,
                                      cd->index))
      {
      bg_log(BG_LOG_INFO, LOG_DOMAIN, "Got metadata from CD-Text");
      have_metadata = 1;
      have_local_metadata = 1; /* We never save cdtext infos */
      }
    }

  /* 2nd try: Local file */

  if(!have_metadata && cd->use_local)
    {
    tmp_filename = bg_search_file_read("cdaudio_metadata", cd->disc_id);
    if(tmp_filename)
      {
      if(bg_cdaudio_load(cd->track_info, tmp_filename))
        {
        have_metadata = 1;
        have_local_metadata = 1;
        bg_log(BG_LOG_INFO, LOG_DOMAIN, "Got metadata from gmerlin cache (%s)", tmp_filename);
        }
      free(tmp_filename);
      }
    }
  
#ifdef HAVE_MUSICBRAINZ
  if(cd->use_musicbrainz && !have_metadata)
    {
    if(bg_cdaudio_get_metadata_musicbrainz(cd->index, cd->track_info,
                                           cd->disc_id,
                                           cd->musicbrainz_host,
                                           cd->musicbrainz_port,
                                           cd->musicbrainz_proxy_host,
                                           cd->musicbrainz_proxy_port))
      {
      bg_log(BG_LOG_INFO, LOG_DOMAIN, "Got metadata from musicbrainz (%s)", cd->musicbrainz_host);
      have_metadata = 1;
      }
    }
#endif

#ifdef HAVE_LIBCDDB
  if(cd->use_cddb && !have_metadata)
    {
    if(bg_cdaudio_get_metadata_cddb(cd->index, cd->track_info,
                                    cd->cddb_host,
                                    cd->cddb_port,
                                    cd->cddb_path,
                                    cd->cddb_proxy_host,
                                    cd->cddb_proxy_port,
                                    cd->cddb_proxy_user,
                                    cd->cddb_proxy_user,
                                    cd->cddb_timeout))
      {
      bg_log(BG_LOG_INFO, LOG_DOMAIN, "Got metadata from CDDB (%s)", cd->cddb_host);
      have_metadata = 1;
      /* Disable gmerlin caching */
      have_local_metadata = 1;
      }
    }
#endif
  
  if(have_metadata && !have_local_metadata)
    {
    tmp_filename = bg_search_file_write("cdaudio_metadata", cd->disc_id);
    if(tmp_filename)
      {
      bg_cdaudio_save(cd->track_info, cd->index->num_audio_tracks, tmp_filename);
      free(tmp_filename);
      }
    }
  
  if(!have_metadata)
    {
    for(i = 0; i < cd->index->num_tracks; i++)
      {
      if(cd->index->tracks[i].is_audio)
        {
        j = cd->index->tracks[i].index;
        if(cd->index->tracks[i].is_audio)
          cd->track_info[j].name = bg_sprintf(TR("Audio CD track %02d"), j+1);
        }
      }
    }
  else
    {
    for(i = 0; i < cd->index->num_tracks; i++)
      {
      if(cd->index->tracks[i].is_audio)
        {
        j = cd->index->tracks[i].index;
        if(cd->index->tracks[i].is_audio)
          cd->track_info[j].name = bg_create_track_name(&(cd->track_info[j].metadata),
                                                        cd->trackname_template);
        }
      }
    if(cd->track_info[0].metadata.album)
      cd->disc_name = cd->track_info[0].metadata.album;
    
    }

  /* We close it again, so other apps won't cry */

  close_cdaudio(cd);
  
  return 1;
  }

static int get_num_tracks_cdaudio(void * data)
  {
  cdaudio_t * cd = (cdaudio_t*)data;
  return cd->index->num_audio_tracks;
  }

static bg_track_info_t * get_track_info_cdaudio(void * data, int track)
  {
  cdaudio_t * cd = (cdaudio_t*)data;
  return &(cd->track_info[track]);
  }

static int set_track_cdaudio(void * data, int track)
  {
  int i;
  cdaudio_t * cd = (cdaudio_t*)data;

  for(i = 0; i < cd->index->num_tracks; i++)
    {
    if(cd->index->tracks[i].is_audio && (cd->index->tracks[i].index == track))
      {
      cd->current_track = i;
      break;
      }
    }

  cd->first_sector = cd->index->tracks[cd->current_track].first_sector;
  
  return 1;
  }

static int set_audio_stream_cdaudio(void * priv, int stream,
                                    bg_stream_action_t action)
  {
  cdaudio_t * cd = (cdaudio_t*)priv;
  if(action == BG_STREAM_ACTION_BYPASS)
    cd->do_bypass = 1;
  else
    cd->do_bypass = 0;
  return 1;
  }

static int start_cdaudio(void * priv)
  {
  int i, last_sector;
  cdaudio_t * cd = (cdaudio_t*)priv;


  if(!cd->cdio)
    {
    cd->cdio = bg_cdaudio_open(cd->device_name);
    if(!cd->cdio)
      return 0;
    }
  
  if(cd->do_bypass)
    {

    last_sector = cd->index->tracks[cd->current_track].last_sector;

    for(i = cd->current_track; i < cd->index->num_tracks; i++)
      {
      if((i == cd->index->num_tracks - 1) || !cd->index->tracks[i+1].is_audio)
        last_sector = cd->index->tracks[i].last_sector;
      }
    if(!bg_cdaudio_play(cd->cdio, cd->first_sector, last_sector))
      {
      bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Play command failed. Disk missing?");
      return 0;
      }
    cd->status.sector = cd->first_sector;
    cd->status.track  = cd->current_track;

    for(i = 0; i < cd->index->num_audio_tracks; i++)
      {
      cd->track_info[i].audio_streams[0].format.samples_per_frame = 588;
      }
    }
  else
    {
    /* Rip */
    
    for(i = 0; i < cd->index->num_audio_tracks; i++)
      {
      cd->track_info[i].audio_streams[0].format.samples_per_frame =
        588;
      }
    
    cd->current_sector = cd->first_sector;
    cd->samples_written = 0;
    }
  return 1;
  }

static void stop_cdaudio(void * priv)
  {
  cdaudio_t * cd = (cdaudio_t*)priv;
  if(cd->do_bypass)
    {
    bg_cdaudio_stop(cd->cdio);
    close_cdaudio(cd);
    }
  else
    {
    if(cd->rip_initialized)
      {
      bg_cdaudio_rip_close(cd->ripper);
      cd->rip_initialized = 0;
      if(cd->frame)
        {
        gavl_audio_frame_destroy(cd->frame);
        cd->frame = (gavl_audio_frame_t*)0;
        }
      }
    }
  cd->cdio = (CdIo_t*)0;
  }

static void read_frame(cdaudio_t * cd)
  {
  if(!cd->rip_initialized)
    {
    gavl_audio_format_t format;
    bg_cdaudio_rip_init(cd->ripper, cd->cdio,
                        cd->first_sector,
                        cd->first_sector - cd->index->tracks[0].first_sector,
                        &(cd->read_sectors));
    
    gavl_audio_format_copy(&format,
                           &(cd->track_info[0].audio_streams[0].format));
    format.samples_per_frame = cd->read_sectors * 588;
    cd->frame = gavl_audio_frame_create(&format);
    cd->rip_initialized = 1;
    }
  bg_cdaudio_rip_rip(cd->ripper, cd->frame);

  if(cd->current_sector + cd->read_sectors >
     cd->index->tracks[cd->current_track].last_sector)
    {
    cd->frame->valid_samples =
      (cd->index->tracks[cd->current_track].last_sector - 
       cd->current_sector + 1) * 588;
    }
  else
    cd->frame->valid_samples = cd->read_sectors * 588;
  cd->last_read_samples = cd->frame->valid_samples;
  cd->current_sector += cd->read_sectors;
  }

static int read_audio_cdaudio(void * priv,
                              gavl_audio_frame_t * frame, int stream,
                              int num_samples)
  {
  int samples_read = 0, samples_copied;
  cdaudio_t * cd = (cdaudio_t*)priv;
  
  if(cd->current_sector > cd->index->tracks[cd->current_track].last_sector)
    {
    if(frame)
      frame->valid_samples = 0;
    return 0;
    }
  while(samples_read < num_samples)
    {
    if(cd->current_sector > cd->index->tracks[cd->current_track].last_sector)
      break;
    
    if(!cd->frame || !cd->frame->valid_samples)
      read_frame(cd);

    samples_copied = gavl_audio_frame_copy(&(cd->track_info[0].audio_streams[0].format),
                                           frame,
                                           cd->frame,
                                           samples_read, /* out_pos */
                                           cd->last_read_samples - cd->frame->valid_samples,  /* in_pos */
                                           num_samples - samples_read, /* out_size, */
                                           cd->frame->valid_samples /* in_size */);
    cd->frame->valid_samples -= samples_copied;
    samples_read += samples_copied;
    
    }
  if(frame)
    frame->valid_samples = samples_read;
  cd->samples_written += samples_read;
  return samples_read;
  }

static int bypass_cdaudio(void * priv)
  {
  int j;
  int seconds;
  
  cdaudio_t * cd = (cdaudio_t*)priv;

  if(!bg_cdaudio_get_status(cd->cdio, &(cd->status)))
    {
    return 0;
    }
  if((cd->status.track < cd->current_track) ||
     (cd->status.track > cd->current_track+1))
    {
    cd->status.track = cd->current_track;
    return 1;
    }
  if(cd->status.track == cd->current_track + 1)
    {
    cd->current_track = cd->status.track;

    j = cd->index->tracks[cd->current_track].index;
    
    if(cd->callbacks)
      {
      if(cd->callbacks->track_changed)
        cd->callbacks->track_changed(cd->callbacks->data,
                                     cd->current_track);

      if(cd->callbacks->name_changed)
        cd->callbacks->name_changed(cd->callbacks->data,
                                    cd->track_info[j].name);

      if(cd->callbacks->duration_changed)
        cd->callbacks->duration_changed(cd->callbacks->data,
                                        cd->track_info[j].duration);

      if(cd->callbacks->metadata_changed)
        cd->callbacks->metadata_changed(cd->callbacks->data,
                                        &(cd->track_info[j].metadata));
      

      }
    cd->first_sector = cd->index->tracks[cd->current_track].first_sector;
    }

  seconds = (cd->status.sector - cd->first_sector) / 75;

  if(cd->old_seconds != seconds)
    {
    cd->old_seconds = seconds;

    if((cd->callbacks) && (cd->callbacks->time_changed))
      {
      cd->callbacks->time_changed(cd->callbacks->data,
                                  ((gavl_time_t)seconds) * GAVL_TIME_SCALE);
      }
    }
  
  return 1;
  }

static void seek_cdaudio(void * priv, int64_t * time, int scale)
  {
  /* We seek with frame accuracy (1/75 seconds) */

  int i, last_sector;
  int sector;
  uint32_t sample_position, samples_to_skip;
  
  cdaudio_t * cd = (cdaudio_t*)priv;
  
  if(cd->do_bypass)
    {
    sector = cd->index->tracks[cd->current_track].first_sector +
      (*time * 75) / scale;
    
    last_sector = cd->index->tracks[cd->current_track].last_sector;

    for(i = cd->current_track; i < cd->index->num_tracks; i++)
      {
      if((i == cd->index->num_tracks - 1) || !cd->index->tracks[i+1].is_audio)
        last_sector = cd->index->tracks[i].last_sector;
      }
    *time = ((int64_t)sector * scale) / 75;
    if(!bg_cdaudio_play(cd->cdio, sector, last_sector))
      return;
    
    if(cd->paused)
      {
      bg_cdaudio_set_pause(cd->cdio, 1);
      }
    }
  else
    {
    if(!cd->rip_initialized)
      {
      gavl_audio_format_t format;
      bg_cdaudio_rip_init(cd->ripper, cd->cdio,
                          cd->first_sector,
                          cd->first_sector - cd->index->tracks[0].first_sector,
                          &(cd->read_sectors));

      gavl_audio_format_copy(&format,
                             &(cd->track_info[0].audio_streams[0].format));
      format.samples_per_frame = cd->read_sectors * 588;
      cd->frame = gavl_audio_frame_create(&format);
      cd->rip_initialized = 1;
      }
    
    sample_position = gavl_time_rescale(scale, 44100, *time);
        
    cd->current_sector =
      sample_position / 588 + cd->index->tracks[cd->current_track].first_sector;
    samples_to_skip = sample_position % 588;

    /* Seek to the point */

    bg_cdaudio_rip_seek(cd->ripper, cd->current_sector,
                        cd->current_sector - cd->index->tracks[0].first_sector);

    /* Read one frame os samples (can be more than one sector) */
    read_frame(cd);

    /* Set skipped samples */
    
    cd->frame->valid_samples -= samples_to_skip;
    }
  }

static void bypass_set_pause_cdaudio(void * priv, int pause)
  {
  cdaudio_t * cd = (cdaudio_t*)priv;
  bg_cdaudio_set_pause(cd->cdio, pause);
  cd->paused = pause;
  }

static void bypass_set_volume_cdaudio(void * priv, float volume)
  {
  cdaudio_t * cd = (cdaudio_t*)priv;
  bg_cdaudio_set_volume(cd->cdio, volume);
  }

static void close_cdaudio(void * priv)
  {
  cdaudio_t * cd = (cdaudio_t*)priv;
  if(cd->cdio)
    {
    bg_cdaudio_close(cd->cdio);
    }
  cd->cdio = (CdIo_t*)0;
  }

/* Configuration stuff */

static const bg_parameter_info_t parameters[] =
  {
    {
      .name =      "general",
      .long_name = TRS("General"),
      .type =      BG_PARAMETER_SECTION
    },
    {
      .name =        "trackname_template",
      .long_name =   TRS("Trackname template"),
      .type =        BG_PARAMETER_STRING,
      .val_default = { .val_str = "%p - %t" },
      .help_string = TRS("Template for track name generation from metadata\n\
%p:    Artist\n\
%a:    Album\n\
%g:    Genre\n\
%t:    Track name\n\
%<d>n: Track number (d = number of digits, 1-9)\n\
%y:    Year\n\
%c:    Comment")
    },
    {
      .name =        "use_cdtext",
      .long_name =   TRS("Use CD-Text"),
      .type =        BG_PARAMETER_CHECKBUTTON,
      .val_default = { .val_i = 1 },
      .help_string = TRS("Try to get CD metadata from CD-Text"),
    },
    {
      .name =        "use_local",
      .long_name =   TRS("Use locally saved metadata"),
      .type =        BG_PARAMETER_CHECKBUTTON,
      .val_default = { .val_i = 1 },
      .help_string = TRS("Whenever we obtain CD metadata from the internet, we save them into \
$HOME/.gmerlin/cdaudio_metadata. If you got wrong metadata for a CD,\
 disabling this option will retrieve the metadata again and overwrite the saved data."),
    },
#ifdef HAVE_MUSICBRAINZ
    {
      .name =      "musicbrainz",
      .long_name = TRS("Musicbrainz"),
      .type =      BG_PARAMETER_SECTION
    },
    {
      .name =        "use_musicbrainz",
      .long_name =   TRS("Use Musicbrainz"),
      .type =        BG_PARAMETER_CHECKBUTTON,
      .val_default = { .val_i = 1 }
    },
    {
      .name =        "musicbrainz_host",
      .long_name =   TRS("Server"),
      .type =        BG_PARAMETER_STRING,
      .val_default = { .val_str = "mm.musicbrainz.org" }
    },
    {
      .name =        "musicbrainz_port",
      .long_name =   TRS("Port"),
      .type =         BG_PARAMETER_INT,
      .val_min =      { .val_i = 1 },
      .val_max =      { .val_i = 65535 },
      .val_default =  { .val_i = 80 }
    },
    {
      .name =        "musicbrainz_proxy_host",
      .long_name =   TRS("Proxy"),
      .type =        BG_PARAMETER_STRING,
      .help_string = TRS("Proxy server (leave empty for direct connection)")
    },
    {
      .name =        "musicbrainz_proxy_port",
      .long_name =   TRS("Proxy port"),
      .type =         BG_PARAMETER_INT,
      .val_min =      { .val_i = 1 },
      .val_max =      { .val_i = 65535 },
      .val_default =  { .val_i = 80 },
      .help_string = TRS("Proxy port")
    },
#endif
#ifdef HAVE_LIBCDDB
    {
      .name =      "cddb",
      .long_name = TRS("Cddb"),
      .type =      BG_PARAMETER_SECTION
    },
    {
      .name =        "use_cddb",
      .long_name =   TRS("Use Cddb"),
      .type =        BG_PARAMETER_CHECKBUTTON,
      .val_default = { .val_i = 1 }
    },
    {
      .name =        "cddb_host",
      .long_name =   TRS("Server"),
      .type =        BG_PARAMETER_STRING,
      .val_default = { .val_str = "freedb.org" }
    },
    {
      .name =        "cddb_port",
      .long_name =   TRS("Port"),
      .type =         BG_PARAMETER_INT,
      .val_min =      { .val_i = 1 },
      .val_max =      { .val_i = 65535 },
      .val_default =  { .val_i = 80 }
    },
    {
      .name =        "cddb_path",
      .long_name =   TRS("Path"),
      .type =        BG_PARAMETER_STRING,
      .val_default = { .val_str = "/~cddb/cddb.cgi" }
    },
    {
      .name =        "cddb_proxy_host",
      .long_name =   TRS("Proxy"),
      .type =        BG_PARAMETER_STRING,
      .help_string = TRS("Proxy server (leave empty for direct connection)")
    },
    {
      .name =        "cddb_proxy_port",
      .long_name =   TRS("Proxy port"),
      .type =         BG_PARAMETER_INT,
      .val_min =      { .val_i = 1 },
      .val_max =      { .val_i = 65535 },
      .val_default =  { .val_i = 80 },
      .help_string = TRS("Proxy port")
    },
    {
      .name =        "cddb_proxy_user",
      .long_name =   TRS("Proxy username"),
      .type =        BG_PARAMETER_STRING,
      .help_string = TRS("User name for proxy (leave empty for poxies, which don't require authentication)")
    },
    {
      .name =        "cddb_proxy_pass",
      .long_name =   TRS("Proxy password"),
      .type =        BG_PARAMETER_STRING_HIDDEN,
      .help_string = TRS("Password for proxy")
    },
    {
      .name =        "cddb_timeout",
      .long_name =   TRS("Timeout"),
      .type =         BG_PARAMETER_INT,
      .val_min =      { .val_i = 0 },
      .val_max =      { .val_i = 1000 },
      .val_default =  { .val_i = 10 },
      .help_string = TRS("Timeout (in seconds) for connections to the CDDB server")
    },
#endif
    { /* End of parmeters */ }
  };

static const bg_parameter_info_t * get_parameters_cdaudio(void * data)
  {
  cdaudio_t * cd = (cdaudio_t*)data;
  bg_parameter_info_t const * srcs[3];

  if(!cd->parameters)
    {
    srcs[0] = parameters;
    srcs[1] = bg_cdaudio_rip_get_parameters();
    srcs[2] = (bg_parameter_info_t*)0;
    cd->parameters = bg_parameter_info_concat_arrays(srcs);
    }
    
  return cd->parameters;
  }

static void set_parameter_cdaudio(void * data, const char * name,
                                  const bg_parameter_value_t * val)
  {
  cdaudio_t * cd = (cdaudio_t*)data;

  if(!name)
    return;

  if(bg_cdaudio_rip_set_parameter(cd->ripper, name, val))
    return;

  if(!strcmp(name, "trackname_template"))
    cd->trackname_template = bg_strdup(cd->trackname_template, val->val_str);

  if(!strcmp(name, "use_cdtext"))
    cd->use_cdtext = val->val_i;
  if(!strcmp(name, "use_local"))
    cd->use_local = val->val_i;
  
#ifdef HAVE_MUSICBRAINZ
  if(!strcmp(name, "use_musicbrainz"))
    cd->use_musicbrainz = val->val_i;
  if(!strcmp(name, "musicbrainz_host"))
    cd->musicbrainz_host = bg_strdup(cd->musicbrainz_host, val->val_str);
  if(!strcmp(name, "musicbrainz_port"))
    cd->musicbrainz_port = val->val_i;
  if(!strcmp(name, "musicbrainz_proxy_host"))
    cd->musicbrainz_proxy_host = bg_strdup(cd->musicbrainz_proxy_host, val->val_str);
  if(!strcmp(name, "musicbrainz_proxy_port"))
    cd->musicbrainz_proxy_port = val->val_i;
#endif

#ifdef HAVE_LIBCDDB
  if(!strcmp(name, "use_cddb"))
    cd->use_cddb = val->val_i;
  if(!strcmp(name, "cddb_host"))
    cd->cddb_host = bg_strdup(cd->cddb_host, val->val_str);
  if(!strcmp(name, "cddb_port"))
    cd->cddb_port = val->val_i;
  if(!strcmp(name, "cddb_path"))
    cd->cddb_path = bg_strdup(cd->cddb_path, val->val_str);
  if(!strcmp(name, "cddb_proxy_host"))
    cd->cddb_proxy_host = bg_strdup(cd->cddb_proxy_host, val->val_str);
  if(!strcmp(name, "cddb_proxy_port"))
    cd->cddb_proxy_port = val->val_i;
  if(!strcmp(name, "cddb_proxy_user"))
    cd->cddb_proxy_user = bg_strdup(cd->cddb_proxy_user, val->val_str);
  if(!strcmp(name, "cddb_proxy_pass"))
    cd->cddb_proxy_pass = bg_strdup(cd->cddb_proxy_pass, val->val_str);
  if(!strcmp(name, "cddb_timeout"))
    cd->cddb_timeout = val->val_i;
#endif
  }

static int eject_disc_cdaudio(const char * device)
  {
#if LIBCDIO_VERSION_NUM >= 78
  
  driver_return_code_t err;
  if((err = cdio_eject_media_drive(device)) != DRIVER_OP_SUCCESS)
    {
#if LIBCDIO_VERSION_NUM >= 77
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Ejecting disk failed: %s", cdio_driver_errmsg(err));
#else
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Ejecting disk failed");
#endif
    return 0;
    }
  else
    return 1;
#else
  return 0;
#endif
  }

char const * const protocols = "cda";

static const char * get_protocols(void * priv)
  {
  return protocols;
  }

const bg_input_plugin_t the_plugin =
  {
    .common =
    {
      BG_LOCALE,
      .name =          "i_cdaudio",
      .long_name =     TRS("Audio CD player/ripper"),
      .description =   TRS("Plugin for audio CDs. Supports both playing with direct connection from the CD-drive to the souncard and ripping with cdparanoia. Metadata are obtained from Musicbrainz, freedb or CD-text. Metadata are cached in $HOME/.gmerlin/cdaudio_metadata."),
      .type =          BG_PLUGIN_INPUT,

      .flags =         BG_PLUGIN_REMOVABLE |
                     BG_PLUGIN_BYPASS |
                     BG_PLUGIN_KEEP_RUNNING |
                     BG_PLUGIN_INPUT_HAS_SYNC,
      
      .priority =      BG_PLUGIN_PRIORITY_MAX,
      .create =        create_cdaudio,
      .destroy =       destroy_cdaudio,
      .get_parameters = get_parameters_cdaudio,
      .set_parameter =  set_parameter_cdaudio,
      .find_devices = bg_cdaudio_find_devices,
      .check_device = bg_cdaudio_check_device,
    },
    .get_protocols = get_protocols,
    /* Open file/device */
    .open = open_cdaudio,
    .get_disc_name = get_disc_name_cdaudio,
#if LIBCDIO_VERSION_NUM >= 78
    .eject_disc = eject_disc_cdaudio,
#endif
    .set_callbacks = set_callbacks_cdaudio,
  /* For file and network plugins, this can be NULL */
    .get_num_tracks = get_num_tracks_cdaudio,
    /* Return track information */
    .get_track_info = get_track_info_cdaudio,
    
    /* Set track */
    .set_track =             set_track_cdaudio,
    /* Set streams */
    .set_audio_stream =      set_audio_stream_cdaudio,
    .set_video_stream =      NULL,
    .set_subtitle_stream =   NULL,

    /*
     *  Start decoding.
     *  Track info is the track, which should be played.
     *  The plugin must take care of the "active" fields
     *  in the stream infos to check out, which streams are to be decoded
     */
    .start =                 start_cdaudio,
    /* Read one audio frame (returns FALSE on EOF) */
    .read_audio =    read_audio_cdaudio,
    
    .bypass =                bypass_cdaudio,
    .bypass_set_pause =      bypass_set_pause_cdaudio,
    .bypass_set_volume =     bypass_set_volume_cdaudio,

    /*
     *  Do percentage seeking (can be NULL)
     *  Media streams are supposed to be seekable, if this
     *  function is non-NULL AND the duration field of the track info
     *  is > 0
     */
    .seek =         seek_cdaudio,
    /* Stop playback, close all decoders */
    .stop =         stop_cdaudio,
    .close =        close_cdaudio,
  };
/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
