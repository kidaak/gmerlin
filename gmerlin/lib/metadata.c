/*****************************************************************
 
  metadata.c
 
  Copyright (c) 2003-2004 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de
 
  http://gmerlin.sourceforge.net
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 
*****************************************************************/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <parameter.h>
#include <streaminfo.h>
#include <utils.h>

#define MY_FREE(ptr) \
  if(ptr) \
    { \
    free(ptr); \
    ptr = NULL; \
    }

void bg_metadata_free(bg_metadata_t * m)
  {
  MY_FREE(m->artist);
  MY_FREE(m->title);
  MY_FREE(m->album);
  MY_FREE(m->genre);
  MY_FREE(m->comment);
  MY_FREE(m->author);
  MY_FREE(m->copyright);
  MY_FREE(m->date);

  memset(m, 0, sizeof(*m));
  }

#define MY_STRCPY(s) \
  dst->s = bg_strdup(dst->s, src->s);

#define MY_INTCOPY(i) \
  dst->i = src->i;

void bg_metadata_copy(bg_metadata_t * dst, const bg_metadata_t * src)
  {
  MY_STRCPY(artist);
  MY_STRCPY(title);
  MY_STRCPY(album);

  MY_STRCPY(date);
  MY_STRCPY(genre);
  MY_STRCPY(comment);

  MY_STRCPY(author);
  MY_STRCPY(copyright);
  MY_INTCOPY(track);
  }

#undef MY_STRCPY
#undef MY_INTCPY

static bg_parameter_info_t parameters[] =
  {
    {
      name:      "artist",
      long_name: "Artist",
      type:      BG_PARAMETER_STRING,
    },
    {
      name:      "title",
      long_name: "Title",
      type:      BG_PARAMETER_STRING,
    },
    {
      name:      "album",
      long_name: "Album",
      type:      BG_PARAMETER_STRING,
    },
    {
      name:      "track",
      long_name: "Track",
      type:      BG_PARAMETER_INT,
    },
    {
      name:      "genre",
      long_name: "Genre",
      type:      BG_PARAMETER_STRING,
    },
    {
      name:      "author",
      long_name: "Author",
      type:      BG_PARAMETER_STRING,
    },
    {
      name:      "copyright",
      long_name: "Copyright",
      type:      BG_PARAMETER_STRING,
    },
    {
      name:        "date",
      long_name:   "Date",
      type:        BG_PARAMETER_STRING,
      help_string: "Complete date or year only"
    },
    {
      name:      "comment",
      long_name: "Comment",
      type:      BG_PARAMETER_STRING,
    },
    { /* End of parameters */ }
  };

#define SP_STR(s) if(!strcmp(ret[i].name, # s)) \
    ret[i].val_default.val_str = bg_strdup(ret[i].val_default.val_str, m->s)

#define SP_INT(s) if(!strcmp(ret[i].name, # s)) \
    ret[i].val_default.val_i = m->s;

bg_parameter_info_t * bg_metadata_get_parameters(bg_metadata_t * m)
  {
  int i;
  
  bg_parameter_info_t * ret;
  ret = bg_parameter_info_copy_array(parameters);

  if(!m)
    return ret;
  
  i = 0;
  while(ret[i].name)
    {
    SP_STR(artist);
    SP_STR(title);
    SP_STR(album);
      
    SP_INT(track);
    SP_STR(date);
    SP_STR(genre);
    SP_STR(comment);

    SP_STR(author);
    SP_STR(copyright);

    i++;
    }
  return ret;
  }

#undef SP_STR
#undef SP_INT

#define SP_STR(s) if(!strcmp(name, # s))                               \
    { \
    m->s = bg_strdup(m->s, val->val_str); \
    return; \
    }
    
#define SP_INT(s) if(!strcmp(name, # s)) \
    { \
    m->s = val->val_i; \
    }

void bg_metadata_set_parameter(void * data, char * name,
                               bg_parameter_value_t * val)
  {
  bg_metadata_t * m = (bg_metadata_t*)data;
  
  if(!name)
    return;

  SP_STR(artist);
  SP_STR(title);
  SP_STR(album);
  
  SP_INT(track);
  SP_STR(date);
  SP_STR(genre);
  SP_STR(comment);
  
  SP_STR(author);
  SP_STR(copyright);
  }

#undef SP_STR
#undef SP_INT

