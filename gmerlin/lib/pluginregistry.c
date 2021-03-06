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

#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

#include <config.h>

#include <gmerlin/cfg_registry.h>
#include <gmerlin/pluginregistry.h>
#include <pluginreg_priv.h>
#include <config.h>
#include <gmerlin/utils.h>
#include <gmerlin/singlepic.h>

#include <gavfenc.h>

#include <gmerlin/translation.h>

#include <gmerlin/log.h>

#include <bgladspa.h>
#include <bgfrei0r.h>

#include <ovl2text.h>


#ifdef HAVE_LV
#include <bglv.h>
#endif

#define LOG_DOMAIN "pluginregistry"

static struct
  {
  const char * name;
  bg_plugin_info_t *        (*get_info)(bg_plugin_registry_t * plugin_reg);
  const bg_plugin_common_t* (*get_plugin)();
  void *                    (*create)(bg_plugin_registry_t * plugin_reg);
  }
meta_plugins[] =
  {
    {
      bg_singlepic_stills_input_name,
      bg_singlepic_stills_input_info,
      bg_singlepic_stills_input_get,
      bg_singlepic_stills_input_create,
    },
    {
      bg_singlepic_input_name,
      bg_singlepic_input_info,
      bg_singlepic_input_get,
      bg_singlepic_input_create,
    },
    {
      bg_singlepic_encoder_name,
      bg_singlepic_encoder_info,
      bg_singlepic_encoder_get,
      bg_singlepic_encoder_create,
    },
    {
      bg_ovl2text_name,
      bg_ovl2text_info,
      bg_ovl2text_get,
      bg_ovl2text_create,
    },
    {
      bg_gavfenc_name,
      bg_gavfenc_info,
      bg_gavfenc_get,
      bg_gavfenc_create,
    },
    { /* End */ }
  };


struct bg_plugin_registry_s
  {
  bg_plugin_info_t * entries;
  bg_cfg_section_t * config_section;

  int changed;
  };

void bg_plugin_info_destroy(bg_plugin_info_t * info)
  {
  
  if(info->gettext_domain)
    free(info->gettext_domain);
  if(info->gettext_directory)
    free(info->gettext_directory);

  if(info->name)
    free(info->name);
  if(info->long_name)
    free(info->long_name);
  if(info->description)
    free(info->description);
  if(info->mimetypes)
    free(info->mimetypes);
  if(info->extensions)
    free(info->extensions);
  if(info->protocols)
    free(info->protocols);
  if(info->module_filename)
    free(info->module_filename);
  if(info->devices)
    bg_device_info_destroy(info->devices);
  if(info->cmp_name)
    free(info->cmp_name);
  if(info->compressions)
    free(info->compressions);
  
  if(info->parameters)
    bg_parameter_info_destroy_array(info->parameters);
  if(info->audio_parameters)
    bg_parameter_info_destroy_array(info->audio_parameters);
  if(info->video_parameters)
    bg_parameter_info_destroy_array(info->video_parameters);
  if(info->text_parameters)
    bg_parameter_info_destroy_array(info->text_parameters);
  if(info->overlay_parameters)
    bg_parameter_info_destroy_array(info->overlay_parameters);
  
  free(info);
  }

static void free_info_list(bg_plugin_info_t * entries)
  {
  bg_plugin_info_t * info;
  
  info = entries;

  while(info)
    {
    entries = info->next;
    bg_plugin_info_destroy(info);
    info = entries;
    }
  }

static void make_cmp_name(bg_plugin_info_t * i)
  {
  char * tmp_string;
  int len;
  bg_bindtextdomain(i->gettext_domain,
                    i->gettext_directory);

  tmp_string =
    bg_utf8_to_system(TRD(i->long_name, i->gettext_domain), -1);
  
  len = strxfrm(NULL, tmp_string, 0);
  i->cmp_name = malloc(len+1);
  strxfrm(i->cmp_name, tmp_string, len+1);
  free(tmp_string);


  }

static int compare_swap(bg_plugin_info_t * i1,
                        bg_plugin_info_t * i2)
  {
  if((i1->flags & BG_PLUGIN_FILTER_1) &&
     (i2->flags & BG_PLUGIN_FILTER_1))
    {
    if(!i1->cmp_name)
      {
      make_cmp_name(i1);
      }
    if(!i2->cmp_name)
      {
      make_cmp_name(i2);
      }

    return strcmp(i1->cmp_name, i2->cmp_name) > 0;
    }
  else if((!(i1->flags & BG_PLUGIN_FILTER_1)) &&
          (!(i2->flags & BG_PLUGIN_FILTER_1)))
    {
    return i1->priority < i2->priority;
    }
  else if((!(i1->flags & BG_PLUGIN_FILTER_1)) &&
          (i2->flags & BG_PLUGIN_FILTER_1))
    return 1;
  
  return 0;
  }
                           

static bg_plugin_info_t * sort_by_priority(bg_plugin_info_t * list)
  {
  int i, j;
  bg_plugin_info_t * info;
  bg_plugin_info_t ** arr;
  int num_plugins = 0;
  int keep_going;

  if(NULL==list)
    return NULL;
  
  /* Count plugins */

  info = list;
  while(info)
    {
    num_plugins++;
    info = info->next;
    }

  /* Allocate array */
  arr = malloc(num_plugins * sizeof(*arr));
  info = list;
  for(i = 0; i < num_plugins; i++)
    {
    arr[i] = info;
    info = info->next;
    }

  /* Bubblesort */

  for(i = 0; i < num_plugins - 1; i++)
    {
    keep_going = 0;
    for(j = num_plugins-1; j > i; j--)
      {
      if(compare_swap(arr[j-1], arr[j]))
        {
        info  = arr[j];
        arr[j]   = arr[j-1];
        arr[j-1] = info;
        keep_going = 1;
        }
      }
    if(!keep_going)
      break;
    }

  /* Rechain */

  for(i = 0; i < num_plugins-1; i++)
    arr[i]->next = arr[i+1];
  if(num_plugins>0)
    arr[num_plugins-1]->next = NULL;
  list = arr[0];
  /* Free array */
  free(arr);
  
  return list;
  }

static bg_plugin_info_t *
find_by_dll(bg_plugin_info_t * info, const char * filename)
  {
  while(info)
    {
    if(info->module_filename && !strcmp(info->module_filename, filename))
      return info;
    info = info->next;
    }
  return NULL;
  }

static bg_plugin_info_t *
find_by_name(bg_plugin_info_t * info, const char * name)
  {
  while(info)
    {
    if(!strcmp(info->name, name))
      return info;
    info = info->next;
    }
  return NULL;
  }

const bg_plugin_info_t * bg_plugin_find_by_name(bg_plugin_registry_t * reg,
                                                const char * name)
  {
  return find_by_name(reg->entries, name);
  }

const bg_plugin_info_t * bg_plugin_find_by_protocol(bg_plugin_registry_t * reg,
                                                    const char * protocol)
  {
  const bg_plugin_info_t * info = reg->entries;
  while(info)
    {
    if(bg_string_match(protocol, info->protocols))
      return info;
    info = info->next;
    }
  return NULL;
  }

const bg_plugin_info_t * bg_plugin_find_by_filename(bg_plugin_registry_t * reg,
                                                    const char * filename,
                                                    int typemask)
  {
  char * extension;
  bg_plugin_info_t * info, *ret = NULL;
  int max_priority = BG_PLUGIN_PRIORITY_MIN - 1;

  if(!filename)
    return NULL;
  
  
  info = reg->entries;
  extension = strrchr(filename, '.');

  if(!extension)
    {
    return NULL;
    }
  extension++;
  
  
  while(info)
    {
    if(!(info->type & typemask) ||
       !(info->flags & BG_PLUGIN_FILE) ||
       !info->extensions)
      {
      info = info->next;
      continue;
      }
    if(bg_string_match(extension, info->extensions))
      {
      if(max_priority < info->priority)
        {
        max_priority = info->priority;
        ret = info;
        }
      // return info;
      }
    info = info->next;
    }
  return ret;
  }

const bg_plugin_info_t * bg_plugin_find_by_mimetype(bg_plugin_registry_t * reg,
                                                    const char * mimetype,
                                                    int typemask)
  {
  bg_plugin_info_t * info, *ret = NULL;
  int max_priority = BG_PLUGIN_PRIORITY_MIN - 1;

  if(!mimetype)
    return NULL;
  
  info = reg->entries;
  
  while(info)
    {
    if(!(info->type & typemask) ||
       !info->mimetypes)
      {
      info = info->next;
      continue;
      }
    if(bg_string_match(mimetype, info->mimetypes))
      {
      if(max_priority < info->priority)
        {
        max_priority = info->priority;
        ret = info;
        }
      // return info;
      }
    info = info->next;
    }
  return ret;
  }


const bg_plugin_info_t *
bg_plugin_find_by_compression(bg_plugin_registry_t * reg,
                              gavl_codec_id_t id,
                              int typemask, int flagmask)
  {
  int i;
  bg_plugin_info_t * info, *ret = NULL;
  int max_priority = BG_PLUGIN_PRIORITY_MIN - 1;

  info = reg->entries;
  
  while(info)
    {
    if(!(info->type & typemask) ||
       !(info->flags & flagmask) ||
       !info->compressions)
      {
      info = info->next;
      continue;
      }

    i = 0;
    while(info->compressions[i] != GAVL_CODEC_ID_NONE)
      {
      if(info->compressions[i] == id)
        {
        if(max_priority < info->priority)
          {
          max_priority = info->priority;
          ret = info;
          }
        }
      i++;
      }
    
    info = info->next;
    }
  return ret;
  }


static bg_plugin_info_t * remove_from_list(bg_plugin_info_t * list,
                                           bg_plugin_info_t * info)
  {
  bg_plugin_info_t * before;
  if(info == list)
    {
    list = list->next;
    info->next = NULL;
    return list;
    }

  before = list;

  while(before->next != info)
    before = before->next;
    
  before->next = info->next;
  info->next = NULL;
  return list;
  }

static bg_plugin_info_t * remove_duplicate(bg_plugin_info_t * list)
  {
  bg_plugin_info_t * info_1, * info_2, * next;
  int del = 0;
  info_1 = list;

  while(info_1)
    {
    /* Check if info_1 is already in the list */
    info_2 = list;
    del = 0;
    
    while(info_2 != info_1)
      {
      if(info_1->name && info_2->name &&
         !strcmp(info_1->name, info_2->name))
        {
        next = info_1->next;
        list = remove_from_list(list, info_1);
        info_1 = next;
        del = 1;
        break;
        }
      else
        info_2 = info_2->next;
      }
    if(!del)
      info_1 = info_1->next;
    }
  return list;
  }

static bg_plugin_info_t * append_to_list(bg_plugin_info_t * list,
                                         bg_plugin_info_t * info)
  {
  bg_plugin_info_t * end;
  if(!list)
    return info;
  
  end = list;
  while(end->next)
    end = end->next;
  end->next = info;
  return list;
  }

static int check_plugin_version(void * handle)
  {
  int (*get_plugin_api_version)();

  get_plugin_api_version = dlsym(handle, "get_plugin_api_version");
  if(!get_plugin_api_version)
    return 0;

  if(get_plugin_api_version() != BG_PLUGIN_API_VERSION)
    return 0;
  return 1;
  }

static void set_preset_path(bg_parameter_info_t * info, const char * prefix)
  {
  //  int i;
  
  if(info->type == BG_PARAMETER_SECTION)
    info->flags |= BG_PARAMETER_GLOBAL_PRESET;
  info->preset_path = gavl_strrep(info->preset_path, prefix);
  }

bg_plugin_info_t * bg_plugin_info_create(const bg_plugin_common_t * plugin)
  {
  bg_plugin_info_t * new_info;
  new_info = calloc(1, sizeof(*new_info));

  new_info->name = gavl_strrep(new_info->name, plugin->name); 	 
	  	 
  new_info->long_name =  gavl_strrep(new_info->long_name, 	 
                                   plugin->long_name); 	 
	  	 
  new_info->description = gavl_strrep(new_info->description, 	 
                                    plugin->description);
  
  new_info->gettext_domain = gavl_strrep(new_info->gettext_domain, 	 
                                       plugin->gettext_domain); 	 
  new_info->gettext_directory = gavl_strrep(new_info->gettext_directory, 	 
                                          plugin->gettext_directory); 	 
  new_info->type        = plugin->type; 	 
  new_info->flags       = plugin->flags; 	 
  new_info->priority    = plugin->priority;

  if(plugin->type & (BG_PLUGIN_ENCODER_AUDIO|
                     BG_PLUGIN_ENCODER_VIDEO|
                     BG_PLUGIN_ENCODER_TEXT |
                     BG_PLUGIN_ENCODER_OVERLAY |
                     BG_PLUGIN_ENCODER ))
    {
    bg_encoder_plugin_t * encoder;
    encoder = (bg_encoder_plugin_t*)plugin;
    new_info->max_audio_streams = encoder->max_audio_streams;
    new_info->max_video_streams = encoder->max_video_streams;
    new_info->max_text_streams = encoder->max_text_streams;
    new_info->max_overlay_streams = encoder->max_overlay_streams;
    }
  
  return new_info;
  }

