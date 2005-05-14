/*****************************************************************

  cdaudio.c

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
#include <stdio.h>
#include <string.h>

#include <utils.h>

#include <cdio/cdio.h>
#include <cdio/cdtext.h>

#include "cdaudio.h"

#define GET_FIELD(dst,key) \
  dst = bg_strdup(dst, cdtext_get_const(key, cdtext));

#define GET_FIELD_DEFAULT(dst,key)                                      \
  field = cdtext_get_const(key, cdtext);                                \
  if(field)                                                             \
    {                                                                   \
    info[idx->tracks[i].index].metadata.dst = bg_strdup(info[idx->tracks[i].index].metadata.dst, field); \
    if(!info[idx->tracks[i].index].metadata.dst)                        \
      info[idx->tracks[i].index].metadata.dst = bg_strdup(info[idx->tracks[i].index].metadata.dst, dst); \
    }

int bg_cdaudio_get_metadata_cdtext(CdIo_t * cdio,
                                   bg_track_info_t * info,
                                   bg_cdaudio_index_t* idx)
  {
  int i;
  const char * field;
    
  /* Global information */

  char * artist = (char*)0;
  char * author = (char*)0;
  char * album = (char*)0;
  char * genre = (char*)0;
  const cdtext_t * cdtext;

  /* Get information for the whole disc */
  
  cdtext = cdio_get_cdtext (cdio, 0);

  if(!cdtext)
    {
    //    fprintf(stderr, 
    return 0;
    }
  
  GET_FIELD(artist,CDTEXT_PERFORMER);
  GET_FIELD(author,CDTEXT_SONGWRITER);
  GET_FIELD(author,CDTEXT_COMPOSER); /* Composer overwrites songwriter */
  GET_FIELD(album,CDTEXT_TITLE);
  GET_FIELD(genre,CDTEXT_GENRE);
  
  for(i = 0; i < idx->num_tracks; i++)
    {
    if(idx->tracks[i].is_audio)
      {
      cdtext = cdio_get_cdtext (cdio, i+1);
      if(!cdtext)
        return 0;
      
      GET_FIELD(info[idx->tracks[i].index].metadata.title, CDTEXT_TITLE);

      if(!info[idx->tracks[i].index].metadata.title)
        return 0;

      GET_FIELD_DEFAULT(artist, CDTEXT_PERFORMER);
      GET_FIELD_DEFAULT(author, CDTEXT_SONGWRITER);
      GET_FIELD_DEFAULT(author, CDTEXT_COMPOSER);
      GET_FIELD_DEFAULT(genre,  CDTEXT_GENRE);
      info[idx->tracks[i].index].metadata.album =
        bg_strdup(info[idx->tracks[i].index].metadata.album,
                  album);
      }
    }
  return 1;
  
  }

#undef GET_FIELD
#undef GET_FIELD_DEFAULT
