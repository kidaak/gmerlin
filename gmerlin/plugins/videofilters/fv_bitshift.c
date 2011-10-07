/*****************************************************************
 * gmerlin - a general purpose multimedia framework and applications
 *
 * Copyright (c) 2001 - 2011 Members of the Gmerlin project
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
#include <string.h>
#include <stdio.h>

#include <config.h>
#include <gmerlin/translation.h>
#include <gmerlin/plugin.h>
#include <gmerlin/utils.h>
#include <gmerlin/log.h>

#define LOG_DOMAIN "fv_bitshift"

typedef struct
  {
  int shift;
  
  bg_read_video_func_t read_func;
  void * read_data;
  int read_stream;
  
  gavl_video_format_t format;
  
  int samples_per_line;
  } shift_priv_t;

static void * create_shift()
  {
  shift_priv_t * ret;
  ret = calloc(1, sizeof(*ret));
  return ret;
  }

static void destroy_shift(void * priv)
  {
  shift_priv_t * vp;
  vp = priv;
  free(vp);
  }

static const bg_parameter_info_t parameters[] =
  {
    {
      .gettext_domain = PACKAGE,
      .gettext_directory = LOCALE_DIR,
      .name = "shift",
      .long_name = TRS("Bits to shift"),
      .type = BG_PARAMETER_SLIDER_INT,
      .val_min = { .val_i = 0 },
      .val_max = { .val_i = 8 },
      .flags = BG_PARAMETER_SYNC,
    },
    { /* End of parameters */ },
  };

static const bg_parameter_info_t * get_parameters_shift(void * priv)
  {
  return parameters;
  }

static void set_parameter_shift(void * priv, const char * name,
                               const bg_parameter_value_t * val)
  {
  shift_priv_t * vp;
  vp = priv;

  if(!name)
    return;

  if(!strcmp(name, "shift"))
    vp->shift = val->val_i;
  }

static void connect_input_port_shift(void * priv,
                                    bg_read_video_func_t func,
                                    void * data, int stream, int port)
  {
  shift_priv_t * vp;
  vp = priv;

  if(!port)
    {
    vp->read_func = func;
    vp->read_data = data;
    vp->read_stream = stream;
    }
  
  }

/* We support all 16 bit formats */
static gavl_pixelformat_t pixelformats[] =
  {
    GAVL_GRAY_16,
    GAVL_GRAYA_32,
    GAVL_RGB_48,
    GAVL_RGBA_64,
    GAVL_PIXELFORMAT_NONE,
  };

static void set_input_format_shift(void * priv, gavl_video_format_t * format, int port)
  {
  shift_priv_t * vp;
  int width_mult;
  vp = priv;

  
  if(!port)
    {
    width_mult = 3;
    format->pixelformat = gavl_pixelformat_get_best(format->pixelformat,
                                                    pixelformats,
                                                    NULL);
    gavl_video_format_copy(&vp->format, format);
    
    if(gavl_pixelformat_is_gray(format->pixelformat))
      width_mult = 1;
    if(gavl_pixelformat_has_alpha(format->pixelformat))
      width_mult++;
    vp->samples_per_line = format->image_width * width_mult;
    }
  }

static void get_output_format_shift(void * priv, gavl_video_format_t * format)
  {
  shift_priv_t * vp;
  vp = priv;
  
  gavl_video_format_copy(format, &vp->format);
  }

static int read_video_shift(void * priv, gavl_video_frame_t * frame, int stream)
  {
  shift_priv_t * vp;
  int i, j;
  uint16_t * ptr;
  
  vp = priv;

  if(!vp->read_func(vp->read_data, frame, vp->read_stream))
    return 0;

  if(vp->shift)
    {
    for(i = 0; i < vp->format.image_height; i++)
      {
      ptr = (uint16_t*)(frame->planes[0] + i * frame->strides[0]);

      for(j = 0; j < vp->samples_per_line; j++)
        {
        (*ptr) <<= vp->shift;
        ptr++;
        }
      }
    }
  return 1;
  }

const bg_fv_plugin_t the_plugin = 
  {
    .common =
    {
      BG_LOCALE,
      .name =      "fv_shift",
      .long_name = TRS("Shift image"),
      .description = TRS("Upshift 16 bit images, where only some lower bits are used"),
      .type =     BG_PLUGIN_FILTER_VIDEO,
      .flags =    BG_PLUGIN_FILTER_1,
      .create =   create_shift,
      .destroy =   destroy_shift,
      .get_parameters =   get_parameters_shift,
      .set_parameter =    set_parameter_shift,
      .priority =         1,
    },
    
    .connect_input_port = connect_input_port_shift,
    
    .set_input_format = set_input_format_shift,
    .get_output_format = get_output_format_shift,

    .read_video = read_video_shift,
    
  };

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