static bg_plugin_info_t * plugin_info_create(const bg_plugin_common_t * plugin,
                                             void * plugin_priv,
                                             const char * module_filename)
  {
  char * prefix;
  bg_plugin_info_t * new_info;
  const bg_parameter_info_t * parameter_info;
  
  new_info = bg_plugin_info_create(plugin);

  new_info->module_filename = gavl_strrep(new_info->module_filename, 	 
                                        module_filename);
  
  if(plugin->get_parameters)
    {
    parameter_info = plugin->get_parameters(plugin_priv);
    if(parameter_info)
      new_info->parameters = bg_parameter_info_copy_array(parameter_info);
    if(new_info->parameters)
      {
      prefix = bg_sprintf("plugins/%s", new_info->name);
      set_preset_path(new_info->parameters, prefix);
      free(prefix);
      }
    }
  
  if(plugin->type & (BG_PLUGIN_ENCODER_AUDIO|
                     BG_PLUGIN_ENCODER_VIDEO|
                     BG_PLUGIN_ENCODER_TEXT |
                     BG_PLUGIN_ENCODER_OVERLAY |
                     BG_PLUGIN_ENCODER ))
    {
    bg_encoder_plugin_t * encoder;
    encoder = (bg_encoder_plugin_t*)plugin;
    
    if(encoder->get_audio_parameters)
      {
      parameter_info = encoder->get_audio_parameters(plugin_priv);
      new_info->audio_parameters = bg_parameter_info_copy_array(parameter_info);
      if(new_info->audio_parameters)
        {
        prefix = bg_sprintf("plugins/%s/audio", new_info->name);
        set_preset_path(new_info->audio_parameters, prefix);
        free(prefix);
        }
      }
    
    if(encoder->get_video_parameters)
      {
      parameter_info = encoder->get_video_parameters(plugin_priv);
      new_info->video_parameters = bg_parameter_info_copy_array(parameter_info);
      if(new_info->video_parameters)
        {
        prefix = bg_sprintf("plugins/%s/video", new_info->name);
        set_preset_path(new_info->video_parameters, prefix);
        free(prefix);
        }
      }
    if(encoder->get_text_parameters)
      {
      parameter_info = encoder->get_text_parameters(plugin_priv);
      new_info->text_parameters = bg_parameter_info_copy_array(parameter_info);
      if(new_info->text_parameters)
        {
        prefix = bg_sprintf("plugins/%s/text", new_info->name);
        set_preset_path(new_info->text_parameters, prefix);
        free(prefix);
        }
      }
    if(encoder->get_overlay_parameters)
      {
      parameter_info = encoder->get_overlay_parameters(plugin_priv);
      new_info->overlay_parameters =
        bg_parameter_info_copy_array(parameter_info);
      if(new_info->overlay_parameters)
        {
        prefix = bg_sprintf("plugins/%s/overlay", new_info->name);
        set_preset_path(new_info->overlay_parameters, prefix);
        free(prefix);
        }
      }
    }
  if(plugin->type & BG_PLUGIN_INPUT)
    {
    bg_input_plugin_t  * input;
    input = (bg_input_plugin_t*)plugin;

    if(input->get_mimetypes)
      new_info->mimetypes =  gavl_strrep(new_info->mimetypes,
                                         input->get_mimetypes(plugin_priv));

    if(input->get_extensions)
      new_info->extensions = gavl_strrep(new_info->extensions,
                                       input->get_extensions(plugin_priv));
    

    if(input->get_protocols)
      new_info->protocols = gavl_strrep(new_info->protocols,
                                      input->get_protocols(plugin_priv));
    }
  if(plugin->type & BG_PLUGIN_IMAGE_READER)
    {
    bg_image_reader_plugin_t  * ir;
    ir = (bg_image_reader_plugin_t*)plugin;
    new_info->extensions = gavl_strrep(new_info->extensions,
                                       ir->extensions);
    new_info->mimetypes = gavl_strrep(new_info->mimetypes,
                                       ir->mimetypes);
    }
  if(plugin->type & BG_PLUGIN_IMAGE_WRITER)
    {
    bg_image_writer_plugin_t  * iw;
    iw = (bg_image_writer_plugin_t*)plugin;
    new_info->extensions = gavl_strrep(new_info->extensions,
                                     iw->extensions);
    new_info->mimetypes = gavl_strrep(new_info->mimetypes,
                                      iw->mimetypes);
    }
  if(plugin->type & BG_PLUGIN_CODEC)
    {
    bg_codec_plugin_t  * p;
    int num = 0;
    const gavl_codec_id_t * compressions;
    p = (bg_codec_plugin_t*)plugin;

    compressions = p->get_compressions(plugin_priv);
    
    while(compressions[num])
      num++;
    new_info->compressions = calloc(num+1, sizeof(*new_info->compressions));
    memcpy(new_info->compressions, compressions,
           num * sizeof(*new_info->compressions));
    }
  
  if(plugin->find_devices)
    new_info->devices = plugin->find_devices();

  return new_info;
  }

static bg_plugin_info_t * get_info(void * test_module,
                                   const char * filename,
                                   const bg_plugin_registry_options_t * opt)
  {
  bg_plugin_info_t * new_info;
  bg_plugin_common_t * plugin;
  void * plugin_priv;
  
  if(!check_plugin_version(test_module))
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN,
           "Plugin %s has no or wrong version. Recompiling the plugin should fix this.",
           filename);
    return NULL;
    }
  plugin = (bg_plugin_common_t*)(dlsym(test_module, "the_plugin"));
  if(!plugin)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "No symbol the_plugin in %s", filename);
    return NULL;
    }
  if(!plugin->priority)
    bg_log(BG_LOG_WARNING, LOG_DOMAIN, "Plugin %s has zero priority",
           plugin->name);

  if(opt->blacklist)
    {
    int i = 0;
    while(opt->blacklist[i])
      {
      if(!strcmp(plugin->name, opt->blacklist[i]))
        {
        bg_log(BG_LOG_INFO, LOG_DOMAIN,
               "Not loading %s (blacklisted)", plugin->name);
        return NULL;
        }
      i++;
      }
    }
  
  /* Get parameters */

  plugin_priv = plugin->create();
  new_info = plugin_info_create(plugin, plugin_priv, filename);
  plugin->destroy(plugin_priv);
  
  return new_info;
  }


static bg_plugin_info_t *
scan_directory_internal(const char * directory, bg_plugin_info_t ** _file_info,
                        int * changed,
                        bg_cfg_section_t * cfg_section, bg_plugin_api_t api,
                        const bg_plugin_registry_options_t * opt)
  {
  bg_plugin_info_t * ret;
  DIR * dir;
  struct dirent * entry;
  char filename[FILENAME_MAX];
  struct stat st;
  char * pos;
  void * test_module;
  
  bg_plugin_info_t * file_info;
  bg_plugin_info_t * new_info;
  bg_plugin_info_t * tmp_info;
  
  bg_cfg_section_t * plugin_section;
  bg_cfg_section_t * stream_section;
  if(_file_info)
    file_info = *_file_info;
  else
    file_info = NULL;
  
  ret = NULL;
    
  dir = opendir(directory);
  
  if(!dir)
    return NULL;

  while((entry = readdir(dir)))
    {
    /* Check for the filename */
    
    pos = strrchr(entry->d_name, '.');
    if(!pos)
      continue;
    
    if(strcmp(pos, ".so"))
      continue;
    
    sprintf(filename, "%s/%s", directory, entry->d_name);
    if(stat(filename, &st))
      continue;
    
    /* Check if the plugin is already in the registry */

    new_info = find_by_dll(file_info, filename);
    if(new_info)
      {
      if((st.st_mtime == new_info->module_time) &&
         (bg_cfg_section_has_subsection(cfg_section,
                                        new_info->name)))
        {
        file_info = remove_from_list(file_info, new_info);
        
        ret = append_to_list(ret, new_info);
        
        /* Remove other plugins as well */
        while((new_info = find_by_dll(file_info, filename)))
          {
          file_info = remove_from_list(file_info, new_info);
          ret = append_to_list(ret, new_info);
          }
        
        continue;
        }
      }
    
    if(!(*changed))
      {
      // fprintf(stderr, "Registry changed %s\n", filename);
      *changed = 1;
      closedir(dir);
      if(_file_info)
        *_file_info = file_info;
      return ret;
      }
    
    /* Open the DLL and see what's inside */
    
    test_module = dlopen(filename, RTLD_NOW);
    if(!test_module)
      {
      bg_log(BG_LOG_ERROR, LOG_DOMAIN, "dlopen failed for %s: %s",
             filename, dlerror());
      continue;
      }

    switch(api)
      {
      case BG_PLUGIN_API_GMERLIN:
        new_info = get_info(test_module, filename, opt);
        break;
      case BG_PLUGIN_API_LADSPA:
        new_info = bg_ladspa_get_info(test_module, filename);
        break;
      case BG_PLUGIN_API_FREI0R:
        new_info = bg_frei0r_get_info(test_module, filename);
        break;
#ifdef HAVE_LV
      case BG_PLUGIN_API_LV:
        new_info = bg_lv_get_info(filename);
        break;
#endif
      }

    tmp_info = new_info;
    while(tmp_info)
      {
      tmp_info->module_time = st.st_mtime;
      
      /* Create parameter entries in the registry */
      
      plugin_section =
        bg_cfg_section_find_subsection(cfg_section, tmp_info->name);
    
      if(tmp_info->parameters)
        {
        bg_cfg_section_create_items(plugin_section,
                                    tmp_info->parameters);
        }
      if(tmp_info->audio_parameters)
        {
        stream_section = bg_cfg_section_find_subsection(plugin_section,
                                                        "$audio");
        
        bg_cfg_section_create_items(stream_section,
                                    tmp_info->audio_parameters);
        }
      if(tmp_info->video_parameters)
        {
        stream_section = bg_cfg_section_find_subsection(plugin_section,
                                                        "$video");
        bg_cfg_section_create_items(stream_section,
                                    tmp_info->video_parameters);
        }
      if(tmp_info->text_parameters)
        {
        stream_section = bg_cfg_section_find_subsection(plugin_section,
                                                        "$text");
        bg_cfg_section_create_items(stream_section,
                                    tmp_info->text_parameters);
        }
      if(tmp_info->overlay_parameters)
        {
        stream_section = bg_cfg_section_find_subsection(plugin_section,
                                                        "$overlay");
        bg_cfg_section_create_items(stream_section,
                                    tmp_info->overlay_parameters);
        }
      tmp_info = tmp_info->next;
      }

    dlclose(test_module);
    ret = append_to_list(ret, new_info);
    }
  
  closedir(dir);
  if(_file_info)
    *_file_info = file_info;
  
  return ret;
  }

static bg_plugin_info_t *
scan_directory(const char * directory, bg_plugin_info_t ** _file_info,
               bg_cfg_section_t * cfg_section, bg_plugin_api_t api,
               const bg_plugin_registry_options_t * opt, int * reg_changed)
  {
  int changed = 0;
  bg_plugin_info_t * file_info;
  bg_plugin_info_t * file_info_next;
  char * tmp_string, *pos;
  bg_plugin_info_t * ret;
  
  ret = scan_directory_internal(directory, _file_info,
                                &changed, cfg_section, api, opt);
  
  /* Check if there are entries from the file info left */
  
  file_info = *_file_info;
  
  while(file_info)
    {
    tmp_string = gavl_strdup(file_info->module_filename);
    pos = strrchr(tmp_string, '/');
    if(pos) *pos = '\0';
    
    if(!strcmp(tmp_string, directory))
      {
      file_info_next = file_info->next;
      *_file_info = remove_from_list(*_file_info, file_info);
      bg_plugin_info_destroy(file_info);
      file_info = file_info_next;
      changed = 1;
      }
    else
      file_info = file_info->next;
    free(tmp_string);
    }
  
  if(!changed)
    return ret;
  
  *reg_changed = 1;
  
  free_info_list(ret);
  ret = scan_directory_internal(directory, _file_info,
                                &changed, cfg_section, api, opt);
  return ret;
  }

