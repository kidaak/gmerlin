
#include <string.h>
#include <plugin.h>
#include <utils.h>

#include "lqt_common.h"

void bg_lqt_create_codec_info(bg_parameter_info_t * info,
                              int audio, int video, int encode, int decode)
  {
  int num_codecs;
  int i, j, k;
  int num_parameters;
  
  lqt_codec_info_t ** codec_info;

  lqt_parameter_info_t * lqt_parameter_info;
    
  codec_info = lqt_query_registry(audio, video, encode, decode);
  info->type = (encode) ? BG_PARAMETER_ENCODER :
    BG_PARAMETER_DECODER;

  num_codecs = 0;
  while(codec_info[num_codecs])
    num_codecs++;

  info->codec_names        = calloc(num_codecs + 1, sizeof(char*));
  info->codec_long_names   = calloc(num_codecs + 1, sizeof(char*));
  info->codec_descriptions = calloc(num_codecs + 1, sizeof(char*));
  info->codec_parameters   = calloc(num_codecs + 1,
                                    sizeof(bg_parameter_info_t*));

  for(i = 0; i < num_codecs; i++)
    {
    lqt_parameter_info = (encode) ? codec_info[i]->encoding_parameters :
      codec_info[i]->decoding_parameters;
    
    info->codec_names[i] = bg_strdup((char*)0,
                                     codec_info[i]->name);
    info->codec_long_names[i] = bg_strdup((char*)0,
                                         codec_info[i]->long_name);
    info->codec_descriptions[i] = bg_strdup((char*)0,
                                           codec_info[i]->description);

    num_parameters = (encode) ? codec_info[i]->num_encoding_parameters :
      codec_info[i]->num_decoding_parameters;

    if(num_parameters)
      info->codec_parameters[i] = calloc(num_parameters + 1,
                                         sizeof(bg_parameter_info_t));
    
    for(j = 0; j < num_parameters; j++)
      {
      info->codec_parameters[i][j].name =
        bg_sprintf("codec_%s_%s", info->codec_names[i],
                   lqt_parameter_info[j].name);
      info->codec_parameters[i][j].long_name = 
        bg_strdup((char*)0, lqt_parameter_info[j].real_name);

      switch(lqt_parameter_info[j].type)
        {
        case LQT_PARAMETER_INT:
          if(lqt_parameter_info[j].val_min < lqt_parameter_info[j].val_max)
            {
            if((lqt_parameter_info[j].val_min == 0) &&
               (lqt_parameter_info[j].val_max == 1))
              {
              info->codec_parameters[i][j].type = BG_PARAMETER_CHECKBUTTON;
              }
            else
              {
              info->codec_parameters[i][j].type = BG_PARAMETER_SLIDER_INT;
              info->codec_parameters[i][j].val_min.val_i =
                lqt_parameter_info[j].val_min;
              info->codec_parameters[i][j].val_max.val_i =
                lqt_parameter_info[j].val_max;
              }
            }
          else
            {
            info->codec_parameters[i][j].type = BG_PARAMETER_INT;
            }
          info->codec_parameters[i][j].val_default.val_i =
            lqt_parameter_info[j].val_default.val_int;
          break;
        case LQT_PARAMETER_STRING:
          info->codec_parameters[i][j].type = BG_PARAMETER_STRING;
          info->codec_parameters[i][j].val_default.val_str =
            bg_strdup((char*)0,
                      lqt_parameter_info[j].val_default.val_string);
          
          break;
        case LQT_PARAMETER_STRINGLIST:
          info->codec_parameters[i][j].type = BG_PARAMETER_STRINGLIST;
          info->codec_parameters[i][j].val_default.val_str =
            bg_strdup((char*)0,
                      lqt_parameter_info[j].val_default.val_string);

          info->codec_parameters[i][j].options =
            calloc(lqt_parameter_info[j].num_stringlist_options+1,
                   sizeof(char*));
          
          for(k = 0; k < lqt_parameter_info[j].num_stringlist_options; k++)
            {
            info->codec_parameters[i][j].options[k] =
              bg_strdup((char*)0, lqt_parameter_info[j].stringlist_options[k]);
            }
          break;
        }
      
      }
    }
  
  lqt_destroy_codec_info(codec_info);
  }

int bg_lqt_set_parameter(const char * name, bg_parameter_value_t * val,
                         bg_parameter_info_t * info)
  {
  int i, j, done;
  if(strncmp(name, "codec_", 6))
    return 0;
    
  done = 0;
  i = 0;
  while(info->codec_names[i])
    {
    if(info->codec_parameters[i])
      {
      j = 0;
      
      while(info->codec_parameters[i][j].name)
        {
        if(!strcmp(name, info->codec_parameters[i][j].name))
          {
          bg_parameter_value_copy(&(info->codec_parameters[i][j].val_default),
                                  val,
                                  &(info->codec_parameters[i][j]));
          done = 1;
          }
        j++;
        }
      }
    i++;
    if(done)
      break;
    }
  return done;
  }

