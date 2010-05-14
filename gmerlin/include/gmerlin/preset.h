/*****************************************************************
 * gmerlin - a general purpose multimedia framework and applications
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

typedef struct bg_preset_s
  {
  char * file;
  char * name;
  struct bg_preset_s * next;
  } bg_preset_t;

bg_preset_t * bg_presets_load(const char * preset_path);

bg_preset_t * bg_preset_append(bg_preset_t *,
                               const char * preset_path,
                               char * name);

void bg_presets_destroy(bg_preset_t *);

bg_cfg_section_t * bg_preset_load(bg_preset_t * p);

void bg_preset_save(bg_preset_t * p, bg_cfg_section_t * s);