/* Tries to get a 4 digit year from an arbitrary formatted
   date string.
   Return 0 is this wasn't possible.
*/

static int check_year(const char * pos1)
  {
  if(isdigit(pos1[0]) &&
     isdigit(pos1[1]) &&
     isdigit(pos1[2]) &&
     isdigit(pos1[3]))
    {
    return
      (int)(pos1[0] -'0') * 1000 + 
      (int)(pos1[1] -'0') * 100 + 
      (int)(pos1[2] -'0') * 10 + 
      (int)(pos1[3] -'0');
    }
  return 0;
  }

int bg_metadata_get_year(const bg_metadata_t * m)
  {
  int result;

  const char * pos1;
  
  if(!m->date)
    return 0;

  pos1 = m->date;

  while(1)
    {
    /* Skip nondigits */
    
    while(!isdigit(*pos1) && (*pos1 != '\0'))
      pos1++;
    if(pos1 == '\0')
      return 0;

    /* Check if we have a 4 digit number */
    result = check_year(pos1);
    if(result)
      return result;

    /* Skip digits */

    while(isdigit(*pos1) && (*pos1 != '\0'))
      pos1++;
    if(pos1 == '\0')
      return 0;
    }
  return 0;
  }

/*
 *  %p:    Artist
 *  %a:    Album
 *  %g:    Genre
 *  %t:    Track name
 *  %<d>n: Track number (d = number of digits, 1-9)
 *  %y:    Year
 *  %c:    Comment
 */

char * bg_create_track_name(const bg_metadata_t * metadata,
                            const char * format)
  {
  char * buf;
  const char * end;
  const char * f;
  char * ret = (char*)0;
  char track_format[5];
  f = format;

  while(*f != '\0')
    {
    end = f;
    while((*end != '%') && (*end != '\0'))
      end++;
    if(end != f)
      ret = bg_strncat(ret, f, end);

    if(*end == '%')
      {
      end++;

      /* %p:    Artist */
      if(*end == 'p')
        {
        end++;
        if(metadata->artist)
          {
          //          fprintf(stderr,  "Artist: %s ", metadata->artist);
          ret = bg_strcat(ret, metadata->artist);
          //          fprintf(stderr,  "%s\n", ret);
          }
        else
          {
          //          fprintf(stderr, "No Artist\n");
          goto fail;
          }
        }
      /* %a:    Album */
      else if(*end == 'a')
        {
        end++;
        if(metadata->album)
          ret = bg_strcat(ret, metadata->album);
        else
          goto fail;
        }
      /* %g:    Genre */
      else if(*end == 'g')
        {
        end++;
        if(metadata->genre)
          ret = bg_strcat(ret, metadata->genre);
        else
          goto fail;
        }
      /* %t:    Track name */
      else if(*end == 't')
        {
        end++;
        if(metadata->title)
          {
          //          fprintf(stderr, "Ret: %s\n", ret);
          ret = bg_strcat(ret, metadata->title);
          //          fprintf(stderr, "Title: %s\n", metadata->title);
          }
        else
          {
          //          fprintf(stderr, "No Title\n");
          goto fail;
          }
        }
      /* %c:    Comment */
      else if(*end == 'c')
        {
        end++;
        if(metadata->comment)
          ret = bg_strcat(ret, metadata->comment);
        else
          goto fail;
        }
      /* %y:    Year */
      else if(*end == 'y')
        {
        end++;
        if(metadata->date)
          {
          buf = bg_sprintf("%d", bg_metadata_get_year(metadata));
          ret = bg_strcat(ret, buf);
          free(buf);
          }
        else
          goto fail;
        }
      /* %<d>n: Track number (d = number of digits, 1-9) */
      else if(isdigit(*end) && end[1] == 'n')
        {
        if(metadata->track)
          {
          track_format[0] = '%';
          track_format[1] = '0';
          track_format[2] = *end;
          track_format[3] = 'd';
          track_format[4] = '\0';
          
          buf = bg_sprintf(track_format, metadata->track);
          ret = bg_strcat(ret, buf);
          free(buf);
          end+=2;
          }
        else
          goto fail;
        }
      else
        {
        ret = bg_strcat(ret, "%");
        end++;
        }
      f = end;
      }
    }
  return ret;
  fail:
  if(ret)
    free(ret);
  return (char*)0;
  }