static bg_plugin_info_t * scan_multi(const char * path,
                                     bg_plugin_info_t ** _file_info,
                                     bg_cfg_section_t * section,
                                     bg_plugin_api_t api, const bg_plugin_registry_options_t * opt,
                                     int * reg_changed)
  {
  char ** paths;
  char ** real_paths;
  int num;
  
  bg_plugin_info_t * ret = NULL;
  bg_plugin_info_t * tmp_info;
  int do_scan;
  int i, j;
  paths = bg_strbreak(path, ':');
  if(!paths)
    return ret;

  num = 0;
  i = 0;
  while(paths[i++])
    num++;
  
  real_paths = calloc(num, sizeof(*real_paths));

  for(i = 0; i < num; i++)
    real_paths[i] = bg_canonical_filename(paths[i]);
  
  for(i = 0; i < num; i++)
    {
    if(!real_paths[i])
      continue;

    do_scan = 1;
    
    for(j = 0; j < i; j++)
      {
      if(real_paths[j] && !strcmp(real_paths[j], real_paths[i]))
        {
        do_scan = 0; /* Path already scanned */
        break;
        }
      }
    
    if(do_scan)
      {
      tmp_info = scan_directory(real_paths[i],
                                _file_info, 
                                section, api, opt, reg_changed);
      if(tmp_info)
        ret = append_to_list(ret, tmp_info);
      }
    }
  bg_strbreak_free(paths);

  for(i = 0; i < num; i++)
    {
    if(real_paths[i])
      free(real_paths[i]);
    }
  free(real_paths);
  
  return ret;
  }

bg_plugin_registry_t *
bg_plugin_registry_create(bg_cfg_section_t * section)
  {
  bg_plugin_registry_options_t opt;
  memset(&opt, 0, sizeof(opt));
  return bg_plugin_registry_create_with_options(section, &opt);
  }

bg_plugin_registry_t *
  bg_plugin_registry_create_with_options(bg_cfg_section_t * section,
                                         const bg_plugin_registry_options_t * opt)
  {
  int i;
  bg_plugin_registry_t * ret;
  bg_plugin_info_t * file_info;
  bg_plugin_info_t * tmp_info;
  bg_plugin_info_t * tmp_info_next;
  char * filename;
  char * env;

  char * path;
    
  ret = calloc(1, sizeof(*ret));
  ret->config_section = section;

  /* Load registry file */

  file_info = NULL; 
  
  filename = bg_search_file_read("", "plugins.xml");
  if(filename)
    {
    file_info = bg_plugin_registry_load(filename);
    free(filename);
    }
  else
    ret->changed = 1;
  
  /* Native plugins */
  env = getenv("GMERLIN_PLUGIN_PATH");
  if(env)
    path = bg_sprintf("%s:%s", env, PLUGIN_DIR);
  else
    path = bg_sprintf("%s", PLUGIN_DIR);
  
  tmp_info = scan_multi(path, &file_info, section, BG_PLUGIN_API_GMERLIN, opt, &ret->changed);
  if(tmp_info)
    ret->entries = append_to_list(ret->entries, tmp_info);
  free(path);
  /* Ladspa plugins */
  
  env = getenv("LADSPA_PATH");
  if(env)
    path = bg_sprintf("%s:/usr/lib64/ladspa:/usr/local/lib64/ladspa:/usr/lib/ladspa:/usr/local/lib/ladspa", env);
  else
    path = bg_sprintf("/usr/lib64/ladspa:/usr/local/lib64/ladspa:/usr/lib/ladspa:/usr/local/lib/ladspa");

  tmp_info = scan_multi(path, &file_info, section, BG_PLUGIN_API_LADSPA, opt, &ret->changed);
  if(tmp_info)
    ret->entries = append_to_list(ret->entries, tmp_info);
  
  free(path);
  
  /* Frei0r */
  tmp_info = scan_multi("/usr/lib64/frei0r-1:/usr/local/lib64/frei0r-1:/usr/lib/frei0r-1:/usr/local/lib/frei0r-1", &file_info, 
                        section, BG_PLUGIN_API_FREI0R, opt, &ret->changed);
  if(tmp_info)
    ret->entries = append_to_list(ret->entries, tmp_info);
    
#ifdef HAVE_LV
  tmp_info = scan_directory(LV_PLUGIN_DIR,
                            &file_info, 
                            section, BG_PLUGIN_API_LV, opt, &ret->changed);
  if(tmp_info)
    ret->entries = append_to_list(ret->entries, tmp_info);
#endif
  
  /* Now we have all external plugins, time to create the meta plugins */

  i = 0;
  while(meta_plugins[i].name)
    {
    tmp_info = meta_plugins[i].get_info(ret);
    if(tmp_info)
      ret->entries = append_to_list(ret->entries, tmp_info);
    i++;
    }

  tmp_info = bg_edldec_get_info();
  if(tmp_info)
    ret->entries = append_to_list(ret->entries, tmp_info);
  
  
  if(ret->entries)
    {
    /* Sort */
    ret->entries = sort_by_priority(ret->entries);

    if(ret->changed && !opt->dont_save)
      bg_plugin_registry_save(ret->entries);
  
    /* Remove duplicate external plugins */
    ret->entries = remove_duplicate(ret->entries);
    }

  /* Kick out unsupported plugins */
  tmp_info = ret->entries;

  while(tmp_info)
    {
    if(tmp_info->flags & BG_PLUGIN_UNSUPPORTED)
      {
      tmp_info_next = tmp_info->next;
      ret->entries = remove_from_list(ret->entries, tmp_info);
      bg_plugin_info_destroy(tmp_info);
      tmp_info = tmp_info_next;
      }
    else
      tmp_info = tmp_info->next;
    }
#if 0 /* Shouldn't be neccesary if the above code is bugfree */
  /* Kick out eventually remaining infos from the file */
  tmp_info = file_info;
  while(tmp_info)
    {
    tmp_info_next = tmp_info->next;
    bg_plugin_info_destroy(tmp_info);
    tmp_info = tmp_info_next;
    }
#endif
  return ret;
  }

int bg_plugin_registry_changed(bg_plugin_registry_t * reg)
  {
  return reg->changed;
  }

void bg_plugin_registry_destroy(bg_plugin_registry_t * reg)
  {
  bg_plugin_info_t * info;

  info = reg->entries;

  while(info)
    {
    reg->entries = info->next;
    bg_plugin_info_destroy(info);
    info = reg->entries;
    }
  free(reg);
  }

static bg_plugin_info_t * find_by_index(bg_plugin_info_t * info,
                                        int index, uint32_t type_mask,
                                        uint32_t flag_mask)
  {
  int i;
  bg_plugin_info_t * test_info;

  i = 0;
  test_info = info;

  while(test_info)
    {
    if((test_info->type & type_mask) &&
       ((flag_mask == BG_PLUGIN_ALL) ||
        (!test_info->flags && !flag_mask) || (test_info->flags & flag_mask)))
      {
      if(i == index)
        return test_info;
      i++;
      }
    test_info = test_info->next;
    }
  return NULL;
  }

static bg_plugin_info_t * find_by_priority(bg_plugin_info_t * info,
                                           uint32_t type_mask,
                                           uint32_t flag_mask)
  {
  bg_plugin_info_t * test_info, *ret = NULL;
  int priority_max = BG_PLUGIN_PRIORITY_MIN - 1;
  
  test_info = info;

  while(test_info)
    {
    if((test_info->type & type_mask) &&
       ((flag_mask == BG_PLUGIN_ALL) ||
        (test_info->flags & flag_mask) ||
        (!test_info->flags && !flag_mask)))
      {
      if(priority_max < test_info->priority)
        {
        priority_max = test_info->priority;
        ret = test_info;
        }
      }
    test_info = test_info->next;
    }
  return ret;
  }

const bg_plugin_info_t *
bg_plugin_find_by_index(bg_plugin_registry_t * reg, int index,
                        uint32_t type_mask, uint32_t flag_mask)
  {
  return find_by_index(reg->entries, index,
                       type_mask, flag_mask);
  }

int bg_plugin_registry_get_num_plugins(bg_plugin_registry_t * reg,
                                       uint32_t type_mask, uint32_t flag_mask)
  {
  bg_plugin_info_t * info;
  int ret = 0;
  
  info = reg->entries;

  while(info)
    {
    if((info->type & type_mask) &&
       ((!info->flags && !flag_mask) || (info->flags & flag_mask)))
      ret++;

    info = info->next;
    }
  return ret;
  }

void bg_plugin_registry_scan_devices(bg_plugin_registry_t * plugin_reg,
                                     uint32_t type_mask, uint32_t flag_mask)
  {
  int i;
  bg_plugin_info_t * info;
  bg_plugin_common_t * plugin;
  void * priv;
  void * module;
  const bg_parameter_info_t * parameters;
  int num = bg_plugin_registry_get_num_plugins(plugin_reg, type_mask, flag_mask);
  
  for(i = 0; i < num; i++)
    {
    info = find_by_index(plugin_reg->entries, i, type_mask, flag_mask);
    
    if(!(info->flags & BG_PLUGIN_DEVPARAM))
      continue;
    module = dlopen(info->module_filename, RTLD_NOW);
    plugin = (bg_plugin_common_t*)(dlsym(module, "the_plugin"));
    if(!plugin)
      {
      dlclose(module);
      continue;
      }
    priv = plugin->create();
    parameters = plugin->get_parameters(priv);

    if(info->parameters)
      bg_parameter_info_destroy_array(info->parameters);
    info->parameters = bg_parameter_info_copy_array(parameters);
    
    dlclose(module);
    }
  
  }


void bg_plugin_registry_set_extensions(bg_plugin_registry_t * reg,
                                       const char * plugin_name,
                                       const char * extensions)
  {
  bg_plugin_info_t * info;
  info = find_by_name(reg->entries, plugin_name);
  if(!info)
    return;
  if(!(info->flags & BG_PLUGIN_FILE))
    return;
  info->extensions = gavl_strrep(info->extensions, extensions);
  
  bg_plugin_registry_save(reg->entries);
  
  }

void bg_plugin_registry_set_protocols(bg_plugin_registry_t * reg,
                                      const char * plugin_name,
                                      const char * protocols)
  {
  bg_plugin_info_t * info;
  info = find_by_name(reg->entries, plugin_name);
  if(!info)
    return;
  if(!(info->flags & BG_PLUGIN_URL))
    return;
  info->protocols = gavl_strrep(info->protocols, protocols);
  bg_plugin_registry_save(reg->entries);

  }

void bg_plugin_registry_set_priority(bg_plugin_registry_t * reg,
                                     const char * plugin_name,
                                     int priority)
  {
  bg_plugin_info_t * info;
  info = find_by_name(reg->entries, plugin_name);
  if(!info)
    return;
  info->priority = priority;
  reg->entries = sort_by_priority(reg->entries);
  bg_plugin_registry_save(reg->entries);
  }

bg_cfg_section_t *
bg_plugin_registry_get_section(bg_plugin_registry_t * reg,
                               const char * plugin_name)
  {
  return bg_cfg_section_find_subsection(reg->config_section, plugin_name);
  }

static const struct
  {
  bg_plugin_type_t type;
  uint32_t flag_mask;
  char * key;
  } default_keys[] =
  {
    { BG_PLUGIN_OUTPUT_AUDIO,                    BG_PLUGIN_PLAYBACK, "audio_output" },
    { BG_PLUGIN_OUTPUT_VIDEO,                    BG_PLUGIN_PLAYBACK, "video_output" },
    { BG_PLUGIN_RECORDER_AUDIO,                  BG_PLUGIN_RECORDER, "audio_recorder" },
    { BG_PLUGIN_RECORDER_VIDEO,                  BG_PLUGIN_RECORDER, "video_recorder" },
    { BG_PLUGIN_ENCODER_AUDIO,                   BG_PLUGIN_FILE,     "audio_encoder"  },
    { BG_PLUGIN_ENCODER_VIDEO|BG_PLUGIN_ENCODER, BG_PLUGIN_FILE,     "video_encoder" },
    { BG_PLUGIN_ENCODER_TEXT,           BG_PLUGIN_FILE,     "text_encoder" },
    { BG_PLUGIN_ENCODER_OVERLAY,        BG_PLUGIN_FILE,     "overlay_encoder" },
    { BG_PLUGIN_IMAGE_WRITER,                    BG_PLUGIN_FILE,     "image_writer"   },
    { BG_PLUGIN_ENCODER_PP,                      BG_PLUGIN_PP,       "encoder_pp"  },
    { BG_PLUGIN_VISUALIZATION,                   BG_PLUGIN_VISUALIZE_FRAME, "visualization_frame" },
    { BG_PLUGIN_VISUALIZATION,                   BG_PLUGIN_VISUALIZE_GL, "visualization_gl" },
    { BG_PLUGIN_VISUALIZATION,                   BG_PLUGIN_VISUALIZE_FRAME | BG_PLUGIN_VISUALIZE_GL,
      "visualization" },
    { BG_PLUGIN_NONE,                            0, (char*)NULL              },
  };

