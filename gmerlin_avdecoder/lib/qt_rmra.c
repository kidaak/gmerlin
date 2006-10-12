/*****************************************************************
 
  qt_rmra.c
 
  Copyright (c) 2003-2006 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de
 
  http://gmerlin.sourceforge.net
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 
*****************************************************************/

#include <string.h>
#include <stdlib.h>

#include <avdec_private.h>
#include <qt.h>

int bgav_qt_rmra_read(qt_atom_header_t * h,
                      bgav_input_context_t * input, qt_rmra_t * ret)
  {
  qt_atom_header_t ch;
  memcpy(&(ret->h), h, sizeof(*h));

  while(input->position < h->start_position + h->size)
    {
    if(!bgav_qt_atom_read_header(input, &ch))
      return 0;
    switch(ch.fourcc)
      {
      case BGAV_MK_FOURCC('r', 'm', 'd', 'a'):
        ret->rmda =
          realloc(ret->rmda, sizeof(*ret->rmda) * (ret->num_rmda+1));
        
        if(!bgav_qt_rmda_read(&ch, input, ret->rmda+ret->num_rmda))
          return 0;
        ret->num_rmda++;
        break;
      default:
        bgav_qt_atom_skip_unknown(input, &ch, h->fourcc);
        break;
        
      }
    }
  return 1;
  }

void bgav_qt_rmra_free(qt_rmra_t * r)
  {
  int i;
  for(i = 0; i < r->num_rmda; i++)
    bgav_qt_rmda_free(r->rmda+i);
  if(r->rmda)
    free(r->rmda);
  }

void bgav_qt_rmra_dump(int indent, qt_rmra_t * c)
  {
  int i;
  bgav_diprintf(indent, "rmra\n");

  for(i = 0; i < c->num_rmda; i++)
    {
    bgav_qt_rmda_dump(indent+2, c->rmda+i);
    }
  bgav_diprintf(indent, "end of rmra\n");
  }
