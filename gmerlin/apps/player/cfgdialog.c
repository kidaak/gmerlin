#include "gmerlin.h"

void gmerlin_create_dialog(gmerlin_t * g)
  {
  bg_parameter_info_t * parameters;
  /* Create the dialog */
    
  g->cfg_dialog = bg_dialog_create_multi("Gmerlin confiuration");
  //  g->cfg_dialog = bg_dialog_create_multi();
  /* Add sections */
  
  parameters = display_get_parameters(g->player_window->display);

  bg_cfg_section_apply(g->display_section, parameters,
                       display_set_parameter, (void*)(g->player_window->display));
  bg_dialog_add(g->cfg_dialog,
                "Display",
                g->display_section,
                display_set_parameter,
                (void*)(g->player_window->display),
                parameters);

  parameters = bg_media_tree_get_parameters(g->tree);
  bg_cfg_section_apply(g->tree_section, parameters,
                       bg_media_tree_set_parameter, (void*)(g->tree));
  
  bg_dialog_add(g->cfg_dialog,
                "Media Tree",
                g->tree_section,
                bg_media_tree_set_parameter,
                (void*)(g->tree),
                parameters);

  g->general_section = bg_cfg_registry_find_section(g->cfg_reg, "General");
  parameters = gmerlin_get_parameters(g);

  bg_cfg_section_apply(g->general_section, parameters,
                       gmerlin_set_parameter, (void*)(g));
  bg_dialog_add(g->cfg_dialog,
                (char*)0,
                g->general_section,
                gmerlin_set_parameter,
                (void*)(g),
                parameters);

  g->audio_section = bg_cfg_registry_find_section(g->cfg_reg, "Audio");
  parameters = bg_player_get_audio_parameters(g->player);
  
  bg_cfg_section_apply(g->audio_section, parameters,
                       bg_player_set_audio_parameter, (void*)(g->player));
  bg_dialog_add(g->cfg_dialog,
                (char*)0,
                g->audio_section,
                bg_player_set_audio_parameter,
                (void*)(g->player),
                parameters);


  }


void gmerlin_configure(gmerlin_t * g)
  {
  bg_dialog_show(g->cfg_dialog);
  }