static const char * get_default_key(bg_plugin_type_t type, uint32_t flag_mask)
  {
  int i = 0;

  /* Try exact match */
  while(default_keys[i].key)
    {
    if((type & default_keys[i].type) && (flag_mask == default_keys[i].flag_mask))
      return default_keys[i].key;
    i++;
    }

  /* Try approximate match */
  i = 0;
  while(default_keys[i].key)
    {
    if((type & default_keys[i].type) && (flag_mask & default_keys[i].flag_mask))
      return default_keys[i].key;
    i++;
    }
  
  return NULL;
  }

void bg_plugin_registry_set_default(bg_plugin_registry_t * r,
                                    bg_plugin_type_t type,
                                    uint32_t flag_mask,
                                    const char * name)
  {
  const char * key;

  key = get_default_key(type, flag_mask);
  if(key)
    bg_cfg_section_set_parameter_string(r->config_section, key, name);
  }

const bg_plugin_info_t *
bg_plugin_registry_get_default(bg_plugin_registry_t * r,
                               bg_plugin_type_t type, uint32_t flag_mask)
  {
  const char * key;
  const char * name = NULL;
  const bg_plugin_info_t * ret;
  
  key = get_default_key(type, flag_mask);
  if(key)  
    bg_cfg_section_get_parameter_string(r->config_section, key, &name);

  if(!name)
    {
    return find_by_priority(r->entries,
                            type, flag_mask);
    }
  else
    {
    ret = bg_plugin_find_by_name(r, name);
    if(!ret)
      ret = find_by_priority(r->entries,
                            type, flag_mask);
    return ret;
    }
  }

void bg_plugin_ref(bg_plugin_handle_t * h)
  {
  bg_plugin_lock(h);
  h->refcount++;

  bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "bg_plugin_ref %s: %d",
         h->info->name, h->refcount);
  bg_plugin_unlock(h);
  
  }

static void unload_plugin(bg_plugin_handle_t * h)
  {
  bg_cfg_section_t * section;
 
  if(h->plugin->get_parameter && h->plugin_reg)
    {
    section = bg_plugin_registry_get_section(h->plugin_reg, h->info->name);
    bg_cfg_section_get(section,
                       h->plugin->get_parameters(h->priv),
                       h->plugin->get_parameter,
                       h->priv);
    }
  if(h->info)
    {
    switch(h->info->api)
      {
      case BG_PLUGIN_API_GMERLIN:
        if(h->priv && h->plugin->destroy)
          h->plugin->destroy(h->priv);
        break;
      case BG_PLUGIN_API_LADSPA:
        bg_ladspa_unload(h);
        break;
      case BG_PLUGIN_API_FREI0R:
        bg_frei0r_unload(h);
        break;
#ifdef HAVE_LV
      case BG_PLUGIN_API_LV:
        bg_lv_unload(h);
        break;
#endif
      }
    }
  else if(h->priv && h->plugin->destroy)
    h->plugin->destroy(h->priv);
  
  if(h->location) free(h->location);

#if 1
  // Some few libs (e.g. the OpenGL lib shipped with NVidia)
  // seem to install pthread cleanup handlers, which point to library
  // functions. dlclosing libraries causes programs to crash
  // mysteriously when the thread lives longer than the plugin.
  //
  // So we leave them open and
  // rely on dlopen() never loading the same lib twice
  if(h->dll_handle)
    dlclose(h->dll_handle);
#endif
  if(h->edl)
    gavl_edl_destroy(h->edl);
  pthread_mutex_destroy(&h->mutex);
  free(h);
  }

void bg_plugin_unref_nolock(bg_plugin_handle_t * h)
  {
  h->refcount--;
  bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "bg_plugin_unref_nolock %s: %d",
         h->plugin->name, h->refcount);
  if(!h->refcount)
    unload_plugin(h);
  }

void bg_plugin_unref(bg_plugin_handle_t * h)
  {
  int refcount;
  bg_plugin_lock(h);
  h->refcount--;
  bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "bg_plugin_unref %s: %d",
         h->plugin->name, h->refcount);

  refcount = h->refcount;
  bg_plugin_unlock(h);
  if(!refcount)
    unload_plugin(h);
  }

gavl_video_frame_t *
bg_plugin_registry_load_image(bg_plugin_registry_t * r,
                              const char * filename,
                              gavl_video_format_t * format,
                              gavl_metadata_t * m)
  {
  const bg_plugin_info_t * info;
  
  bg_image_reader_plugin_t * ir;
  bg_plugin_handle_t * handle = NULL;
  gavl_video_frame_t * ret = NULL;
  
  info = bg_plugin_find_by_filename(r, filename, BG_PLUGIN_IMAGE_READER);

  if(!info)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "No plugin found for image %s", filename);
    goto fail;
    }
  
  handle = bg_plugin_load(r, info);
  if(!handle)
    goto fail;
  
  ir = (bg_image_reader_plugin_t*)(handle->plugin);

  if(!ir->read_header(handle->priv, filename, format))
    goto fail;

  if(ir->get_metadata && m)
    {
    const gavl_metadata_t * m_ret;
    m_ret = ir->get_metadata(handle->priv);
    if(m_ret)
      gavl_metadata_copy(m, m_ret);
    }
  
  ret = gavl_video_frame_create(format);
  if(!ir->read_image(handle->priv, ret))
    goto fail;
  
  bg_plugin_unref(handle);
  return ret;

  fail:
  if(ret)
    gavl_video_frame_destroy(ret);
  return NULL;
  }

void
bg_plugin_registry_save_image(bg_plugin_registry_t * r,
                              const char * filename,
                              gavl_video_frame_t * frame,
                              const gavl_video_format_t * format,
                              const gavl_metadata_t * m)
  {
  const bg_plugin_info_t * info;
  gavl_video_format_t tmp_format;
  gavl_video_converter_t * cnv;
  bg_image_writer_plugin_t * iw;
  bg_plugin_handle_t * handle = NULL;
  gavl_video_frame_t * tmp_frame = NULL;
  
  info = bg_plugin_find_by_filename(r, filename, BG_PLUGIN_IMAGE_WRITER);

  cnv = gavl_video_converter_create();
  
  if(!info)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "No plugin found for image %s", filename);
    goto fail;
    }
  
  handle = bg_plugin_load(r, info);
  if(!handle)
    goto fail;
  
  iw = (bg_image_writer_plugin_t*)(handle->plugin);

  gavl_video_format_copy(&tmp_format, format);
  
  if(!iw->write_header(handle->priv, filename, &tmp_format, m))
    goto fail;

  if(gavl_video_converter_init(cnv, format, &tmp_format))
    {
    tmp_frame = gavl_video_frame_create(&tmp_format);
    gavl_video_convert(cnv, frame, tmp_frame);
    if(!iw->write_image(handle->priv, tmp_frame))
      goto fail;
    }
  else
    {
    if(!iw->write_image(handle->priv, frame))
      goto fail;
    }
  bg_plugin_unref(handle);
  fail:
  if(tmp_frame)
    gavl_video_frame_destroy(tmp_frame);
  gavl_video_converter_destroy(cnv);
  }

bg_plugin_handle_t * bg_plugin_handle_create()
  {
  bg_plugin_handle_t * ret;
  ret = calloc(1, sizeof(*ret));
  pthread_mutex_init(&ret->mutex, NULL);
  return ret;
  }

static bg_plugin_handle_t * load_plugin(bg_plugin_registry_t * reg,
                                        const bg_plugin_info_t * info)
  {
  bg_plugin_handle_t * ret;

  if(!info)
    return NULL;
  
  ret = bg_plugin_handle_create();
  ret->plugin_reg = reg;
  
  pthread_mutex_init(&ret->mutex, NULL);

  if(info->module_filename)
    {
    if(info->api != BG_PLUGIN_API_LV)
      {
      /* We need all symbols global because some plugins might reference them */
      ret->dll_handle = dlopen(info->module_filename, RTLD_NOW | RTLD_GLOBAL);
      if(!ret->dll_handle)
        {
        bg_log(BG_LOG_ERROR, LOG_DOMAIN, "dlopen failed for %s: %s", info->module_filename,
               dlerror());
        goto fail;
        }
      }
    
    switch(info->api)
      {
      case BG_PLUGIN_API_GMERLIN:
        if(!check_plugin_version(ret->dll_handle))
          {
          bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Plugin %s has no or wrong version",
                 info->module_filename);
          goto fail;
          }
        ret->plugin = dlsym(ret->dll_handle, "the_plugin");
        if(!ret->plugin)
          {
          bg_log(BG_LOG_ERROR, LOG_DOMAIN, "dlsym failed for %s: %s",
                 info->module_filename, dlerror());
          goto fail;
          }
        ret->priv = ret->plugin->create();
        break;
      case BG_PLUGIN_API_LADSPA:
        if(!bg_ladspa_load(ret, info))
          goto fail;
        break;
      case BG_PLUGIN_API_FREI0R:
        if(!bg_frei0r_load(ret, info))
          goto fail;
        break;
#ifdef HAVE_LV
      case BG_PLUGIN_API_LV:
        if(!bg_lv_load(ret, info->name, info->flags, NULL))
          goto fail;
        break;
#endif
      }
    }
  else
    {
    int i = 0;
    while(meta_plugins[i].name)
      {
      if(!strcmp(meta_plugins[i].name, info->name))
        {
        ret->plugin = meta_plugins[i].get_plugin();
        ret->priv   = meta_plugins[i].create(reg);
        break;
        }
      i++;
      }
    }
  
  ret->info = info;
  bg_plugin_ref(ret);
  return ret;

fail:
  pthread_mutex_destroy(&ret->mutex);
  if(ret->dll_handle)
    dlclose(ret->dll_handle);
  free(ret);
  return NULL;
  }

static void apply_parameters(bg_plugin_registry_t * reg,
                             bg_plugin_handle_t * ret)
  {
  const bg_parameter_info_t * parameters;
  bg_cfg_section_t * section;
  
  /* Apply saved parameters */

  if(ret->plugin->get_parameters)
    {
    parameters = ret->plugin->get_parameters(ret->priv);
    
    section = bg_plugin_registry_get_section(reg, ret->info->name);
    
    bg_cfg_section_apply(section, parameters, ret->plugin->set_parameter,
                         ret->priv);
    }
  
  }

bg_plugin_handle_t * bg_plugin_load(bg_plugin_registry_t * reg,
                                    const bg_plugin_info_t * info)
  {
  bg_plugin_handle_t * ret;
  ret = load_plugin(reg, info);
  if(ret)
    apply_parameters(reg, ret);
  return ret;
  }

bg_plugin_handle_t * bg_ov_plugin_load(bg_plugin_registry_t * reg,
                                       const bg_plugin_info_t * info,
                                       const char * window_id)
  {
  bg_plugin_handle_t * ret;
  bg_ov_plugin_t * plugin;
  
  if(info->type != BG_PLUGIN_OUTPUT_VIDEO)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Invalid plugin type for video output");
    return NULL;
    }
#if 0
  if(!(info->flags & BG_PLUGIN_EMBED_WINDOW) && window_id)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN,
           "Plugin %s doesn't support embedded windows", info->name);
    return NULL;
    }
#endif
  ret = load_plugin(reg, info);
  
  if(window_id)
    {
    plugin = (bg_ov_plugin_t*)(ret->plugin);
    if(plugin->set_window)
      plugin->set_window(ret->priv, window_id);
    }
  
  if(ret)
    apply_parameters(reg, ret);
  return ret;
  }

void bg_plugin_lock(void * p)
  {
  bg_plugin_handle_t * h = p;
  pthread_mutex_lock(&h->mutex);
  }

void bg_plugin_unlock(void * p)
  {
  bg_plugin_handle_t * h = p;
  pthread_mutex_unlock(&h->mutex);
  }

void bg_plugin_registry_add_device(bg_plugin_registry_t * reg,
                                   const char * plugin_name,
                                   const char * device,
                                   const char * name)
  {
  bg_plugin_info_t * info;

  info = find_by_name(reg->entries, plugin_name);
  if(!info)
    return;

  info->devices = bg_device_info_append(info->devices,
                                        device, name);

  bg_plugin_registry_save(reg->entries);
  }

