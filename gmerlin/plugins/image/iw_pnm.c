/*****************************************************************
 
  iw_pnm.c
 
  Copyright (c) 2003-2004 by Michael Gruenert - one78@web.de
 
  http://gmerlin.sourceforge.net
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 
*****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <plugin.h>
#include <utils.h>

#define BINARY    6
#define ASCII     3

typedef struct
  {
  FILE *output;
  char *comment;
  uint32_t Width;
  uint32_t Height;
  gavl_video_format_t format;
  uint16_t pnm_format;
  } pnm_t;

static void * create_pnm()
  {
  pnm_t * ret;
  ret = calloc(1, sizeof(*ret));
  return ret;
  }

static void destroy_pnm(void* priv)
  {
  pnm_t * pnm = (pnm_t*)priv;
  if(pnm->comment)
    free(pnm->comment);
  free(pnm);
  }

static int write_header_pnm(void * priv, const char * filename,
                     gavl_video_format_t * format)
  {
  pnm_t * p = (pnm_t*)priv;

  p->Width =  format->image_width;
  p->Height =  format->image_height;

  format->pixelformat = GAVL_RGB_24;

  p->output = fopen(filename,"wb");

  if(!p->output)
    return 0;

  /* Write the header lines */  
  if(p->pnm_format == BINARY)
    {
    fprintf(p->output,"P%d\n# %s\n%d %d\n255\n", BINARY, p->comment, p->Width, p->Height);
    }

  if(p->pnm_format == ASCII)
    {
    fprintf(p->output,"P%d\n# %s\n%d %d\n255\n", ASCII, p->comment, p->Width, p->Height);
    }
  
  return 1;
  }

static int write_image_pnm(void *priv, gavl_video_frame_t *frame)
  {
  int i, j;
  pnm_t *p = (pnm_t*)priv;
  uint8_t * frame_ptr;
  uint8_t * frame_ptr_start;

  
  /* write image data binary */
  if(p->pnm_format == BINARY)
    {
    frame_ptr_start = frame->planes[0];
    
    for (i = 0; i < p->Height; i++)
      {
      frame_ptr = frame_ptr_start ;
      fwrite(frame_ptr, 3, p->Width, p->output);
      frame_ptr_start += frame->strides[0];
      }
    }

  /* write image data ascii */
  if(p->pnm_format == ASCII)
    {
    frame_ptr_start = frame->planes[0];
    
    for (i = 0; i < p->Height; i++)
      {
      frame_ptr = frame_ptr_start ;

      for(j = 0; j < p->Width; j++)
        {
        fprintf(p->output," %d %d %d", frame_ptr[0], frame_ptr[1], frame_ptr[2]);
        frame_ptr+=3;
        }
      fprintf(p->output,"\n");
      frame_ptr_start += frame->strides[0];
      }
    }
    
  fclose(p->output);
  return 1;
  }


/* Configuration stuff */

static bg_parameter_info_t parameters[] =
  {
    {
      name:        "format",
      long_name:   "Format",
      type:        BG_PARAMETER_STRINGLIST,
      multi_names: (char*[]){ "binary", "ascii", (char*)0 },
      multi_labels:  (char*[]){ "Binary", "ASCII", (char*)0 },

      val_default: { val_str: "binary" },
    },
    {
      name:        "comment",
      long_name:   "Comment",
      type:        BG_PARAMETER_STRING,

      val_default: { val_str: "Created with gmerlin" },
      help_string: "Comment which will be written in front of every file"
    },
   { /* End of parameters */ }
  };

static bg_parameter_info_t * get_parameters_pnm(void * p)
  {
  return parameters;
  }

static void set_parameter_pnm(void * p, char * name,
                               bg_parameter_value_t * val)
  {
  pnm_t * pnm;
  pnm = (pnm_t *)p;

  if(!name)
    return;

  else if(!strcmp(name, "format"))
    {
    if(!strcmp(val->val_str, "binary"))
      pnm->pnm_format = BINARY;
    else if(!strcmp(val->val_str, "ascii"))
      pnm->pnm_format = ASCII;
    }
  else if(!strcmp(name, "comment"))
    {
    pnm->comment = bg_strdup(pnm->comment, val->val_str);
    }
   
  }

static char * pnm_extension = ".ppm";

static const char * get_extension_pnm(void * p)
  {
  return pnm_extension;
  }

bg_image_writer_plugin_t the_plugin =
  {
    common:
    {
      name:           "iw_pnm",
      long_name:      "PNM writer",
      mimetypes:      (char*)0,
      extensions:     "ppm",
      type:           BG_PLUGIN_IMAGE_WRITER,
      flags:          BG_PLUGIN_FILE,
      priority:       5,
      create:         create_pnm,
      destroy:        destroy_pnm,
      get_parameters: get_parameters_pnm,
      set_parameter:  set_parameter_pnm
    },
    get_extension: get_extension_pnm,
    write_header:  write_header_pnm,
    write_image:   write_image_pnm,
  };

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;