void bg_plugin_registry_set_device_name(bg_plugin_registry_t * reg,
                                        const char * plugin_name,
                                        const char * device,
                                        const char * name)
  {
  int i;
  bg_plugin_info_t * info;

  info = find_by_name(reg->entries, plugin_name);
  if(!info || !info->devices)
    return;
  
  i = 0;
  while(info->devices[i].device)
    {
    if(!strcmp(info->devices[i].device, device))
      {
      info->devices[i].name = gavl_strrep(info->devices[i].name, name);
      bg_plugin_registry_save(reg->entries);
      return;
      }
    i++;
    }
  
  }

static int my_strcmp(const char * str1, const char * str2)
  {
  if(!str1 && !str2)
    return 0;
  else if(str1 && str2)
    return strcmp(str1, str2); 
  return 1;
  }

void bg_plugin_registry_remove_device(bg_plugin_registry_t * reg,
                                      const char * plugin_name,
                                      const char * device,
                                      const char * name)
  {
  bg_plugin_info_t * info;
  int index;
  int num_devices;
  info = find_by_name(reg->entries, plugin_name);
  if(!info)
    return;
    
  index = -1;
  num_devices = 0;
  while(info->devices[num_devices].device)
    {
    if(!my_strcmp(info->devices[num_devices].name, name) &&
       !strcmp(info->devices[num_devices].device, device))
      {
      index = num_devices;
      }
    num_devices++;
    }


  if(index != -1)
    memmove(&info->devices[index], &info->devices[index+1],
            sizeof(*(info->devices)) * (num_devices - index));
    
  bg_plugin_registry_save(reg->entries);
  }

void bg_plugin_registry_find_devices(bg_plugin_registry_t * reg,
                                     const char * plugin_name)
  {
  bg_plugin_info_t * info;
  bg_plugin_handle_t * handle;
  
  info = find_by_name(reg->entries, plugin_name);
  if(!info)
    return;

  handle = bg_plugin_load(reg, info);
    
  bg_device_info_destroy(info->devices);
  info->devices = NULL;
  
  if(!handle || !handle->plugin->find_devices)
    return;

  info->devices = handle->plugin->find_devices();
  bg_plugin_registry_save(reg->entries);
  bg_plugin_unref(handle);
  }

char ** bg_plugin_registry_get_plugins(bg_plugin_registry_t*reg,
                                       uint32_t type_mask,
                                       uint32_t flag_mask)
  {
  int num_plugins, i;
  char ** ret;
  const bg_plugin_info_t * info;
  
  num_plugins = bg_plugin_registry_get_num_plugins(reg, type_mask, flag_mask);
  ret = calloc(num_plugins + 1, sizeof(char*));
  for(i = 0; i < num_plugins; i++)
    {
    info = bg_plugin_find_by_index(reg, i, type_mask, flag_mask);
    ret[i] = gavl_strdup(info->name);
    }
  return ret;
  
  }

void bg_plugin_registry_free_plugins(char ** plugins)
  {
  int index = 0;
  if(!plugins)
    return;
  while(plugins[index])
    {
    free(plugins[index]);
    index++;
    }
  free(plugins);
  
  }

static void load_input_plugin(bg_plugin_registry_t * reg,
                              const bg_plugin_info_t * info,
                              bg_plugin_handle_t ** ret)
  {
  if(!(*ret) || !(*ret)->info || strcmp((*ret)->info->name, info->name))
    {
    if(*ret)
      {
      bg_plugin_unref(*ret);
      *ret = NULL;
      }
    *ret = bg_plugin_load(reg, info);
    }
  }

static int input_plugin_load(bg_plugin_registry_t * reg,
                             const char * location,
                             const bg_plugin_info_t * info,
                             bg_plugin_handle_t ** ret,
                             bg_input_callbacks_t * callbacks)
  {
  const char * real_location;
  char * protocol = NULL, * path = NULL;
  
  int num_plugins, i;
  uint32_t flags;
  bg_input_plugin_t * plugin;
  int try_and_error = 1;
  const bg_plugin_info_t * first_plugin = NULL;
  
  if(!location)
    return 0;

  if(!strncmp(location, "file://", 7))
    location += 7;
  
  real_location = location;
  
  if(!info) /* No plugin given, seek one */
    {
    if(bg_string_is_url(location))
      {
      if(bg_url_split(location,
                      &protocol,
                      NULL, // user,
                      NULL, // password,
                      NULL, // hostname,
                      NULL,   //  port,
                      &path))
        {
        info = bg_plugin_find_by_protocol(reg, protocol);
        if(info)
          {
          if(info->flags & BG_PLUGIN_REMOVABLE)
            real_location = path;
          }
        }
      }
    else if(!strcmp(location, "-"))
      {
      info = bg_plugin_find_by_protocol(reg, "stdin");
      }
    else
      {
      info = bg_plugin_find_by_filename(reg, real_location,
                                        (BG_PLUGIN_INPUT));
      }
    first_plugin = info;
    }
  else
    try_and_error = 0; /* We never try other plugins than the given one */
  
  if(info)
    {
    /* Try to load this */

    load_input_plugin(reg, info, ret);

    if(!(*ret))
      {
      bg_log(BG_LOG_ERROR, LOG_DOMAIN, TRS("Loading plugin \"%s\" failed"),
                                           info->long_name);
      return 0;
      }
    
    plugin = (bg_input_plugin_t*)((*ret)->plugin);

    if(plugin->set_callbacks)
      plugin->set_callbacks((*ret)->priv, callbacks);
    
    if(!plugin->open((*ret)->priv, real_location))
      {
      bg_log(BG_LOG_ERROR, LOG_DOMAIN, TRS("Opening %s with \"%s\" failed"),
             real_location, info->long_name);
      }
    else
      {
      if(protocol) free(protocol);
      if(path)     free(path);
      (*ret)->location = gavl_strrep((*ret)->location, real_location);
      return 1;
      }
    }
  
  if(protocol) free(protocol);
  if(path)     free(path);
  
  if(!try_and_error)
    return 0;
  
  flags = bg_string_is_url(real_location) ? BG_PLUGIN_URL : BG_PLUGIN_FILE;
  
  num_plugins = bg_plugin_registry_get_num_plugins(reg,
                                                   BG_PLUGIN_INPUT, flags);
  for(i = 0; i < num_plugins; i++)
    {
    info = bg_plugin_find_by_index(reg, i, BG_PLUGIN_INPUT, flags);

    if(info == first_plugin)
      continue;
        
    load_input_plugin(reg, info, ret);

    if(!*ret)
      continue;
    
    plugin = (bg_input_plugin_t*)((*ret)->plugin);
    if(!plugin->open((*ret)->priv, real_location))
      {
      bg_log(BG_LOG_ERROR, LOG_DOMAIN, TRS("Opening %s with \"%s\" failed"),
             location, info->long_name);
      }
    else
      {
      (*ret)->location = gavl_strrep((*ret)->location, real_location);
      return 1;
      }
    }
  return 0;
  }

int bg_input_plugin_load(bg_plugin_registry_t * reg,
                         const char * location,
                         const bg_plugin_info_t * info,
                         bg_plugin_handle_t ** ret,
                         bg_input_callbacks_t * callbacks, int prefer_edl)
  {
  bg_input_plugin_t * plugin;
  const gavl_edl_t * edl_c;
  gavl_edl_t * edl;
  int num_tracks = 1;
  if(!input_plugin_load(reg, location, info, ret, callbacks))
    return 0;

  plugin = (bg_input_plugin_t*)((*ret)->plugin);

  if(plugin->get_num_tracks)
    num_tracks = plugin->get_num_tracks((*ret)->priv);
  
  if(num_tracks && (!plugin->get_edl || !prefer_edl))
    return 1;
  
  edl_c = plugin->get_edl((*ret)->priv);
  
  if(!edl_c)
    return 1;

  /* Debug */
  //  bg_edl_save(edl_c, "test.edl");
    
  /* Load EDL instead */
  edl = gavl_edl_copy(edl_c);

  info = bg_plugin_find_by_name(reg, "i_edldec");
  
  if(!bg_input_plugin_load_edl(reg, edl, info, ret,
                               callbacks))
    {
    gavl_edl_destroy(edl);
    return 0;
    }
  (*ret)->edl = edl;
  return 1;
  }

void
bg_plugin_registry_set_encode_audio_to_video(bg_plugin_registry_t * reg,
                                                  int audio_to_video)
  {
  bg_cfg_section_set_parameter_int(reg->config_section, "encode_audio_to_video", audio_to_video);
  }

int
bg_plugin_registry_get_encode_audio_to_video(bg_plugin_registry_t * reg)
  {
  int ret;
  bg_cfg_section_get_parameter_int(reg->config_section,
                                   "encode_audio_to_video",
                                   &ret);
  return ret;
  }

void bg_plugin_registry_set_visualize(bg_plugin_registry_t * reg,
                                       int visualize)
  {
  bg_cfg_section_set_parameter_int(reg->config_section, "visualize", visualize);
  }

int bg_plugin_registry_get_visualize(bg_plugin_registry_t * reg)
  {
  int ret;
  bg_cfg_section_get_parameter_int(reg->config_section,
                                   "visualize",
                                   &ret);
  return ret;
  }




void
bg_plugin_registry_set_encode_text_to_video(bg_plugin_registry_t * reg,
                                                  int text_to_video)
  {
  bg_cfg_section_set_parameter_int(reg->config_section, "encode_text_to_video",
                                   text_to_video);
  }

int
bg_plugin_registry_get_encode_text_to_video(bg_plugin_registry_t * reg)
  {
  int ret;
  bg_cfg_section_get_parameter_int(reg->config_section,
                                   "encode_text_to_video",
                                   &ret);
  return ret;
  }

void bg_plugin_registry_set_encode_overlay_to_video(bg_plugin_registry_t * reg,
                                                  int overlay_to_video)
  {
  bg_cfg_section_set_parameter_int(reg->config_section, "encode_overlay_to_video",
                                   overlay_to_video);
  }

int bg_plugin_registry_get_encode_overlay_to_video(bg_plugin_registry_t * reg)
  {
  int ret;
  bg_cfg_section_get_parameter_int(reg->config_section,
                                   "encode_overlay_to_video",
                                   &ret);
  return ret;
  }

void bg_plugin_registry_set_encode_pp(bg_plugin_registry_t * reg,
                                      int use_pp)
  {
  bg_cfg_section_set_parameter_int(reg->config_section, "encode_pp", use_pp);
  }

int bg_plugin_registry_get_encode_pp(bg_plugin_registry_t * reg)
  {
  int ret;
  bg_cfg_section_get_parameter_int(reg->config_section, "encode_pp",
                                   &ret);
  
  return ret;
  }

static const bg_parameter_info_t encoder_section_general[] =
  {
    {
      .name        = "$general",
      .long_name   = TRS("General"),
      .type        = BG_PARAMETER_SECTION,
      //      .flags       = BG_PARAMETER_OWN_SECTION,
    },
    { } // End
  };
    
static const bg_parameter_info_t encoder_section_audio[] =
  {
    {
      .name      = "$audio",
      .long_name = TRS("Audio"),
      .type      = BG_PARAMETER_SECTION,
      .flags     = BG_PARAMETER_OWN_SECTION,
    },
    { } // End
  };

static const bg_parameter_info_t encoder_section_video[] =
  {
    {
      .name      = "$video",
      .long_name = TRS("Video"),
      .type      = BG_PARAMETER_SECTION,
      .flags     = BG_PARAMETER_OWN_SECTION,
    },
    { } // End
  };

static const bg_parameter_info_t encoder_section_overlay[] =
  {
    {
      .name      = "$overlay",
      .long_name = TRS("Overlay subtitles"),
      .type      = BG_PARAMETER_SECTION,
      .flags     = BG_PARAMETER_OWN_SECTION,
    },
    { } // End
  };

static const bg_parameter_info_t encoder_section_text[] =
  {
    {
      .name      = "$text",
      .long_name = TRS("Text subtitles"),
      .type      = BG_PARAMETER_SECTION,
      .flags     = BG_PARAMETER_OWN_SECTION,
    },
    { } // End
  };

static bg_parameter_info_t *
create_encoder_parameters(const bg_plugin_info_t * info, int stream_params)
  {
  bg_parameter_info_t * ret = NULL;
  
  //  if(!strcmp(info->name, "e_mpeg"))
  //    fprintf(stderr, "e_mpeg\n");
  
  if(stream_params &&
     (info->audio_parameters ||
      info->video_parameters ||
      info->text_parameters ||
      info->overlay_parameters))
    {
    int i = 0;
    const bg_parameter_info_t * src[11];
    if(info->parameters)
      {
      if(info->parameters[0].type != BG_PARAMETER_SECTION)
        {
        src[i] = encoder_section_general;
        i++;
        }
      src[i] = info->parameters;
      i++;
      }

    if(stream_params)
      {
      if(info->audio_parameters)
        {
        src[i] = encoder_section_audio;
        i++;
        src[i] = info->audio_parameters;
        i++;
        }

      if(info->text_parameters)
        {
        src[i] = encoder_section_text;
        i++;
        src[i] = info->text_parameters;
        i++;
        }

      if(info->overlay_parameters)
        {
        src[i] = encoder_section_overlay;
        i++;
        src[i] = info->overlay_parameters;
        i++;
        }

      if(info->video_parameters)
        {
        src[i] = encoder_section_video;
        i++;
        src[i] = info->video_parameters;
        i++;
        }

      }
    src[i] = NULL;
    ret = bg_parameter_info_concat_arrays(src);
    }
  else if(info->parameters)
    ret = bg_parameter_info_copy_array(info->parameters);

  if(ret)
    {
    char * tmp_string;
    ret->flags |= BG_PARAMETER_GLOBAL_PRESET;
    tmp_string = bg_sprintf("plugins/%s", info->name);
    ret->preset_path = gavl_strrep(ret->preset_path, tmp_string);
    free(tmp_string);
    }

  //  if(!strcmp(info->name, "e_mpeg"))
  //    bg_parameters_dump(ret, "encoder_parameters");
    
  return ret;
  }

static void set_parameter_info(bg_plugin_registry_t * reg,
                              uint32_t type_mask,
                              uint32_t flag_mask,
                              bg_parameter_info_t * ret, int stream_params)
  {
  int num_plugins, start_entries, i;
  const bg_plugin_info_t * info;
  
  num_plugins =
    bg_plugin_registry_get_num_plugins(reg, type_mask, flag_mask);

  start_entries = 0;
  if(ret->multi_names_nc)
    {
    while(ret->multi_names_nc[start_entries])
      start_entries++;
    }

#define REALLOC(arr) \
  ret->arr = realloc(ret->arr, (start_entries + num_plugins + 1)*sizeof(*ret->arr)); \
  memset(ret->arr + start_entries, 0, (num_plugins + 1)*sizeof(*ret->arr));

  REALLOC(multi_names_nc);
  REALLOC(multi_labels_nc);
  REALLOC(multi_parameters_nc);
  REALLOC(multi_descriptions_nc);
#undef REALLOC
    
  bg_parameter_info_set_const_ptrs(ret);
  
  for(i = 0; i < num_plugins; i++)
    {
    info = bg_plugin_find_by_index(reg, i,
                                   type_mask, flag_mask);
    ret->multi_names_nc[start_entries+i] = gavl_strdup(info->name);

    /* First plugin is the default one */
    if((ret->type != BG_PARAMETER_MULTI_CHAIN) && !ret->val_default.val_str)
      {
      ret->val_default.val_str = gavl_strdup(info->name);
      }
    
    bg_bindtextdomain(info->gettext_domain, info->gettext_directory);
    ret->multi_descriptions_nc[start_entries+i] = gavl_strdup(TRD(info->description,
                                                     info->gettext_domain));
    
    ret->multi_labels_nc[start_entries+i] =
      gavl_strdup(TRD(info->long_name,
                          info->gettext_domain));
    
    if(info->type & (BG_PLUGIN_ENCODER_AUDIO |
                     BG_PLUGIN_ENCODER_VIDEO |
                     BG_PLUGIN_ENCODER_TEXT |
                     BG_PLUGIN_ENCODER_OVERLAY |
                     BG_PLUGIN_ENCODER))
      ret->multi_parameters_nc[start_entries+i] =
        create_encoder_parameters(info, stream_params);
    else if(info->parameters)
      {
      ret->multi_parameters_nc[start_entries+i] =
        bg_parameter_info_copy_array(info->parameters);
      }
    }
  }
  
void bg_plugin_registry_set_parameter_info(bg_plugin_registry_t * reg,
                                           uint32_t type_mask,
                                           uint32_t flag_mask,
                                           bg_parameter_info_t * ret)
  {
  set_parameter_info(reg, type_mask, flag_mask, ret, 1);
  }

static const bg_parameter_info_t registry_settings_parameter =
  {
    .name = "$registry",
    .long_name = TRS("Registry settings"),
    .type = BG_PARAMETER_SECTION,
  };

static const bg_parameter_info_t plugin_settings_parameter =
  {
    .name = "$plugin",
    .long_name = TRS("Plugin settings"),
    .type = BG_PARAMETER_SECTION,
  };

static const bg_parameter_info_t extensions_parameter =
  {
    .name = "$extensions",
    .long_name = TRS("Extensions"),
    .type = BG_PARAMETER_STRING,
  };

static const bg_parameter_info_t protocols_parameter =
  {
    .name = "$protocols",
    .long_name = TRS("Protocols"),
    .type = BG_PARAMETER_STRING,
  };

static const bg_parameter_info_t priority_parameter =
  {
    .name = "$priority",
    .long_name = TRS("Priority"),
    .type = BG_PARAMETER_INT,
    .val_min = { .val_i = 1 },
    .val_max = { .val_i = 10 },
  };

void bg_plugin_registry_set_parameter_info_input(bg_plugin_registry_t * reg,
                                                 uint32_t type_mask,
                                                 uint32_t flag_mask,
                                                 bg_parameter_info_t * ret)
  {
  int num_plugins, i;
  const bg_plugin_info_t * info;
  int index, index1, num_parameters;
  char * prefix;
  
  num_plugins =
    bg_plugin_registry_get_num_plugins(reg, type_mask, flag_mask);

  ret->type = BG_PARAMETER_MULTI_LIST;
  ret->flags |= BG_PARAMETER_NO_SORT;
  
  ret->multi_names_nc      = calloc(num_plugins + 1, sizeof(*ret->multi_names));
  ret->multi_labels_nc     = calloc(num_plugins + 1, sizeof(*ret->multi_labels));
  ret->multi_parameters_nc = calloc(num_plugins + 1,
                                 sizeof(*ret->multi_parameters));

  ret->multi_descriptions_nc = calloc(num_plugins + 1,
                                   sizeof(*ret->multi_descriptions));

  bg_parameter_info_set_const_ptrs(ret);
  
  for(i = 0; i < num_plugins; i++)
    {
    info = bg_plugin_find_by_index(reg, i,
                                   type_mask, flag_mask);
    ret->multi_names_nc[i] = gavl_strdup(info->name);

    /* First plugin is the default one */
    if(!i && (ret->type != BG_PARAMETER_MULTI_CHAIN)) 
      {
      ret->val_default.val_str = gavl_strdup(info->name);
      }
    
    bg_bindtextdomain(info->gettext_domain, info->gettext_directory);
    ret->multi_descriptions_nc[i] = gavl_strdup(TRD(info->description,
                                                        info->gettext_domain));
    
    ret->multi_labels_nc[i] = gavl_strdup(TRD(info->long_name,
                                               info->gettext_domain));

    /* Create parameters: Extensions and protocols are added to the array
       if necessary */

    num_parameters = 1; /* Priority */
    if(info->flags & BG_PLUGIN_FILE)
      num_parameters++;
    if(info->flags & BG_PLUGIN_URL)
      num_parameters++;

    if(info->parameters && (info->parameters[0].type != BG_PARAMETER_SECTION))
      num_parameters++; /* Plugin section */

    if(info->parameters)
      num_parameters++; /* Registry */
    
    //    prefix = bg_sprintf("%s.", info->name);

    prefix = NULL;
    
    if(info->parameters)
      {
      index = 0;
      while(info->parameters[index].name)
        {
        index++;
        num_parameters++;
        }
      }
    
    ret->multi_parameters_nc[i] =
      calloc(num_parameters+1, sizeof(*ret->multi_parameters_nc[i]));

    index = 0;

    /* Now, build the parameter array */

    if(info->parameters && (info->parameters[0].type != BG_PARAMETER_SECTION))
      {
      bg_parameter_info_copy(&ret->multi_parameters_nc[i][index],
                             &plugin_settings_parameter);
      index++;
      }
    
    if(info->parameters)
      {
      index1 = 0;

      while(info->parameters[index1].name)
        {
        bg_parameter_info_copy(&ret->multi_parameters_nc[i][index],
                               &info->parameters[index1]);
        index++;
        index1++;
        }
      }

    if(info->parameters)
      {
      bg_parameter_info_copy(&ret->multi_parameters_nc[i][index],
                             &registry_settings_parameter);
      index++;
      }

    if(info->flags & BG_PLUGIN_FILE)
      {
      bg_parameter_info_copy(&ret->multi_parameters_nc[i][index],
                             &extensions_parameter);
      ret->multi_parameters_nc[i][index].val_default.val_str =
        gavl_strdup(info->extensions);
      index++;
      }
    if(info->flags & BG_PLUGIN_URL)
      {
      bg_parameter_info_copy(&ret->multi_parameters_nc[i][index],
                             &protocols_parameter);
      ret->multi_parameters_nc[i][index].val_default.val_str =
        gavl_strdup(info->protocols);
      index++;
      }

    bg_parameter_info_copy(&ret->multi_parameters_nc[i][index],
                           &priority_parameter);
    ret->multi_parameters_nc[i][index].val_default.val_i =
      info->priority;
    index++;
    }
  
  }

static int find_parameter_input(bg_plugin_registry_t * plugin_reg,
                                const char * name,
                                const bg_parameter_info_t ** parameter_info,
                                bg_plugin_info_t ** plugin_info,
                                bg_cfg_section_t ** section,
                                const char ** parameter_name)
  {
  const char * pos1;
  const char * pos2;
  char * plugin_name;
  int ret = 0;
  
  pos1 = strchr(name, '.');
  if(!pos1)
    return 0;
  pos1++;

  pos2 = strchr(pos1, '.');
  if(!pos2)
    return 0;

  plugin_name = gavl_strndup( pos1, pos2);
  pos2++;

  *parameter_name = pos2;
  
  *plugin_info = find_by_name(plugin_reg->entries, plugin_name);
  if(!(*plugin_info))
    goto fail;
  
  if(*pos2 != '$')
    {
    *section = bg_cfg_section_find_subsection(plugin_reg->config_section,
                                                  plugin_name);

    *parameter_info = bg_parameter_find((*plugin_info)->parameters, pos2);
    if(!(*parameter_info))
      goto fail;
    }
  
  ret = 1;
  fail:
  free(plugin_name);
  return ret;
  }

void bg_plugin_registry_set_parameter_input(void * data, const char * name,
                                            const bg_parameter_value_t * val)
  {
  bg_plugin_registry_t * plugin_reg = data;
  bg_cfg_section_t * cfg_section;
  const bg_parameter_info_t * parameter_info;
  bg_plugin_info_t * plugin_info;
  const char * parameter_name;
  
  if(!name)
    return;

  if(!find_parameter_input(plugin_reg, name, &parameter_info,
                           &plugin_info, &cfg_section, &parameter_name))
    return;

  //  fprintf(stderr,
  //          "bg_plugin_registry_set_parameter_input name: %s parameter_name: %s plugin_name: %s\n",
  //          name, parameter_name, plugin_info->name);
  
  
  if(!strcmp(parameter_name, "$priority"))
    {
    bg_plugin_registry_set_priority(plugin_reg, plugin_info->name, val->val_i);
    }
  else if(!strcmp(parameter_name, "$extensions"))
    bg_plugin_registry_set_extensions(plugin_reg, plugin_info->name, val->val_str);
  else if(!strcmp(parameter_name, "$protocols"))
    bg_plugin_registry_set_protocols(plugin_reg, plugin_info->name, val->val_str);
  else
    bg_cfg_section_set_parameter(cfg_section, parameter_info, val);
  }

int bg_plugin_registry_get_parameter_input(void * data, const char * name,
                                            bg_parameter_value_t * val)
  {
  bg_plugin_registry_t * plugin_reg = data;
  bg_cfg_section_t * cfg_section;
  const bg_parameter_info_t * parameter_info;
  bg_plugin_info_t * plugin_info;
  const char * parameter_name;
  
  if(!name)
    return 0;

  if(!find_parameter_input(plugin_reg, name, &parameter_info,
                           &plugin_info, &cfg_section, &parameter_name))
    return 0;

  //  fprintf(stderr,
  //          "bg_plugin_registry_get_parameter_input name: %s parameter_name: %s plugin_name: %s\n",
  //          name, parameter_name, plugin_info->name);
  
  //  fprintf(stderr, "plugin name: %s\n", plugin_info->name);
  
  if(!strcmp(parameter_name, "$priority"))
    {
    val->val_i = plugin_info->priority;
    //    fprintf(stderr, "get priority: %d\n", val->val_i);
    }
  else if(!strcmp(parameter_name, "$extensions"))
    val->val_str = gavl_strrep(val->val_str, plugin_info->extensions);
  else if(!strcmp(parameter_name, "$protocols"))
    val->val_str = gavl_strrep(val->val_str, plugin_info->protocols);
  else
    bg_cfg_section_get_parameter(cfg_section, parameter_info, val);
  return 1;
  }

static const bg_parameter_info_t audio_to_video_param =
  {
    .name      = "encode_audio_to_video",
    .opt       = "a2v",
    .long_name = TRS("Write audio stream to video file if possible"),
    .type      = BG_PARAMETER_CHECKBUTTON,
  };

static const bg_parameter_info_t text_to_video_param =
  {
    .name      = "encode_text_to_video",
    .opt       = "t2v",
    .long_name = TRS("Write text subtitles to video file if possible"),
    .type      = BG_PARAMETER_CHECKBUTTON,
  };

static const bg_parameter_info_t overlay_to_video_param =
  {
    .name      = "encode_overlay_to_video",
    .opt       = "o2v",
    .long_name = TRS("Write overlay subtitles to video file if possible"),
    .type      = BG_PARAMETER_CHECKBUTTON,
  };

static const bg_parameter_info_t audio_encoder_param =
  {
    .name      = "audio_encoder",
    .opt       = "ae",
    .long_name = TRS("Audio"),
    .type      = BG_PARAMETER_MULTI_MENU,
  };

static const bg_parameter_info_t video_encoder_param =
  {
    .name      = "video_encoder",
    .opt       = "ve",
    .long_name = TRS("Video"),
    .type      = BG_PARAMETER_MULTI_MENU,
  };

static const bg_parameter_info_t text_encoder_param =
  {
    .name      = "text_encoder",
    .opt       = "te",
    .long_name = TRS("Text subtitles"),
    .type      = BG_PARAMETER_MULTI_MENU,
  };

static const bg_parameter_info_t overlay_encoder_param =
  {
    .name      = "overlay_encoder",
    .opt       = "oe",
    .long_name = TRS("Overlay subtitles"),
    .type      = BG_PARAMETER_MULTI_MENU,
  };

bg_parameter_info_t *
bg_plugin_registry_create_encoder_parameters(bg_plugin_registry_t * reg,
                                             uint32_t type_mask,
                                             uint32_t flag_mask, int stream_params)
  {
  int do_audio = 0;
  int do_video = 0;
  int do_text = 0;
  int do_overlay = 0;
  int i;
  
  bg_parameter_info_t * ret;
  
  /* Determine what stream we want */

  if(type_mask & BG_STREAM_AUDIO)
    do_audio = 1;
  if(type_mask & BG_STREAM_VIDEO)
    do_video = 1;
  if(type_mask & BG_STREAM_TEXT)
    do_text = 1;
  if(type_mask & BG_STREAM_OVERLAY)
    do_overlay = 1;
  
  /* Count parameters */
  i = 0;
  if(do_audio)
    i += 1 + do_video;
  if(do_text)
    i += 1 + do_video;
  if(do_overlay)
    i += 1 + do_video;
  if(do_video)
    i++;

  ret = calloc(i+1, sizeof(*ret));

  i = 0;

  if(do_audio)
    {
    if(do_video)
      {
      bg_parameter_info_copy(&ret[i], &audio_to_video_param);
      i++;
      }
    bg_parameter_info_copy(&ret[i], &audio_encoder_param);

    set_parameter_info(reg,
                       BG_PLUGIN_ENCODER_AUDIO,
                       flag_mask, &ret[i], stream_params);

    i++;
    }
  if(do_text)
    {
    if(do_video)
      {
      bg_parameter_info_copy(&ret[i], &text_to_video_param);
      i++;
      }
    bg_parameter_info_copy(&ret[i], &text_encoder_param);

    set_parameter_info(reg,
                       BG_PLUGIN_ENCODER_TEXT,
                       flag_mask, &ret[i], stream_params);
    
    i++;
    }
  if(do_overlay)
    {
    if(do_video)
      {
      bg_parameter_info_copy(&ret[i], &overlay_to_video_param);
      i++;
      }
    bg_parameter_info_copy(&ret[i], &overlay_encoder_param);
    set_parameter_info(reg,
                       BG_PLUGIN_ENCODER_OVERLAY,
                       flag_mask, &ret[i], stream_params);
    i++;
    }
  if(do_video)
    {
    bg_parameter_info_copy(&ret[i], &video_encoder_param);

    set_parameter_info(reg,
                       BG_PLUGIN_ENCODER_VIDEO | BG_PLUGIN_ENCODER,
                       flag_mask, &ret[i], stream_params);
    i++;
    }

  /* Set preset path */
  
  ret[0].preset_path = gavl_strdup("encoders");
  
  return ret;
  }

static int
encode_to_video(bg_plugin_registry_t * plugin_reg,
                bg_cfg_section_t * s,
                bg_stream_type_t type,
                int stream_mask)
  {
  int preference;
  const char * plugin_name = NULL;
  const bg_plugin_info_t * info;

  if(!(stream_mask & BG_STREAM_VIDEO))
    return 0;
  
  bg_cfg_section_get_parameter_string(s, "video_encoder",
                                      &plugin_name);
  info = bg_plugin_find_by_name(plugin_reg, plugin_name);

  if(!info)
    return 0;
  
  switch(type)
    {
    case BG_STREAM_AUDIO:
      bg_cfg_section_get_parameter_int(s, "encode_audio_to_video",
                                       &preference);
      if(preference && info->max_audio_streams)
        return 1;
      break;
    case BG_STREAM_TEXT:
      bg_cfg_section_get_parameter_int(s, "encode_text_to_video",
                                       &preference);
      if(preference && info->max_text_streams)
        return 1;
      break;
    case BG_STREAM_OVERLAY:
      bg_cfg_section_get_parameter_int(s, "encode_overlay_to_video",
                                       &preference);
      if(preference && info->max_overlay_streams)
        return 1;
      break;
    default:
      break;
    }
  return 0;
  }
                              

const char * 
bg_encoder_section_get_plugin(bg_plugin_registry_t * plugin_reg,
                              bg_cfg_section_t * s,
                              bg_stream_type_t stream_type,
                              int stream_mask)
  {
  const char * ret = NULL;
  
  switch(stream_type)
    {
    case BG_STREAM_AUDIO:
      if(!encode_to_video(plugin_reg, s, BG_STREAM_AUDIO, stream_mask))
        bg_cfg_section_get_parameter_string(s, "audio_encoder",
                                            &ret);
      break;
    case BG_STREAM_TEXT:
      if(!encode_to_video(plugin_reg, s, BG_STREAM_TEXT, stream_mask))
        bg_cfg_section_get_parameter_string(s, "text_encoder",
                                            &ret);
      break;
    case BG_STREAM_OVERLAY:
      if(!encode_to_video(plugin_reg, s, BG_STREAM_OVERLAY,
                          stream_mask))
        bg_cfg_section_get_parameter_string(s, "overlay_encoder",
                                            &ret);
      break;
    case BG_STREAM_VIDEO:
      bg_cfg_section_get_parameter_string(s, "video_encoder", &ret);
      break;
    }
  return ret;
  }

void
bg_encoder_section_get_plugin_config(bg_plugin_registry_t * plugin_reg,
                                     bg_cfg_section_t * s,
                                     bg_stream_type_t stream_type,
                                     int stream_mask,
                                     bg_cfg_section_t ** section_ret,
                                     const bg_parameter_info_t ** params_ret)
  {
  const char * plugin_name;
  const bg_plugin_info_t * info;

  plugin_name =
    bg_encoder_section_get_plugin(plugin_reg,
                                  s, stream_type,
                                  stream_mask);
  
  if(section_ret)
    *section_ret = NULL;
  if(params_ret)
    *params_ret = NULL;

  if(!plugin_name)
    return;

  info = bg_plugin_find_by_name(plugin_reg, plugin_name);

  if(!info->parameters)
    return;
  
  if(params_ret)
    {
    *params_ret = info->parameters;
    }
  
  switch(stream_type)
    {
    case BG_STREAM_AUDIO:
      if(section_ret)
        {
        *section_ret = bg_cfg_section_find_subsection(s, "audio_encoder");
        *section_ret = bg_cfg_section_find_subsection(*section_ret, plugin_name);
        }
      break;
    case BG_STREAM_TEXT:
      if(section_ret)
        {
        *section_ret = bg_cfg_section_find_subsection(s, "text_encoder");
        *section_ret = bg_cfg_section_find_subsection(*section_ret, plugin_name);
        }
      break;
    case BG_STREAM_OVERLAY:
      if(section_ret)
        {
        *section_ret = bg_cfg_section_find_subsection(s, "overlay_encoder");
        *section_ret = bg_cfg_section_find_subsection(*section_ret, plugin_name);
        }
      break;
    case BG_STREAM_VIDEO:
      if(section_ret)
        {
        *section_ret = bg_cfg_section_find_subsection(s, "video_encoder");
        *section_ret = bg_cfg_section_find_subsection(*section_ret, plugin_name);
        }
      break;
    }
  
  }

void
bg_encoder_section_get_stream_config(bg_plugin_registry_t * plugin_reg,
                                     bg_cfg_section_t * s,
                                     bg_stream_type_t stream_type,
                                     int stream_mask,
                                     bg_cfg_section_t ** section_ret,
                                     const bg_parameter_info_t ** params_ret)
  {
  const char * plugin_name;
  const bg_plugin_info_t * info;
  bg_cfg_section_t * subsection = NULL;
  
  int to_video = 0;
  
  plugin_name =
    bg_encoder_section_get_plugin(plugin_reg,
                                  s, stream_type,
                                  stream_mask);
  
  if(!plugin_name)
    {
    plugin_name =
      bg_encoder_section_get_plugin(plugin_reg,
                                    s, BG_STREAM_VIDEO,
                                    stream_mask);
    
    subsection = bg_cfg_section_find_subsection(s, "video_encoder");
    
    to_video = 1;
    }

  info = bg_plugin_find_by_name(plugin_reg, plugin_name);
  
  if(section_ret)
    *section_ret = NULL;
  if(params_ret)
    *params_ret = NULL;
  
  switch(stream_type)
    {
    case BG_STREAM_AUDIO:
      if(params_ret)
        *params_ret = info->audio_parameters;
      
      if(section_ret && info->audio_parameters)
        {
        if(!subsection)
          subsection = bg_cfg_section_find_subsection(s, "audio_encoder");
        subsection = bg_cfg_section_find_subsection(subsection, plugin_name);
        *section_ret = bg_cfg_section_find_subsection(subsection, "$audio");
        }

      break;
    case BG_STREAM_TEXT:
      if(params_ret)
        *params_ret = info->text_parameters;

      if(section_ret && info->text_parameters)
        {
        if(!subsection)
          subsection = bg_cfg_section_find_subsection(s, "text_encoder");
        subsection = bg_cfg_section_find_subsection(subsection, plugin_name);
        *section_ret = bg_cfg_section_find_subsection(subsection, "$text");
        
        }
      
      break;
    case BG_STREAM_OVERLAY:
      if(params_ret)
        *params_ret = info->overlay_parameters;

      if(section_ret && info->overlay_parameters)
        {
        if(!subsection)
          subsection = bg_cfg_section_find_subsection(s, "overlay_encoder");
        subsection = bg_cfg_section_find_subsection(subsection, plugin_name);
        *section_ret = bg_cfg_section_find_subsection(subsection, "$overlay");
        
        }
      
      break;
    case BG_STREAM_VIDEO:
      if(params_ret)
        *params_ret = info->video_parameters;

      if(section_ret && info->video_parameters)
        {
        if(!subsection)
          subsection = bg_cfg_section_find_subsection(s, "video_encoder");
        subsection = bg_cfg_section_find_subsection(subsection, plugin_name);
        *section_ret = bg_cfg_section_find_subsection(subsection, "$video");
        }
      break;
    }
  }

bg_cfg_section_t *
bg_encoder_section_get_from_registry(bg_plugin_registry_t * plugin_reg,
                                     const bg_parameter_info_t * parameters,
                                     uint32_t type_mask,
                                     uint32_t flag_mask)
  {
  int i, j;
  const bg_plugin_info_t * info;
  
  bg_parameter_info_t * parameters_priv = NULL;
  bg_cfg_section_t * ret;
  bg_cfg_section_t * s_src;
  bg_cfg_section_t * s_dst;
  
  if(!parameters)
    {
    parameters_priv =
      bg_plugin_registry_create_encoder_parameters(plugin_reg, type_mask, flag_mask, 1);
    parameters = parameters_priv;
    }

  /* Create section */
  ret = bg_cfg_section_create_from_parameters("encoders", parameters);
  //  bg_cfg_section_dump(ret, "encoder_section_1.xml");
  
  /* Get the plugin defaults */

  if(type_mask & BG_STREAM_AUDIO)
    {
    info = bg_plugin_registry_get_default(plugin_reg,
                                          BG_PLUGIN_ENCODER_AUDIO, flag_mask);
    bg_cfg_section_set_parameter_string(ret, "audio_encoder", info->name);

    bg_cfg_section_get_parameter_int(plugin_reg->config_section,
                                     "encode_audio_to_video", &i);
    bg_cfg_section_set_parameter_int(ret, "encode_audio_to_video", i);

    i = 0;
    while(strcmp(parameters[i].name, "audio_encoder"))
      i++;
    j = 0;
    while(parameters[i].multi_names[j])
      {
      s_src = bg_cfg_section_find_subsection(plugin_reg->config_section,
                                             parameters[i].multi_names[j]);

      s_dst = bg_cfg_section_find_subsection(ret, "audio_encoder");
      s_dst = bg_cfg_section_find_subsection(s_dst, parameters[i].multi_names[j]);
      bg_cfg_section_transfer(s_src, s_dst);
      j++;
      }
    }
  if(type_mask & BG_STREAM_TEXT)
    {
    info = bg_plugin_registry_get_default(plugin_reg,
                                          BG_PLUGIN_ENCODER_TEXT, flag_mask);
    bg_cfg_section_set_parameter_string(ret, "text_encoder", info->name);

    bg_cfg_section_get_parameter_int(plugin_reg->config_section,
                                     "encode_text_to_video", &i);
    bg_cfg_section_set_parameter_int(ret, "encode_text_to_video", i);

    i = 0;
    while(strcmp(parameters[i].name, "text_encoder"))
      i++;
    j = 0;
    while(parameters[i].multi_names[j])
      {
      s_src = bg_cfg_section_find_subsection(plugin_reg->config_section,
                                             parameters[i].multi_names[j]);

      s_dst = bg_cfg_section_find_subsection(ret, "text_encoder");
      s_dst = bg_cfg_section_find_subsection(s_dst, parameters[i].multi_names[j]);
      bg_cfg_section_transfer(s_src, s_dst);
      j++;
      }
    
    }
  if(type_mask & BG_STREAM_OVERLAY)
    {
    info = bg_plugin_registry_get_default(plugin_reg,
                                          BG_PLUGIN_ENCODER_OVERLAY, flag_mask);
    bg_cfg_section_set_parameter_string(ret, "overlay_encoder", info->name);

    bg_cfg_section_get_parameter_int(plugin_reg->config_section,
                                     "encode_overlay_to_video", &i);
    bg_cfg_section_set_parameter_int(ret, "encode_overlay_to_video", i);

    i = 0;
    while(strcmp(parameters[i].name, "overlay_encoder"))
      i++;
    j = 0;
    while(parameters[i].multi_names[j])
      {
      s_src = bg_cfg_section_find_subsection(plugin_reg->config_section,
                                             parameters[i].multi_names[j]);

      s_dst = bg_cfg_section_find_subsection(ret, "overlay_encoder");
      s_dst = bg_cfg_section_find_subsection(s_dst, parameters[i].multi_names[j]);
      bg_cfg_section_transfer(s_src, s_dst);
      j++;
      }
    }
  if(type_mask & BG_STREAM_VIDEO)
    {
    info = bg_plugin_registry_get_default(plugin_reg, BG_PLUGIN_ENCODER_VIDEO, flag_mask);
    bg_cfg_section_set_parameter_string(ret, "video_encoder", info->name);

    i = 0;
    while(strcmp(parameters[i].name, "video_encoder"))
      i++;
    j = 0;
    while(parameters[i].multi_names[j])
      {
      s_src = bg_cfg_section_find_subsection(plugin_reg->config_section,
                                             parameters[i].multi_names[j]);

      s_dst = bg_cfg_section_find_subsection(ret, "video_encoder");
      s_dst = bg_cfg_section_find_subsection(s_dst, parameters[i].multi_names[j]);
      bg_cfg_section_transfer(s_src, s_dst);
      j++;
      }
    }
  
  if(parameters_priv)
    bg_parameter_info_destroy_array(parameters_priv);

  //  bg_cfg_section_dump(ret, "encoder_section_2.xml");
  
  return ret;
  }
                                   
void
bg_encoder_section_store_in_registry(bg_plugin_registry_t * plugin_reg,
                                     bg_cfg_section_t * s,
                                     const bg_parameter_info_t * parameters,
                                     uint32_t type_mask,
                                     uint32_t flag_mask)
  {
  int i, j;
  const char * name;
  
  bg_parameter_info_t * parameters_priv = NULL;
  bg_cfg_section_t * s_src;
  bg_cfg_section_t * s_dst;
  
  if(!parameters)
    {
    parameters_priv =
      bg_plugin_registry_create_encoder_parameters(plugin_reg,
                                                   type_mask, flag_mask, 1);
    parameters = parameters_priv;
    }
  
  /* Get the plugin defaults */

  if(type_mask & BG_STREAM_AUDIO)
    {
    bg_cfg_section_get_parameter_string(s, "audio_encoder", &name);
    bg_plugin_registry_set_default(plugin_reg, BG_PLUGIN_ENCODER_AUDIO,
                                   flag_mask, name);

    bg_cfg_section_get_parameter_int(s, "encode_audio_to_video", &i);
    bg_cfg_section_set_parameter_int(plugin_reg->config_section,
                                     "encode_audio_to_video", i);
    
    i = 0;
    while(strcmp(parameters[i].name, "audio_encoder"))
      i++;
    j = 0;
    while(parameters[i].multi_names[j])
      {
      s_dst = bg_cfg_section_find_subsection(plugin_reg->config_section,
                                             parameters[i].multi_names[j]);

      s_src = bg_cfg_section_find_subsection(s, "audio_encoder");
      s_src = bg_cfg_section_find_subsection(s_src,
                                             parameters[i].multi_names[j]);
      bg_cfg_section_transfer(s_src, s_dst);
      j++;
      }
    }
  if(type_mask & BG_STREAM_TEXT)
    {
    bg_cfg_section_get_parameter_string(s, "text_encoder",
                                        &name);
    bg_plugin_registry_set_default(plugin_reg,
                                   BG_PLUGIN_ENCODER_TEXT,
                                   flag_mask, name);
    
    bg_cfg_section_get_parameter_int(s, "encode_text_to_video", &i);
    bg_cfg_section_set_parameter_int(plugin_reg->config_section,
                                     "encode_text_to_video", i);
    
    i = 0;
    while(strcmp(parameters[i].name, "text_encoder"))
      i++;
    j = 0;
    while(parameters[i].multi_names[j])
      {
      s_dst = bg_cfg_section_find_subsection(plugin_reg->config_section,
                                             parameters[i].multi_names[j]);

      s_src = bg_cfg_section_find_subsection(s, "text_encoder");
      s_src = bg_cfg_section_find_subsection(s_src,
                                             parameters[i].multi_names[j]);
      bg_cfg_section_transfer(s_src, s_dst);
      j++;
      }
    
    }
  if(type_mask & BG_STREAM_OVERLAY)
    {
    bg_cfg_section_get_parameter_string(s, "overlay_encoder", &name);
    bg_plugin_registry_set_default(plugin_reg,
                                   BG_PLUGIN_ENCODER_OVERLAY,
                                   flag_mask, name);
    
    bg_cfg_section_get_parameter_int(s, "encode_overlay_to_video", &i);
    bg_cfg_section_set_parameter_int(plugin_reg->config_section,
                                     "encode_overlay_to_video", i);
    
    i = 0;
    while(strcmp(parameters[i].name, "overlay_encoder"))
      i++;
    j = 0;
    while(parameters[i].multi_names[j])
      {
      s_dst = bg_cfg_section_find_subsection(plugin_reg->config_section,
                                             parameters[i].multi_names[j]);

      s_src = bg_cfg_section_find_subsection(s, "overlay_encoder");
      s_src = bg_cfg_section_find_subsection(s_src,
                                             parameters[i].multi_names[j]);
      bg_cfg_section_transfer(s_src, s_dst);
      j++;
      }
    }
  if(type_mask & BG_STREAM_VIDEO)
    {
    bg_cfg_section_get_parameter_string(s, "video_encoder", &name);
    bg_plugin_registry_set_default(plugin_reg, BG_PLUGIN_ENCODER_VIDEO,
                                   flag_mask, name);
    
    i = 0;
    while(strcmp(parameters[i].name, "video_encoder"))
      i++;
    j = 0;
    while(parameters[i].multi_names[j])
      {
      s_dst = bg_cfg_section_find_subsection(plugin_reg->config_section,
                                             parameters[i].multi_names[j]);

      s_src = bg_cfg_section_find_subsection(s, "video_encoder");
      s_src = bg_cfg_section_find_subsection(s_src,
                                             parameters[i].multi_names[j]);
      bg_cfg_section_transfer(s_src, s_dst);
      j++;
      }
    }
  
  if(parameters_priv)
    bg_parameter_info_destroy_array(parameters_priv);
  
  }

static const bg_parameter_info_t compressor_parameters[] =
  {
    {
      .name = "codec",
      .long_name = TRS("Codec"),
      .type = BG_PARAMETER_MULTI_MENU,
      .flags = BG_PARAMETER_PLUGIN,
      .val_default = { .val_str = "none" },
      .multi_names = (const char *[]){ "none", NULL },
      .multi_labels = (const char *[]){ TRS("None"), NULL },
      .multi_descriptions = (const char *[]){ TRS("Write stream as uncompressed if possible"), NULL },
      .multi_parameters = (const bg_parameter_info_t*[]) { NULL, NULL },
    },
    { /* End */ },
  };


bg_parameter_info_t *
bg_plugin_registry_create_compressor_parameters(bg_plugin_registry_t * plugin_reg,
                                                uint32_t flag_mask)
  {
  bg_parameter_info_t * ret =
    bg_parameter_info_copy_array(compressor_parameters);
  bg_plugin_registry_set_parameter_info(plugin_reg, BG_PLUGIN_CODEC,
                                        flag_mask, ret);
  return ret;
  }

void
bg_plugin_registry_set_compressor_parameter(bg_plugin_registry_t * plugin_reg,
                                            bg_plugin_handle_t ** plugin,
                                            const char * name,
                                            const bg_parameter_value_t * val)
  {
  if(!name)
    {
    if(*plugin && (*plugin)->plugin->set_parameter)
      (*plugin)->plugin->set_parameter((*plugin)->priv, NULL, NULL);
    return;
    }

  if(!strcmp(name, "codec"))
    {
    if(*plugin && (!val->val_str || strcmp((*plugin)->info->name, val->val_str)))
      {
      bg_plugin_unref(*plugin);
      *plugin = NULL;
      }

    if(val->val_str && !strcmp(val->val_str, "none"))
      return;
    
    if(val->val_str && !(*plugin))
      {
      const bg_plugin_info_t * info;
      info = bg_plugin_find_by_name(plugin_reg, val->val_str);
      if(!info)
        {
        bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Cannot find plugin %s",
               val->val_str);
        return;
        }
      *plugin = bg_plugin_load(plugin_reg, info);
      }
    
    }
  else
    {
    if(*plugin)
      (*plugin)->plugin->set_parameter((*plugin)->priv, name, val);
    }
  
  }

gavl_codec_id_t
bg_plugin_registry_get_compressor_id(bg_plugin_registry_t * plugin_reg,
                                     bg_cfg_section_t * section)
  {
  const bg_plugin_info_t * info;
  const char * codec = NULL;
  if(!bg_cfg_section_get_parameter_string(section, "codec", &codec))
    return GAVL_CODEC_ID_NONE;

  info = bg_plugin_find_by_name(plugin_reg, codec);
  if(!info)
    return GAVL_CODEC_ID_NONE;
  return info->compressions[0];
  
  }
